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

// variables globales
sem_t *semaforo_transacciones;

// Definimos un mutex para controlar las operaciones que realizan los usuarios
pthread_mutex_t mutex_u = PTHREAD_MUTEX_INITIALIZER;

typedef struct
{
    const char *archivoLog;
    TablaCuentas *tabla;
    int posicionCuenta;
    BufferEstructurado *buffer;
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

    // Bloqueamos el mutex para evitar condiciones de carrera
    pthread_mutex_lock(&mutex_u);

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

    pthread_mutex_lock(&args->buffer->mutex);

    // Comprobar si el buffer está lleno antes de insertar
    int siguienteFin = (args->buffer->fin + 1) % TAM_BUFFER;
    if (siguienteFin == args->buffer->inicio)
    {
        printf("Buffer lleno, no se puede insertar la operación\n");
    }
    else
    {
        int posicionGuardada = args->buffer->fin;

        // Insertar elemento en la posición fin
        args->buffer->operaciones[posicionGuardada] = args->tabla->cuenta[args->posicionCuenta];

        // Avanzar fin
        args->buffer->fin = siguienteFin;

        printf("Elemento insertado en la posición %d del buffer\n", posicionGuardada);
        printf("inicio = %d, fin = %d\n", args->buffer->inicio, args->buffer->fin);
    }

    pthread_mutex_unlock(&args->buffer->mutex);

    printf("Deposito realizado con éxito. Nuevo saldo: %.2f\n", args->tabla->cuenta[args->posicionCuenta].saldo);

    // Desbloqueamos el mutex
    pthread_mutex_unlock(&mutex_u);
    return NULL;
}

// Retirar dinero de la cuenta, usándo semáforos para los hilos y guardando las modificaciones en el archivo
void *Retirar(void *arg)
{
    // Variables
    float cantidad;
    char FechaHora[20]; // Para almacenar la fecha y la hora
    char rutaLog[100];

    // Bloqueamos el mutex para evitar condiciones de carrera
    pthread_mutex_lock(&mutex_u);

    ArgsHilo *args = (ArgsHilo *)arg;

    printf("Ingrese la cantidad a retirar: ");
    if (scanf("%f", &cantidad) != 1 || cantidad <= 0)
    {
        printf("Cantidad inválida\n");

        // Desbloqueamos el mutex
        pthread_mutex_unlock(&mutex_u);

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
        pthread_mutex_lock(&args->buffer->mutex);

        // Guardar posición actual
        int posicionGuardada = args->buffer->fin;

        // Insertar elemento en esa posición
        args->buffer->operaciones[posicionGuardada] = args->tabla->cuenta[args->posicionCuenta];

        // Incrementar el índice fin del buffer circular
        args->buffer->fin = (args->buffer->fin + 1) % TAM_BUFFER;

        // Comprobar si el buffer está lleno (fin == inicio)
        if (args->buffer->fin == args->buffer->inicio)
        {
            printf("Buffer lleno después de insertar en la posición %d\n", posicionGuardada);
        }
        else
        {
            printf("Elemento insertado en la posición %d del buffer\n", posicionGuardada);
            printf("inicio = %d, fin = %d\n", args->buffer->inicio, args->buffer->fin);
        }

        pthread_mutex_unlock(&args->buffer->mutex);
        printf("Retiro realizado con éxito. Nuevo saldo: %.2f\n", args->tabla->cuenta[args->posicionCuenta].saldo);

        sem_post(semaforo_transacciones);
    }
    else
    {
        printf("Fondos insuficientes\n");
    }

    // Desbloqueamos el mutex
    pthread_mutex_unlock(&mutex_u);

    return NULL;
}

// Consultar el saldo de la cuenta
void *ConsultarSaldo(void *arg)
{
    // Bloqueamos el mutex para evitar condiciones de carrera
    pthread_mutex_lock(&mutex_u);

    ArgsHilo *args = (ArgsHilo *)arg;
    printf("Saldo actual: %.2f\n", args->tabla->cuenta[args->posicionCuenta].saldo);

    // Desbloqueamos el mutex
    pthread_mutex_unlock(&mutex_u);

    return NULL;
}

// Realizar transacción a otra cuenta
void *Transferencia(void *arg)
{
    // Bloqueamos el mutex para evitar condiciones de carrera
    pthread_mutex_lock(&mutex_u);

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

        // Desbloqueamos el mutex antes de salir
        pthread_mutex_unlock(&mutex_u);
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

        // Desbloqueamos el mutex antes de salir
        pthread_mutex_unlock(&mutex_u);

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

        // Guardar en buffer la cuenta origen y la cuenta destino
        pthread_mutex_lock(&args->buffer->mutex);

        // Inserción para cuenta origen
        int pos = args->buffer->fin;
        args->buffer->operaciones[pos] = args->tabla->cuenta[args->posicionCuenta];
        args->buffer->fin = (args->buffer->fin + 1) % TAM_BUFFER;

        if (args->buffer->fin == args->buffer->inicio)
            printf("Buffer lleno después de insertar en la posición %d\n", pos);
        else
            printf("Elemento insertado en la posición %d del buffer (cuenta origen)\n", pos);
        printf("inicio = %d, fin = %d\n", args->buffer->inicio, args->buffer->fin);

        // Inserción para cuenta destino
        pos = args->buffer->fin;
        args->buffer->operaciones[pos] = args->tabla->cuenta[posicionCuentaDestino];
        args->buffer->fin = (args->buffer->fin + 1) % TAM_BUFFER;

        if (args->buffer->fin == args->buffer->inicio)
            printf("Buffer lleno después de insertar en la posición %d\n", pos);
        else
            printf("Elemento insertado en la posición %d del buffer (cuenta destino)\n", pos);
        pthread_mutex_unlock(&args->buffer->mutex);

        printf("Transferencia realizada con éxito. Nuevo saldo: %.2f\n", args->tabla->cuenta[args->posicionCuenta].saldo);
        pthread_mutex_unlock(&mutex_u);
    }
    else
    {
        printf("Fondos insuficientes\n");
    }
    // Desbloqueamos el mutex
    return NULL;
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

int main(int argc, char *argv[])
{
    pthread_mutex_init(&mutex_u, NULL);

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
    int shm_buffer = atoi(argv[4]);

    // Variables
    char FechaInicioCuenta[148];
    char MensajeDeInicio[256];

    // Asociar la memoria compartida
    TablaCuentas *tabla = (TablaCuentas *)shmat(shm_id, NULL, 0);
    BufferEstructurado *buffer = (BufferEstructurado *)shmat(shm_buffer, NULL, 0);

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

        ArgsHilo args = {archivoLog, tabla, PosicionCuenta, buffer};

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

            pthread_mutex_destroy(&mutex_u);

            // Cierra la terminal que ejecutó el proceso (en la mayoría de casos)
            pid_t terminalPid = getppid();
            kill(terminalPid, SIGKILL);
            exit(EXIT_SUCCESS);
        default:
            printf("Opción no válida\n");
        }
    } while (opcion != 5);

    pthread_mutex_destroy(&mutex_u);

    return EXIT_SUCCESS;
}
