#include <stdio.h>
#include "filamento_pwm.h"
#include "driver/ledc.h"
#include "esp_err.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "esp_timer.h"
static const char *TAG = "PWM";

// Variables del temporizador hardware
static esp_timer_handle_t timer_seguridad = NULL;
static bool temporizador_activo = false;
volatile bool filamento_habilitado = false;
extern volatile bool filamento_habilitado;

// Callback que se ejecuta CUANDO EL TEMPORIZADOR EXPIRA (en interrupción)
static void temporizador_seguridad_callback(void *arg)
{
    // ⚠️ ESTO SE EJECUTA EN CONTEXTO DE INTERRUPCIÓN
    // No usar ESP_LOGI aquí (puede causar problemas)

    // Apagar el filamento DIRECTAMENTE
    Desactivar_filamento();
    Luz_piloto_filamento(false);

    temporizador_activo = false;

    // Marcar que el filamento está apagado (variable global)
    // Usar una variable volátil para que el compilador no optimice
    filamento_habilitado = false;
}

// Inicializar el temporizador hardware
void temporizador_seguridad_init(void)
{
    esp_timer_create_args_t timer_args = {
        .callback = temporizador_seguridad_callback,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK, // Ejecutar en la tarea de temporizadores
        .name = "seguridad_timer"};
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &timer_seguridad));
    ESP_LOGI(TAG, "✅ Temporizador de seguridad inicializado");
}

// Iniciar el temporizador (NUNCA se reinicia si ya está activo)
void temporizador_seguridad_iniciar(uint16_t tiempo_ms)
{
    // Si el temporizador ya está activo, NO hacer nada
    if (temporizador_activo)
    {
        ESP_LOGD(TAG, "⏰ Temporizador ya activo, ignorando reinicio");
        return;
    }

    // Detener cualquier temporizador anterior
    if (timer_seguridad != NULL)
    {
        esp_timer_stop(timer_seguridad);
    }

    // Iniciar el temporizador
    temporizador_activo = true;
    ESP_ERROR_CHECK(esp_timer_start_once(timer_seguridad, tiempo_ms * 1000)); // microsegundos
    ESP_LOGI(TAG, "⏰ Temporizador iniciado: %dms", tiempo_ms);
}

// Resetear el temporizador
void temporizador_seguridad_resetear(void)
{
    if (timer_seguridad != NULL)
    {
        esp_timer_stop(timer_seguridad);
    }
    temporizador_activo = false;
    ESP_LOGD(TAG, "⏰ Temporizador reseteado");
}

// Función para obtener tiempo seguro según porcentaje
uint16_t obtener_tiempo_seguro(uint8_t porcentaje)
{
    switch (porcentaje)
    {
    case 5:
        return 6000;
    case 10:
        return 5000;
    case 20:
        return 4000;
    case 30:
        return 3000; // 3 segundos para 30%
    case 40:
        return 2000;
    case 50:
        return 1500; // 1.5 segundos para 50%
    case 60:
        return 1200;
    case 70:
        return 1000;
    case 80:
        return 800;
    case 90:
        return 600;
    case 100:
        return 400;
    default:
        return 2000;
    }
}
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

    // Luz piloto
    gpio_set_direction(LED_PILOTO, GPIO_MODE_OUTPUT);
    // Limpiar el pin GPIO
    gpio_set_level(LED_PILOTO, !false);
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
void Luz_piloto_filamento(bool estado)
{
    // Esta negado por que el led que lleva integrado la esp32c3 esta conectado al revez
    //  el catodo al gpio y el anodo debe estar conectado a VCC
    gpio_set_level(LED_PILOTO, !estado);
}