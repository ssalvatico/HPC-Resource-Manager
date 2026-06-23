CC = gcc

CFLAGS = -Wall -Wextra -g -Iinclude -Iinclude/comms -Iinclude/resources -pthread

SRCS = $(wildcard src/comms/*.c) \
       $(wildcard src/resources/*.c) \
			 main.c

OBJDIR = c_build

OBJS = $(addprefix $(OBJDIR)/,$(SRCS:.c=.o))

build: c_agent

c_agent: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^
	@echo "¡Compilación exitosa! Ejecuta con: ./c_agent <ip> <puerto> <cpu> <gpu> <ram>"

# Regla genérica para compilar cada .c en un .o en su respectiva carpeta
$(OBJDIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

run: c_agent
	./c_agent 127.0.0.1 8000 4 1 4096

clear:
	rm -rf $(OBJDIR) c_agent

.PHONY: all run clear
