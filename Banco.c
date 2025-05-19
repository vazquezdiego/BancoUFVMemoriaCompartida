// +------------------------------------------------------------------------------------------------------------------------------------------------+/
//  Archivo: Banco.c
//  Descripción: El programa principal, Banco.c, es el encargado de gestionar la creación de usuarios y la interacción con el monitor.
//
//
//
// +------------------------------------------------------------------------------------------------------------------------------------------------+/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <semaphore.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdbool.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>

// Archivos.c Necesarios
#include "Usuarios.h"
#include "Banco.h"
#include "init_cuentas.c"
#include "crearUsuario.h"
#include "Config.h"
#include "Monitor.h"
#include "Usuarios.h"

// Variables globales
Config configuracion;
Cuenta cuenta;
TablaCuentas tablaCuentas;
BufferEstructurado buffer;

void *gestionar_buffer(void *arg)
{
    int shm_id = shmget(SHM_BUFFER, sizeof(BufferEstructurado), 0666);
    BufferEstructurado *buffer = (BufferEstructurado *)shmat(shm_id, NULL, 0);
    if (buffer == (void *)-1)
    {
        perror("Error mapeando memoria compartida del buffer en gestionar_buffer");
        pthread_exit(NULL);
    }

    while (1)
    {
        pthread_mutex_lock(&buffer->mutex);

        if (buffer->inicio == buffer->fin)
        {
            pthread_mutex_unlock(&buffer->mutex);
            usleep(10000); // 10ms de espera activa
            continue;
        }

        Cuenta cuenta_actualizada = buffer->operaciones[buffer->inicio];
        buffer->inicio = (buffer->inicio + 1) % TAM_BUFFER;

        pthread_mutex_unlock(&buffer->mutex);

        // Ahora abrimos el archivo de texto y reescribimos la cuenta modificada

        FILE *archivo = fopen("cuentas.txt", "r");
        if (archivo == NULL)
        {
            perror("Error abriendo cuentas.txt");
            continue;
        }

        // Leer todas las líneas en memoria
        char lineas[1000][256]; // Ajustar según máximo esperado
        int num_lineas = 0;
        while (fgets(lineas[num_lineas], sizeof(lineas[num_lineas]), archivo))
        {
            num_lineas++;
        }
        fclose(archivo);

        // Abrir archivo para escribir (sobrescribir)
        archivo = fopen("cuentas.txt", "w");
        if (archivo == NULL)
        {
            perror("Error abriendo cuentas.txt para escritura");
            continue;
        }

        for (int i = 0; i < num_lineas; i++)
        {
            int numero_cuenta;
            char titular[100];
            float saldo;
            int num_transacciones;
            char fecha_hora[22];
            // Parsear línea
            if(sscanf(lineas[i], "%d,%[^,],%f,%d,[%[^]]", &numero_cuenta, titular, &saldo, &num_transacciones, fecha_hora))
            {
                if (numero_cuenta == cuenta_actualizada.numero_cuenta)
                {
                    // Reescribir con datos actualizados
                    fprintf(archivo, "%d,%s,%.2f,%d,[%s]\n", cuenta_actualizada.numero_cuenta, cuenta_actualizada.titular, cuenta_actualizada.saldo, cuenta_actualizada.num_transacciones, cuenta_actualizada.fecha_hora);
                }
                else
                {
                    // Reescribir línea original
                    fprintf(archivo, "%s", lineas[i]);
                }
            }
            else
            {
                // Si no se pudo parsear, escribir línea original
                fprintf(archivo, "%s", lineas[i]);
            }
        }

        fclose(archivo);
    }

    return NULL;
}

void EscribirEnLog(const char *mensaje)
{
    FILE *archivoLog = fopen(configuracion.archivo_log, "a"); // "a" → Añadir al final
    if (!archivoLog)
    {
        perror("Error al abrir el archivo de log");
        return;
    }

    fprintf(archivoLog, "%s", mensaje);

    fclose(archivoLog);
}

// Como llamamos a la funcion de obtener la fecha y la hora
// ObtenerFechaHora(FechaHora, sizeof(FechaHora));
void ObtenerFechaHora(char *buffer, size_t bufferSize)
{
    time_t t;
    struct tm *tm_info;

    time(&t);
    tm_info = localtime(&t);

    strftime(buffer, bufferSize, "%Y-%m-%d %H:%M:%S", tm_info);
}

