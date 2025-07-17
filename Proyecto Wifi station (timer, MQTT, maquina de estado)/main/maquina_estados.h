#ifndef MAQUINA_ESTADOS_H
#define MAQUINA_ESTADOS_H

#include <stdbool.h>
#include <time.h>

#define ESTADO_INICIAL 0
#define ESTADO_CERRANDO 1
#define ESTADO_ABRIENDO 2
#define ESTADO_CERRADO 3
#define ESTADO_ABIERTO 4
#define ESTADO_ERR 5
#define ESTADO_STOP 6

#define ERR_OK 0
#define ERR_OT 1
#define ERR_LSW 3

int Func_ESTADO_INICIAL(void);
int Func_ESTADO_CERRANDO(void);
int Func_ESTADO_ABRIENDO(void);
int Func_ESTADO_CERRADO(void);
int Func_ESTADO_ABIERTO(void);
int Func_ESTADO_ERR(void);
int Func_ESTADO_STOP(void);
void ejecutar_maquina_estados(void);

extern int EstadoActual;
extern int EstadoSiguiente;
extern int EstadoAnterior;

struct IO
{
    unsigned int LSC : 1;
    unsigned int LSA : 1;
    unsigned int BA : 1;
    unsigned int BC : 1;
    unsigned int BPP : 1;
    unsigned int SE : 1;
    unsigned int MA : 1;
    unsigned int MC : 1;
    unsigned int BZZ : 1;
    unsigned int LAMP : 1;
    unsigned int MQTT_CMD : 3;
};
extern struct IO io;

struct STATUS
{
    unsigned int cntTimerCA;
    unsigned int cntRunTimer;
    int ERR_COD;
};
extern struct STATUS status;

struct CONFIG
{
    unsigned int RunTimer;
    unsigned int TimerCA;
};
extern struct CONFIG config;

void LAMPParpadeoLento(void);
void LampParpadeoRapido(void);
void EmergenciaBuzzer(void);
void manejar_comando_mqtt(const char *comando);

#endif

