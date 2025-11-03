kCC = cc
CFLAGS = -Iinclude -Wall -Wextra -g
SRC = src/main.c src/shell.c src/execute.c
LDFLAGS = -lreadline

myshell: $(SRC)
	$(CC) $(CFLAGS) -o myshell $(SRC) $(LDFLAGS)

clean:
	rm -f myshell

