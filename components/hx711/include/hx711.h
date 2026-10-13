/*
 * hx711.h — Driver mínimo para el ADC de 24 bits HX711 (celdas de carga).
 *
 * Implementado desde cero con las APIs de GPIO de ESP-IDF a partir del
 * protocolo descrito en la hoja de datos del HX711 (Avia Semiconductor):
 * DOUT baja cuando hay un dato listo; se leen 24 bits (MSB primero) con
 * flancos de SCK y 1–3 pulsos extra seleccionan canal y ganancia para la
 * siguiente conversión.
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Canal y ganancia. El valor es el número de pulsos extra tras los 24 bits. */
typedef enum {
    HX711_GAIN_A_128 = 1, /**< Canal A, ganancia 128 (por defecto, la del sketch original). */
    HX711_GAIN_B_32 = 2,  /**< Canal B, ganancia 32. */
    HX711_GAIN_A_64 = 3,  /**< Canal A, ganancia 64. */
} hx711_gain_t;

/** Descriptor del dispositivo. */
typedef struct {
    gpio_num_t dout;   /**< Pin conectado a DOUT (entrada). */
    gpio_num_t sck;    /**< Pin conectado a PD_SCK (salida). */
    hx711_gain_t gain; /**< Canal/ganancia deseados. */
} hx711_t;

/**
 * @brief Configura los GPIO, despierta el HX711 y fija la ganancia.
 * @return ESP_OK, ESP_ERR_INVALID_ARG si los pines no sirven,
 *         o ESP_ERR_TIMEOUT si el HX711 no responde (revise el cableado).
 */
esp_err_t hx711_init(const hx711_t *dev);

/** @brief true si el HX711 tiene una conversión lista (DOUT en bajo). */
bool hx711_is_ready(const hx711_t *dev);

/** @brief Espera a que haya un dato listo, cediendo la CPU mientras tanto. */
esp_err_t hx711_wait_ready(const hx711_t *dev, uint32_t timeout_ms);

/** @brief Lee una conversión (24 bits con signo extendido a 32 bits). */
esp_err_t hx711_read_raw(const hx711_t *dev, int32_t *out);

/** @brief Promedio de @p veces lecturas consecutivas (equivale a read_average() de la librería Arduino). */
esp_err_t hx711_read_average(const hx711_t *dev, uint8_t veces, int32_t *out);

#ifdef __cplusplus
}
#endif
