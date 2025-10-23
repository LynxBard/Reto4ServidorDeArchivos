// cliente.c - Cliente para servidor de archivos
#include <stdio.h>      // Para entrada/salida estándar (printf, fgets)
#include <stdlib.h>     // Para funciones generales (exit)
#include <string.h>     // Para manipulación de strings (strlen, strcmp)
#include <unistd.h>     // Para llamadas al sistema (read, close)
#include <sys/socket.h> // Para la API de sockets
#include <arpa/inet.h>  // Para manipulación de direcciones IP (inet_pton)

// --- Constantes de Configuración ---
// Deben coincidir con las del servidor
#define PORT 8080
#define BUFFER_SIZE 4096
#define SERVER_IP "127.0.0.1" // IP del servidor (localhost)

/**
 * @brief Limpia el buffer de entrada estándar (stdin).
 * Es útil después de usar scanf para leer un solo carácter,
 * para consumir el salto de línea que queda en el buffer.
 */
void limpiar_buffer() {
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}

// --- Función Principal del Cliente ---
int main() {
    int sock = 0; // Descriptor de archivo para el socket del cliente
    struct sockaddr_in serv_addr; // Estructura para la dirección del servidor
    char buffer[BUFFER_SIZE] = {0}; // Buffer para recibir datos del servidor
    char comando[256]; // Buffer para leer los comandos del usuario
    
    // --- 1. Crear el socket del cliente ---
    // AF_INET: Familia IPv4, SOCK_STREAM: TCP, 0: Protocolo por defecto
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("Error: No se pudo crear el socket\n");
        return -1;
    }
    
    // --- 2. Configurar la dirección del servidor al que nos conectaremos ---
    serv_addr.sin_family = AF_INET; // Familia IPv4
    serv_addr.sin_port = htons(PORT); // Puerto del servidor en formato de red
    
    // Convertir la dirección IP de texto a formato binario
    if (inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr) <= 0) {
        printf("Error: Dirección IP inválida\n");
        return -1;
    }
    
    // --- 3. Conectar al servidor ---
    printf("Conectando al servidor %s:%d...\n", SERVER_IP, PORT);
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("Error: No se pudo conectar al servidor\n");
        printf("Asegúrese de que el servidor esté ejecutándose\n");
        return -1;
    }
    
    printf("¡Conectado exitosamente!\n\n");
    
    // --- 4. Recibir y mostrar el mensaje de bienvenida del servidor ---
    memset(buffer, 0, BUFFER_SIZE); // Limpiar buffer
    read(sock, buffer, BUFFER_SIZE); // Leer del socket
    printf("%s\n", buffer); // Imprimir mensaje
    
    // --- Bucle principal para interacción con el usuario ---
    while (1) {
        printf("\n> Ingrese comando: ");
        // Leer un comando desde la entrada estándar (teclado)
        fgets(comando, sizeof(comando), stdin);
        
        // Eliminar el salto de línea '\n' que fgets agrega al final
        comando[strcspn(comando, "\n")] = 0;
        
        // Si el usuario presiona Enter sin escribir nada, continuar al siguiente ciclo
        if (strlen(comando) == 0) {
            continue;
        }
        
        // --- 5. Enviar el comando al servidor ---
        send(sock, comando, strlen(comando), 0);
        
        // Si el comando es "EXIT", nos preparamos para terminar
        if (strcmp(comando, "EXIT") == 0) {
            memset(buffer, 0, BUFFER_SIZE);
            read(sock, buffer, BUFFER_SIZE); // Leer el mensaje de despedida
            printf("%s", buffer);
            break; // Romper el bucle principal para cerrar el cliente
        }
        
        // --- 6. Recibir la respuesta del servidor ---
        printf("\n--- Respuesta del servidor ---\n");
        
        // --- Lógica especial para el comando GET ---
        if (strncmp(comando, "GET ", 4) == 0) {
            // El comando es GET, la respuesta puede ser un archivo grande.
            
            printf("¿Desea guardar el archivo localmente? (s/n): ");
            char opcion;
            scanf("%c", &opcion);
            limpiar_buffer(); // Limpiar el '\n' que queda después de scanf
            
            FILE *file = NULL;
            char filename[256];
            
            // Si el usuario quiere guardar el archivo
            if (opcion == 's' || opcion == 'S') {
                printf("Nombre del archivo local: ");
                fgets(filename, sizeof(filename), stdin);
                filename[strcspn(filename, "\n")] = 0; // Quitar salto de línea
                file = fopen(filename, "w"); // Abrir archivo en modo escritura
                if (file == NULL) {
                    printf("Error: No se pudo crear el archivo local\n");
                }
            }
            
            // Bucle para recibir los datos del archivo
            int bytes_received;
            while ((bytes_received = read(sock, buffer, BUFFER_SIZE - 1)) > 0) {
                buffer[bytes_received] = '\0'; // Asegurar que el buffer es un string válido
                
                // Mostrar siempre el contenido en la pantalla
                printf("%s", buffer);
                
                // Si estamos guardando, escribir al archivo
                if (file != NULL) {
                    // Esta lógica es simple y puede fallar. Intenta quitar el 
                    // encabezado y el pie de página que envía el servidor.
                    // Escribe todo lo que recibe en el archivo. Una mejora
                    // sería parsear el buffer para no escribir el encabezado/pie.
                    fprintf(file, "%s", buffer);
                }
                
                // Condición de parada: si en el buffer recibido encontramos el pie de página
                // o un mensaje de error del servidor, dejamos de leer.
                if (strstr(buffer, "=== FIN DEL ARCHIVO ===") || strstr(buffer, "Error:")) {
                    break;
                }
                
                // Limpiar el buffer para la siguiente lectura
                memset(buffer, 0, BUFFER_SIZE);
            }
            
            // Si se creó un archivo, cerrarlo y notificar al usuario
            if (file != NULL) {
                fclose(file);
                // La respuesta del servidor ya incluye un salto de línea, por eso se
                // imprime el mensaje de guardado después, para que no se mezcle.
                printf("\nArchivo guardado como: %s\n", filename);
            }
        }
        else {
            // Para otros comandos (como LIST), la respuesta es corta y simple.
            // Se realiza una sola lectura.
            memset(buffer, 0, BUFFER_SIZE);
            int valread = read(sock, buffer, BUFFER_SIZE - 1);
            if (valread > 0) {
                buffer[valread] = '\0';
                printf("%s", buffer);
            }
        }
        
        printf("------------------------------\n");
    }
    
    // --- 7. Cerrar el socket ---
    close(sock);
    printf("\nConexión cerrada\n");
    
    return 0;
}