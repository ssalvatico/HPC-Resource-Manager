# ==========================================
# Configuración del Compilador
# ==========================================
CC = gcc
# -Wall -Wextra: Muestra todas las advertencias (buenas prácticas)
# -g: Permite usar GDB para debuggear
# -Iinclude: Le dice al compilador que busque los .h en la carpeta "include"
# -pthread: Obligatorio para compilar código multihilo en Linux
CFLAGS = -Wall -Wextra -g -Iinclude -pthread

# Nombre del archivo ejecutable final
TARGET = c_agent

# ==========================================
# Archivos del Proyecto
# ==========================================
# Lista de todos tus archivos fuente (.c)
SRCS = main.c src/network_core.c src/thread_pool.c src/mock_juani.c

# Si Juani ya te pasó su archivo con la lógica (ej: resource_manager.c), 
# descomenta la línea de abajo y borra la de arriba:
# SRCS = main.c src/network_core.c src/thread_pool.c src/resource_manager.c

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

# Regla genérica para compilar cada .c en un .o
%.o: %.c
	@echo "Compilando $<..."
	$(CC) $(CFLAGS) -c $< -o $@

# Regla para limpiar los archivos compilados al escribir "make clean"
clean:
	@echo "Limpiando archivos binarios..."
	rm -f $(OBJS) $(TARGET)

# Indica que "all" y "clean" no son nombres de archivos
.PHONY: all clean