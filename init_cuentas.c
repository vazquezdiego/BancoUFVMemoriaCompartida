// +------------------------------------------------------------------------------------------------------------------------------+*
// Archivo: init_cuentas.c
// Descripción: Inicializa el archivo de cuentas, con cuentas predefinidas si está vacío.
// +------------------------------------------------------------------------------------------------------------------------------+*
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>

struct Cuenta
{
    int numero_cuenta;
    char titular[50];
    float saldo;
    int num_transacciones;
    char fecha_hora[22];  // Tamaño suficiente para "[YYYY-MM-DD HH:MM:SS]"
};

void InitCuentas(const char *nombreArchivo)
{
    FILE *archivo = fopen(nombreArchivo, "a+");
    if (!archivo)
    {
        perror("Error al crear el archivo");
        return;
    }

    fseek(archivo, 0, SEEK_END);
    long tamano = ftell(archivo);

    if (tamano != 0)
    {
        printf("El archivo ya contiene cuentas.\n");
        fclose(archivo);
        return;
    }

    // Obtener fecha y hora actual formateada con corchetes
    char fechaHora[22];       // "[YYYY-MM-DD HH:MM:SS]"
    char fechaHoraTemp[20];   // "YYYY-MM-DD HH:MM:SS"

    time_t t = time(NULL);
    struct tm *tm_info = localtime(&t);

    strftime(fechaHoraTemp, sizeof(fechaHoraTemp), "%Y-%m-%d %H:%M:%S", tm_info);
    snprintf(fechaHora, sizeof(fechaHora), "[%s]", fechaHoraTemp);

    struct Cuenta cuentas[3] = {
        {1001, "John Doe", 5000.00f, 0, ""},
        {1002, "Jane Smith", 3000.00f, 0, ""},
        {1003, "Alice Johnson", 7000.00f, 0, ""}
    };

    for (int i = 0; i < 3; i++)
    {
        // Copiar la fecha con corchetes en la cuenta
        strncpy(cuentas[i].fecha_hora, fechaHora, sizeof(cuentas[i].fecha_hora) - 1);
        cuentas[i].fecha_hora[sizeof(cuentas[i].fecha_hora) - 1] = '\0';

        fprintf(archivo, "%d,%s,%.2f,%d,%s\n",
                cuentas[i].numero_cuenta,
                cuentas[i].titular,
                cuentas[i].saldo,
                cuentas[i].num_transacciones,
                cuentas[i].fecha_hora);
    }

    fclose(archivo);

    // Crear carpetas y archivos de transacciones para cada cuenta
    for (int i = 0; i < 3; i++)
    {
        char path[100];
        snprintf(path, sizeof(path), "./transacciones/%d", cuentas[i].numero_cuenta);
        mkdir(path, 0777);

        char rutaArchivo[100];
        snprintf(rutaArchivo, sizeof(rutaArchivo), "./transacciones/%d/transacciones.log", cuentas[i].numero_cuenta);

        FILE *archivoTransacciones = fopen(rutaArchivo, "w");
        if (!archivoTransacciones)
        {
            perror("Error al crear el archivo de transacciones");
            return;
        }
        fclose(archivoTransacciones);
    }

    printf("Cuentas inicializadas correctamente en %s.\n", nombreArchivo);
}
