# Nombre del ejecutable final
TARGET = c_agent

# Compilador y banderas de compilación
CC = gcc
CFLAGS = -Wall -Wextra -pthread -g

# Archivos fuente (Mapeados exactamente según tu estructura)
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

# Generar automáticamente los nombres de los archivos objeto (.o)
OBJS = $(SRCS:.c=.o)

# Regla principal por defecto (cuando escribes "make")
all: $(TARGET)

# Regla de Enlazado (Linker): Junta todos los .o en el ejecutable final
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

# Regla de Compilación: Convierte cada .c en su respectivo .o
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Limpiar los archivos generados
clean:
	rm -f $(OBJS) $(TARGET)

# Alias por si prefieres escribir "make clear" en vez de "make clean"
clear: clean

.PHONY: all clean clear