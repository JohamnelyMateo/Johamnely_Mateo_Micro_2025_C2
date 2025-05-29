#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>

//Definición de Estados del Sistema
#define MIRO_ESP32 1
#define ESTADO_INICIAL 0
#define ESTADO_CERRANDO 1
#define ESTADO_ABRIENDO 2
#define ESTADO_CERRADO 3
#define ESTADO_ABIERTO 4
#define ESTADO_ERR 5
#define ESTADO_STOP 6

 //Códigos de Error
#define ERR_OK 0  // No hay error
#define ERR_OT 1 // EROR TECNICO (Over Time)
#define ERR_LSW 3 //Error en los Limit Switch (ambos activos)

//Declaración de funciones de cada estado
int Func_ESTADO_INICIAL(void);
int Func_ESTADO_CERRANDO(void);
int Func_ESTADO_ABRIENDO(void);
int Func_ESTADO_CERRADO(void);
int Func_ESTADO_ABIERTO(void);
int Func_ESTADO_ERR(void);
int Func_ESTADO_STOP(void);

//Variables globales auxiliares
bool estadoLamp = false;
time_t tiempoUltimoCambio = 0;
bool estadobuzzer = false;


//variables globales de estado
int EstadoSiguiente = ESTADO_INICIAL;
int EstadoActual = ESTADO_INICIAL;
int EstadoAnterior = ESTADO_INICIAL;

//Estructura IO: Entradas y salidas del sistema
struct IO
{
    unsigned int LSC:1;// Limit Switch Cerrado (puerta completamente cerrada)
    unsigned int LSA:1;// Limit Switch Abierto (puerta completamente abierta)
    unsigned int BA:1;//Boton abrir
    unsigned int BC:1;//Boton cerrar
    unsigned int BPP:1;// Botón Polifuncional (PP)
    unsigned int SE:1;// Botón de Emergencia
    unsigned int MA:1; // Motor dirección Abrir
    unsigned int MC:1; // Motor dirección Cerrar
    unsigned int BZZ:1;// Buzzer
    unsigned int LAMP:1; // Lámpara

    unsigned int MQTT_CMD:3; // Entrada desde red WiFi/MQTT (puede tener 8 valores posibles de 3 bits)

}io;

// Estructura STATUS: Variables del estado actual
struct STATUS {
    unsigned int cntTimerCA;    // Contador de tiempo de cierre automático
    unsigned int cntRunTimer;   // Contador de tiempo de rodamiento
    int ERR_COD; // Esta variable ERR_COD es usada para almacenar qué tipo de error ocurrió,
                // y está asociada con los valores anteriores (ERR_OK, ERR_OT, ERR_LSW).
};

// Estructura CONFIG: Parámetros de configuración del sistema
struct CONFIG {
    unsigned int RunTimer;// Tiempo máximo permitido de operación del motor (en segundos)
    unsigned int TimerCA;  // Tiempo que se espera antes del cierre automático
};

struct STATUS status = {0, 0, ERR_OK}; // Inicializa el estado desde cero y sin errores
struct CONFIG config = {180, 100}; // RunTimer: 180s, TimerCA: 100s

// Bucle principal del sistema
int main()
{

    for(;;)
    {
        if(EstadoSiguiente == ESTADO_INICIAL)
        {
            EstadoSiguiente = Func_ESTADO_INICIAL();
        }
        if(EstadoSiguiente == ESTADO_ABIERTO)
        {
            EstadoSiguiente = Func_ESTADO_ABIERTO();
        }
        if(EstadoSiguiente == ESTADO_ABRIENDO)
        {
            EstadoSiguiente = Func_ESTADO_ABRIENDO();
        }
        if(EstadoSiguiente == ESTADO_CERRADO)
        {
            EstadoSiguiente = Func_ESTADO_CERRADO();
        }
        if(EstadoSiguiente == ESTADO_CERRANDO)
        {
            EstadoSiguiente = Func_ESTADO_CERRANDO();
        }
        if(EstadoSiguiente == ESTADO_ERR)
        {
            EstadoSiguiente = Func_ESTADO_ERR();
        }
        if(EstadoSiguiente == ESTADO_STOP)
        {
            EstadoSiguiente = Func_ESTADO_STOP();
        }
    }
    return 0;
}

