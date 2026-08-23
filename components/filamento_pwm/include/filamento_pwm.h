#pragma once

#include "driver/gpio.h"
#include "stdbool.h"
// Definicion de variables del pwm
#define PWM_TIMER LEDC_TIMER_0
#define PWM_MODO LEDC_LOW_SPEED_MODE
#define PWM_OUT_IO 4                     // Salida de la señal PWM por el GPIO_4
#define PWM_CANAL LEDC_CHANNEL_0         // Canal PWM a utilizar
#define RESOLUCION_PWM LEDC_TIMER_13_BIT // Rosolución PWM a 13 bits
// #define CICLO_UTIL		(4096) // Set duty to 50%--> (2 ** 13) * 50% = 4096 -->formula: D=(2^13)*0.5=4096
#define FRECUENCIA_PWM (1000) // Frecuencia a 1 kHz //AQUI SE PUEDE CAMBIAR LA FRECUENCIA

#define LED_PILOTO GPIO_NUM_8 // Es el gpio donde esta conectado el led que trae la esp32c3 pero esta invertido
void filamento_pwm_init(void);
void Activar_filamento(uint8_t porcentaje_potencia);
void Desactivar_filamento(void);
void Luz_piloto_filamento(bool estado);