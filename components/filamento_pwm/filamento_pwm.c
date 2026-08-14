#include <stdio.h>
#include "filamento_pwm.h"
#include "driver/ledc.h"
#include "esp_err.h"
#include "esp_log.h"
static const char *TAG = "PWM";
void filamento_pwm_init(void)
{
    // Prepare and then apply the LEDC PWM timer configuration
    ledc_timer_config_t pwm_timer = {
        .speed_mode = PWM_MODO,
        .duty_resolution = RESOLUCION_PWM,
        .timer_num = PWM_TIMER,
        .freq_hz = FRECUENCIA_PWM,
        .clk_cfg = LEDC_AUTO_CLK};
    esp_err_t err_pwm = ledc_timer_config(&pwm_timer);
    if (err_pwm != ESP_OK)
    {
        ESP_LOGE(TAG, "Error tmr pwm");
    }
    else
    {
        ESP_LOGI(TAG, "Tmr pwm con exito configurado");
    }

    // Prepare and then apply the LEDC PWM channel configuration
    ledc_channel_config_t pwm_channel = {
        .speed_mode = PWM_MODO,
        .channel = PWM_CANAL,
        .timer_sel = PWM_TIMER,
        .gpio_num = PWM_OUT_IO,
        .duty = 0,
        .hpoint = 0};
    esp_err_t err_chanel_pwm = ledc_channel_config(&pwm_channel);
    if (err_chanel_pwm != ESP_OK)
    {
        ESP_LOGE(TAG, "Error en la configuracion del canal pwm");
    }
    else
    {
        ESP_LOGI(TAG, "Canal pwm configurado exitosamente");
    }
}
void Activar_filamento(uint8_t porcentaje_potencia)
{
    // si el dato ingresado es mayor a 100
    if (porcentaje_potencia > 100)
        porcentaje_potencia = 100;
    // ciclo util
    // 2^13 = 8192-->resolucion del TMR
    // CORRECCIÓN: Primero multiplicamos por 8192 y luego dividimos entre 100
    // Usamos uint32_t temporalmente para evitar cualquier desbordamiento numérico
    uint32_t duty_cycle = (8192 * (uint32_t)porcentaje_potencia) / 100;

    // Imprime en el monitor el valor real del Duty que se aplicará (debe ser entre 0 y 8192)
    // ESP_LOGI(TAG, "Porcentaje: %d%% -> Valor Duty Aplicado: %lu", porcentaje_potencia, duty_cycle);
    ESP_ERROR_CHECK(ledc_set_duty(PWM_MODO, PWM_CANAL, duty_cycle));
    // Update duty to apply the new value
    ESP_ERROR_CHECK(ledc_update_duty(PWM_MODO, PWM_CANAL));
}
void Desactivar_filamento(void)
{
    ledc_set_duty(PWM_MODO, PWM_CANAL, 0);
    ledc_update_duty(PWM_MODO, PWM_CANAL);
}