// Leer configuración desde archivo
Config leer_configuracion(const char *ruta)
{
    FILE *archivo = fopen(ruta, "r");
    if (archivo == NULL)
    {
        perror("Error al abrir config.txt");
        exit(1);
    }

    Config config;
    char linea[100];

    while (fgets(linea, sizeof(linea), archivo))
    {
        if (linea[0] == '#' || strlen(linea) < 3)
            continue;

        if (strstr(linea, "LIMITE_RETIRO"))
            sscanf(linea, "LIMITE_RETIRO=%d", &config.limite_retiro);
        else if (strstr(linea, "LIMITE_TRANSFERENCIA"))
            sscanf(linea, "LIMITE_TRANSFERENCIA=%d", &config.limite_transferencia);
        else if (strstr(linea, "UMBRAL_RETIROS"))
            sscanf(linea, "UMBRAL_RETIROS=%d", &config.umbral_retiros);
        else if (strstr(linea, "UMBRAL_TRANSFERENCIAS"))
            sscanf(linea, "UMBRAL_TRANSFERENCIAS=%d", &config.umbral_transferencias);
        else if (strstr(linea, "NUM_HILOS"))
            sscanf(linea, "NUM_HILOS=%d", &config.num_hilos);
        else if (strstr(linea, "ARCHIVO_CUENTAS"))
            sscanf(linea, "ARCHIVO_CUENTAS=%s", config.archivo_cuentas);
        else if (strstr(linea, "ARCHIVO_LOG"))
            sscanf(linea, "ARCHIVO_LOG=%s", config.archivo_log);
        else if (strstr(linea, "RUTA_USUARIO"))
            sscanf(linea, "RUTA_USUARIO=%s", config.ruta_usuario);
        else if (strstr(linea, "RUTA_CREARUSUARIO"))
            sscanf(linea, "RUTA_CREARUSUARIO=%s", config.ruta_crearusuario);
        else if (strstr(linea, "RUTA_MONITOR"))
            sscanf(linea, "RUTA_MONITOR=%s", config.ruta_monitor);
        else if (strstr(linea, "MAX_USUARIOS"))
            sscanf(linea, "MAX_USUARIOS=%d", &config.max_usuarios);
    }
    fclose(archivo);
    return config;
}

void *MostrarMonitor(void *arg)
{

    pid_t pidMonitor;
    pidMonitor = fork();
    if (pidMonitor == 0)
    {
        const char *rutaMonitor = configuracion.ruta_monitor;
        char comandoMonitor[512];
        snprintf(comandoMonitor, sizeof(comandoMonitor), "%s %d %d", rutaMonitor, configuracion.umbral_retiros, configuracion.umbral_transferencias);
        // Ejecutar gnome-terminal con el comando
        execlp("gnome-terminal", "gnome-terminal", "--", "bash", "-c", comandoMonitor, NULL);
    }
    else
    {
    }

    return NULL;
}

void *MostrarMenu(void *arg)
{
    // Zona para declarar variables
    int PosicionCuenta = 0; // Variable para guardar la posicion de la cuenta
    bool cuentaExistente = false;
    int numeroCuenta;
    char datosUsuario[100];
    pid_t pidUsuario;      // El pidUsuario
    pid_t pidCrearUsuario; // Variable for creating user process
    const char *titularCuenta;

    // Accedemos a la memoria de buffer
    int shm_buffer = shmget(SHM_BUFFER, sizeof(BufferEstructurado), 0666);

    // Accedemos a la memoria compartida
    int shm_id = shmget(SHM_KEY, sizeof(TablaCuentas), 0666);

    // Asociar la memoria compartida
    TablaCuentas *tabla = (TablaCuentas *)shmat(shm_id, NULL, 0);

    // Bucle de espera de conexiones

    do
    {
        printf("+-----------------------------+\n");
        printf("|    Bienvenido al Banco      |\n");
        printf("|  salir(1)                   |\n");
        for(int i = 0; i < tabla->num_cuentas; i++)
        {
            printf("| %d. %s\n", tabla->cuenta[i].numero_cuenta);
        }
        printf("+-----------------------------+\n");
        printf("Introduce tu número de cuenta:\n");
        scanf("%d", &numeroCuenta);

        if (numeroCuenta == 1)
        {
            // Matar procesos
            int killUsuario = system("killall ./usuario");
            int killMonitor = system("killall ./monitor");
            int killBanco = system("killall ./banco");
        }
        else
        {
            for (int i = 0; i < tabla->num_cuentas; i++)
            {
                if (tabla->cuenta[i].numero_cuenta == numeroCuenta)
                {
                    titularCuenta = tabla->cuenta[i].titular;
                    // Si encontramos una cuenta con el mismo numeroCuenta, retornamos true
                    // Guardamos la posicion de la cuenta
                    PosicionCuenta = i;
                    cuentaExistente = true;
                    break;
                }
                else
                {
                    cuentaExistente = false;
                }
            }

            if (cuentaExistente)
            {
                printf("Nuevo usuario conectado. Iniciando sesión...\n");

                pidUsuario = fork();

                if (pidUsuario < 0)
                {
                    EscribirEnLog("Error al crear un usuario");
                    exit(EXIT_FAILURE);
                }

                if (pidUsuario == 0)
                { // Proceso hijo
                    // Ruta absoluta del ejecutable usuario
                    const char *rutaUsuario = configuracion.ruta_usuario;

                    // Construcción del comando con pausa al final
                    char comandoUsuario[512];
                    snprintf(comandoUsuario, sizeof(comandoUsuario), "\"%s\" \"%s\" \"%d\" \"%d\" \"%d\"; exit", rutaUsuario, configuracion.archivo_log, shm_id, PosicionCuenta, shm_buffer);

                    // Ejecutar gnome-terminal con el comando
                    execlp("gnome-terminal", "gnome-terminal", "--", "bash", "-c", comandoUsuario, NULL);

                    // Si execlp falla
                    perror("Error al ejecutar gnome-terminal");
                    exit(EXIT_FAILURE);
                }
                else
                { // Proceso padre
                }
            }
            else
            {

                // Sirve para la creacion de Usuario
                pidCrearUsuario = fork();

                if (pidCrearUsuario < 0)
                {
                    EscribirEnLog("Error al iniciar el proceso de creación de usuario");
                    exit(EXIT_FAILURE);
                }

                if (pidCrearUsuario == 0)
                { // proceso hijo

                    // Ruta absoluta del ejecutable menu usuario
                    const char *rutaCrearUsuario = configuracion.ruta_crearusuario;

                    // Construcción del comando con pausa al final
                    char comandoCrearUsuario[512];
                    snprintf(comandoCrearUsuario, sizeof(comandoCrearUsuario), "%s %d %s", rutaCrearUsuario, numeroCuenta, configuracion.archivo_log);

                    // Ejecutar gnome-terminal con el comando
                    execlp("gnome-terminal", "gnome-terminal", "--", "bash", "-c", comandoCrearUsuario, NULL);
                }
                else
                {
                }
            }
        }

    } while (numeroCuenta != 1);
}

