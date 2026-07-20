TARGET = c_agent

CC = gcc
CFLAGS = -Wall -Wextra -pthread -g

# Si tus headers están en c_agent/include, esta línea es correcta
INCLUDES = -Iinclude

BUILD_DIR = c_build

IP ?= $(shell hostname -I | awk '{print $$1}')
PORT ?= 8001
CPU ?= 4
GPU ?= 1
RAM ?= 8192
THREADS ?= 4

# 1. SOLO ARCHIVOS DE PRODUCCIÓN (Cero tests aquí para evitar choques de main)
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

OBJS = $(SRCS:.c=.o)
OBJS := $(addprefix $(BUILD_DIR)/,$(OBJS))

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# 2. Comando RUN actualizado con 6 parámetros: IP Port CPU GPU RAM Hilos
run: $(TARGET)
	./$(TARGET) "$(IP)" "$(PORT)" "$(CPU)" "$(RAM)" "$(GPU)" "$(THREADS)"


clear:
	@rm -rf $(BUILD_DIR)
	@rm -f $(TARGET)

.PHONY: all run clear
