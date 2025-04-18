//+---------------------------------------------------------------------------------------------------------------+
// Archivo: Usuario.c
// Descripción: Este archivo contiene la implementación de las funciones relacionadas con el manejo de cuentas de usuario en un sistema bancario.
//
//
// Funciones:
//  - ejecutar_menu_usuario(int IdUsuario): Ejecuta el menú del usuario y gestiona las opciones seleccionadas
//  - buscarUsuarioEnArchivo(const char *rutaBuscar): Busca un usuario en el archivo de cuentas
//  - verificarUsuario(const char *archivoLectura, int IdCuenta): Verifica si un usuario existe en el archivo de cuentas
//  - leer_configuracion(const char *ruta): Lee la configuración desde un archivo de texto y devuelve una estructura Config
//  - MostrarMonitor(void *arg): Crea un proceso hijo que ejecuta el monitor en una nueva terminal
//  - MostrarMenu(void *arg): Muestra el menú principal del banco y gestiona la creación de usuarios y la conexión de usuarios existentes
//  - EscucharTuberiaMonitor(void *arg): Escucha mensajes de una tubería FIFO y los muestra en la consola
//  - main(): Función principal que inicializa la configuración, crea las tuberías y los hilos, y gestiona el flujo del programa
//
//+---------------------------------------------------------------------------------------------------------------+
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include "Usuarios.h"
#include "Config.h"
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <sys/shm.h>


//variables globales
sem_t *semaforo_transacciones;

typedef struct
{
    const char *archivoLog;
    TablaCuentas *tabla;
    int posicionCuenta;
} ArgsHilo;

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
    FILE *archivo = fopen(archivoLog, "a"); // "a" → Añadir al final
    if (!archivo)
    {
        perror("Error al abrir el archivo de log");
        return;
    }

    fprintf(archivo, "%s", mensaje);

    fclose(archivo);
}

// Depositar dinero en la cuenta, usándo semáforos para los hilos y guardando las modificaciones en el archivo
void *Depositar(void *arg)
{
    // Variables
    float cantidad;
    char FechaHora[20]; // Para almacenar la fecha y la hora
    char rutaLog[100];

    ArgsHilo *args = (ArgsHilo *)arg;

    printf("Ingrese la cantidad que quiere depositar: ");
    if (scanf("%f", &cantidad) != 1 || cantidad <= 0)
    {
        printf("Cantidad inválida\n");
        return NULL;
    }

    // Sumamos la cantidad al saldo, y suma el número de transacciones
    args->tabla->cuenta[args->posicionCuenta].saldo += cantidad;
    args->tabla->cuenta[args->posicionCuenta].num_transacciones++;

    // Escribimos en el log
    // Usamos semaforo para controlar el acceso
    sem_wait(semaforo_transacciones);
    snprintf(rutaLog, sizeof(rutaLog), "./transacciones/%d/transacciones.log", args->tabla->cuenta[args->posicionCuenta].numero_cuenta);
    ObtenerFechaHora(FechaHora, sizeof(FechaHora));
    FILE *log = fopen(rutaLog, "a");
    fprintf(log, "[%s] Deposito: +%.2f\n", FechaHora, cantidad);
    fclose(log);
    sem_post(semaforo_transacciones);


    printf("Deposito realizado con éxito. Nuevo saldo: %.2f\n", args->tabla->cuenta[args->posicionCuenta].saldo);
    return NULL;
}

// Retirar dinero de la cuenta, usándo semáforos para los hilos y guardando las modificaciones en el archivo
void *Retirar(void *arg)
{
    // Variables
    float cantidad;
    char FechaHora[20]; // Para almacenar la fecha y la hora
    char rutaLog[100];

    ArgsHilo *args = (ArgsHilo *)arg;

    printf("Ingrese la cantidad a retirar: ");
    if (scanf("%f", &cantidad) != 1 || cantidad <= 0)
    {
        printf("Cantidad inválida\n");

        return NULL;
    }

    // Restamos la cantidad al saldo, y suma el número de transacciones
    if (args->tabla->cuenta[args->posicionCuenta].saldo >= cantidad)
    {
        args->tabla->cuenta[args->posicionCuenta].saldo -= cantidad;
        args->tabla->cuenta[args->posicionCuenta].num_transacciones++;

        // escribimos en el log
        // Semaforo para controlar el acceso
        sem_wait(semaforo_transacciones);

        ObtenerFechaHora(FechaHora, sizeof(FechaHora));
        snprintf(rutaLog, sizeof(rutaLog), "./transacciones/%d/transacciones.log", args->tabla->cuenta[args->posicionCuenta].numero_cuenta);
        FILE *log = fopen(rutaLog, "a");
        fprintf(log, "[%s] Retiro: -%.2f\n", FechaHora, cantidad);
        fclose(log);
        printf("Retiro realizado con éxito. Nuevo saldo: %.2f\n", args->tabla->cuenta[args->posicionCuenta].saldo);

        sem_post(semaforo_transacciones);
        
    }
    else
    {
        printf("Fondos insuficientes\n");
    }

    return NULL;
}

// Consultar el saldo de la cuenta
void *ConsultarSaldo(void *arg)
{
    ArgsHilo *args = (ArgsHilo *)arg;
    printf("Saldo actual: %.2f\n", args->tabla->cuenta[args->posicionCuenta].saldo);
    return NULL;
}

