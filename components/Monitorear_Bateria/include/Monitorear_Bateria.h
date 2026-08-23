#pragma once
#include <stdbool.h>

#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#define SENSOR ADC_CHANNEL_0 // Corresponde al GPIO 0 ADC1

// Definición formal de los estados de carga de las celdas 18650
typedef enum
{
    BATTERY_STATE_NORMAL,
    BATTERY_STATE_LOW,     // Alerta visual en Flutter (Voltaje <= 6.5V)
    BATTERY_STATE_CRITICAL // Bloqueo total del MOSFET por hardware (Voltaje <= 6.0V)
} battery_state_t;

/**
 * @brief Inicializa los periféricos del ADC y calibra la curva interna del eFuse de la ESP32-C3.
 */
void battery_init(void);

/**
 * @brief Lee el ADC, aplica el filtro EMA, calcula el porcentaje vía LUT y gestiona la histéresis.
 *        Debe llamarse periódicamente (ej. cada 1 segundo en una tarea de FreeRTOS).
 */
void battery_update(void);

/**
 * @brief Obtiene el porcentaje de batería filtrado y libre de oscilaciones.
 * @return Entero entre 0 y 100.
 */
int battery_get_percentage(void);

/**
 * @brief Obtiene el voltaje real filtrado de la batería (para depuración o telemetría).
 * @return Flotante con el voltaje total de las celdas 2S.
 */
float battery_get_voltage(void);

/**
 * @brief Verifica si la batería se encuentra en un estado específico.
 */
bool battery_is_low(void);
bool battery_is_critical(void);
bool battery_is_normal(void);
