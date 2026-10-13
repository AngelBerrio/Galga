/*
 * Ejecutor de las pruebas Unity.
 * En el PC (target linux) termina con código de salida = número de fallos,
 * para que se pueda usar en scripts o en integración continua.
 *
 * SPDX-License-Identifier: MIT
 */
#include <stdlib.h>

#include "sdkconfig.h"
#include "unity.h"

void app_main(void)
{
    UNITY_BEGIN();
    unity_run_all_tests();
    const int fallos = UNITY_END();

#if CONFIG_IDF_TARGET_LINUX
    exit(fallos);
#else
    (void)fallos;
#endif
}