// Realizar transacción a otra cuenta
void *Transferencia(void *arg)
{

    ArgsHilo *args = (ArgsHilo *)arg;

    int cuentaDestino;
    int posicionCuentaDestino;
    float cantidad;
    char FechaHora[20]; // Para almacenar la fecha y la hora
    char rutaLog[100];

    // Leemos los datos de la cuenta destino y la cantidad a transferir
    printf("Ingrese el número de cuenta destino: ");
    if (scanf("%d", &cuentaDestino) != 1)
    {
        perror("Número de cuenta inválido");
        return NULL;
    }

    for (int i = 0; i <= args->tabla->num_cuentas; i++)
    {
        if (args->tabla->cuenta[i].numero_cuenta == cuentaDestino)
        {
            posicionCuentaDestino = i;
            break;
        }
    }

    // Leemos la cantidad a transferir
    printf("Ingrese la cantidad a transferir: ");
    if (scanf("%f", &cantidad) != 1 || cantidad <= 0)
    {
        printf("Cantidad inválida\n");

        return NULL;
    }

    if (args->tabla->cuenta[args->posicionCuenta].saldo >= cantidad)
    {
        // Restamos la cantidad al saldo de la cuenta origen
        args->tabla->cuenta[args->posicionCuenta].saldo -= cantidad;
        args->tabla->cuenta[args->posicionCuenta].num_transacciones++;

        // Sumamos la cantidad al saldo de la cuenta destino
        args->tabla->cuenta[posicionCuentaDestino].saldo += cantidad;
        args->tabla->cuenta[posicionCuentaDestino].num_transacciones++;

        // Escribimos en el log de la cuenta origen
        // Usamos semaforo para controlar el acceso al log
        sem_wait(semaforo_transacciones);
        ObtenerFechaHora(FechaHora, sizeof(FechaHora));
        snprintf(rutaLog, sizeof(rutaLog), "./transacciones/%d/transacciones.log", args->tabla->cuenta[args->posicionCuenta].numero_cuenta);
        FILE *log = fopen(rutaLog, "a");
        fprintf(log, "[%s] Transferencia a cuenta %d: -%.2f\n", FechaHora, cuentaDestino, cantidad);
        fclose(log);
        sem_post(semaforo_transacciones);

        printf("Transferencia realizada con éxito. Nuevo saldo: %.2f\n", args->tabla->cuenta[args->posicionCuenta].saldo);
    }
    else
    {
        printf("Fondos insuficientes\n");
    }
    sleep(28);
}

void *MostrarMenuUsuario()
{
    printf("+--------------------------------------+\n");
    printf("|             MENÚ USUARIO             |\n");
    printf("|             1. Depositar             |\n");
    printf("|             2. Retirar               |\n");
    printf("|             3. Consultar saldo       |\n");
    printf("|             4. Transferencia         |\n");
    printf("|             5. Salir                 |\n");
    printf("+--------------------------------------+\n");
    printf("\nSeleccione una opción: ");
}

// Para conseguir el pid de la terminal actual
pid_t get_terminal_pid()
{
    return getppid(); // devuelve el ppid del padre
}

int main(int argc, char *argv[])
{

    semaforo_transacciones = sem_open("/semaforo_transacciones", O_CREAT, 0666, 1);
    if (semaforo_transacciones == SEM_FAILED)
    {
        perror("Error al abrir el semáforo");
        exit(EXIT_FAILURE);
    }

    // parametros de entrada
    const char *archivoLog = argv[1];
    int shm_id = atoi(argv[2]);
    int PosicionCuenta = atoi(argv[3]);

    // Variables
    char FechaInicioCuenta[148];
    char MensajeDeInicio[256];

    // Asociar la memoria compartida
    TablaCuentas *tabla = (TablaCuentas *)shmat(shm_id, NULL, 0);

    ObtenerFechaHora(FechaInicioCuenta, sizeof(FechaInicioCuenta));
    snprintf(MensajeDeInicio, sizeof(MensajeDeInicio), "[%s] Inicio de sesión de cuenta: %s\n", FechaInicioCuenta, argv[1]);
    EscribirEnLog(MensajeDeInicio, archivoLog);

    printf("Bienvenido, %s (Cuenta: %d)\n", tabla->cuenta[PosicionCuenta].titular, tabla->cuenta[PosicionCuenta].numero_cuenta);

    int opcion;
    pthread_t hilo;

    do
    {
        MostrarMenuUsuario();

        if (scanf("%d", &opcion) != 1)
        {
            printf("Opción inválida\n");
            while (getchar() != '\n')
                ; // Limpiar buffer
            continue;
        }

        ArgsHilo args = {archivoLog, tabla, PosicionCuenta};

        // Con las opciones creamos los hilos, y hacemos que esperen a que terminen
        switch (opcion)
        {
        case 1:
            pthread_create(&hilo, NULL, Depositar, &args);
            pthread_join(hilo, NULL);
            break;
        case 2:
            pthread_create(&hilo, NULL, Retirar, &args);
            pthread_join(hilo, NULL);
            break;
        case 3:
            pthread_create(&hilo, NULL, ConsultarSaldo, &args);
            pthread_join(hilo, NULL);
            break;
        case 4:
            pthread_create(&hilo, NULL, Transferencia, &args);
            pthread_join(hilo, NULL);
            break;
        case 5:
            printf("Cerrando la cuenta...\n");
            char FechaFinCuenta[148];
            char MensajeDeSalida[256];
            ObtenerFechaHora(FechaFinCuenta, sizeof(FechaFinCuenta));
            snprintf(MensajeDeSalida, sizeof(MensajeDeSalida), "[%s] Cierre de sesión de cuenta: %d\n", FechaFinCuenta, tabla->cuenta[PosicionCuenta].numero_cuenta);
            EscribirEnLog(MensajeDeSalida, archivoLog);

            // Cierra la terminal que ejecutó el proceso (en la mayoría de casos)
            pid_t terminalPid = getppid();
            kill(terminalPid, SIGKILL);
            exit(EXIT_SUCCESS);

        default:
            printf("Opción no válida\n");
        }
    } while (opcion != 5);

    return EXIT_SUCCESS;
}
