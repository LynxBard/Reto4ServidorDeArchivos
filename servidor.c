// servidor.c - Servidor de archivos sencillo
#include <stdio.h>      // Para entrada/salida estándar (printf, perror)
#include <stdlib.h>     // Para funciones generales (exit, malloc, etc.)
#include <string.h>     // Para manipulación de strings (strcpy, strcmp, etc.)
#include <unistd.h>     // Para llamadas al sistema (read, write, close)
#include <sys/types.h>  // Definiciones de tipos de datos para sistemas
#include <sys/socket.h> // Para la API de sockets (socket, bind, listen, accept)
#include <netinet/in.h> // Para estructuras de direcciones de internet (sockaddr_in)
#include <arpa/inet.h>  // Para manipulación de direcciones IP (inet_ntoa)
#include <dirent.h>     // Para manejar directorios (opendir, readdir)
#include <sys/stat.h>   // Para obtener información de archivos (stat)

// --- Constantes de Configuración ---
#define PORT 8080               // Puerto en el que el servidor escuchará conexiones
#define BUFFER_SIZE 4096        // Tamaño del buffer para enviar y recibir datos
#define MAX_FILENAME 256        // Longitud máxima para nombres de archivo
#define FILES_DIR "./archivos"  // Directorio donde se almacenarán los archivos a servir

/**
 * @brief Lista los archivos en el directorio FILES_DIR y envía la lista al cliente.
 * @param client_socket El descriptor del socket del cliente conectado.
 */

void listar_archivos(int client_socket) {
    DIR *dir; // Puntero al directorio
    struct dirent *entry; // Puntero a una entrada del directorio (archivo/subdirectorio)
    struct stat file_stat; // Estructura para almacenar información del archivo
    char path[512]; // Buffer para construir la ruta completa del archivo
    char lista[BUFFER_SIZE] = "=== LISTA DE ARCHIVOS ===\n"; // Buffer para la lista de archivos a enviar

    // Abrir el directorio especificado en FILES_DIR
    dir = opendir(FILES_DIR);
    if (dir == NULL) {
        // Si no se puede abrir, enviar un mensaje de error y terminar la función
        strcpy(lista, "Error: No se puede abrir el directorio\n");
        send(client_socket, lista, strlen(lista), 0);
        return;
    }

    // Leer cada entrada del directorio
    while ((entry = readdir(dir)) != NULL) {
        // Omitir las entradas "." (directorio actual) y ".." (directorio padre)
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
        
        // Construir la ruta completa del archivo para usarla con stat()
        snprintf(path, sizeof(path), "%s/%s", FILES_DIR, entry->d_name);
        
        // Obtener información del archivo (como el tamaño y el tipo)
        if (stat(path, &file_stat) == 0) {
            // S_ISREG verifica si la entrada es un archivo regular (no un directorio)
            if (S_ISREG(file_stat.st_mode)) {
                char file_info[256];
                // Formatear la línea de información del archivo: "- nombre (tamaño bytes)"
                snprintf(file_info, sizeof(file_info), "- %s (%ld bytes)\n", 
                         entry->d_name, file_stat.st_size);
                // Añadir la información del archivo a la lista completa
                strcat(lista, file_info);
            }
        }
    }

    closedir(dir); // Cerrar el directorio
    strcat(lista, "========================\n"); // Añadir un pie de página a la lista
    
    // Enviar la lista completa al cliente
    send(client_socket, lista, strlen(lista), 0);
    printf("Lista de archivos enviada al cliente\n");
}

/**
 * @brief Envía el contenido de un archivo específico al cliente.
 * @param client_socket El descriptor del socket del cliente.
 * @param filename El nombre del archivo a enviar.
 */
void enviar_archivo(int client_socket, const char *filename) {
    char filepath[512]; // Buffer para la ruta completa del archivo
    char buffer[BUFFER_SIZE]; // Buffer para leer y enviar el contenido del archivo
    FILE *file; // Puntero al archivo
    size_t bytes_read; // Variable para almacenar cuántos bytes se leyeron del archivo
    
    // Medida de seguridad básica para evitar "path traversal".
    // No permite ".." ni "/" en el nombre del archivo para evitar que el cliente
    // acceda a archivos fuera del directorio `FILES_DIR`.
    if (strstr(filename, "..") != NULL || strchr(filename, '/') != NULL) {
        char *error_msg = "Error: Nombre de archivo inválido\n";
        send(client_socket, error_msg, strlen(error_msg), 0);
        return;
    }
    
    // Construir la ruta completa del archivo
    snprintf(filepath, sizeof(filepath), "%s/%s", FILES_DIR, filename);
    
    // Abrir el archivo en modo lectura ("r")
    file = fopen(filepath, "r");
    if (file == NULL) {
        // Si el archivo no existe o no se puede abrir, notificar al cliente
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "Error: No se puede abrir el archivo '%s'\n", filename);
        send(client_socket, error_msg, strlen(error_msg), 0);
        printf("Archivo no encontrado: %s\n", filename);
        return;
    }
    
    // Notificar en la consola del servidor que se está enviando el archivo
    printf("Enviando archivo: %s\n", filename);
    
    // Enviar un encabezado para que el cliente sepa qué está recibiendo
    char header[256];
    snprintf(header, sizeof(header), "=== CONTENIDO DE %s ===\n", filename);
    send(client_socket, header, strlen(header), 0);
    
    // Leer el archivo en trozos (chunks) y enviarlos al cliente
    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE - 1, file)) > 0) {
        // Se envía la cantidad exacta de bytes leídos
        send(client_socket, buffer, bytes_read, 0);
    }
    
    // Enviar un pie de página para señalar el fin del archivo
    char footer[] = "\n=== FIN DEL ARCHIVO ===\n";
    send(client_socket, footer, strlen(footer), 0);
    
    fclose(file); // Cerrar el archivo
    printf("Archivo enviado completamente\n");
}

