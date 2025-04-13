// +------------------------------------------------------------------------------------------------------------------------------+*
// Archivo: init_cuentas.c
// Descripción: Inicializa el archivo de cuentas, con cuentas predefinidas si está vacío.
// +------------------------------------------------------------------------------------------------------------------------------+*
#include <stdio.h>
#include <stdlib.h>

struct Cuenta {
    int numero_cuenta;
    char titular[50];
    float saldo;
    int num_transacciones;
};

void InitCuentas(const char *nombreArchivo) {
    FILE *archivo = fopen(nombreArchivo, "a");
    if (!archivo) {
        perror("Error al crear el archivo");
        return;
    }

    fseek(archivo, 0, SEEK_END);
    long tamano = ftell(archivo);

    if(tamano != 0) {
        printf("El archivo ya contiene cuentas. \n");
        fclose(archivo);
    }
    else{
        struct Cuenta cuentas[3] = {
            {1001, "John Doe", 5000.00, 0},
            {1002, "Jane Smith", 3000.00, 0},
            {1003, "Alice Johnson", 7000.00, 0}
        };
        
        for (int i = 0; i < 3; i++) {
            fprintf(archivo, "%d,%s,%.2f,%d\n", cuentas[i].numero_cuenta, cuentas[i].titular, cuentas[i].saldo, cuentas[i].num_transacciones);
        }
        
        fclose(archivo);
        printf("Cuentas inicializadas correctamente en %s.\n", nombreArchivo);
    }
    

}

