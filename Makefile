TARGET = gdbstub

gdbstub:gdbstub.c
	gcc -Wall -Wextra -o $@ $< -g

clean:
	rm -rf $(TARGET)
