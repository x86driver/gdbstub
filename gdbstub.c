#include <stdio.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>
#include "gdbstub.h"
#include "wrapper.h"

struct GDBState {
    int fd;
    struct CPUState *env;
};

static inline uint8_t tohex(uint8_t value)
{
    return (value > 9 ? value + 'W' : value + '0');
}

static void cpu_read_registers(struct GDBState *s, char *buf)
{
    size_t i;
    uint32_t fpa_len;
    uint32_t regs[CPU_GP_REGS];
    uint32_t cpsr;
    uint8_t *regptr = (uint8_t*)&regs[0];
    uint8_t tmp;

    for (i = 0; i < CPU_GP_REGS; ++i) {  /* general purpose r0-r15 */
        regs[i] = get_reg(s->env, i);
    }

    for (i = 0; i < sizeof(regs); ++i) {
        tmp = *regptr++;
        *buf++ = tohex(tmp >> 4);
        *buf++ = tohex(tmp & 0x0f);
    }

    fpa_len = (((CPU_FPA_REGS * 12) + 4) * 2);
    memset(buf, '0', fpa_len);
    buf += fpa_len;
#if 0
    for (i = 0; i < (CPU_FPA_REGS * 12); ++i) { /* FPA register, 8 * 12 bytes */
        *buf++ = '0';
        *buf++ = '0';
    }

    for (i = 0; i < 4; ++i) {   /* FPA status register, 4 bytes */
        *buf++ = '0';
        *buf++ = '0';
    }
#endif

    cpsr = get_reg(s->env, 16);
    regptr = (uint8_t*)&cpsr;
    for (i = 0; i < sizeof(cpsr); ++i) {    /* CPSR */
        tmp = *regptr++;
        *buf++ = tohex(tmp >> 4);
        *buf++ = tohex(tmp & 0x0f);
    }

    *buf = '\0';
}

static void cpu_read_memory(struct GDBState *s, uint32_t addr, uint32_t len, char *buf)
{
    uint32_t i;
    uint32_t mem[MAX_PACKET_LEN / 8];
    uint8_t *memptr = (uint8_t*)&mem[0];
    uint8_t tmp;

    if ((len * 2) > (MAX_PACKET_LEN / 8)) { /* one byte must use two byte ascii */
        printf("read memory greater than MAX_PACKET_LEN: %d\n", len);
        return;
    }

    for (i = 0; i < len / 4; ++i) {
        mem[i] = get_mem(s->env, addr);
        addr += 4;
    }

    for (i = 0; i < len; ++i) {
        tmp = *memptr++;
        *buf++ = tohex(tmp >> 4);
        *buf++ = tohex(tmp & 0x0f);
    }

    *buf = '\0';
}

static void gdbserver_accept(struct GDBState *s)
{
    struct sockaddr_in sockaddr;
    socklen_t len;
    int val, fd;

    for (;;) {
        len = sizeof(sockaddr);
        fd = accept(s->fd, (struct sockaddr *)&sockaddr, &len);
        if (fd < 0 && errno != EINTR) {
            perror("accept");
            return;
        } else if (fd >= 0) {
            break;
        }
    }

    /* set short latency */
    val = 1;
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (char *)&val, sizeof(val));

    s->fd = fd;

    fcntl(fd, F_SETFL, O_NONBLOCK);
}

static int gdbserver_open(int port)
{
    struct sockaddr_in sockaddr;
    int fd, val, ret;

    fd = socket(PF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        return -1;
    }

    val = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char *)&val, sizeof(val));

    sockaddr.sin_family = AF_INET;
    sockaddr.sin_port = htons(port);
    sockaddr.sin_addr.s_addr = 0;

    ret = bind(fd, (struct sockaddr *)&sockaddr, sizeof(sockaddr));
    if (ret < 0) {
        perror("bind");
        close(fd);
        return -1;
    }

    ret = listen(fd, 0);
    if (ret < 0) {
        perror("listen");
        close(fd);
        return -1;
    }

    return fd;
}

