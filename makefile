# Makefile para servidor de archivos

CC = gcc
CFLAGS = -Wall -Wextra -g
TARGETS = servidor cliente

all: $(TARGETS) setup

servidor: servidor.c
	$(CC) $(CFLAGS) -o servidor servidor.c

cliente: cliente.c
	$(CC) $(CFLAGS) -o cliente cliente.c

setup:
	@mkdir -p archivos
	@echo "Creando archivos de ejemplo..."
	@echo "Este es un archivo de prueba." > archivos/prueba.txt
	@echo "Contenido del archivo readme." > archivos/readme.txt
	@echo "Lista de tareas:\n1. Tarea 1\n2. Tarea 2\n3. Tarea 3" > archivos/tareas.txt
	@echo "Configuración del sistema\nVersion: 1.0\nAutor: Usuario" > archivos/config.txt
	@echo "Archivos de ejemplo creados en ./archivos/"

clean:
	rm -f $(TARGETS)

run-server: servidor
	./servidor

run-client: cliente
	./cliente

help:
	@echo "Comandos disponibles:"
	@echo "  make all        - Compila servidor y cliente, crea archivos de ejemplo"
	@echo "  make servidor   - Compila solo el servidor"
	@echo "  make cliente    - Compila solo el cliente"
	@echo "  make setup      - Crea directorio y archivos de ejemplo"
	@echo "  make run-server - Ejecuta el servidor"
	@echo "  make run-client - Ejecuta el cliente"
	@echo "  make clean      - Elimina los ejecutables"
	@echo "  make help       - Muestra esta ayuda"

.PHONY: all clean setup run-server run-client help