void *EscucharTuberiaMonitor(void *arg)
{
    int fdBancoMonitor;
    char mensaje[512];

    // Abrir la tubería FIFO para lectura
    fdBancoMonitor = open("fifo_bancoMonitor", O_RDONLY);
    if (fdBancoMonitor == -1)
    {
        EscribirEnLog("Error al abrir la tubería fifo_bancoMonitor");
        exit(EXIT_FAILURE);
    }

    while (1)
    {
        // Leer mensajes de la tubería
        int bytes_leidos = read(fdBancoMonitor, mensaje, sizeof(mensaje) - 1);
        if (bytes_leidos > 0)
        {
            mensaje[bytes_leidos] = '\0'; // Asegurar terminación de cadena
            printf("%s\n", mensaje);      // Mostrar el mensaje
        }
        else if (bytes_leidos == 0)
        {
            break; // Salir del bucle si no hay más datos
        }
    }
    close(fdBancoMonitor); // Cerrar la tubería
}

int main()
{
    pthread_t hilo_menu, hilo_pipes, hilo_monitor, hilo_escuchar, hilo_buffer;
    configuracion = leer_configuracion("config.txt");

    // Inicializamos las cuentas
    InitCuentas(configuracion.archivo_cuentas);

    char linea[256];
    int id_shmem;
    int id_buffer;

    id_shmem = shmget(SHM_KEY, sizeof(TablaCuentas), IPC_CREAT | 0666);
    if (id_shmem == -1)
    {
        perror("Error al crear la memoria compartida");
        exit(1);
    }

    id_buffer = shmget(SHM_BUFFER, sizeof(BufferEstructurado), IPC_CREAT | 0666);
    if (id_buffer == -1)
    {
        perror("Error al crear el segmento de memoria compartida del buffer");
        exit(EXIT_FAILURE);
    }

    BufferEstructurado *buffer = (BufferEstructurado *)shmat(id_buffer, NULL, 0);
    if (buffer == (void *)-1)
    {
        perror("Error al mapear el segmento de memoria compartida del buffer");
        exit(EXIT_FAILURE);
    }

    // Inicialización del buffer
    buffer->inicio = 0;
    buffer->fin = 0;

    // Inicialización del mutex como compartido entre procesos
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&buffer->mutex, &attr);

    TablaCuentas *tabla = (TablaCuentas *)shmat(id_shmem, NULL, 0);
    tabla->num_cuentas = 0;

    FILE *archivo = fopen(configuracion.archivo_cuentas, "r");
    while (fgets(linea, sizeof(linea), archivo))
    {
        sscanf(linea, "%d,%[^,],%f,%d,%s", &tabla->cuenta[tabla->num_cuentas].numero_cuenta, tabla->cuenta[tabla->num_cuentas].titular, &tabla->cuenta[tabla->num_cuentas].saldo, &tabla->cuenta[tabla->num_cuentas].num_transacciones, tabla->cuenta[tabla->num_cuentas].fecha_hora);
        tabla->num_cuentas++;
    }
    fclose(archivo);

    // Tuberias
    if (mkfifo("fifo_bancoMonitor", 0666) == -1 && errno != EEXIST)
    {
        EscribirEnLog("Error al crear la tubería");
        exit(EXIT_FAILURE);
    }

    pthread_create(&hilo_escuchar, NULL, EscucharTuberiaMonitor, NULL);
    pthread_create(&hilo_menu, NULL, MostrarMenu, NULL);
    pthread_create(&hilo_monitor, NULL, MostrarMonitor, NULL);
    pthread_create(&hilo_buffer, NULL, gestionar_buffer, NULL);

    pthread_join(hilo_menu, NULL);
    pthread_join(hilo_escuchar, NULL);

    return 0;
}