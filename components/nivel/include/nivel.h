/*
 * nivel.h — Lógica pura del sensor de nivel (sin dependencias de hardware).
 *
 * Convierte la lectura cruda del HX711 en peso, volumen y altura de líquido
 * en un tanque cilíndrico, y procesa los comandos de calibración que llegan
 * por la consola serie. Al no tocar GPIO ni FreeRTOS, este componente se
 * puede probar con Unity tanto en la tarjeta como en el host (target linux).
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Paso con que '+' y '-' cambian el factor de calibración (igual que el sketch original). */
#define NIVEL_PASO_FACTOR 500.0f

/** Valor mínimo permitido del factor; evita llegar a 0 (división por cero) o a negativos. */
#define NIVEL_FACTOR_MIN NIVEL_PASO_FACTOR

/** Longitud máxima de un comando de texto (sin contar el terminador). */
#define NIVEL_CMD_MAX_LEN 15

/** Resultado completo de una medición. */
typedef struct {
    float peso_kg;   /**< Peso neto sobre la celda de carga. */
    float volumen_l; /**< Volumen equivalente del líquido. */
    float nivel_cm;  /**< Altura del líquido en el tanque. */
} nivel_medida_t;

/**
 * @brief Peso en kg a partir de la lectura cruda del HX711.
 *
 * peso = (lectura - offset) / factor
 *
 * @param lectura_cruda Valor de 24 bits con signo entregado por el HX711.
 * @param offset        Lectura cruda con la báscula vacía (tara).
 * @param factor        Cuentas del ADC por kg. Debe ser distinto de 0.
 * @return Peso en kg, o NAN si el factor es 0.
 */
float nivel_peso_kg(int32_t lectura_cruda, int32_t offset, float factor);

/**
 * @brief Volumen en litros a partir del peso y la densidad.
 * @param densidad_kg_l Densidad en kg/L (agua = 1.0). Debe ser > 0.
 * @return Volumen en L, o NAN si la densidad no es válida.
 */
float nivel_volumen_l(float peso_kg, float densidad_kg_l);

/**
 * @brief Altura del líquido en cm para un tanque de sección constante.
 * @param area_base_cm2 Área de la base en cm². Debe ser > 0.
 * @return Altura en cm, o NAN si el área no es válida.
 */
float nivel_altura_cm(float volumen_l, float area_base_cm2);

/**
 * @brief Área de un círculo a partir del diámetro (útil para calcular la base del tanque).
 * @return Área en cm², o NAN si el diámetro no es positivo.
 */
float nivel_area_circulo_cm2(float diametro_cm);

/**
 * @brief Encadena peso → volumen → altura en un solo paso.
 * @param[out] out Resultado. No se modifica si la función retorna false.
 * @return true si todos los parámetros son válidos y el resultado es finito.
 */
bool nivel_calcular(int32_t lectura_cruda, int32_t offset, float factor,
                    float densidad_kg_l, float area_base_cm2, nivel_medida_t *out);

/**
 * @brief Ajusta el factor de calibración en múltiplos de NIVEL_PASO_FACTOR.
 * @param pasos Positivo para subir, negativo para bajar.
 * @return Nuevo factor, nunca menor que NIVEL_FACTOR_MIN.
 */
float nivel_ajustar_factor(float factor, int pasos);

/** Comandos reconocidos en la consola serie. */
typedef enum {
    NIVEL_CMD_NINGUNO = 0,  /**< Nada que hacer todavía. */
    NIVEL_CMD_FACTOR_MAS,   /**< '+' : sube el factor. */
    NIVEL_CMD_FACTOR_MENOS, /**< '-' : baja el factor. */
    NIVEL_CMD_TARA,         /**< "t" o "tare" + Enter : pone la báscula en 0. */
} nivel_cmd_t;

/** Estado del intérprete de comandos (se procesa carácter a carácter). */
typedef struct {
    char buf[NIVEL_CMD_MAX_LEN + 1];
    size_t len;
    bool desbordado; /**< true si la línea actual superó NIVEL_CMD_MAX_LEN. */
} nivel_parser_t;

/** @brief Deja el intérprete listo para una nueva línea. */
void nivel_parser_init(nivel_parser_t *p);

/**
 * @brief Procesa un carácter recibido por la consola.
 *
 * '+' y '-' actúan de inmediato. Las letras se acumulan hasta recibir
 * '\r' o '\n'; en ese momento, si la línea es "t" o "tare" (sin importar
 * mayúsculas), se devuelve NIVEL_CMD_TARA.
 */
nivel_cmd_t nivel_parser_procesar(nivel_parser_t *p, char c);

#ifdef __cplusplus
}
#endif
