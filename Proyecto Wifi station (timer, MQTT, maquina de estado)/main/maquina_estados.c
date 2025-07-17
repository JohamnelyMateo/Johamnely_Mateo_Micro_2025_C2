#include <stdio.h>
#include <stdbool.h>
#include <time.h>
#include <string.h>
#include "maquina_estados.h"

int EstadoActual = ESTADO_INICIAL;
int EstadoSiguiente = ESTADO_INICIAL;
int EstadoAnterior = ESTADO_INICIAL;

bool estadoLamp = false;
time_t tiempoUltimoCambio = 0;
bool estadobuzzer = false;

struct IO io = {0};
struct STATUS status = {0, 0, ERR_OK};
struct CONFIG config = {10, 5};

int Func_ESTADO_INICIAL(void)
{
    EstadoAnterior = EstadoActual;
    EstadoActual = ESTADO_INICIAL;

    io.LSC = false;
    io.LSA = true;

    if (io.LSC && io.LSA)
    {
        status.ERR_COD = ERR_LSW;
        return ESTADO_ERR;
    }

    if (io.LSC && !io.LSA)
        return ESTADO_CERRADO;

    if (!io.LSC && io.LSA)
        return ESTADO_CERRANDO;

    return ESTADO_STOP;
}

int Func_ESTADO_CERRANDO(void)
{
    EstadoAnterior = EstadoActual;
    EstadoActual = ESTADO_CERRANDO;

    status.cntRunTimer++;
    io.MA = false;
    io.MC = true;
    io.BZZ = true;

    LampParpadeoRapido();

    if (io.LSC)
        return ESTADO_CERRADO;

    if (io.BA || io.BC)
        return ESTADO_STOP;

    if (status.cntRunTimer > config.RunTimer)
    {
        status.ERR_COD = ERR_OT;
        return ESTADO_ERR;
    }

    return ESTADO_CERRANDO;
}

int Func_ESTADO_ABRIENDO(void)
{
    EstadoAnterior = EstadoActual;
    EstadoActual = ESTADO_ABRIENDO;

    status.cntRunTimer++;
    io.MA = true;
    io.MC = false;
    io.BZZ = true;

    LAMPParpadeoLento();

    if (io.LSA)
        return ESTADO_ABIERTO;

    if (io.BA || io.BC)
        return ESTADO_STOP;

    if (status.cntRunTimer > config.RunTimer)
    {
        status.ERR_COD = ERR_OT;
        return ESTADO_ERR;
    }

    return ESTADO_ABRIENDO;
}

int Func_ESTADO_CERRADO(void)
{
    EstadoAnterior = EstadoActual;
    EstadoActual = ESTADO_CERRADO;

    io.MA = false;
    io.MC = false;

    if (io.BA || io.BPP)
        return ESTADO_ABRIENDO;

    return ESTADO_CERRADO;
}

int Func_ESTADO_ABIERTO(void)
{
    EstadoAnterior = EstadoActual;
    EstadoActual = ESTADO_ABIERTO;

    io.MA = false;
    io.MC = false;
    io.LAMP = true;

    status.cntTimerCA++;

    if (io.BC || io.BPP)
        return ESTADO_CERRANDO;

    if (status.cntTimerCA >= config.TimerCA)
        return ESTADO_CERRANDO;

    return ESTADO_ABIERTO;
}

int Func_ESTADO_ERR(void)
{
    EstadoAnterior = EstadoActual;
    EstadoActual = ESTADO_ERR;

    io.MA = false;
    io.MC = false;
    io.BZZ = true;

    EmergenciaBuzzer();

    if (status.ERR_COD == ERR_LSW && (!io.LSC || !io.LSA))
    {
        status.ERR_COD = ERR_OK;
        return ESTADO_INICIAL;
    }

    return ESTADO_ERR;
}

int Func_ESTADO_STOP(void)
{
    EstadoAnterior = EstadoActual;
    EstadoActual = ESTADO_STOP;

    io.MA = false;
    io.MC = false;

    if (io.BC && !io.LSC)
        return ESTADO_CERRANDO;

    if (io.BA && !io.LSA)
        return ESTADO_ABRIENDO;

    if (io.BPP && !io.LSA && !io.LSC)
        return ESTADO_CERRANDO;

    return ESTADO_STOP;
}

void LAMPParpadeoLento()
{
    time_t actual = time(NULL);
    if (difftime(actual, tiempoUltimoCambio) >= 1.0)
    {
        estadoLamp = !estadoLamp;
        io.LAMP = estadoLamp;
        tiempoUltimoCambio = actual;
    }
}

void LampParpadeoRapido()
{
    time_t actual = time(NULL);
    if (difftime(actual, tiempoUltimoCambio) >= 0.5)
    {
        estadoLamp = !estadoLamp;
        io.LAMP = estadoLamp;
        tiempoUltimoCambio = actual;
    }
}

void EmergenciaBuzzer()
{
    time_t actual = time(NULL);
    if (difftime(actual, tiempoUltimoCambio) >= 5.0)
    {
        estadobuzzer = !estadobuzzer;
        io.BZZ = estadobuzzer;
        tiempoUltimoCambio = actual;
    }
}

void ejecutar_maquina_estados(void)
{
    switch (EstadoActual)
    {
    case ESTADO_INICIAL:
        EstadoSiguiente = Func_ESTADO_INICIAL();
        break;
    case ESTADO_CERRANDO:
        EstadoSiguiente = Func_ESTADO_CERRANDO();
        break;
    case ESTADO_ABRIENDO:
        EstadoSiguiente = Func_ESTADO_ABRIENDO();
        break;
    case ESTADO_CERRADO:
        EstadoSiguiente = Func_ESTADO_CERRADO();
        break;
    case ESTADO_ABIERTO:
        EstadoSiguiente = Func_ESTADO_ABIERTO();
        break;
    case ESTADO_ERR:
        EstadoSiguiente = Func_ESTADO_ERR();
        break;
    case ESTADO_STOP:
        EstadoSiguiente = Func_ESTADO_STOP();
        break;
    default:
        EstadoSiguiente = ESTADO_ERR;
        break;
    }

    EstadoActual = EstadoSiguiente;
     // Limpiar botones luego de procesarlos
    io.BA = 0;
    io.BC = 0;
    io.BPP = 0;
}
void manejar_comando_mqtt(const char *comando)
{
    if (strcmp(comando, "abrir") == 0)
    {
        io.BA = 1;
        io.BC = 0;
        io.BPP = 0;
    }
    else if (strcmp(comando, "cerrar") == 0)
    {
        io.BC = 1;
        io.BA = 0;
        io.BPP = 0;
    }
    else if (strcmp(comando, "parar") == 0)
    {
        io.BPP = 1;
        io.BA = 0;
        io.BC = 0;
    }
    else if (strcmp(comando, "reset") == 0)
    {
        io.BA = 0;
        io.BC = 0;
        io.BPP = 0;
        status.ERR_COD = ERR_OK;
    }
}


