#include <stdio.h>
#include "NimBLEDevice.h"
#include "nvs_flash.h" // sistema de almacenamiento no volátil
#include "esp_log.h"

extern "C"
{
#include "filamento_pwm.h"
}

const static char *TAG = "MAIN";

// Variable de estado global para controlar si el canal está habilitado o no
static bool filamento_habilitado = false;
// Guardamos el último porcentaje configurado por el usuario (por defecto 30%)
static uint8_t ultima_potencia_configurada = 30;
// 1. Crear la clase de callbacks para el servidor global
// Callback corregido para el servidor global
class MisCallbacksServidor : public NimBLEServerCallbacks
{
    // Agregamos "int reason" como tercer parámetro obligatorio de la firma en v6
    void onDisconnect(NimBLEServer *pServer, NimBLEConnInfo &connInfo, int reason) override
    {
        ESP_LOGI(TAG, "Celular desconectado (Razón: %d). Reiniciando anuncios...", reason);
        // Forzamos a la antena a volver a transmitir su nombre al aire de inmediato
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
            case 0x00:                        // Apagado total de seguridad
                filamento_habilitado = false; // Bloqueamos el estado
                Desactivar_filamento();
                ESP_LOGI(TAG, "Comando recibido: Sistema DESACTIVADO (0x%02X)", comando);
                break;

            case 0x22:                       // Encendido / Activación de la señal
                filamento_habilitado = true; // Desbloqueamos el estado
                // Encendemos utilizando el último valor guardado o el valor por defecto
                Activar_filamento(ultima_potencia_configurada);
                ESP_LOGI(TAG, "Comando recibido: Sistema ACTIVADO al %d%% (0x%02X)", ultima_potencia_configurada, comando);
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
            uint8_t potencia_directa = valor[0]; // El byte recibido es el % (0 a 100)

            // Guardamos siempre el valor que el usuario desliza en la app
            ultima_potencia_configurada = potencia_directa;

            // Sincronización: Solo aplicamos el PWM al hardware si el switch principal está encendido
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

extern "C" void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        nvs_flash_erase();
        nvs_flash_init();
    }

    filamento_pwm_init();

    NimBLEDevice::init("Control-Globos");
    NimBLEServer *pServer = NimBLEDevice::createServer();
    NimBLEService *pServicio = pServer->createService("1234");
    // VINCULAR LOS CALLBACKS AL SERVIDOR AQUÍ:
    pServer->setCallbacks(new MisCallbacksServidor());
    // Registro de la Característica 1: Comandos Estatales
    NimBLECharacteristic *pCaracteristicaCmd = pServicio->createCharacteristic("5678", NIMBLE_PROPERTY::WRITE);
    pCaracteristicaCmd->setCallbacks(new CallbacksComandos());

    // Registro de la Característica 2: Control PWM Directo
    NimBLECharacteristic *pCaracteristicaPwm = pServicio->createCharacteristic("9ABC", NIMBLE_PROPERTY::WRITE);
    pCaracteristicaPwm->setCallbacks(new CallbacksPwmManual());

    NimBLEAdvertising *pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(pServicio->getUUID());

    NimBLEAdvertisementData scanResponseData;
    scanResponseData.setName("Control-Globos");
    pAdvertising->setScanResponseData(scanResponseData);

    NimBLEDevice::startAdvertising();
    ESP_LOGI(TAG, "Sistema listo con dos características independientes.");
}
