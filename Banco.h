#ifndef BANCO_H
#define BANCO_H

#include "Usuarios.h"


void iniciar_banco();
void procesar_transaccion(int id_usuario, float monto, int tipo);

#endif // BANCO_H
