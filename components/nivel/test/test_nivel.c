/*
 * Pruebas unitarias del componente "nivel" con el framework Unity de ESP-IDF.
 *
 * SPDX-License-Identifier: MIT
 */
#include <math.h>
#include <string.h>

#include "nivel.h"
#include "unity.h"

#define TOL 0.001f

/* Envía una cadena completa al intérprete y devuelve el último comando no nulo. */
static nivel_cmd_t enviar(nivel_parser_t *p, const char *s)
{
    nivel_cmd_t ultimo = NIVEL_CMD_NINGUNO;
    for (; *s != '\0'; s++) {
        const nivel_cmd_t c = nivel_parser_procesar(p, *s);
        if (c != NIVEL_CMD_NINGUNO) {
            ultimo = c;
        }
    }
    return ultimo;
}

/* ---------- Conversión lectura → peso ---------- */

TEST_CASE("peso: lectura igual a la tara da 0 kg", "[nivel]")
{
    TEST_ASSERT_FLOAT_WITHIN(TOL, 0.0f, nivel_peso_kg(84213, 84213, 395000.0f));
}

TEST_CASE("peso: un factor completo por encima de la tara da 1 kg", "[nivel]")
{
    const int32_t offset = 84213;
    TEST_ASSERT_FLOAT_WITHIN(TOL, 1.0f, nivel_peso_kg(offset + 395000, offset, 395000.0f));
}

TEST_CASE("peso: lectura por debajo de la tara da peso negativo", "[nivel]")
{
    TEST_ASSERT_FLOAT_WITHIN(TOL, -0.5f, nivel_peso_kg(-197500, 0, 395000.0f));
}

TEST_CASE("peso: factor 0 devuelve NAN en vez de dividir por cero", "[nivel]")
{
    TEST_ASSERT_TRUE(isnan(nivel_peso_kg(1000, 0, 0.0f)));
}

TEST_CASE("peso: no desborda con valores extremos de 24 bits", "[nivel]")
{
    /* Rango del HX711: -8388608 .. 8388607 */
    const float p = nivel_peso_kg(8388607, -8388608, 1.0f);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 16777215.0f, p);
}

/* ---------- Volumen y altura ---------- */

TEST_CASE("volumen: 1 kg de agua son 1 L", "[nivel]")
{
    TEST_ASSERT_FLOAT_WITHIN(TOL, 1.0f, nivel_volumen_l(1.0f, 1.0f));
}

TEST_CASE("volumen: 1 kg de un liquido de 0.8 kg/L son 1.25 L", "[nivel]")
{
    TEST_ASSERT_FLOAT_WITHIN(TOL, 1.25f, nivel_volumen_l(1.0f, 0.8f));
}

TEST_CASE("volumen: densidad no positiva devuelve NAN", "[nivel]")
{
    TEST_ASSERT_TRUE(isnan(nivel_volumen_l(1.0f, 0.0f)));
    TEST_ASSERT_TRUE(isnan(nivel_volumen_l(1.0f, -1.0f)));
}

TEST_CASE("altura: 1 L en una base de 164.26 cm2 son 6.088 cm", "[nivel]")
{
    TEST_ASSERT_FLOAT_WITHIN(TOL, 1000.0f / 164.26f, nivel_altura_cm(1.0f, 164.26f));
}

TEST_CASE("altura: area no positiva devuelve NAN", "[nivel]")
{
    TEST_ASSERT_TRUE(isnan(nivel_altura_cm(1.0f, 0.0f)));
}

TEST_CASE("area: circulo de 14 cm de diametro mide 153.94 cm2", "[nivel]")
{
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 153.94f, nivel_area_circulo_cm2(14.0f));
    TEST_ASSERT_TRUE(isnan(nivel_area_circulo_cm2(0.0f)));
}

TEST_CASE("calculo completo: 2 kg de agua en el tanque", "[nivel]")
{
    nivel_medida_t m;
    const int32_t offset = 50000;
    TEST_ASSERT_TRUE(nivel_calcular(offset + 2 * 395000, offset, 395000.0f, 1.0f, 164.26f, &m));
    TEST_ASSERT_FLOAT_WITHIN(TOL, 2.0f, m.peso_kg);
    TEST_ASSERT_FLOAT_WITHIN(TOL, 2.0f, m.volumen_l);
    TEST_ASSERT_FLOAT_WITHIN(TOL, 2000.0f / 164.26f, m.nivel_cm);
}