// --- Función Principal del Servidor ---
int main() {
    int server_fd, client_socket; // Descriptores de archivo para los sockets
    struct sockaddr_in address; // Estructura para la dirección del servidor
    int opt = 1;
    int addrlen = sizeof(address);
    char buffer[BUFFER_SIZE] = {0}; // Buffer para recibir comandos del cliente
    
    // Crear el directorio de archivos si no existe. 
    // Los permisos 0755 significan: dueño (rwx), grupo (r-x), otros (r-x).
    mkdir(FILES_DIR, 0755);
    
    // --- 1. Crear el socket del servidor ---
    // AF_INET: Familia de direcciones IPv4
    // SOCK_STREAM: Socket de tipo TCP (orientado a la conexión)
    // 0: Protocolo por defecto (TCP para SOCK_STREAM)
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Error al crear socket");
        exit(EXIT_FAILURE);
    }
    
    // --- 2. Configurar opciones del socket ---
    // Esto permite reutilizar el puerto inmediatamente después de cerrar el servidor,
    // evitando el error "Address already in use".
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("Error en setsockopt");
        exit(EXIT_FAILURE);
    }
    
    // --- 3. Configurar la dirección del servidor ---
    address.sin_family = AF_INET; // Familia IPv4
    address.sin_addr.s_addr = INADDR_ANY; // Aceptar conexiones en cualquier IP de la máquina
    address.sin_port = htons(PORT); // Convertir el puerto al formato de red (big-endian)
    
    // --- 4. Vincular (Bind) el socket a la dirección y puerto ---
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Error en bind");
        exit(EXIT_FAILURE);
    }
    
    // --- 5. Poner el socket en modo escucha (Listen) ---
    // El 3 es el "backlog", el número máximo de conexiones pendientes en la cola.
    if (listen(server_fd, 3) < 0) {
        perror("Error en listen");
        exit(EXIT_FAILURE);
    }
    
    // Mensajes de inicio del servidor
    printf("=================================\n");
    printf("Servidor de archivos iniciado\n");
    printf("Puerto: %d\n", PORT);
    printf("Directorio de archivos: %s\n", FILES_DIR);
    printf("=================================\n");
    printf("Esperando conexiones...\n\n");
    
    // --- Bucle infinito para aceptar clientes ---
    while (1) {
        // --- 6. Aceptar una nueva conexión ---
        // accept() es una llamada bloqueante: el programa espera aquí hasta que un cliente se conecte.
        // Retorna un nuevo descriptor de socket para comunicarse con ese cliente.
        if ((client_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("Error en accept");
            continue; // Continuar al siguiente ciclo para esperar otra conexión
        }
        
        // Mostrar información del cliente que se conectó
        printf("Nueva conexión desde %s:%d\n", inet_ntoa(address.sin_addr), ntohs(address.sin_port));
        
        // Enviar un mensaje de bienvenida con los comandos disponibles al cliente
        char *welcome = "=== SERVIDOR DE ARCHIVOS ===\n"
                       "Comandos disponibles:\n"
                       "  LIST - Listar archivos\n"
                       "  GET <nombre> - Obtener archivo\n"
                       "  EXIT - Salir\n"
                       "============================\n";
        send(client_socket, welcome, strlen(welcome), 0);
        
        // --- Bucle para manejar los comandos de un cliente conectado ---
        while (1) {
            memset(buffer, 0, BUFFER_SIZE); // Limpiar el buffer antes de leer
            
            // --- 7. Leer el comando del cliente ---
            // read() también es bloqueante. Espera a que el cliente envíe datos.
            int valread = read(client_socket, buffer, BUFFER_SIZE);
            
            // Si read() devuelve 0 o -1, el cliente se ha desconectado.
            if (valread <= 0) {
                printf("Cliente desconectado\n");
                break; // Salir del bucle de comandos para este cliente
            }
            
            // Reemplazar el salto de línea (\n) al final del comando por un nulo (\0)
            buffer[strcspn(buffer, "\n")] = 0;
            
            printf("Comando recibido: '%s'\n", buffer);
            
            // --- 8. Procesar el comando ---
            if (strcmp(buffer, "LIST") == 0) {
                listar_archivos(client_socket);
            }
            else if (strncmp(buffer, "GET ", 4) == 0) {
                // Si el comando empieza con "GET ", el nombre del archivo es lo que sigue
                char *filename = buffer + 4;
                enviar_archivo(client_socket, filename);
            }
            else if (strcmp(buffer, "EXIT") == 0) {
                // Si el comando es EXIT, enviar un mensaje de despedida y romper el bucle
                char *bye = "Cerrando conexión...\n";
                send(client_socket, bye, strlen(bye), 0);
                printf("Cliente solicitó desconexión\n");
                break;
            }
            else {
                // Si el comando no se reconoce, enviar un mensaje de error
                char *error = "Comando no reconocido. Use LIST, GET <archivo> o EXIT\n";
                send(client_socket, error, strlen(error), 0);
            }
        }
        
        // --- 9. Cerrar la conexión con el cliente actual ---
        close(client_socket);
        printf("Conexión cerrada\n\n");
    } // El bucle principal vuelve a esperar una nueva conexión en accept()
    
    // Esta parte del código nunca se alcanza en este programa, pero es buena práctica
    close(server_fd);
    return 0;
}