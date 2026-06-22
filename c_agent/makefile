# ==========================================
# Configuración del Compilador
# ==========================================
CC = gcc

# -Iinclude : Le dice que busque en la carpeta base "include"
# -Iinclude/comms y -Iinclude/resources : Le dice que también busque dentro de las subcarpetas.
# Esto es vital para que no tire error si en tus .c pusiste #include "sys_sockets.h" 
# en lugar de #include "comms/sys_sockets.h".
CFLAGS = -Wall -Wextra -g -Iinclude -Iinclude/comms -Iinclude/resources -pthread

# Nombre del archivo ejecutable final
TARGET = c_agent

# ==========================================
# Archivos del Proyecto (¡Automatizado!)
# ==========================================
# main.c + Todos los .c dentro de comms + Todos los .c dentro de resources
SRCS = main.c \
       $(wildcard src/comms/*.c) \
       $(wildcard src/resources/*.c)

# Transforma automáticamente la lista de .c en una lista de .o (Objetos)
OBJS = $(SRCS:.c=.o)

# ==========================================
# Reglas de Compilación
# ==========================================

# Regla por defecto al escribir "make"
all: $(TARGET)

# Cómo construir el ejecutable final
$(TARGET): $(OBJS)
	@echo "Vinculando el ejecutable $@..."
	$(CC) $(CFLAGS) -o $@ $^
	@echo "¡Compilación exitosa! Ejecuta con: ./$(TARGET) <puerto> <ip>"

# Regla genérica para compilar cada .c en un .o en su respectiva carpeta
%.o: %.c
	@echo "Compilando $<..."
	$(CC) $(CFLAGS) -c $< -o $@

# Regla para limpiar los archivos compilados
clean:
	@echo "Limpiando archivos binarios..."
	rm -f $(OBJS) $(TARGET)

# Indica que "all" y "clean" no son nombres de archivos
.PHONY: all clean