TEST_CASE("calculo completo: parametros invalidos no modifican la salida", "[nivel]")
{
    nivel_medida_t m = {.peso_kg = 7.0f, .volumen_l = 7.0f, .nivel_cm = 7.0f};
    TEST_ASSERT_FALSE(nivel_calcular(1000, 0, 0.0f, 1.0f, 164.26f, &m));
    TEST_ASSERT_FALSE(nivel_calcular(1000, 0, 395000.0f, 1.0f, 0.0f, &m));
    TEST_ASSERT_FALSE(nivel_calcular(1000, 0, 395000.0f, 1.0f, 164.26f, NULL));
    TEST_ASSERT_EQUAL_FLOAT(7.0f, m.peso_kg);
}

/* ---------- Ajuste del factor ---------- */

TEST_CASE("factor: '+' y '-' cambian el factor en pasos de 500", "[nivel]")
{
    TEST_ASSERT_EQUAL_FLOAT(395500.0f, nivel_ajustar_factor(395000.0f, +1));
    TEST_ASSERT_EQUAL_FLOAT(394500.0f, nivel_ajustar_factor(395000.0f, -1));
    TEST_ASSERT_EQUAL_FLOAT(396500.0f, nivel_ajustar_factor(395000.0f, +3));
}

TEST_CASE("factor: nunca baja del minimo (evita division por cero)", "[nivel]")
{
    TEST_ASSERT_EQUAL_FLOAT(NIVEL_FACTOR_MIN, nivel_ajustar_factor(500.0f, -1));
    TEST_ASSERT_EQUAL_FLOAT(NIVEL_FACTOR_MIN, nivel_ajustar_factor(1000.0f, -10));
}

/* ---------- Intérprete de comandos ---------- */

TEST_CASE("comandos: '+' y '-' actuan sin esperar Enter", "[nivel][consola]")
{
    nivel_parser_t p;
    nivel_parser_init(&p);
    TEST_ASSERT_EQUAL(NIVEL_CMD_FACTOR_MAS, nivel_parser_procesar(&p, '+'));
    TEST_ASSERT_EQUAL(NIVEL_CMD_FACTOR_MENOS, nivel_parser_procesar(&p, '-'));
}

TEST_CASE("comandos: 't', 'tare' y 'TARE' + Enter piden tara", "[nivel][consola]")
{
    nivel_parser_t p;
    nivel_parser_init(&p);
    TEST_ASSERT_EQUAL(NIVEL_CMD_TARA, enviar(&p, "t\n"));
    TEST_ASSERT_EQUAL(NIVEL_CMD_TARA, enviar(&p, "tare\r"));
    TEST_ASSERT_EQUAL(NIVEL_CMD_TARA, enviar(&p, "TARE\n"));
    TEST_ASSERT_EQUAL(NIVEL_CMD_TARA, enviar(&p, "T\r\n"));
}

TEST_CASE("comandos: '\\r\\n' produce una sola tara", "[nivel][consola]")
{
    nivel_parser_t p;
    nivel_parser_init(&p);
    int taras = 0;
    for (const char *s = "t\r\n"; *s; s++) {
        if (nivel_parser_procesar(&p, *s) == NIVEL_CMD_TARA) {
            taras++;
        }
    }
    TEST_ASSERT_EQUAL(1, taras);
}

TEST_CASE("comandos: palabras desconocidas se ignoran", "[nivel][consola]")
{
    nivel_parser_t p;
    nivel_parser_init(&p);
    TEST_ASSERT_EQUAL(NIVEL_CMD_NINGUNO, enviar(&p, "tara\n"));
    TEST_ASSERT_EQUAL(NIVEL_CMD_NINGUNO, enviar(&p, "hola\n"));
    TEST_ASSERT_EQUAL(NIVEL_CMD_NINGUNO, enviar(&p, "\n"));
}

TEST_CASE("comandos: una linea demasiado larga se descarta sin desbordar el buffer", "[nivel][consola]")
{
    nivel_parser_t p;
    nivel_parser_init(&p);
    char larga[64];
    memset(larga, 'x', sizeof(larga) - 1);
    larga[sizeof(larga) - 1] = '\0';
    TEST_ASSERT_EQUAL(NIVEL_CMD_NINGUNO, enviar(&p, larga));
    TEST_ASSERT_EQUAL(NIVEL_CMD_NINGUNO, enviar(&p, "tare\n")); /* sigue siendo la misma línea */
    TEST_ASSERT_EQUAL(NIVEL_CMD_TARA, enviar(&p, "tare\n"));    /* la siguiente ya funciona */
}
