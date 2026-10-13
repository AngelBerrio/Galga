# Driver del HX711. "driver" aporta driver/gpio.h y es compatible con todas las versiones 5.x de ESP-IDF.
idf_component_register(SRCS "hx711.c"
                       INCLUDE_DIRS "include"
                       REQUIRES driver)
