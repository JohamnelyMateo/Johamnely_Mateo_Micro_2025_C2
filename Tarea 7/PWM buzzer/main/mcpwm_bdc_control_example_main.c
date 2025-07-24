#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/ledc.h"
#include "esp_err.h"

#define BUZZER_GPIO     23                  // GPIO conectado al buzzer o al TIP31
#define BUZZER_FREQ_HZ  2000                // Frecuencia del sonido (2kHz)
#define BUZZER_DUTY     128                 // 50% de duty (en resolución de 8 bits: 128/255)
#define BUZZER_CHANNEL  LEDC_CHANNEL_0
#define BUZZER_TIMER    LEDC_TIMER_0
#define BUZZER_SPEED_MODE LEDC_HIGH_SPEED_MODE

void app_main(void)
{
    // Configuración del temporizador PWM
    ledc_timer_config_t ledc_timer = {
        .duty_resolution = LEDC_TIMER_8_BIT,
        .freq_hz = BUZZER_FREQ_HZ,
        .speed_mode = BUZZER_SPEED_MODE,
        .timer_num = BUZZER_TIMER,
        .clk_cfg = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    // Configuración del canal PWM
    ledc_channel_config_t ledc_channel = {
        .channel    = BUZZER_CHANNEL,
        .duty       = 0,  // Inicia apagado
        .gpio_num   = BUZZER_GPIO,
        .speed_mode = BUZZER_SPEED_MODE,
        .hpoint     = 0,
        .timer_sel  = BUZZER_TIMER
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));

    printf("Iniciando bip repetitivo del buzzer en GPIO %d\n", BUZZER_GPIO);

    while (1) {
        // ENCENDER buzzer (duty 50%)
        ledc_set_duty(BUZZER_SPEED_MODE, BUZZER_CHANNEL, BUZZER_DUTY);
        ledc_update_duty(BUZZER_SPEED_MODE, BUZZER_CHANNEL);
        printf("Buzzer ON\n");
        vTaskDelay(pdMS_TO_TICKS(1000));  // 1 segundo encendido

        // APAGAR buzzer (duty 0%)
        ledc_set_duty(BUZZER_SPEED_MODE, BUZZER_CHANNEL, 0);
        ledc_update_duty(BUZZER_SPEED_MODE, BUZZER_CHANNEL);
        printf("Buzzer OFF\n");
        vTaskDelay(pdMS_TO_TICKS(1000));  // 1 segundo apagado
    }
}


