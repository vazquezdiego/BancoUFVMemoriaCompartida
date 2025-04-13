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
} Cuenta;

typedef struct
{
    Cuenta cuenta[100];
    int num_cuentas;
} TablaCuentas;

extern TablaCuentas tablaCuentas;

#endif //USUARIOS_H