//Estado Inicial
int Func_ESTADO_INICIAL(void)
{
    printf("ESTADO INICIAL!\n");
    EstadoAnterior = EstadoActual;
    EstadoActual = ESTADO_INICIAL;
    io.LSC = true;
    io.LSA = true;

    //verifica si existe un  Error físico: ambos sensores activos
    if(io.LSC == true && io.LSA == true)
    {
    status.ERR_COD = ERR_LSW; //
    return ESTADO_ERR;
    }

    //puerta cerrada
    if(io.LSC == true && io.LSA == false)
    {
        return ESTADO_CERRADO;
    }
    //puerta abierta
    if(io.LSC == false && io.LSA == true)
    {
        return ESTADO_CERRANDO;
    }
    // Ningún sensor activado: puerta entreabierta o estado desconocido
    if(io.LSC == false && io.LSA == false)
    {
        return ESTADO_STOP;
    }
}

//Estado CERRANDO
int Func_ESTADO_CERRANDO(void)
{
    EstadoAnterior = EstadoActual;
    EstadoActual = ESTADO_CERRANDO;
    //funciones de estado estaticas (una sola vez)
     status.cntRunTimer = 0;//reinicio del timer
    io.MA = false;
    io.MC = true;
    io.BA = false;
    io.BC = false;
    io.BZZ = true; // Buzzer encendido mientras el porton esta en mov

    // CONTROLAR LAMPARA
    printf("ESTADO CERRANDO!\n");

    //ciclo de estado
    for(;;)
    {

       LampParpadeoRapido(); // Lámpara parpadea
        if(io.LSC == true)
        {
           return ESTADO_CERRADO;
        }
        if(io.BA == true || io.BC == true)
        {
          return ESTADO_STOP;
        }
        //verifica error de run timer
        if(status.cntRunTimer > config.RunTimer)
        {
            status.ERR_COD = ERR_OT;
            return ESTADO_ERR;
        }
    }
}

//Estado ABRIENDO

int Func_ESTADO_ABRIENDO(void)
{
    printf("ESTADO ABRIENDO!\n");
    EstadoAnterior = EstadoActual;
    EstadoActual = ESTADO_ABRIENDO;

    status.cntRunTimer = 0;//reinicio del timer
    io.MA = true;
    io.MC = false;
    io.BA = false;
    io.BC = false;
    io.BZZ = true; //Cuando Porton este en moviemiento BZZ en on


    for(;;)
    {
        LAMPParpadeoLento(); //
        if(io.LSA == true)
        {
           return ESTADO_ABIERTO;
        }
        if(io.BA == true || io.BC == true)
        {
          return ESTADO_STOP;
        }
        //verifica error de run timer
        if(status.cntRunTimer > config.RunTimer)
        {
            status.ERR_COD = ERR_OT;
            return ESTADO_ERR;
        }
    }
}

//ESTADO CERRADO
int Func_ESTADO_CERRADO(void)
{
    printf("ESTADO CERRADO!\n");
    EstadoAnterior = EstadoActual;
    EstadoActual = ESTADO_CERRADO;

    //funciones de estado estaticas
    io.MA = false;
    io.MC = false;
    io.BA = false;


    for(;;)
    {
        if(io.BA == true || io.BPP == true)
            return ESTADO_ABRIENDO;
    }
}

//ESTADO ABIERTO
int Func_ESTADO_ABIERTO(void)
{
    printf("ESTADO ABIERTO!\n");
    EstadoAnterior = EstadoActual;
    EstadoActual = ESTADO_ABIERTO;

// Apaga motores y botones, enciende lámpara indicando estado ABIERTO.
    io.MA = false;
    io.MC = false;
    io.BC = false;
    io.LAMP = true;

    time_t ultimoTiempo = time(NULL);  // Tiempo inicial para el contador de segundos.
    status.cntTimerCA = 0; // Reinicia el contador de tiempo en estado abierto.

    for (;;)
    {
        // Verifica si ha pasado 1 segundo desde la última vez que se imprimió.
        time_t ahora = time(NULL);
        if (difftime(ahora, ultimoTiempo) >= 1)
        {
            status.cntTimerCA++;
            printf("LAMPARA ON, PORTON ABIERTO - Tiempo: %u segundos\n", status.cntTimerCA);
            fflush(stdout);
            ultimoTiempo = ahora;
        }

        // Si se presiona botón de cierre o BPP, pasa a estado CERRANDO.
        if (io.BC == true || io.BPP == true)
        {
            return ESTADO_CERRANDO;
        }

// Si el tiempo en estado abierto supera el límite configurado, cierra automáticamente.
        if (status.cntTimerCA >= config.RunTimer)
        {
            return ESTADO_CERRANDO;
        }
    }
}

