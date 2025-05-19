#ifndef USUARIOS_H
#define USUARIOS_H

#include "Banco.h"

#define SHM_KEY 1234 // Clave para la memoria compartida

void realizar_deposito(int cuenta, float monto);
void realizar_retiro(int cuenta, float monto);
void realizar_transferencia(int origen, int destino, float monto);
void consultar_saldo(int cuenta);
void ejecutar_menu_usuario();


typedef struct
{
    int numero_cuenta;
    char titular[50];
    float saldo;
    int num_transacciones;
    char fecha_hora[50];
} Cuenta;

typedef struct
{
    Cuenta cuenta[100];
    int num_cuentas;
} TablaCuentas;

#define TAM_BUFFER 6

#define SHM_BUFFER 3456

typedef struct {
    Cuenta operaciones[TAM_BUFFER];
    int inicio;
    int fin;
    pthread_mutex_t mutex;
} BufferEstructurado;

// Declaración del buffer global
extern BufferEstructurado buffer;



extern TablaCuentas tablaCuentas;

#endif //USUARIOS_H