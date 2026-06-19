CC = gcc
CFLAGS = -Wall -Wextra -g -I./include

TARGET = agent
SRCS = main.c src/network_core.c
OBJS = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^
	@echo "Exec ./agent <port> <ip_lan>"

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)
	@echo "Limpieza completada."