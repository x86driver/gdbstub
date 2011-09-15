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
};

#if 0
static void cpu_read_registers(char *buf)
{
    int i;
    int regs[CPU_REGS];

    for (i = 0; i < CPU_REGS; ++i) {
        regs[i] = get_reg(env, i);
    }
}
#endif

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

static uint8_t tohex(uint8_t value)
{
    return (value > 9 ? value + 'W' : value + '0');
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
    write(s->fd, outbuf, strlen(outbuf));
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
    char *ptr;

    for (;;) {
        get_packet(s, &buf[0]);
        printf("%s\n", buf);
        ptr = &buf[3];
        switch (buf[2]) {
            case '?':
                gdb_reply(s, "S05");
                break;
//            case 'g':
//                cpu_read_registers(buf);
//                gdb_reply(s, buf);
//                break;
            case 'H':
                gdb_reply(s, "");
                break;
            case 'q':
                if (!strncmp(ptr, "Supported", 9))
                    gdb_reply(s, "");
                else if (*ptr == 'C')
                    gdb_reply(s, "QC1");
                else
                    gdb_reply(s, "");
                break;
            default:
                return 0;
        }
    }

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

    close(fd);
    free(s);
    return 0;
}

