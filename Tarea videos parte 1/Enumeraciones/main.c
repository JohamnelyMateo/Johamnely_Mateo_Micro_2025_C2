#include <stdio.h>
#include <stdlib.h>
enum DiaDeLaSemana

{
    Dom = 0,
    Lun =1,
    Mar =2,
    Mie=3,
    Jue=4,
    Vie=5,
    Sab = 6
};
int main(void)
{
    int x;
    x=Vie;
    switch (x){
case Dom:
    printf("Dom\n");
    break;
case Lun:
    printf("Lun\n");
    break;
case Mar:
    printf("Mar\n");
    break;
case Mie:
    printf("Mie\n");
    break;
case Jue:
    printf("Jue\n");
    break;
case Vie:
    printf("Vie\n");
    break;
case Sab:
    printf("Sab\n");
    break;
default:
    printf("Error\n");
    break;
    }
    return 0;
}
