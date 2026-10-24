TARGET = c_agent

CC = gcc
CFLAGS = -Wall -Wextra -pthread -g -MMD -MP
INCLUDES = -Iinclude

BUILD_DIR = c_build

IP ?= $(shell hostname -I | awk '{print $$1}')
PORT ?= 8000
CPU ?= 4
GPU ?= 1
RAM ?= 8192
THREADS ?= 4

SRCS = main.c \
	src/comms/event_handler.c \
	src/comms/resource_adapter.c \
	src/comms/server_init.c \
	src/comms/sys_epoll.c \
	src/comms/sys_socket.c \
	src/comms/thread_pool.c \
	src/resources/node_structures.c \
	src/resources/local_resource.c \
	src/resources/owned_jobs.c \
	src/resources/known_nodes.c \
	src/resources/queues.c \
	src/resources/tablahash.c

OBJS = $(addprefix $(BUILD_DIR)/,$(SRCS:.c=.o))
DEPS = $(OBJS:.o=.d)

.PHONY: all run clear

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

-include $(DEPS)

run: $(TARGET)
	@test -n "$(IP)" || (echo "No se pudo detectar IP; pasala con IP=..." && exit 1)
	./$(TARGET) "$(IP)" "$(PORT)" "$(CPU)" "$(RAM)" "$(GPU)" "$(THREADS)"

clear:
	@rm -rf $(BUILD_DIR) $(TARGET)