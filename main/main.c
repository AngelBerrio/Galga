/*
 * Sensor de nivel con HX711 — versión ESP-IDF
 *
 * Lee una celda de carga con el HX711, calcula peso, volumen y nivel de agua
 * en un tanque cilíndrico y permite calibrar desde el monitor serie:
 *   '+'          sube el factor de calibración
 *   '-'          baja el factor de calibración
 *   't' / 'tare' + Enter  pone la báscula en 0
 *
 * Arquitectura (dos tareas de FreeRTOS):
 *   tarea_consola  → lee la UART, interpreta comandos y los envía por una cola
 *   tarea_medicion → dueña del HX711 y de la calibración; aplica los comandos,
 *                    mide y muestra el resultado cada CONFIG_NIVEL_PERIODO_MS
 *
 * Origen y créditos:
 *   El punto de partida fue el sketch de Arduino publicado por el usuario
 *   "eggybacon" en el foro de Arduino (14-jun-2023), hilo "HX711 Calibration":
 *     https://forum.arduino.cc/t/hx711-calibration/1137872
 *   Ese sketch sigue a su vez el ejemplo "SparkFun_HX711_Calibration" de
 *   Nathan Seidle, SparkFun Electronics (2014, licencia Beerware / dominio público):
 *     https://github.com/sparkfun/HX711-Load-Cell-Amplifier
 *   y usaba la librería Arduino bogde/HX711.
 *
 *   Sobre esa base se hizo primero una versión Arduino propia (ESP32, cálculo
 *   de volumen y nivel en un tanque cilíndrico, tara por comando 't'/'tare')
 *   y luego esta reescritura completa en C para ESP-IDF: driver HX711 propio
 *   sobre GPIO, tareas y cola de FreeRTOS, configuración por Kconfig, lógica
 *   separada en un componente con pruebas Unity. No se copia código de la
 *   librería bogde/HX711.
 *
 * SPDX-License-Identifier: MIT
 */
#include <stdio.h>

#include "driver/uart.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "sdkconfig.h"

#include "hx711.h"
#include "nivel.h"

static const char *TAG = "nivel";

#ifdef CONFIG_ESP_CONSOLE_UART_NUM
#define UART_CONSOLA CONFIG_ESP_CONSOLE_UART_NUM
#else
#define UART_CONSOLA UART_NUM_0
#endif

#define UART_RX_BUF   256 /* debe ser mayor que la FIFO de hardware (128 B) */
#define COLA_COMANDOS 8

static QueueHandle_t s_cola_cmd;

static hx711_t s_bascula = {
    .dout = (gpio_num_t)CONFIG_NIVEL_HX711_DOUT_GPIO,
    .sck = (gpio_num_t)CONFIG_NIVEL_HX711_SCK_GPIO,
    .gain = HX711_GAIN_A_128,
};

/* Lee caracteres de la consola y convierte en comandos lo que reconoce. */
static void tarea_consola(void *arg)
{
    (void)arg;
    nivel_parser_t parser;
    nivel_parser_init(&parser);
    uint8_t buf[32];

    for (;;) {
        const int n = uart_read_bytes(UART_CONSOLA, buf, sizeof(buf), pdMS_TO_TICKS(100));
        for (int i = 0; i < n; i++) {
            const nivel_cmd_t cmd = nivel_parser_procesar(&parser, (char)buf[i]);
            if (cmd != NIVEL_CMD_NINGUNO && xQueueSend(s_cola_cmd, &cmd, 0) != pdTRUE) {
                ESP_LOGW(TAG, "Cola de comandos llena, comando descartado");
            }
        }
    }
}

static esp_err_t hacer_tara(int32_t *offset)
{
    return hx711_read_average(&s_bascula, CONFIG_NIVEL_MUESTRAS_TARA, offset);
}

