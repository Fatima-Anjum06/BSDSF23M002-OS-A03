CC = gcc
CFLAGS = -Iinclude -Wall -Wextra -g
LDFLAGS = -lreadline
SRC = src/main.c src/shell.c src/execute.c
TARGET = myshell

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(LDFLAGS)

clean:
	rm -f $(TARGET)
