#include <stdio.h>
#include "Monitorear_Bateria.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_log.h"
#include "esp_err.h"

static const char *TAG = "BATTERY_LIB";

// ---- CONFIGURACIONES DE HARDWARE Y FILTRO ----
const float EMA_ALPHA = 0.05f;
static float v_filtrado_ema = 8.4f;
static int pct_estable = 100;
static battery_state_t estado_actual = BATTERY_STATE_NORMAL;

//--------Variables del ADC--------------
static adc_oneshot_unit_handle_t adc1_handle;
static adc_cali_handle_t cali_handle = NULL;
static bool adc_calibrado = false;

// ---- LOOKUP TABLE (LUT) PARA BATERÍAS 18650 2S ----
typedef struct
{
    float voltaje;
    int porcentaje;
} lut_battery_t;

static const lut_battery_t tabla_descarga[] = {
    {8.40f, 100},
    {8.20f, 95},
    {8.00f, 90},
    {7.80f, 80},
    {7.60f, 70},
    {7.40f, 50},
    {7.20f, 30},
    {7.00f, 15},
    {6.50f, 5},
    {6.00f, 0},
};
static const int TAMANO_TABLA = sizeof(tabla_descarga) / sizeof(lut_battery_t);

// Función interna para calcular la interpolación lineal en la LUT
static int calcular_porcentaje_lut(float voltaje)
{
    if (voltaje >= tabla_descarga[0].voltaje)
        return 100;
    if (voltaje <= tabla_descarga[TAMANO_TABLA - 1].voltaje)
        return 0;

    for (int i = 0; i < TAMANO_TABLA - 1; i++)
    {
        if (voltaje <= tabla_descarga[i].voltaje && voltaje >= tabla_descarga[i + 1].voltaje)
        {
            float v_alta = tabla_descarga[i].voltaje;
            float v_baja = tabla_descarga[i + 1].voltaje;
            int p_alto = tabla_descarga[i].porcentaje;
            int p_bajo = tabla_descarga[i + 1].porcentaje;

            return p_bajo + (int)((voltaje - v_baja) * (p_alto - p_bajo) / (v_alta - v_baja));
        }
    }
    return 0;
}

void battery_init(void)
{
    // 1. Inicializar Unidad ADC1
    adc_oneshot_unit_init_cfg_t init_config1 = {
        .unit_id = ADC_UNIT_1,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));

    // 2. Configurar Canal (GPIO0 / Canal 0)
    adc_oneshot_chan_cfg_t config = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };
    adc_oneshot_config_channel(adc1_handle, SENSOR, &config);

    // 3. Crear esquema de calibración de curva para la ESP32-C3 (eFuse)
    adc_cali_curve_fitting_config_t cali_config = {
        .unit_id = ADC_UNIT_1,
        .chan = ADC_CHANNEL_0,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };
    esp_err_t err_calibracion = adc_cali_create_scheme_curve_fitting(&cali_config, &cali_handle);

    if (err_calibracion == ESP_OK)
    {
        adc_calibrado = true;
        ESP_LOGI(TAG, "Calibración por Curve Fitting activa en ESP32-C3.");
    }
    else
    {
        ESP_LOGW(TAG, "Fallo de calibración. Usando aproximación matemática.");
    }
    // =========================================================================
    // CORRECCIÓN: INICIALIZACIÓN FORZADA DEL FILTRO DIGITAL (SEEDING)
    // =========================================================================
    // Tomamos una lectura instantánea inicial para romper los 8.4V por defecto
    // y clavar la memoria en el voltaje real exacto de la batería al arrancar.
    int raw_inicial = 0;
    int mv_inicial = 0;

    adc_oneshot_read(adc1_handle, ADC_CHANNEL_0, &raw_inicial);

    if (adc_calibrado)
    {
        adc_cali_raw_to_voltage(cali_handle, raw_inicial, &mv_inicial);
    }
    else
    {
        mv_inicial = (raw_inicial * 2500) / 4095;
    }

    // Calculamos el voltaje inicial con tu constante definitiva calibrada de hardware (4.4336f)
    float v_inicial = (mv_inicial / 1000.0f) * 4.5445f;

    // Sobrescribimos el historial de la variable global con la realidad de tus baterías
    v_filtrado_ema = v_inicial;

    // Calculamos el porcentaje inicial directo desde la tabla para inicializar la histéresis
    pct_estable = calcular_porcentaje_lut(v_filtrado_ema);

    ESP_LOGI(TAG, "Filtro inicializado con éxito. Voltaje de arranque detectado: %.2fV (%d%%)", v_filtrado_ema, pct_estable);
}

