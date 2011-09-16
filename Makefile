TARGET = gdbstub
OBJS = gdbstub.o wrapper.o
CFLAGS = -Wall -Wextra

all:$(TARGET)

wrapper.o: wrapper.c wrapper.h gdbstub.h
	gcc $(CFLAGS) -c -o $@ $< -g

gdbstub.o: gdbstub.c gdbstub.h wrapper.h
	gcc $(CFLAGS) -c -o $@ $< -g

gdbstub: $(OBJS)
	gcc $(CFLAGS) -o $@ $^ -g

clean:
	rm -rf $(TARGET)
