/*
 * hx711.c — Driver bit-bang del HX711 con GPIO de ESP-IDF.
 *
 * SPDX-License-Identifier: MIT
 */
#include "hx711.h"

#include "esp_check.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "hx711";

/* El HX711 entra en modo de bajo consumo si SCK queda en alto más de 60 µs,
 * así que los 25–27 pulsos se generan dentro de una sección crítica para que
 * una interrupción o un cambio de tarea no estire ningún pulso. */
static portMUX_TYPE s_spinlock = portMUX_INITIALIZER_UNLOCKED;

#define HX711_TIMEOUT_MS 1000 /* a 10 muestras/s cada dato tarda ~100 ms */

esp_err_t hx711_init(const hx711_t *dev)
{
    ESP_RETURN_ON_FALSE(dev != NULL, ESP_ERR_INVALID_ARG, TAG, "dev es NULL");
    ESP_RETURN_ON_FALSE(GPIO_IS_VALID_GPIO(dev->dout), ESP_ERR_INVALID_ARG, TAG, "GPIO DOUT inválido");
    ESP_RETURN_ON_FALSE(GPIO_IS_VALID_OUTPUT_GPIO(dev->sck), ESP_ERR_INVALID_ARG, TAG, "GPIO SCK no es de salida");
    ESP_RETURN_ON_FALSE(dev->gain >= HX711_GAIN_A_128 && dev->gain <= HX711_GAIN_A_64,
                        ESP_ERR_INVALID_ARG, TAG, "ganancia inválida");

    const gpio_config_t sck_cfg = {
        .pin_bit_mask = 1ULL << dev->sck,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&sck_cfg), TAG, "config SCK");

    const gpio_config_t dout_cfg = {
        .pin_bit_mask = 1ULL << dev->dout,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE, /* DOUT del HX711 es push-pull */
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&dout_cfg), TAG, "config DOUT");

    /* SCK en bajo saca al HX711 del modo power-down. */
    gpio_set_level(dev->sck, 0);

    /* La ganancia se aplica a la conversión siguiente: se hace una lectura
     * descartable para que la primera medición útil ya use la ganancia pedida. */
    int32_t descarte;
    return hx711_read_raw(dev, &descarte);
}

bool hx711_is_ready(const hx711_t *dev)
{
    return gpio_get_level(dev->dout) == 0;
}

esp_err_t hx711_wait_ready(const hx711_t *dev, uint32_t timeout_ms)
{
    const TickType_t inicio = xTaskGetTickCount();
    const TickType_t limite = pdMS_TO_TICKS(timeout_ms);
    while (!hx711_is_ready(dev)) {
        if ((xTaskGetTickCount() - inicio) > limite) {
            return ESP_ERR_TIMEOUT;
        }
        vTaskDelay(1); /* cede la CPU a otras tareas en vez de esperar activamente */
    }
    return ESP_OK;
}

esp_err_t hx711_read_raw(const hx711_t *dev, int32_t *out)
{
    ESP_RETURN_ON_FALSE(dev != NULL && out != NULL, ESP_ERR_INVALID_ARG, TAG, "argumento NULL");
    ESP_RETURN_ON_ERROR(hx711_wait_ready(dev, HX711_TIMEOUT_MS), TAG, "HX711 no responde");

    uint32_t valor = 0;

    portENTER_CRITICAL(&s_spinlock);
    for (int i = 0; i < 24; i++) {
        gpio_set_level(dev->sck, 1);
        esp_rom_delay_us(1);
        valor = (valor << 1) | (uint32_t)gpio_get_level(dev->dout);
        gpio_set_level(dev->sck, 0);
        esp_rom_delay_us(1);
    }
    /* Pulsos extra: seleccionan canal y ganancia de la próxima conversión. */
    for (int i = 0; i < (int)dev->gain; i++) {
        gpio_set_level(dev->sck, 1);
        esp_rom_delay_us(1);
        gpio_set_level(dev->sck, 0);
        esp_rom_delay_us(1);
    }
    portEXIT_CRITICAL(&s_spinlock);

    /* Complemento a 2 de 24 bits → int32_t (extensión de signo). */
    if (valor & 0x800000u) {
        valor |= 0xFF000000u;
    }
    *out = (int32_t)valor;
    return ESP_OK;
}

esp_err_t hx711_read_average(const hx711_t *dev, uint8_t veces, int32_t *out)
{
    ESP_RETURN_ON_FALSE(veces > 0 && out != NULL, ESP_ERR_INVALID_ARG, TAG, "argumento inválido");
    int64_t suma = 0;
    for (uint8_t i = 0; i < veces; i++) {
        int32_t lectura;
        ESP_RETURN_ON_ERROR(hx711_read_raw(dev, &lectura), TAG, "lectura %u", (unsigned)i);
        suma += lectura;
    }
    *out = (int32_t)(suma / veces);
    return ESP_OK;
}
