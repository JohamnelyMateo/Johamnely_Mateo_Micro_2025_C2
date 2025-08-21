#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_timer.h"
#include "mqtt_client.h"
#include "driver/gpio.h"


/* Configuración WiFi */
#define EXAMPLE_ESP_WIFI_SSID      "CLAROJRC3E"
#define EXAMPLE_ESP_WIFI_PASS      "8097931343"
#define EXAMPLE_ESP_MAXIMUM_RETRY  5
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_PSK


/*  Configuración MQTT */
#define MQTT_URI "mqtt://broker.emqx.io:1883" //Servidor MQTT
#define MQTT_TOPIC "johamnely_mqtt/esp32_ej"  //TOPIC


//Definicion de Pines
// Salidas
#define PIN_MOTOR_A    GPIO_NUM_5    // Motor abrir
#define PIN_MOTOR_C    GPIO_NUM_18   // Motor cerrar
#define PIN_BUZZER     GPIO_NUM_27   // Buzzer error/emergencia
#define PIN_LAMPARA    GPIO_NUM_26   // Lámpara indicador

// Entradas botones (activo en LOW)
#define PIN_BTN_ABRIR  GPIO_NUM_33
#define PIN_BTN_CERRAR GPIO_NUM_32
#define PIN_BTN_STOP   GPIO_NUM_35
#define PIN_BTN_PP     GPIO_NUM_34


// Finales de carrera físicos (DIP switch / interruptores)
// LSA activo = puerta totalmente abierta
// LSC activo = puerta totalmente cerrada
#define PIN_LSA GPIO_NUM_25
#define PIN_LSC GPIO_NUM_14


/*Definición estados FSM */
#define ESTADO_INICIAL      0
#define ESTADO_CERRANDO     1
#define ESTADO_ABRIENDO     2
#define ESTADO_CERRADO      3
#define ESTADO_ABIERTO      4
#define ESTADO_ERR          5
#define ESTADO_STOP         6
#define ESTADO_EMERGENCIA   7

//VARIABLES GLOBALES
volatile int  EstadoSiguiente = ESTADO_INICIAL;
volatile int  EstadoActual    = ESTADO_INICIAL;
volatile bool init_estado     = true; //sirve para ejecutar una sola vez la acción al entrar a un estado.

static EventGroupHandle_t s_wifi_event_group; //Sirve para sincronizar eventos de conexión WiFi (conectado o fallo).
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1


//Etiquetas para identificar en consola de dónde viene cada ESP_LOGI/W/E.
static const char *TAG = "wifi_station";
static const char *TAG_MQTT = "mqtt";
static const char *TAG_FSM = "FSM";
static const char *TAG_LAMP = "LAMPARA";

esp_mqtt_client_handle_t global_client = NULL;

/* Variables para controlar salidas */
struct IO {
    unsigned int MA:1, MC:1, BZZ:1, LAMP:1;
} io;

/* Variables para control de parpadeo de la lampara */
static bool lamp_state = false;
static int64_t last_toggle_time = 0;
static int lamp_blink_interval_ms = 0;

/* Prototipos */
static void fsm_request_state(int s);
static void actualizar_salidas(void);
static void ejecutar_maquina_estados(void);


/* Configurar pines GPIO (entradas y salidas)*/
static void gpio_init(void) {
    gpio_config_t io_conf = {0};

    // Salidas: Motor A y C, Buzzer, Lámpara
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << PIN_MOTOR_A) | (1ULL << PIN_MOTOR_C) | (1ULL << PIN_BUZZER) | (1ULL << PIN_LAMPARA);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);


    // Entradas: botones (pull-up, activo en LOW)
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << PIN_BTN_ABRIR) | (1ULL << PIN_BTN_CERRAR) | (1ULL << PIN_BTN_STOP) | (1ULL << PIN_BTN_PP);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&io_conf);


    // Entradas: finales de carrera (pull-down, activo en HIGH)
    // Uso DIP/interruptor que conecte el pin a Vcc cuando esté ACTIVO.
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << PIN_LSA) | (1ULL << PIN_LSC);
    io_conf.pull_down_en = GPIO_PULLDOWN_ENABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);
}

/* Actualizar GPIO según estructura io */
//Manda el valor guardado en io hacia los pines físicos.
static void actualizar_salidas(void) {
    gpio_set_level(PIN_MOTOR_A, io.MA);
    gpio_set_level(PIN_MOTOR_C, io.MC);
    gpio_set_level(PIN_BUZZER, io.BZZ);
    gpio_set_level(PIN_LAMPARA, io.LAMP);
}


