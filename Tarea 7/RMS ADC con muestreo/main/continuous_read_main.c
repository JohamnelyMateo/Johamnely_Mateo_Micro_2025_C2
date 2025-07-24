#include <stdio.h>
#include <math.h>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "freertos/timers.h"
#include "driver/adc.h"
#include "esp_timer.h" // Para temporizador de alta resolución

// Pines de salida
#define LED_INDICADOR 2
#define LED_ROJO 33
#define LED_VERDE 25
#define LED_AZUL 26

// Configuración de muestreo
#define SAMPLE_PERIOD_US 416     // Intervalo entre muestras (2400 Hz)
#define SAMPLE_COUNT 2400        // Muestras por segundo
#define VREF 3.3                 // Voltaje de referencia del ADC

// Variables globales
uint8_t led_level = 0;
static const char *tag = "MEDIDOR_RMS";

int samples[SAMPLE_COUNT];  // Buffer para muestras
int sample_index = 0;
bool buffer_full = false;
esp_timer_handle_t adc_timer;

// Declaración de funciones
esp_err_t init_leds(void);
esp_err_t toggle_led(void);
esp_err_t setup_adc(void);
esp_err_t start_adc_timer(void);

// Función llamada periódicamente por el temporizador
void vTimerCallback(void *arg)
{
    toggle_led(); // Parpadeo indicador

    int adc_val = adc1_get_raw(ADC1_CHANNEL_4);
    samples[sample_index++] = adc_val;

    if (sample_index >= SAMPLE_COUNT)
    {
        buffer_full = true;
        sample_index = 0;
    }

    if (buffer_full)
    {
        double suma_cuadrados = 0;
        for (int i = 0; i < SAMPLE_COUNT; i++)
        {
            double voltaje = ((double)samples[i] / 4095.0) * VREF;
            suma_cuadrados += voltaje * voltaje;
        }

        double voltaje_rms = sqrt(suma_cuadrados / SAMPLE_COUNT);
        ESP_LOGI(tag, "🔌 Voltaje RMS: %.3f V", voltaje_rms);

        buffer_full = false;
    }

    // Control de LEDs según el nivel analógico
    int nivel = adc_val / 1000;

    switch (nivel)
    {
        case 0:
            gpio_set_level(LED_ROJO, 0);
            gpio_set_level(LED_VERDE, 0);
            gpio_set_level(LED_AZUL, 0);
            break;
        case 1:
            gpio_set_level(LED_ROJO, 1);
            gpio_set_level(LED_VERDE, 0);
            gpio_set_level(LED_AZUL, 0);
            break;
        case 2:
            gpio_set_level(LED_ROJO, 1);
            gpio_set_level(LED_VERDE, 1);
            gpio_set_level(LED_AZUL, 0);
            break;
        case 3:
        case 4:
            gpio_set_level(LED_ROJO, 1);
            gpio_set_level(LED_VERDE, 1);
            gpio_set_level(LED_AZUL, 1);
            break;
        default:
            break;
    }
}

void app_main(void)
{
    init_leds();
    setup_adc();
    start_adc_timer();

    esp_log_level_set("*", ESP_LOG_INFO);
    ESP_LOGI(tag, "Sistema de medición RMS iniciado correctamente.");
}

esp_err_t init_leds(void)
{
    gpio_reset_pin(LED_INDICADOR);
    gpio_set_direction(LED_INDICADOR, GPIO_MODE_OUTPUT);

    gpio_reset_pin(LED_ROJO);
    gpio_set_direction(LED_ROJO, GPIO_MODE_OUTPUT);

    gpio_reset_pin(LED_VERDE);
    gpio_set_direction(LED_VERDE, GPIO_MODE_OUTPUT);

    gpio_reset_pin(LED_AZUL);
    gpio_set_direction(LED_AZUL, GPIO_MODE_OUTPUT);

    return ESP_OK;
}

esp_err_t toggle_led(void)
{
    led_level = !led_level;
    gpio_set_level(LED_INDICADOR, led_level);
    return ESP_OK;
}

esp_err_t setup_adc(void)
{
    adc1_config_channel_atten(ADC1_CHANNEL_4, ADC_ATTEN_DB_11); // GPIO32
    adc1_config_width(ADC_WIDTH_BIT_12);
    return ESP_OK;
}

esp_err_t start_adc_timer(void)
{
    const esp_timer_create_args_t timer_args = {
        .callback = &vTimerCallback,
        .name = "adc_timer"
    };

    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &adc_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(adc_timer, SAMPLE_PERIOD_US));

    ESP_LOGI(tag, "Temporizador de muestreo iniciado: 416us por muestra.");
    return ESP_OK;
}
