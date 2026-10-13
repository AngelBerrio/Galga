# Proyecto ESP-IDF: sensor de nivel con celda de carga y HX711.
# Los componentes propios (hx711, nivel) están en components/ y se detectan solos.
cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(nivel_tanque_hx711)