/* Solicitar cambio de estado */
static inline void fsm_request_state(int s) {
    EstadoSiguiente = s;
    init_estado = true;
}


/* Leer botones (activo en LOW, por eso == 0) */
static bool leer_btn_abrir(void)  { return gpio_get_level(PIN_BTN_ABRIR)  == 0; }
static bool leer_btn_cerrar(void) { return gpio_get_level(PIN_BTN_CERRAR) == 0; }
static bool leer_btn_stop(void)   { return gpio_get_level(PIN_BTN_STOP)   == 0; }
static bool leer_btn_pp(void)     { return gpio_get_level(PIN_BTN_PP)     == 0; }


/* Leer finales de carrera físicos (activo en HIGH) */
static bool leer_LSA(void)        { return gpio_get_level(PIN_LSA) == 1; }
static bool leer_LSC(void)        { return gpio_get_level(PIN_LSC) == 1; }


/* Procesar botones: se llama periódicamente */
static void procesar_botones(void) {
    static bool btn_abrir_last = false;
    static bool btn_cerrar_last = false;
    static bool btn_stop_last = false;
    static bool btn_pp_last = false;

    bool btn_abrir = leer_btn_abrir();
    bool btn_cerrar = leer_btn_cerrar();
    bool btn_stop = leer_btn_stop();
    bool btn_pp = leer_btn_pp();

    // Detectar flanco de bajada (pulsación)

    if (btn_abrir && !btn_abrir_last) {
        ESP_LOGI(TAG_FSM, "Botón ABRIR presionado");
        fsm_request_state(ESTADO_ABRIENDO);
    }
    if (btn_cerrar && !btn_cerrar_last) {
        ESP_LOGI(TAG_FSM, "Botón CERRAR presionado");
        fsm_request_state(ESTADO_CERRANDO);
    }
    if (btn_stop && !btn_stop_last) {
        ESP_LOGI(TAG_FSM, "Botón STOP presionado");
        fsm_request_state(ESTADO_STOP);
    }
    if (btn_pp && !btn_pp_last) {
        ESP_LOGI(TAG_FSM, "Botón PP (Polifuncional) presionado");


        // Lógica PP:
        // Si está cerrado, abre
        // Si está abierto, cierra
        // Si está en lugar desconocido, cierra

        if (EstadoActual == ESTADO_CERRADO) {
            fsm_request_state(ESTADO_ABRIENDO);
        } else if (EstadoActual == ESTADO_ABIERTO) {
            fsm_request_state(ESTADO_CERRANDO);
        } else {
            fsm_request_state(ESTADO_CERRANDO);
        }
    }

    btn_abrir_last = btn_abrir;
    btn_cerrar_last = btn_cerrar;
    btn_stop_last = btn_stop;
    btn_pp_last = btn_pp;
}

//MAQUINA DE ESTADOS (FSM)
/* Ejecutar FSM */

