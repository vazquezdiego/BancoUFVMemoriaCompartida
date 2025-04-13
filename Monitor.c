//+---------------------------------------------------------------------------------------------------------------------------------------------------+
//  Nombre: Monitor.c                                                                                                        
//  Descripción: Controla las transacciones que hay en el archivo de transacciones y envía alertas al banco si se superan los umbrales de operaciones.                                                                          
//                                                                                              
//+---------------------------------------------------------------------------------------------------------------------------------------------------+
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <semaphore.h>
#include <fcntl.h>

#define MAX_TEXT 512

int main(int argc, char *argv[])
{

    sem_t *semaforo_transacciones = sem_open("/semaforo_transacciones", O_CREAT, 0666, 1);
    if (semaforo_transacciones == SEM_FAILED)
    {
        perror("Error al abrir semáforo");
        exit(EXIT_FAILURE);
    }

    int UmbralRetiros = atoi(argv[1]);
    int UmbralTransferencias = atoi(argv[2]);
    const char *archivoTransacciones = argv[3];
    char TipoOperacion[20];
    int cuenta;
    int ContadorRetiros = 0;
    int ContadorTransferencias = 0;
    int cuentaTemporal = 0;

    while (1)
    {
        sem_wait(semaforo_transacciones);

        FILE *file = fopen(archivoTransacciones, "r");
        if (file == NULL)
        {
            perror("Error al abrir el archivo de transacciones");
        }
        else
        {
            char linea[MAX_TEXT];
            printf("Revisando transacciones en %s:\n", archivoTransacciones);
            while (fgets(linea, sizeof(linea), file))
            {
                sscanf(linea, "[%*[^]]] %s en cuenta %d:", TipoOperacion, &cuenta);

                if (strcmp(TipoOperacion, "Retiro") == 0)
                {

                    if (cuentaTemporal == 0)
                    {
                        cuentaTemporal = cuenta;
                    }
                    else if (cuentaTemporal != cuenta)
                    {
                        ContadorRetiros = 1; // Reiniciar el contador si la cuenta cambia
                        cuentaTemporal = cuenta;
                    }
                    else
                    {
                        ContadorRetiros += 1;
                        if (ContadorRetiros >= UmbralRetiros)
                        {
                            char mensaje[MAX_TEXT];
                            int fd = open("fifo_bancoMonitor", O_WRONLY);
                            if (fd == -1)
                            {
                                perror("Error al abrir la tubería fifo_bancoMonitor");
                                exit(EXIT_FAILURE);
                            }
                            snprintf(mensaje, sizeof(mensaje), "ALERTA: Retiros consecutivos en cuenta %d excede el límite de %d \n", cuenta, UmbralRetiros);
                            write(fd, mensaje, strlen(mensaje) + 1); // Enviar mensaje a la tubería
                            close(fd);
                        }
                    }
                }
                else if (strcmp(TipoOperacion, "Transferencia") == 0)
                {

                    if (cuentaTemporal == 0)
                    {
                        cuentaTemporal = cuenta;
                    }
                    else if (cuentaTemporal != cuenta)
                    {
                        ContadorTransferencias = 1; // Reiniciar el contador si la cuenta cambia
                        cuentaTemporal = cuenta;
                    }
                    else
                    {
                        // Si la cuenta es la misma, incrementamos el contador
                        ContadorTransferencias += 1;
                        if (ContadorTransferencias >= UmbralTransferencias)
                        {
                            char mensaje[MAX_TEXT];
                            snprintf(mensaje, sizeof(mensaje), "ALERTA: Transferencias consecutivas en cuenta %d excede el límite de %d \n", cuenta, UmbralTransferencias);
                            int fd = open("fifo_bancoMonitor", O_WRONLY);
                            if (fd == -1)
                            {
                                perror("Error al abrir la tubería fifo_bancoMonitor");
                                exit(EXIT_FAILURE);
                            }
                            write(fd, mensaje, strlen(mensaje) + 1); // Enviar mensaje a la tubería
                            close(fd);
                        }
                    }
                }
            }
            fclose(file);
            sem_post(semaforo_transacciones);
        }
        sleep(60); // Esperar 1 minuto
    }
    sem_close(semaforo_transacciones);
    sem_unlink("/semaforo_transacciones"); // Eliminamos el semaforo

    return 0;
}