static void tarea_medicion(void *arg)
{
    (void)arg;
    float factor = (float)CONFIG_NIVEL_FACTOR_CALIBRACION;
    const float densidad = CONFIG_NIVEL_DENSIDAD_MILESIMAS / 1000.0f;
    const float area_cm2 = CONFIG_NIVEL_AREA_BASE_CENTESIMAS / 100.0f;

    /* Tara inicial al arrancar, como en setup() del sketch. */
    int32_t offset = 0;
    if (hacer_tara(&offset) != ESP_OK) {
        ESP_LOGE(TAG, "No se pudo hacer la tara inicial; se usa offset 0");
    }
    ESP_LOGI(TAG, "Zero factor (offset de tara): %ld", (long)offset);

    TickType_t ultimo = xTaskGetTickCount();
    for (;;) {
        /* 1. Aplicar todos los comandos pendientes. */
        nivel_cmd_t cmd;
        while (xQueueReceive(s_cola_cmd, &cmd, 0) == pdTRUE) {
            switch (cmd) {
            case NIVEL_CMD_FACTOR_MAS:
                factor = nivel_ajustar_factor(factor, +1);
                break;
            case NIVEL_CMD_FACTOR_MENOS:
                factor = nivel_ajustar_factor(factor, -1);
                break;
            case NIVEL_CMD_TARA:
                printf("🔄 Tarando báscula... Nivel reiniciado a 0\n");
                if (hacer_tara(&offset) != ESP_OK) {
                    ESP_LOGE(TAG, "Fallo al tarar; se conserva el offset anterior");
                }
                break;
            default:
                break;
            }
        }

        /* 2. Medir y mostrar. */
        int32_t cruda;
        nivel_medida_t m;
        if (hx711_read_raw(&s_bascula, &cruda) != ESP_OK) {
            ESP_LOGW(TAG, "Sin lectura del HX711");
        } else if (!nivel_calcular(cruda, offset, factor, densidad, area_cm2, &m)) {
            ESP_LOGW(TAG, "Parámetros de cálculo inválidos");
        } else {
            printf("⚖️ Peso: %.3f kg | 💧 Vol: %.3f L | 📏 Nivel: %.2f cm | Factor: %.1f\n",
                   m.peso_kg, m.volumen_l, m.nivel_cm, factor);
        }

        /* 3. Esperar al siguiente periodo (cadencia fija, a diferencia de delay()). */
        vTaskDelayUntil(&ultimo, pdMS_TO_TICKS(CONFIG_NIVEL_PERIODO_MS));
    }
}

void app_main(void)
{
    printf("=== Calibración y Nivel HX711 (ESP-IDF) ===\n");
    printf("Área de la base: %.2f cm^2\n", CONFIG_NIVEL_AREA_BASE_CENTESIMAS / 100.0f);
    printf("Comandos disponibles en el monitor serie:\n");
    printf("  '+'    -> Aumenta el factor de calibración\n");
    printf("  '-'    -> Disminuye el factor de calibración\n");
    printf("  't' o 'tare' + Enter -> Reinicia la báscula a 0 (Tara)\n");
    printf("------------------------------------------------------\n");

    /* Driver UART en la consola para poder leer lo que se escribe en el monitor. */
    ESP_ERROR_CHECK(uart_driver_install(UART_CONSOLA, UART_RX_BUF, 0, 0, NULL, 0));

    s_cola_cmd = xQueueCreate(COLA_COMANDOS, sizeof(nivel_cmd_t));
    configASSERT(s_cola_cmd != NULL);

    esp_err_t err = hx711_init(&s_bascula);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HX711 no inicializado (%s). Revise DOUT=GPIO%d y SCK=GPIO%d",
                 esp_err_to_name(err), CONFIG_NIVEL_HX711_DOUT_GPIO, CONFIG_NIVEL_HX711_SCK_GPIO);
    }

    xTaskCreate(tarea_consola, "consola", 3072, NULL, 4, NULL);
    xTaskCreate(tarea_medicion, "medicion", 4096, NULL, 5, NULL);
}
