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

#define MAX_TEXT 512

int main(int argc, char *argv[])
{
    if (argc < 4) {
        fprintf(stderr, "Uso: %s <UmbralRetiros> <UmbralTransferencias> <archivoTransacciones>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int UmbralRetiros = atoi(argv[1]);
    int UmbralTransferencias = atoi(argv[2]);
    const char *archivoTransacciones = argv[3];

    while (1) {
        FILE *file = fopen(archivoTransacciones, "r");
        if (file == NULL) {
            perror("Error al abrir el archivo de transacciones");
            sleep(60);
            continue; // Vuelve a intentarlo en el siguiente ciclo
        }

        char linea[MAX_TEXT];
        char TipoOperacion[20];
        char TipoOperacionAnterior[20] = "";
        int ContadorRetiros = 0;
        int ContadorTransferencias = 0;

        while (fgets(linea, sizeof(linea), file) != NULL)
        {
            if (sscanf(linea, "[%*[^]]] %[^:]:", TipoOperacion) == 1)
            {
                if (strcmp(TipoOperacion, "Retiro") == 0)
                {
                    if (strcmp(TipoOperacionAnterior, "Retiro") == 0)
                        ContadorRetiros++;
                    else
                        ContadorRetiros = 1;

                    strcpy(TipoOperacionAnterior, "Retiro");

                    if (ContadorRetiros >= UmbralRetiros)
                    {
                        char mensaje[MAX_TEXT];
                        snprintf(mensaje, sizeof(mensaje), "ALERTA: Se han realizado %d Retiros consecutivos.\n", ContadorRetiros);
                        int fd = open("../../fifo_bancoMonitor", O_WRONLY);
                        if (fd != -1) {
                            write(fd, mensaje, strlen(mensaje) + 1);
                            close(fd);
                        } else {
                            perror("Error al abrir fifo_bancoMonitor");
                        }
                    }
                }
                else if (strcmp(TipoOperacion, "Transferencia") == 0)
                {
                    if (strcmp(TipoOperacionAnterior, "Transferencia") == 0)
                        ContadorTransferencias++;
                    else
                        ContadorTransferencias = 1;

                    strcpy(TipoOperacionAnterior, "Transferencia");

                    if (ContadorTransferencias >= UmbralTransferencias)
                    {
                        char mensaje[MAX_TEXT];
                        snprintf(mensaje, sizeof(mensaje), "ALERTA: Se han realizado %d Transferencias consecutivas.\n", ContadorTransferencias);
                        int fd = open("../../fifo_bancoMonitor", O_WRONLY);
                        if (fd != -1) {
                            write(fd, mensaje, strlen(mensaje) + 1);
                            close(fd);
                        } else {
                            perror("Error al abrir fifo_bancoMonitor");
                        }
                    }
                }
                else
                {
                    // Otro tipo de operación: reiniciar contadores
                    ContadorRetiros = 0;
                    ContadorTransferencias = 0;
                    strcpy(TipoOperacionAnterior, "");
                }
            }
        }

        fclose(file);
        sleep(60); // Esperar 60 segundos antes del próximo chequeo
    }

    return 0;
}