void ejecutar_maquina_estados(void) {
    EstadoActual = EstadoSiguiente;
    int64_t now = esp_timer_get_time() / 1000; // ms


    // Procesar botones cada ciclo para detección de pulsaciones
    procesar_botones();


    // NUEVO: Protección por finales de carrera contradictorios 
    if (leer_LSA() && leer_LSC()) {
        if (EstadoActual != ESTADO_ERR) {
            ESP_LOGE(TAG_FSM, "ERROR: LSA y LSC activos al mismo tiempo");
            fsm_request_state(ESTADO_ERR);
        }
    }

    switch (EstadoActual) {
        case ESTADO_INICIAL:
            if (init_estado) {
                ESP_LOGI(TAG_FSM, "Estado INICIAL: Todo apagado.");
                init_estado = false;
                io.MA = 0; io.MC = 0; io.LAMP = 0; io.BZZ = 0;
                lamp_blink_interval_ms = 0;
                actualizar_salidas();
            }
            break;

        case ESTADO_ABRIENDO:
            if (init_estado) {
                ESP_LOGI(TAG_FSM, "Estado ABRIENDO: Motor adelante, lámpara parpadeo lento (0.5s).");
                init_estado = false;
                io.MA = 1; io.MC = 0; io.BZZ = 0;
                lamp_blink_interval_ms = 500;
                lamp_state = false;
                last_toggle_time = now;
                actualizar_salidas();
            }

            if (lamp_blink_interval_ms > 0 && (now - last_toggle_time >= lamp_blink_interval_ms)) {
                lamp_state = !lamp_state;
                io.LAMP = lamp_state;
                last_toggle_time = now;
                actualizar_salidas();
                ESP_LOGW(TAG_LAMP, "Lámpara %s (parpadeo lento)", lamp_state ? "Encendida" : "Apagada");
            }

            // Fin de carrera ABIERTO (LSA) físico

            if (leer_LSA()) {
                ESP_LOGI(TAG_FSM, "LSA activado: PUERTA ABIERTA");
                fsm_request_state(ESTADO_ABIERTO);
            }
            break;

        case ESTADO_CERRANDO:
            if (init_estado) {
                ESP_LOGI(TAG_FSM, "Estado CERRANDO: Motor reversa, lámpara parpadeo rápido (0.25s).");
                init_estado = false;
                io.MA = 0; io.MC = 1; io.BZZ = 0;
                lamp_blink_interval_ms = 250;
                lamp_state = false;
                last_toggle_time = now;
                actualizar_salidas();
            }

            if (lamp_blink_interval_ms > 0 && (now - last_toggle_time >= lamp_blink_interval_ms)) {
                lamp_state = !lamp_state;
                io.LAMP = lamp_state;
                last_toggle_time = now;
                actualizar_salidas();
                ESP_LOGW(TAG_LAMP, "Lámpara %s (parpadeo rápido)", lamp_state ? "Encendida" : "Apagada");
            }

            // Fin de carrera CERRADO (LSC) físico

            if (leer_LSC()) {
                ESP_LOGI(TAG_FSM, "LSC activado: PUERTA CERRADA");
                fsm_request_state(ESTADO_CERRADO);
            }
            break;

        case ESTADO_ABIERTO:
            if (init_estado) {
                ESP_LOGI(TAG_FSM, "Estado ABIERTO: Motor apagado, lámpara encendida fija.");
                init_estado = false;
                io.MA = 0; io.MC = 0; io.BZZ = 0; io.LAMP = 1;
                lamp_blink_interval_ms = 0;
                actualizar_salidas();
                ESP_LOGW(TAG_LAMP, "Lámpara Encendida fija (abierto)");
            }
            break;

        case ESTADO_CERRADO:
            if (init_estado) {
                ESP_LOGI(TAG_FSM, "Estado CERRADO: Todo apagado.");
                init_estado = false;
                io.MA = 0; io.MC = 0; io.BZZ = 0; io.LAMP = 0;
                lamp_blink_interval_ms = 0;
                actualizar_salidas();
                ESP_LOGW(TAG_LAMP, "Lámpara Apagada (cerrado)");
            }
            break;

        case ESTADO_STOP:
            if (init_estado) {
                ESP_LOGI(TAG_FSM, "Estado STOP: Motor detenido, lámpara apagada.");
                init_estado = false;
                io.MA = 0; io.MC = 0; io.BZZ = 0; io.LAMP = 0;
                lamp_blink_interval_ms = 0;
                actualizar_salidas();
                ESP_LOGW(TAG_LAMP, "Lámpara Apagada (stop)");
            }
            break;

        case ESTADO_EMERGENCIA:
            if (init_estado) {
                ESP_LOGE(TAG_FSM, "Estado EMERGENCIA: Motores apagados, buzzer activo, lámpara encendida fija.");
                init_estado = false;
                io.MA = 0; io.MC = 0; io.BZZ = 1; io.LAMP = 1;
                lamp_blink_interval_ms = 0;
                actualizar_salidas();
                ESP_LOGE(TAG_LAMP, "Lámpara Encendida fija (emergencia)");
            }
            break;

        case ESTADO_ERR:
            if (init_estado) {
                ESP_LOGE(TAG_FSM, "Estado ERROR: LSA y LSC activos. Motores apagados, buzzer y lámpara intermitente.");
                init_estado = false;
                io.MA = 0; io.MC = 0; io.BZZ = 1;
                lamp_blink_interval_ms = 300; // Parpadeo cada 300 ms
                lamp_state = false;
                last_toggle_time = now;
                actualizar_salidas();
            }

            if (lamp_blink_interval_ms > 0 && (now - last_toggle_time >= lamp_blink_interval_ms)) {
                lamp_state = !lamp_state;
                io.LAMP = lamp_state;
                last_toggle_time = now;
                actualizar_salidas();
            }
            break;

        default:
            ESP_LOGW(TAG_FSM, "Estado desconocido.");
            break;
    }
}

