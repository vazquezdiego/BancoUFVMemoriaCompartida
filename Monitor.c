//+---------------------------------------------------------------------------------------------------------------------------------------------------+
//  Nombre: Monitor.c
//  Descripción: Controla las transacciones que hay en el archivo de transacciones y envía alertas al banco si se superan los umbrales de operaciones.
//
//+---------------------------------------------------------------------------------------------------------------------------------------------------+
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>

#define MAX_TEXT 512
#define PATH_SIZE 1024

void procesarArchivo(const char *ruta, int UmbralRetiros, int UmbralTransferencias, const char *cuenta) {
    FILE *file = fopen(ruta, "r");
    if (file == NULL) {
        perror("Error al abrir archivo de transacciones");
        return;
    }

    char linea[MAX_TEXT];
    char TipoOperacion[20];
    char TipoOperacionAnterior[20] = "";
    int ContadorRetiros = 0;
    int ContadorTransferencias = 0;

    while (fgets(linea, sizeof(linea), file) != NULL) {
        if (sscanf(linea, "[%*[^]]] %19[^:]", TipoOperacion) == 1) {
            if (strcmp(TipoOperacion, "Retiro") == 0) {
                // ... (lógica original de retiros)
                if (ContadorRetiros >= UmbralRetiros) {
                    char mensaje[MAX_TEXT];
                    snprintf(mensaje, sizeof(mensaje), 
                            "ALERTA [Cuenta %s]: %d Retiros consecutivos\n", 
                            cuenta, ContadorRetiros);
                    int fd = open("../../fifo_bancoMonitor", O_WRONLY);
                    if (fd != -1) {
                        write(fd, mensaje, strlen(mensaje) + 1);
                        close(fd);
                    } else {
                        perror("Error al abrir fifo_bancoMonitor");
                    }
                    // ... (envío por FIFO)
                }
            }
            else if (strcmp(TipoOperacion, "Transferencia") == 0) {
                if (strcmp(TipoOperacionAnterior, "Transferencia") == 0)
                    ContadorTransferencias++;
                else
                    ContadorTransferencias = 1;

                strcpy(TipoOperacionAnterior, "Transferencia");

                if (ContadorTransferencias >= UmbralTransferencias) {
                    char mensaje[MAX_TEXT];
                    snprintf(mensaje, sizeof(mensaje),
                             "ALERTA [Cuenta %s]: %d Transferencias consecutivas\n",
                             cuenta, ContadorTransferencias);
                            int fd = open("../../fifo_bancoMonitor", O_WRONLY);
                            if (fd != -1) {
                                write(fd, mensaje, strlen(mensaje) + 1);
                                close(fd);
                            } else {
                                perror("Error al abrir fifo_bancoMonitor");
                            }
                }
            } else {
                // Otro tipo de operación: reiniciar contadores
                ContadorRetiros = 0;
                ContadorTransferencias = 0;
                strcpy(TipoOperacionAnterior, "");
            }

        }
    }
    fclose(file);
}

int main(int argc, char *argv[]) {
    if (argc < 4) {
        fprintf(stderr, "Uso: %s <UmbralRetiros> <UmbralTransferencias> <carpetaTransacciones>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    printf("Monitor iniciado...\n");

    int UmbralRetiros = atoi(argv[1]);
    int UmbralTransferencias = atoi(argv[2]);

    while (1) {
        DIR *dir = opendir("transacciones");
        if (!dir) {
            perror("Error al abrir carpeta de transacciones");
            sleep(60);
            continue;
        }

        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) { // Corrección en la condición
            char path[PATH_SIZE];
            struct stat st;

            // Ignorar directorios "." y ".."
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                continue;
            }

            snprintf(path, sizeof(path), "transacciones/%s", entry->d_name);

            // Verificar si es un directorio usando stat()
            if (stat(path, &st) == -1) {
                continue;
            }

            if (S_ISDIR(st.st_mode)) { // Usar stat en lugar de d_type
                snprintf(path, sizeof(path), "transacciones/%s/transacciones.log", entry->d_name);

                if (stat(path, &st) == 0 && S_ISREG(st.st_mode)) {
                    procesarArchivo(path, UmbralRetiros, UmbralTransferencias, entry->d_name);
                }
            }
        }
        closedir(dir);
        sleep(60);
    }

    return 0;
}