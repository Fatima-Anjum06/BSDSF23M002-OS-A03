CC = gcc
CFLAGS = -Iinclude
SRC = src/main.c src/shell.c src/execute.c
OUT = myshell

all: $(OUT)

$(OUT): $(SRC)
	$(CC) $(CFLAGS) -o $(OUT) $(SRC)

clean:
	rm -f $(OUT)