static int get_char(struct GDBState *s)
{
    uint8_t ch;
    int ret;
    ret = read(s->fd, &ch, 1);
    if (ret < 0) {
        if (errno == EINTR || errno == EAGAIN)
            return -1;
        perror("recv");
        return -1;
    } else if (ret == 0) {
        return -1;
    }

    return ch;
}

static uint8_t do_checksum(char *ptr)
{
    uint8_t ret = 0;

    if (strlen(ptr) == 3)
        return 0;

    ptr += 2;
    do {
        ret += *ptr++;
    } while (*ptr != '#');

    return ret;
}

static void gdb_reply(struct GDBState *s, char *ptr)
{
    char outbuf[MAX_PACKET_LEN] = "";
    char *outptr = &outbuf[0];
    size_t len = strlen(ptr);
    uint8_t checksum;
    ssize_t ret;

    if (len > sizeof(outbuf)) {
        printf("%s: input size is too long\n", __FUNCTION__);
        exit(1);
    }

    *outptr++ = '+';
    *outptr++ = '$';
    strncpy(outptr, ptr, len);
    outptr += len;
    *outptr++ = '#';
    checksum = do_checksum(&outbuf[0]);
    *outptr++ = tohex(checksum >> 4);
    *outptr++ = tohex(checksum & 0x0f);
    *outptr = '\0';

    printf("reply: %s\n", outbuf);
    ret = write(s->fd, outbuf, strlen(outbuf));
    if (ret <= 0)   /* FIXME: strlen(outbuf) != 0 always */
        perror("write error:");
}

static int get_packet(struct GDBState *s, char *ptr)
{
    int count = 0;
    int ret;

    do {
        ret = get_char(s);
        if (ret > 0)
            *ptr++ = (char)ret;
        if (ret == '#')
            count = -3;
        ++count;
    } while (count != 0);

    *ptr = '\0';

/* TODO: must check checksum here

    uint8_t checksum = do_checksum(&buf[0]);
*/

//    printf("%s\n", buf);
//    gdb_reply(s, "PacketSize=1000");
    return 0;
}

static int gdbserver_main(struct GDBState *s)
{
    char buf[MAX_PACKET_LEN];
    char outbuf[MAX_PACKET_LEN];
    char *ptr;
    uint32_t addr, len;

    for (;;) {
        get_packet(s, &buf[0]);
        printf("%s\n", buf);
        ptr = &buf[3];
        switch (buf[2]) {
            case '?':
                gdb_reply(s, "S05");
                break;
            case 'g':
                cpu_read_registers(s, outbuf);
                gdb_reply(s, outbuf);
                break;
            case 'H':
                gdb_reply(s, "OK");
                break;
            case 'k':
                goto end_command;
            case 'm':
                /* +$m9200,4#98 */
                addr = strtoull(ptr, (char **)&ptr, 16);
                if (*ptr == ',')
                    ++ptr;
                len = strtoull(ptr, NULL, 16);
                cpu_read_memory(s, addr, len, outbuf);
                gdb_reply(s, outbuf);
                break;
            case 'p':
                outbuf[0] = '\0';
                gdb_reply(s, outbuf);
                break;
            case 'q':
                if (!strncmp(ptr, "Supported", 9)) {
                    snprintf(outbuf, sizeof(outbuf), "PacketSize=%x", MAX_PACKET_LEN);
                    gdb_reply(s, outbuf);
                } else if (*ptr == 'C')
                    gdb_reply(s, "QC1");
                else
                    gdb_reply(s, "");
                break;
            default:
                return 0;
        }
    }

end_command:

    return 0;
}

int main()
{
    struct GDBState *s;
    int fd = gdbserver_open(1234);

    if (fd == -1) {
        return -1;
    }

    s = (struct GDBState*)malloc(sizeof(struct GDBState));
    s->fd = fd;

    gdbserver_accept(s);
    gdbserver_main(s);

    printf("GDB stub had been done.\n");
    close(fd);
    free(s);
    return 0;
}

