TARGET = c_agent

CC = gcc
CFLAGS = -Wall -Wextra -pthread -g

INCLUDES = -Iinclude

BUILD_DIR = c_build

SRCS = main.c \
       src/comms/event_handler.c \
       src/comms/resource_adapter.c \
       src/comms/server_init.c \
       src/comms/sys_epoll.c \
       src/comms/sys_socket.c \
       src/comms/thread_pool.c \
       src/resources/node-structures.c \
       src/resources/queues.c \
       src/resources/tablahash.c

OBJS = $(SRCS:.c=.o)
OBJS := $(addprefix $(BUILD_DIR)/,$(OBJS))

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

run: $(TARGET)
	./$(TARGET) 127.0.0.1 8000 4 8192 1

clean:
	rm -rf $(BUILD_DIR)
	rm -f $(TARGET)

clear: clean

.PHONY: all run clean clear