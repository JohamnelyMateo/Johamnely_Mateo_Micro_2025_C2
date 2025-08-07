#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    //Variables
    int **matriz, i, j, nfilas, ncols;
    nfilas = 5;
    ncols = 5;

    //Memoria dinamica
    matriz = (int **) calloc(nfilas, sizeof(int *));

    for (i = 0; i < nfilas; ++i)
        matriz[i] = (int *) calloc(ncols, sizeof(int));

    //Asignacion de valores

    for (i = 0; i < nfilas; ++i)
        for (j = 0; j < ncols; ++j)
            matriz[i][j] = i + j;

    //Salida por la pantalla

    for (i = 0; i < nfilas; ++i) {
        for (j = 0; j < ncols; ++j) {
            printf("%d ", matriz[i][j]);
        }
        printf("\n");
    }

    //Liberacion de la memoria dinamica
    for (i = 0; i < nfilas; ++i)
        free(matriz[i]);
    free(matriz);

    return 0;
}

