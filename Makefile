TARGET = gdbstub wrapper.o
CFLAGS = -Wall -Wextra

all:$(TARGET)

wrapper.o:wrapper.c wrapper.h
	gcc $(CFLAGS) -c -o $@ $< -g

gdbstub:gdbstub.c wrapper.o
	gcc $(CFLAGS) -o $@ $^ -g

clean:
	rm -rf $(TARGET)
