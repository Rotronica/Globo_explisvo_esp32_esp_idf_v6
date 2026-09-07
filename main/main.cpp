#include <stdio.h>
#include "NimBLEDevice.h"
#include "nvs_flash.h" // sistema de almacenamiento no volátil
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TEMPORIZADOR_DE_SEGURIDAD

extern "C"
{
#include "filamento_pwm.h"
#include "Monitorear_Bateria.h"
}

const static char *TAG = "MAIN";

// Variable de estado global para controlar si el canal está habilitado o no
// static bool filamento_habilitado = false;
// Guardamos el último porcentaje configurado por el usuario (por defecto 30%)
static uint8_t ultima_potencia_configurada = 30;

// INTEGRACIÓN: Puntero global para poder enviar notificaciones de batería a Flutter desde la tarea
static NimBLECharacteristic *pCaracteristicaBat = nullptr;

// Callback corregido para el servidor global
class MisCallbacksServidor : public NimBLEServerCallbacks
{
    void onDisconnect(NimBLEServer *pServer, NimBLEConnInfo &connInfo, int reason) override
    {
        ESP_LOGI(TAG, "Celular desconectado (Razón: %d). Reiniciando anuncios...", reason);
        NimBLEDevice::startAdvertising();
    }
};

// Callback para la Característica de Comandos Generales (UUID: 5678)
class CallbacksComandos : public NimBLECharacteristicCallbacks
{
    void onWrite(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo &connInfo) override
    {
        std::string valor = pCharacteristic->getValue();
        if (!valor.empty())
        {
            uint8_t comando = valor[0];
            switch (comando)
            {
            case 0x00:                        // APAGADO TOTAL DE SEGURIDAD
                filamento_habilitado = false; // 1. Cambia el estado a falso
                Desactivar_filamento();       // 2. Apaga el MOSFET físicamente
                Luz_piloto_filamento(false);
#ifdef TEMPORIZADOR_DE_SEGURIDAD
                temporizador_seguridad_resetear(); // ← Resetear el temporizador
#endif
                // El monitoreo se volverá a encender automáticamente en la tarea de FreeRTOS
                ESP_LOGI(TAG, "Comando recibido: Sistema DESACTIVADO. Monitoreo de batería ENCENDIDO.");
                break;

            case 0x22: // ENCENDIDO / DISPARO DEL FILAMENTO
                if (battery_is_critical())
                {
                    ESP_LOGE(TAG, "¡DISPARO DENEGADO! Batería en nivel crítico (%.2fV).", battery_get_voltage());
                    break;
                }
#ifdef TEMPORIZADOR_DE_SEGURIDAD
                // Si el filamento ya está activo, NO hacer nada
                if (filamento_habilitado)
                {
                    ESP_LOGD(TAG, "Filamento ya activo, ignorando");
                    break;
                }

                // Activar filamento
                filamento_habilitado = true;
                Activar_filamento(ultima_potencia_configurada);
                Luz_piloto_filamento(true);

                // Iniciar temporizador de seguridad
                static uint16_t tiempo_seguro = 0;
                tiempo_seguro = obtener_tiempo_seguro(ultima_potencia_configurada);
                temporizador_seguridad_iniciar(tiempo_seguro);
#else
                filamento_habilitado = true;
                Activar_filamento(ultima_potencia_configurada);
                Luz_piloto_filamento(true);
#endif
                ESP_LOGI(TAG, "🔥 Filamento ACTIVADO al %d%%. Tiempo seguro: 3000ms",
                         ultima_potencia_configurada);
                break;

            default:
                ESP_LOGW(TAG, "Comando desconocido recibido: 0x%02X", comando);
                break;
            }
        }
    }
};

// Callback para la Característica del PWM Manual (UUID: 9ABC)
class CallbacksPwmManual : public NimBLECharacteristicCallbacks
{
    void onWrite(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo &connInfo) override
    {
        std::string valor = pCharacteristic->getValue();
        if (!valor.empty())
        {
            uint8_t potencia_directa = valor[0];
            ultima_potencia_configurada = potencia_directa;

            if (filamento_habilitado)
            {
                Activar_filamento(potencia_directa);
                ESP_LOGI(TAG, "Ajuste en tiempo real aplicado: %d%%", potencia_directa);
            }
            else
            {
                ESP_LOGI(TAG, "Potencia pre-configurada a %d%% (Filamento apagado actualmente)", potencia_directa);
            }
        }
    }
};

