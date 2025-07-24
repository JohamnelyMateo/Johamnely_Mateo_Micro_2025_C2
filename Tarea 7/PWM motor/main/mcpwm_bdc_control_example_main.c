#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/ledc.h"
#include "esp_err.h"

#define PWM_GPIO        18  // GPIO conectado a la base del TIP31 (con resistencia)
#define PWM_FREQ_HZ     25000  // Frecuencia de 25 kHz para evitar zumbidos
#define PWM_RESOLUTION  LEDC_TIMER_8_BIT  // Resolución de 8 bits (0-255)
#define PWM_CHANNEL     LEDC_CHANNEL_0
#define PWM_SPEED_MODE  LEDC_HIGH_SPEED_MODE  // HIGH_SPEED_MODE es recomendado

void app_main(void)
{
    // Configurar el temporizador del PWM
    ledc_timer_config_t pwm_timer = {
        .duty_resolution = PWM_RESOLUTION,
        .freq_hz = PWM_FREQ_HZ,
        .speed_mode = PWM_SPEED_MODE,
        .timer_num = LEDC_TIMER_0,
        .clk_cfg = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&pwm_timer));

    // Configurar el canal PWM
    ledc_channel_config_t pwm_channel = {
        .gpio_num = PWM_GPIO,
        .speed_mode = PWM_SPEED_MODE,
        .channel = PWM_CHANNEL,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
        .hpoint = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&pwm_channel));

    // Bucle que aumenta y luego disminuye el PWM
    while (1) {
        // Subir duty cycle
        for (int duty = 0; duty <= 255; duty += 5) {
            ledc_set_duty(PWM_SPEED_MODE, PWM_CHANNEL, duty);
            ledc_update_duty(PWM_SPEED_MODE, PWM_CHANNEL);
            printf("PWM aumentando: Duty = %d\n", duty);
            vTaskDelay(pdMS_TO_TICKS(50));
        }

        // Bajar duty cycle
        for (int duty = 255; duty >= 0; duty -= 5) {
            ledc_set_duty(PWM_SPEED_MODE, PWM_CHANNEL, duty);
            ledc_update_duty(PWM_SPEED_MODE, PWM_CHANNEL);
            printf("PWM disminuyendo: Duty = %d\n", duty);
            vTaskDelay(pdMS_TO_TICKS(50));
        }
    }
}
