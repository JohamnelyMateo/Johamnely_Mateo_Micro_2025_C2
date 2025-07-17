#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "protocol_examples_common.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

#include "esp_log.h"
#include "mqtt_client.h"
#include "esp_timer.h"

#include "maquina_estados.h"

static const char *TAG = "mqtt_example";

esp_mqtt_client_handle_t global_mqtt_client = NULL;

const char* estado_a_texto(int estado) {
    switch (estado) {
        case ESTADO_INICIAL: return "ESTADO_INICIAL";
        case ESTADO_CERRANDO: return "ESTADO_CERRANDO";
        case ESTADO_ABRIENDO: return "ESTADO_ABRIENDO";
        case ESTADO_CERRADO: return "ESTADO_CERRADO";
        case ESTADO_ABIERTO: return "ESTADO_ABIERTO";
        case ESTADO_ERR: return "ESTADO_ERR";
        case ESTADO_STOP: return "ESTADO_STOP";
        default: return "DESCONOCIDO";
    }
}

void timer_callback(void *arg)
{
    ejecutar_maquina_estados();

    if (global_mqtt_client != NULL) {
        char mensaje[50];
        snprintf(mensaje, sizeof(mensaje), "Estado: %s", estado_a_texto(EstadoActual));
        esp_mqtt_client_publish(global_mqtt_client, "johamnely_mqtt/estado", mensaje, 0, 0, 0);
    }
}

static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0) {
        ESP_LOGE(TAG, "Último error %s: 0x%x", message, error_code);
    }
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;

    switch ((esp_mqtt_event_id_t)event_id)
    {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "Conectado al broker MQTT");

        msg_id = esp_mqtt_client_subscribe(client, "johamnely_mqtt/porton", 0);
        ESP_LOGI(TAG, "Suscripción exitosa, msg_id=%d", msg_id);

        msg_id = esp_mqtt_client_publish(client, "johamnely_mqtt/estado", "ESP32 conectado y listo", 0, 0, 0);
        ESP_LOGI(TAG, "Publicación exitosa, msg_id=%d", msg_id);
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "Desconectado del broker MQTT");
        break;

    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "Mensaje recibido:");
        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("DATA=%.*s\r\n", event->data_len, event->data);

        char comando[50] = {0};
        snprintf(comando, sizeof(comando), "%.*s", event->data_len, event->data);
        manejar_comando_mqtt(comando);
        break;

    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "Ocurrió un error MQTT");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT)
        {
            log_error_if_nonzero("Error desde esp-tls", event->error_handle->esp_tls_last_esp_err);
            log_error_if_nonzero("Error desde la pila TLS", event->error_handle->esp_tls_stack_err);
            log_error_if_nonzero("Error de socket de transporte", event->error_handle->esp_transport_sock_errno);
            ESP_LOGI(TAG, "Descripción del errno: %s", strerror(event->error_handle->esp_transport_sock_errno));
        }
        break;

    default:
        ESP_LOGI(TAG, "Otro evento recibido: id=%d", event->event_id);
        break;
    }
}

static void mqtt_app_start(void)
{
    const esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = "mqtt://broker.emqx.io",
    };

    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);

    global_mqtt_client = client;
}

void app_main(void)
{
    ESP_LOGI(TAG, "[APP] Iniciando aplicación...");
    ESP_LOGI(TAG, "[APP] Memoria libre: %" PRIu32 " bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "[APP] Versión del IDF: %s", esp_get_idf_version());

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(example_connect());

    mqtt_app_start();

    const esp_timer_create_args_t periodic_timer_args = {
        .callback = &timer_callback,
        .name = "mi_timer_50ms"};

    esp_timer_handle_t periodic_timer;
    ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_args, &periodic_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(periodic_timer, 50000));
}
