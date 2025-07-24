#include <stdio.h>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "freertos/timers.h"
#include "driver/adc.h"
#include "esp_timer.h"  // Se incluye para utilizar temporizador de alta resolución

// Pines usados para los LEDs
#define led1 2      // LED indicador de actividad
#define ledR 33     // LED rojo
#define ledG 25     // LED verde
#define ledB 26     // LED azul

// Duración entre muestras: 1s / 2400 muestras = ~416 microsegundos
#define SAMPLE_PERIOD_US 416

// Variable para alternar el estado del LED de actividad
uint8_t led_level = 0;

// Etiqueta usada en los mensajes de log
static const char *tag = "ADC_SAMPLE";

int adc_val = 0; // Variable global que almacenará la última lectura del ADC

// Manejador del temporizador de alta resolución (esp_timer)
esp_timer_handle_t adc_timer;

// Declaración de funciones
esp_err_t init_led(void);             // Configura los pines como salidas para los LEDs
esp_err_t blink_led(void);            // Enciende/apaga LED de actividad
esp_err_t set_adc(void);              // Inicializa el canal ADC
esp_err_t set_highres_timer(void);    // Crea e inicia el timer para muestreo a 2400 Hz

// Función que se ejecuta automáticamente en cada periodo del timer
void vTimerCallback(void *arg)
{
    blink_led();  // Cambia el estado del LED de actividad como indicador visual

    adc_val = adc1_get_raw(ADC1_CHANNEL_4);  // Toma una lectura directa del canal 4 del ADC1

    int adc_case = adc_val / 1000;  // Divide en rangos: 0–999, 1000–1999, ..., hasta ~4000

    ESP_LOGI(tag, "Monitorizando señal analógica: valor ADC = %i", adc_val);  // Muestra el valor de la señal analógica digitalizada

    // Enciende los LEDs de forma progresiva según el valor del ADC
    switch (adc_case)
    {
        case 0:
            gpio_set_level(ledR, 0);
            gpio_set_level(ledG, 0);
            gpio_set_level(ledB, 0);
            break;

        case 1:
            gpio_set_level(ledR, 1);
            gpio_set_level(ledG, 0);
            gpio_set_level(ledB, 0);
            break;

        case 2:
            gpio_set_level(ledR, 1);
            gpio_set_level(ledG, 1);
            gpio_set_level(ledB, 0);
            break;

        case 3:
        case 4:
            gpio_set_level(ledR, 1);
            gpio_set_level(ledG, 1);
            gpio_set_level(ledB, 1);
            break;

        default:
            // No hacer nada si está fuera de rango
            break;
    }
}

// Función principal
void app_main(void)
{
    init_led();              // Configura pines para los LEDs
    set_adc();               // Prepara el ADC1 canal 4 con 12 bits de resolución
    set_highres_timer();     // Inicia el muestreo a 2400 Hz con un timer de alta precisión

    // Muestra en la consola que todo se inicializó correctamente
    esp_log_level_set("*", ESP_LOG_INFO);
    ESP_LOGI(tag, "Sistema listo: muestreo ADC activo a 2400 Hz");
}

// Configura los pines de salida para cada LED
esp_err_t init_led(void)
{
    gpio_reset_pin(led1);
    gpio_set_direction(led1, GPIO_MODE_OUTPUT);

    gpio_reset_pin(ledR);
    gpio_set_direction(ledR, GPIO_MODE_OUTPUT);

    gpio_reset_pin(ledG);
    gpio_set_direction(ledG, GPIO_MODE_OUTPUT);

    gpio_reset_pin(ledB);
    gpio_set_direction(ledB, GPIO_MODE_OUTPUT);

    return ESP_OK;
}

// Alterna el LED1 entre encendido y apagado cada vez que se llama
esp_err_t blink_led(void)
{
    led_level = !led_level;  // Invierte el valor (0 → 1, 1 → 0)
    gpio_set_level(led1, led_level);
    return ESP_OK;
}

// Configura el ADC1 en el canal 4 con atenuación de 11 dB (rango hasta ~3.3 V)
esp_err_t set_adc(void)
{
    adc1_config_channel_atten(ADC1_CHANNEL_4, ADC_ATTEN_DB_11);  // Más rango de voltaje
    adc1_config_width(ADC_WIDTH_BIT_12);  // Resolución de 12 bits (0-4095)
    return ESP_OK;
}

// Configura el temporizador para que dispare la función de muestreo cada 416 µs
esp_err_t set_highres_timer(void)
{
    const esp_timer_create_args_t adc_timer_args = {
        .callback = &vTimerCallback,     // Función a ejecutar en cada disparo
        .name = "adc_sample_timer"       // Nombre interno del temporizador
    };

    // Crea e inicia el timer con el periodo deseado (416 µs)
    ESP_ERROR_CHECK(esp_timer_create(&adc_timer_args, &adc_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(adc_timer, SAMPLE_PERIOD_US));

    ESP_LOGI(tag, "Temporizador ADC configurado con periodo de 416 microsegundos");
    return ESP_OK;
}
