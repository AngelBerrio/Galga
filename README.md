# Sensor de nivel con galga extensiométrica y HX711 (ESP-IDF)

Este proyecto mide el nivel de agua en un tanque cilíndrico usando una celda de carga en la base. En lugar de usar un sensor ultrasónico, pesamos el tanque con el amplificador HX711 y, conociendo la densidad del líquido y el área del recipiente, calculamos el volumen y el nivel de agua. El código permite además calibrar la báscula y hacer la tara desde el monitor serie.

**Autor:** Alex Ramirez — Angel Tabla

## Hardware

- Placa: ESP32 Dev Module (30 pines)
- Amplificador/ADC: HX711
- Celda de carga: 5 kg, tipo barra de aluminio
- Tanque: cilíndrico de plástico, diámetro aprox. 14.5 cm (área ~164 cm²)

| HX711 | ESP32 |
|---|---|
| VCC | 3V3 |
| GND | GND |
| DT (DOUT) | GPIO4 |
| SCK | GPIO14 |

| Celda de carga | HX711 |
|---|---|
| Rojo | E+ |
| Negro | E− |
| Verde | A+ |
| Blanco | A− |

## Requisitos

- ESP-IDF v5.3.1
- Target: esp32

## Cómo compilar y flashear

Abre una terminal con el entorno de ESP-IDF configurado, ve a la raíz del proyecto y ejecuta:

```bash
idf.py set-target esp32
idf.py build
idf.py -p COM3 flash monitor
```

Cambia `COM3` por el puerto de tu placa.

## Cómo ejecutar las pruebas

Las pruebas unitarias validan la lógica de cálculo y el intérprete de comandos con el framework Unity de ESP-IDF.

En la tarjeta ESP32:

```bash
cd test_app
idf.py set-target esp32
idf.py -p COM3 flash monitor
```

En el PC (Linux o WSL, requiere el paquete `libbsd-dev`):

```bash
cd test_app
idf.py --preview set-target linux
idf.py build
./build/nivel_test_app.elf
```

Resultado esperado: `20 Tests 0 Failures 0 Ignored`.

## Estructura del proyecto

```text
.
├── CMakeLists.txt            # Proyecto principal ESP-IDF
├── sdkconfig                 # Configuración generada por menuconfig
├── sdkconfig.defaults        # Valores por defecto (target esp32)
├── main/
│   ├── main.c                # Tareas de FreeRTOS (consola y medición)
│   ├── Kconfig.projbuild     # Menú de configuración "Sensor de nivel HX711"
│   └── CMakeLists.txt
├── components/
│   ├── hx711/                # Driver del HX711 sobre GPIO
│   └── nivel/                # Lógica de cálculo, conversiones e intérprete
│       └── test/             # Pruebas unitarias en Unity
├── test_app/                 # Aplicación independiente que ejecuta las pruebas
├── LICENSE
└── README.md
```

## Créditos

- Sketch base: usuario *eggybacon*, foro de Arduino, hilo [HX711 Calibration](https://forum.arduino.cc/t/hx711-calibration/1137872) (2023), derivado del ejemplo [SparkFun_HX711_Calibration](https://github.com/sparkfun/HX711-Load-Cell-Amplifier) de Nathan Seidle (2014, licencia Beerware).
- Lo adaptamos en Arduino para calcular volumen y nivel y hacer tara por consola, y luego lo reescribimos en C para ESP-IDF con driver propio, tareas de FreeRTOS y pruebas Unity. No usa la librería `bogde/HX711`.

## Licencia

MIT. Ver el archivo [LICENSE](LICENSE).