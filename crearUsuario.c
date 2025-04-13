//+---------------------------------------------------------------------------------------------------------------------------------------------------+
// Archivo: crearUsuario.c
// Descripción: Crea un nuevo usuario y lo guarda en el archivo cuentas.txt.
//
//
// Funciones:
//
// - ObtenerFechaHora(char *buffer, size_t bufferSize): Función que obtiene la fecha y hora actual y la guarda en el buffer.
// - EscribirEnLog(const char *mensaje, const char *archivoLog): Función que escribe un mensaje en el archivo de log.
// - main(int argc, char *argv[]): Función principal que recibe los argumentos de la línea de comandos y crea un nuevo usuario.
//+---------------------------------------------------------------------------------------------------------------------------------------------------+
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include "Usuarios.h"
#include "Config.h"
#include "crearUsuario.h"
#include <unistd.h>

void ObtenerFechaHora(char *buffer, size_t bufferSize)
{
    time_t t;
    struct tm *tm_info;

    time(&t);
    tm_info = localtime(&t);

    strftime(buffer, bufferSize, "%Y-%m-%d %H:%M:%S", tm_info);
}

void EscribirEnLog(const char *mensaje, const char *archivoLog)
{
    FILE *archivo = fopen(archivoLog, "a");  // Abrir archivo en modo de agregar
    if (!archivo)
    {
        perror("Error al abrir el archivo de log");
        return;
    }

    fprintf(archivo, "%s\n", mensaje);  // Escribir mensaje con salto de línea
    fclose(archivo);
}


// Para crear un usuario nuevo
int main (int argc, char *argv[]) {

    // Obtener los valores pasados desde el programa principal
    int NumeroCuenta = atoi(argv[1]);
    char *archivoLog = argv[2];
    char Titular[100];
    float Saldo = 0;
    int NumeroOperaciones = 0;

    // Inicio de log
    char FechaInicioCuenta[148];
    char mensajeDeLog[256];
    ObtenerFechaHora(FechaInicioCuenta, sizeof(FechaInicioCuenta));
    snprintf(mensajeDeLog, sizeof(mensajeDeLog), "[%s] Usuario con número de cuenta %s creado.", FechaInicioCuenta, argv[1]);

    // Escribir en el archivo de log
    EscribirEnLog(mensajeDeLog, archivoLog);

    printf("+-----------------------------+\n");
    printf("| Menu de creación de usuario |\n");
    printf("+-----------------------------+\n");

    printf("Ingrese Titular: ");
    scanf(" %s", Titular);
    printf("+------------------------------------------------------+\n");
    printf("|   Su saldo y numero de transacciones empezara en 0.  |\n");


    // Guardar en archivo cuentas.txt
    FILE *archivo = fopen("cuentas.txt", "a");
    if (archivo == NULL) {
        perror("Error al abrir el archivo");
        return EXIT_FAILURE;
    }

    fprintf(archivo, "%d, %s, %.2f, %d\n", NumeroCuenta, Titular, Saldo, NumeroOperaciones);
    fclose(archivo);

    printf("|   Usuario guardado con éxito.                        |\n");
    printf("+------------------------------------------------------+\n");

    char FechaFinCuenta[148];
    char MensajeDeSalida[256];
    ObtenerFechaHora(FechaFinCuenta, sizeof(FechaFinCuenta));
    snprintf(MensajeDeSalida, sizeof(MensajeDeSalida), "[%s] Cierre de sesión de creación de cuenta: %s\n", FechaFinCuenta, argv[1]);
    EscribirEnLog(MensajeDeSalida, archivoLog);

}


    /*FILE *archivo = fopen(archivoEscritura, "a");
    if (archivo == NULL) {
        perror("Error al abrir el archivo de cuentas");
        return;
    }

    fprintf(archivo, "%d,%s,0.00,0\n", IdCuenta, Nombre);
    fclose(archivo);*/