int Func_ESTADO_ERR(void)
{
    printf("ESTADO ERROR!\n");
    EstadoAnterior = EstadoActual;
    EstadoActual = ESTADO_ERR;

    // Apaga motores y botones, activa buzzer como señal de error.
    io.MA = false;
    io.MC = false;
    io.BA = false;
    io.BC = false;
    io.BZZ = true;
    for(;;)
    {
        // Error por ambos limit switches activos
       // Si alguno se desactiva, se considera que el error fue corregido y se retorna al inicio
        if (status.ERR_COD == ERR_LSW)
        {
            if(io.LSA == false || io.LSC == false)
        {
            status.ERR_COD = ERR_OK;
            return ESTADO_INICIAL; // Vuelve al estado inicial si el error desaparece.
        }
        }


    }
}

//ESTADO STOP
int Func_ESTADO_STOP(void)
{printf("ESTADO STOP!\n");
    EstadoAnterior = EstadoActual;
    EstadoActual = ESTADO_STOP;

     // Apaga los motores.
    io.MA = false;
    io.MC = false;

     for(;;)
    {   // Si se presiona botón de cerrar y no está en límite cerrado, pasa a cerrando.
        if (io.BC == true && io.LSC == false)
        {
            return ESTADO_CERRANDO;
        }
         // Si se presiona botón de abrir y no está en límite abierto, pasa a abriendo.
        if (io.BA == true && io.LSA == false)
        {
            return ESTADO_ABRIENDO;
        }
        // Si se presionan ambos botones, se mantiene en STOP.
        if (io.BA == true && io.BC == true)
        {
            return ESTADO_STOP;
        }
       // Si se presiona botón PP (puerta parada) estando entre ambos límites, cierra.
        if (io.BPP == true && io.LSA == false && io.LSC == false){
            return ESTADO_CERRANDO;
        }
    }
}


// Parpadeo lento de la lámpara y buzzer cada 0.5 segundos
void LAMPParpadeoLento()
{
    time_t tiempoActual = time(NULL);

    if (difftime(tiempoActual, tiempoUltimoCambio) >= 0.5)
    {
        estadoLamp = !estadoLamp;
        io.LAMP = estadoLamp;
        io.BZZ = estadoLamp;

        printf("Lámpara %s\n", estadoLamp ? "ON" : "OFF");
        fflush(stdout);

        tiempoUltimoCambio = tiempoActual;
    }
}



// Parpadeo rápido de la lámpara y buzzer cada 0.25 segundos
void LampParpadeoRapido()
{
    time_t tiempoActual = time(NULL);

    if (difftime(tiempoActual, tiempoUltimoCambio) >= 0.25)
    {
        estadoLamp = !estadoLamp;
        io.LAMP = estadoLamp;
        io.BZZ = estadoLamp;

        printf("Lámpara %s\n", estadoLamp ? "ON" : "OFF");
        fflush(stdout);

        tiempoUltimoCambio = tiempoActual;
    }
}

// Alarma de error con buzzer cada 5 segundos
void EmergenciaBuzzer()
{
    time_t tiempoActual = time(NULL);

    if (difftime(tiempoActual, tiempoUltimoCambio) >= 5.0)
    {
        estadobuzzer = !estadobuzzer;
        io.BZZ = estadobuzzer;

        printf("BUZZER DE EMERGENCIA\n");
        fflush(stdout);

        tiempoUltimoCambio = tiempoActual;
    }
}

//Funcion que se ejecuta cada 100ms con timer del micro
//esta funcion depende del microcontrolador

void TimerCallback (void)
{

}