// INTEGRACIÓN: Tarea asíncrona de FreeRTOS para el monitoreo matemático de la batería
void tarea_monitoreo_bateria(void *pvParameters)
{
    battery_init();

    // INTEGRACIÓN: Contador auxiliar para espaciar la transmisión BLE
    uint8_t ciclo_ble = 0;

    while (1)
    {
        if (!filamento_habilitado)
        {
            battery_update();
        }

        if (battery_is_critical())
        {
            if (filamento_habilitado)
            {
                filamento_habilitado = false;
                Desactivar_filamento();
                Luz_piloto_filamento(false);
                ESP_LOGE("SEGURIDAD", "¡EMERGENCIA! Batería crítica. Cortando MOSFET.");
            }
        }

        int pct = battery_get_percentage();
        char str_payload[16]; // Declaración explícita del buffer de texto seguro

        if (battery_is_critical())
        {
            snprintf(str_payload, sizeof(str_payload), "CRIT:%d", pct);
        }
        else if (battery_is_low())
        {
            snprintf(str_payload, sizeof(str_payload), "LOW:%d", pct);
        }
        else
        {
            snprintf(str_payload, sizeof(str_payload), "OK:%d", pct);
        }

        // --- ENVIAR AL CELULAR SOLO CADA 1 SEGUNDO (2 ciclos de 500ms) ---
        ciclo_ble++;
        if (ciclo_ble >= 2)
        {
            if (pCaracteristicaBat != nullptr && NimBLEDevice::getServer()->getConnectedCount() > 0)
            {
                pCaracteristicaBat->setValue((uint8_t *)str_payload, strlen(str_payload));
                pCaracteristicaBat->notify();
            }
            ciclo_ble = 0; // Reiniciamos el contador de la antena
        }

        // --- CORRECCIÓN DE LOGS: Usamos "BAT" explícito entre comillas para evitar el error de compilación ---
        // Tus logs ahora saldrán a toda velocidad en la consola cada 500 ms de forma impecable.
        ESP_LOGI("BAT", "Porcentaje: %d%% | Voltaje: %.2fV | Paquete BLE: %s",
                 pct,
                 battery_get_voltage(),
                 str_payload);

        // Tu modificación de velocidad: Espera exacta de medio segundo
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

extern "C" void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        nvs_flash_erase();
        nvs_flash_init();
    }

    filamento_pwm_init();
    temporizador_seguridad_init(); // ← INICIALIZAR EL TEMPORIZADOR HARDWARE

    NimBLEDevice::init("Control-Globos");
    NimBLEServer *pServer = NimBLEDevice::createServer();
    NimBLEService *pServicio = pServer->createService("1234");

    pServer->setCallbacks(new MisCallbacksServidor());

    // Registro de la Característica 1: Comandos Estatales
    NimBLECharacteristic *pCaracteristicaCmd = pServicio->createCharacteristic("5678", NIMBLE_PROPERTY::WRITE);
    pCaracteristicaCmd->setCallbacks(new CallbacksComandos());

    // Registro de la Característica 2: Control PWM Directo
    NimBLECharacteristic *pCaracteristicaPwm = pServicio->createCharacteristic("9ABC", NIMBLE_PROPERTY::WRITE);
    pCaracteristicaPwm->setCallbacks(new CallbacksPwmManual());

    // INTEGRACIÓN: Registro de la Característica 3 para la Batería (UUID: DEF0)
    // Permisos READ para consultar y NOTIFY para que la ESP32 empuje el dato sola cada segundo
    pCaracteristicaBat = pServicio->createCharacteristic("DEF0", NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);

    NimBLEAdvertising *pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(pServicio->getUUID());

    NimBLEAdvertisementData scanResponseData;
    scanResponseData.setName("Control-Globos");
    pAdvertising->setScanResponseData(scanResponseData);

    NimBLEDevice::startAdvertising();

    // INTEGRACIÓN: Lanzamos la tarea de la batería al planificador de tareas de FreeRTOS
    // Asignada al núcleo 0 con prioridad media (2) y 3KB de memoria de pila asignada
    xTaskCreatePinnedToCore(tarea_monitoreo_bateria, "tarea_bat", 3072, NULL, 2, NULL, 0);

    ESP_LOGI(TAG, "Sistema listo con tres características independientes (Comandos, PWM y Batería).");
}