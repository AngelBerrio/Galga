# App de pruebas unitarias (Unity). Compila los tests de components/nivel/test.
# Funciona en la tarjeta (target esp32) y en el PC (target linux).
cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS "../components/nivel")
set(TEST_COMPONENTS "nivel" CACHE STRING "Componentes cuyas pruebas se compilan")

# Solo lo necesario (main + el componente a probar): así la app también compila
# para el target linux. "nivel" debe estar en la lista para que TEST_COMPONENTS lo incluya.
set(COMPONENTS main nivel)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(nivel_test_app)