void battery_update(void)
{
    int raw_val = 0;
    int voltaje_mv = 0;

    // 1. Muestreo del hardware
    adc_oneshot_read(adc1_handle, SENSOR, &raw_val);

    if (adc_calibrado)
    {
        adc_cali_raw_to_voltage(cali_handle, raw_val, &voltaje_mv);
    }
    else
    {
        voltaje_mv = (raw_val * 2500) / 4095;
    }

    // Convertimos los milivoltios leídos en el pin a Voltios flotantes
    float voltaje_pin = voltaje_mv / 1000.0f;
    /* =========================================================================
     * CALIBRACIÓN Y CORRECCIÓN EMPÍRICA DEL DIVISOR DE TENSIÓN (Celdas 18650 2S)
     * =========================================================================
     *
     * 1. MULTIPLICADOR TEÓRICO COMPENSADO POR HARDWARE (4.4104f):
     *    A diferencia del factor teórico puro de 4.3v (asumiendo resistencias ideales
     *    de 33k y 10k), se midieron los componentes físicos reales con multímetro:
     *    - R1 (Resistencia Superior) = 33.32 kΩ
     *    - R2 (Resistencia Inferior)  = 9.77 kΩ
     *
     *    Fórmula del divisor inverso: (R1 + R2) / R2
     *    (33.32 + 9.77) / 9.77 = 43.09 / 9.77 = 4.4104f
     *
     * 2. AJUSTE POR IMPEDANCIA Y DESFASE INTERNO DEL ADC (eFuse):
     *    Al realizar pruebas en caliente, la ESP32-C3 reportaba un voltaje promedio
     *    de 7.57V, mientras que el multímetro de banco medía 7.61V reales en bornes.
     *    Esto se debe a la atenuación por la resistencia de carga del ADC.
     *
     *    Cálculo del Factor de Ajuste Empírico:
     *    Factor de Ajuste = V_real (Multímetro) / V_leído (ESP32)
     *    Factor de Ajuste = 7.61V / 7.57V ≈ 1.00528
     *
     * 3. CONSTANTE DEFINITIVA CALIBRADA (4.4336f):
     *    Multiplicamos el factor real de hardware por el ajuste de desfase del ADC:
     *    4.4104f (Hardware) * 1.00528 (ADC) = 4.4336f
     *
     * Al aplicar 4.4336f, alineamos con precisión milimétrica el sensor del sistema
     * IoT con el instrumento de medición físico, eliminando el error residual.
     * ========================================================================= */
    float v_inst_bateria = voltaje_pin * 4.;

    // 2. Filtro Digital EMA (Suaviza los bajones masivos del filamento)
    v_filtrado_ema = (EMA_ALPHA * v_inst_bateria) + ((1.0f - EMA_ALPHA) * v_filtrado_ema);

    // 3. Mapeo no lineal con la LUT
    int pct_calculado = calcular_porcentaje_lut(v_filtrado_ema);

    // 4. Algoritmo de Histéresis
    if (pct_calculado < pct_estable)
    {
        pct_estable = pct_calculado; // Permite bajar la batería libremente
    }
    else if (pct_calculado > pct_estable + 2)
    {
        pct_estable = pct_calculado; // Solo sube si la recuperación es real y sostenida (>2%)
    }

    // 5. Máquina de estados lógicos de seguridad
    if (v_filtrado_ema <= 6.0f)
    {
        estado_actual = BATTERY_STATE_CRITICAL;
    }
    else if (v_filtrado_ema <= 6.5f)
    {
        estado_actual = BATTERY_STATE_LOW;
    }
    else
    {
        estado_actual = BATTERY_STATE_NORMAL;
    }
}

int battery_get_percentage(void)
{
    return pct_estable;
}

float battery_get_voltage(void)
{
    return v_filtrado_ema;
}

bool battery_is_low(void)
{
    return (estado_actual == BATTERY_STATE_LOW);
}

bool battery_is_critical(void)
{
    return (estado_actual == BATTERY_STATE_CRITICAL);
}

bool battery_is_normal(void)
{
    return (estado_actual == BATTERY_STATE_NORMAL);
}