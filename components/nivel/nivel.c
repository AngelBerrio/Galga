/*
 * nivel.c — Implementación de la lógica del sensor de nivel.
 *
 * SPDX-License-Identifier: MIT
 */
#include "nivel.h"

#include <ctype.h>
#include <math.h>
#include <string.h>

#define NIVEL_PI 3.14159265358979f

float nivel_peso_kg(int32_t lectura_cruda, int32_t offset, float factor)
{
    if (factor == 0.0f) {
        return NAN;
    }
    /* La resta se hace en 64 bits para que no desborde con offsets extremos. */
    const int64_t neto = (int64_t)lectura_cruda - (int64_t)offset;
    return (float)neto / factor;
}

float nivel_volumen_l(float peso_kg, float densidad_kg_l)
{
    if (!(densidad_kg_l > 0.0f)) { /* también rechaza NAN */
        return NAN;
    }
    return peso_kg / densidad_kg_l;
}

float nivel_altura_cm(float volumen_l, float area_base_cm2)
{
    if (!(area_base_cm2 > 0.0f)) {
        return NAN;
    }
    const float volumen_cm3 = volumen_l * 1000.0f; /* 1 L = 1000 cm³ */
    return volumen_cm3 / area_base_cm2;
}

float nivel_area_circulo_cm2(float diametro_cm)
{
    if (!(diametro_cm > 0.0f)) {
        return NAN;
    }
    const float radio = diametro_cm / 2.0f;
    return NIVEL_PI * radio * radio;
}

bool nivel_calcular(int32_t lectura_cruda, int32_t offset, float factor,
                    float densidad_kg_l, float area_base_cm2, nivel_medida_t *out)
{
    if (out == NULL) {
        return false;
    }
    const float peso = nivel_peso_kg(lectura_cruda, offset, factor);
    const float volumen = nivel_volumen_l(peso, densidad_kg_l);
    const float altura = nivel_altura_cm(volumen, area_base_cm2);

    if (!isfinite(peso) || !isfinite(volumen) || !isfinite(altura)) {
        return false;
    }
    out->peso_kg = peso;
    out->volumen_l = volumen;
    out->nivel_cm = altura;
    return true;
}

float nivel_ajustar_factor(float factor, int pasos)
{
    const float nuevo = factor + (float)pasos * NIVEL_PASO_FACTOR;
    return (nuevo < NIVEL_FACTOR_MIN) ? NIVEL_FACTOR_MIN : nuevo;
}

void nivel_parser_init(nivel_parser_t *p)
{
    memset(p, 0, sizeof(*p));
}

/* Comparación sin distinguir mayúsculas (strcasecmp no es estándar C). */
static bool iguales_sin_mayusculas(const char *a, const char *b)
{
    while (*a != '\0' && *b != '\0') {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) {
            return false;
        }
        a++;
        b++;
    }
    return *a == *b;
}

nivel_cmd_t nivel_parser_procesar(nivel_parser_t *p, char c)
{
    if (c == '\r' || c == '\n') {
        nivel_cmd_t cmd = NIVEL_CMD_NINGUNO;
        if (!p->desbordado && p->len > 0) {
            p->buf[p->len] = '\0';
            if (iguales_sin_mayusculas(p->buf, "t") || iguales_sin_mayusculas(p->buf, "tare")) {
                cmd = NIVEL_CMD_TARA;
            }
        }
        nivel_parser_init(p); /* lista para la siguiente línea */
        return cmd;
    }
    if (c == '+') {
        return NIVEL_CMD_FACTOR_MAS;
    }
    if (c == '-') {
        return NIVEL_CMD_FACTOR_MENOS;
    }
    if (p->len < NIVEL_CMD_MAX_LEN) {
        p->buf[p->len++] = c;
    } else {
        p->desbordado = true; /* la línea se descarta al llegar el Enter */
    }
    return NIVEL_CMD_NINGUNO;
}