/* Timer callback */
//Cada 50ms se llama para evaluar la FSM.

void fsm_timer_callback(void* arg) {
    ejecutar_maquina_estados();
}

/* Limpiar mensaje MQTT recibido */
//Quita espacios, saltos de línea y convierte a minúsculas el mensaje recibido.

void limpiar_mensaje(char *s) {
    char *start = s;
    while (*start == ' ' || *start == '\r' || *start == '\n' || *start == '\t') start++;
    if (start != s) memmove(s, start, strlen(start) + 1);

    int len = strlen(s);
    while (len > 0 && (s[len - 1] == ' ' || s[len - 1] == '\r' || s[len - 1] == '\n' || s[len - 1] == '\t')) {
        s[len - 1] = '\0';
        len--;
    }

    for (int i = 0; s[i]; i++) {
        if (s[i] >= 'A' && s[i] <= 'Z') s[i] += 'a' - 'A';
    }
}


/* MQTT event handler */
//Cuando conecta → se suscribe al topic.
//Cuando recibe datos → limpia el mensaje y decide el estado (abrir, cerrar, stop, emergencia, reset).

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    esp_mqtt_event_handle_t event = event_data;
    global_client = event->client;

    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_BEFORE_CONNECT:
            ESP_LOGI(TAG_MQTT, "MQTT: Antes de conectar...");
            break;

        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG_MQTT, "MQTT conectado");
            esp_mqtt_client_subscribe(global_client, MQTT_TOPIC, 0);
            break;

        case MQTT_EVENT_DATA: {
            char msg[64] = {0};
            int len = (event->data_len < (int)sizeof(msg) - 1) ? event->data_len : (int)sizeof(msg) - 1;
            strncpy(msg, event->data, len);
            msg[len] = '\0';

            limpiar_mensaje(msg);

            ESP_LOGI(TAG_MQTT, "Comando recibido limpio: '%s'", msg);

            if      (strcmp(msg, "abrir") == 0)       fsm_request_state(ESTADO_ABRIENDO);
            else if (strcmp(msg, "cerrar") == 0)      fsm_request_state(ESTADO_CERRANDO);
            else if (strcmp(msg, "stop") == 0)        fsm_request_state(ESTADO_STOP);
            else if (strcmp(msg, "reset") == 0)       fsm_request_state(ESTADO_INICIAL);
            else if (strcmp(msg, "emergencia") == 0)  fsm_request_state(ESTADO_EMERGENCIA);
            else ESP_LOGW(TAG_MQTT, "Comando desconocido: '%s'", msg);
            break;
        }

        default:
            break;
    }
}

/* Inicializar MQTT */

static void mqtt_app_start(void) {
    const esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = MQTT_URI
    };
    global_client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(global_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(global_client);
}


/* WiFi event handler */
//Maneja la reconexión WiFi y notifica si falla.

static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        static int s_retry_num = 0;
        if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}


/* Inicializar STA */
//Crea la interfaz WiFi
//Si conecta correctamente → arranca MQTT.
//Si falla → muestra error.

static void wifi_init_sta(void) {
    s_wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t any_id_handler, got_ip_handler;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, &any_id_handler));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, &got_ip_handler));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .password = EXAMPLE_ESP_WIFI_PASS,
            .threshold.authmode = ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    EventBits_t bits = xEventGroupWaitBits(
        s_wifi_event_group,
        WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
        pdFALSE, pdFALSE,
        portMAX_DELAY
    );

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Conectado a WiFi");
        mqtt_app_start();
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGE(TAG, "Fallo conexión WiFi");
    } else {
        ESP_LOGE(TAG, "Evento inesperado en WiFi");
    }
}

//Función principal

void app_main(void) {
    ESP_ERROR_CHECK(nvs_flash_init());
    gpio_init();
    wifi_init_sta();

    // Comenzar en estado STOP para seguridad
    fsm_request_state(ESTADO_STOP);

    // Crear timer periódico para ejecutar la FSM cada 50 ms

    const esp_timer_create_args_t periodic_timer_args = {
        .callback = &fsm_timer_callback,
        .name = "fsm_timer"
    };
    esp_timer_handle_t periodic_timer;
    ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_args, &periodic_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(periodic_timer, 50 * 1000));

    // La FSM se ejecuta desde el timer; no se necesita loop aquí.
}






