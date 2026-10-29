# Domo — registrador ambiental para ESP32

Domo mide temperatura, humedad y presión, muestra lecturas en una pantalla OLED y guarda archivos CSV diarios en una tarjeta MicroSD. Este documento describe el comportamiento de [domo.ino](domo.ino).

## Componentes

| Componente | Función y configuración del firmware |
| --- | --- |
| SHT40 | Temperatura y humedad por I2C, dirección `0x44`. |
| HMP60 + ADS1115 | Temperatura en A0 y humedad en A1 del ADS1115 (`0x48`), con salidas analógicas de 0–1 V. Ganancia `GAIN_ONE` (rango ±4,096 V). |
| BME280 | Temperatura, humedad y presión por I2C, dirección `0x76`. |
| DHT22 | Temperatura y humedad por GPIO 19. |
| DS3231 | Fecha y hora para la pantalla y el registro. |
| OLED SH1106 de 128 × 64 | Dirección `0x3C`; controlador activo `U8G2_SH1106_128X64_NONAME_F_HW_I2C`. Hay una alternativa SSD1306 comentada en el código. |
| MicroSD | Almacenamiento por SPI, bus HSPI. |

Para el HMP60, el código convierte `temperatura (°C) = mV × 0,1 − 40` y `humedad (% HR) = mV × 0,1`, con humedad limitada a 0–100 %. Comprueba que estas fórmulas correspondan a la configuración de salida del sensor utilizado.

## Conexiones

`Wire.begin()` usa los pines I2C predeterminados de la placa seleccionada. Para el ESP32 Dev Module configurado en el repositorio:

| Interfaz | Señal | Pin ESP32 |
| --- | --- | --- |
| I2C compartido | SDA | GPIO 21 |
| I2C compartido | SCL | GPIO 22 |
| MicroSD / HSPI | MISO | GPIO 2 |
| MicroSD / HSPI | MOSI | GPIO 15 |
| MicroSD / HSPI | CLK | GPIO 14 |
| MicroSD / HSPI | CS | GPIO 13 |
| DHT22 | DATA | GPIO 19 |

Conecta la salida de temperatura del HMP60 a A0 del ADS1115 y la de humedad a A1, con tierra común. Comprueba alimentación, niveles eléctricos y conexiones antes de energizar el montaje. Esta tabla describe el firmware; revisa también el esquema electrónico.

## Compilación y puesta en marcha

1. Abre `domo.ino` desde una carpeta llamada `domo` en Arduino IDE.
2. Instala el paquete de placas **esp32 de Espressif Systems** y selecciona **ESP32 Dev Module** (`esp32:esp32:esp32`). Los artefactos incluidos hacen referencia al core **2.0.17**; las versiones de las bibliotecas no están fijadas en un manifiesto.
3. Instala las bibliotecas indicadas abajo y sus dependencias.
4. Selecciona el puerto de la placa, compila y carga el sketch. La configuración de VS Code está en [`.vscode/arduino.json`](.vscode/arduino.json); su puerto `COM1` debe ajustarse al equipo utilizado.
5. Abre el monitor serie a **115200 baudios** para revisar mensajes `[INIT]`, `[OK]`, `[ERR]`, `[ADS RAW]` y `[LOG]`. Envía `h` o `help` con un final de línea para ver instrucciones y el estado del equipo.
6. Espera el mensaje `[OK] RTC Found` y ajusta el DS3231 por serial como se indica abajo. El sketch no comprueba automáticamente `rtc.lostPower()`.
7. Inserta una MicroSD compatible con la biblioteca `SD` y verifica que aparezca `[LOG] Data written`.

| Biblioteca | Autor / proveedor | Cabeceras |
| --- | --- | --- |
| Adafruit ADS1X15 | Adafruit | `Adafruit_ADS1X15.h` |
| Adafruit BME280 Library | Adafruit | `Adafruit_BME280.h` |
| Adafruit Unified Sensor | Adafruit | `Adafruit_Sensor.h` |
| DHT sensor library | Adafruit | `DHT.h`, `DHT_U.h` |
| RTClib | Adafruit | `RTClib.h` |
| SHT4x | Rob Tillaart | `SHT4x.h` |
| U8g2 | olikraus / Oliver Kraus | `U8g2lib.h` |
| Adafruit BusIO | Adafruit | Dependencia de bibliotecas Adafruit |

`Arduino.h`, `Wire.h`, `SPI.h` y `SD.h` forman parte del core ESP32. El repositorio no incluye un archivo `platformio.ini`.

## Comandos por serial

Abre el monitor serie a **115200 baudios** y selecciona un final de línea **Nueva línea (LF)**, **Retorno de carro (CR)** o **Ambos (CRLF)**. Los comandos se ejecutan al recibir el final de línea.

| Comando | Función |
| --- | --- |
| `h` | Muestra la ayuda y el estado de inicialización de los componentes. |
| `help` | Equivalente a `h`. |
| `DD/MM/AAAA HH:MM:SS` | Ajusta la fecha y hora del RTC DS3231. |

### Ayuda y componentes del dispositivo

Escribe `h` o `help` en minúsculas y pulsa **Enviar**. La respuesta explica el ajuste de fecha y hora, muestra un ejemplo y enumera los componentes configurados: OLED SH1106, SHT40, ADS1115/HMP60, BME280, DS3231, DHT22 y MicroSD, con sus direcciones I2C o conexiones. También muestra la fecha y hora actual del RTC si está inicializado.

Los estados corresponden a las banderas de inicialización del firmware; no constituyen una comprobación continua de conexión física. La inicialización del ADS1115 no confirma la conexión del HMP60, y la del DHT22 no garantiza una lectura válida. Un componente pendiente puede estar ausente, haber fallado o estar esperando el próximo intento de inicialización.

### Ajuste de fecha y hora

Una vez que aparezca `[OK] RTC Found`, envía una línea con el formato exacto `DD/MM/AAAA HH:MM:SS`, usando hora de **24 horas**. Por ejemplo:

```text
29/10/2026 14:35:00
```

El firmware valida los días de cada mes, los años bisiestos y los rangos de hora (00–23), minutos y segundos (00–59). Se admiten años de **2000 a 2099**. Una línea inválida o demasiado larga, o un RTC sin inicializar, produce un mensaje `[ERR]` y no modifica la hora.

Al aceptar el comando, llama a `rtc.adjust()` y muestra `[OK] Fecha/hora enviada al RTC: 29/10/2026 14:35:00`. El próximo registro utiliza la fecha ajustada para seleccionar el archivo diario. No se convierte la zona horaria: introduce la hora que quieras mantener en el RTC. El formato del CSV permanece igual.

## Operación y formato CSV

Las tareas se programan con `millis()` y estos intervalos:

| Constante | Valor | Función |
| --- | --- | --- |
| `READ_INTERVAL_MS` | 2000 ms | Lectura de sensores. |
| `LOG_INTERVAL_MS` | 1000 ms | Escritura si SD y RTC están inicializados. |
| `DISPLAY_INTERVAL_MS` | 500 ms | Actualización de la OLED y salida de estado por serie cuando la OLED está inicializada. |
| `OLED_ROTATION_MS` | 20000 ms | Alternancia de pantallas: SHT40/DHT22 y HMP60/BME280. |
| `INIT_RETRY_MS` | 3000 ms | Intervalo mínimo entre intentos de inicialización. |

La escritura es más frecuente que la lectura: filas consecutivas pueden repetir una medición. El acceso a periféricos y los `delay()` pueden añadir demoras; los intervalos no garantizan tiempo real.

El archivo diario se crea en la raíz de la tarjeta como `YYYYMMDD.csv`, según la fecha del RTC. Se escribe una cabecera al crear un archivo nuevo y las filas se anexan. La cabecera actual es:

```csv
Timestamp,Timeon,SHT_Temp,SHT_Hum,HMP_Temp,HMP_Hum,RefBME_Temp,RefBME_RefHum,BME_Press,DHT_Temp,DHT_Hum
```

| Campo | Contenido |
| --- | --- |
| `Timestamp` | Fecha y hora del RTC: `YYYY-MM-DD HH:MM:SS`, sin zona horaria. |
| `Timeon` | Tiempo encendido: `on: HH:MM:SS`, calculado al actualizar la OLED. |
| `SHT_Temp`, `HMP_Temp`, `RefBME_Temp`, `DHT_Temp` | Temperatura en °C, con un decimal. |
| `SHT_Hum`, `HMP_Hum`, `RefBME_RefHum`, `DHT_Hum` | Humedad relativa en %, con un decimal. |
| `BME_Press` | Presión en hPa, con un decimal. |

`RefBME_RefHum` es el nombre generado actualmente por el código. Los datos no disponibles se representan como `nan`; una lectura fallida del SHT40 puede conservar el último valor válido.

## Limitaciones actuales

- La adquisición y la visualización pueden continuar con componentes ausentes, pero **sin RTC o sin SD no se guardan filas CSV**.
- El primer intento de inicialización está sujeto al intervalo de 3 segundos. La condición de reintento del bucle incluye OLED, SHT40, ADS1115, RTC y SD; no incluye BME280 ni DHT22. Estos últimos solo vuelven a intentarse mientras alguno de los componentes contemplados siga pendiente.
- La detección del DHT22 usa metadatos de la biblioteca y no confirma que el sensor esté conectado o responda.
- `Timeon` puede quedar vacío sin OLED: `upStr` se calcula en `updateOLED()`. El estado `SD LOG: OK` refleja la inicialización de la tarjeta, no confirma cada escritura ni la disponibilidad del RTC.
- No se implementan sincronización de hora por red ni recuperación de una SD desconectada después de inicializarse.

## Estructura

- [`domo.ino`](domo.ino): fuente del firmware.
- [`dataloggerDomo/`](dataloggerDomo/): esquema `domoLogger.sch`, placa `domoLogger.brd`, archivos de proyecto y respaldos EAGLE. Los archivos principales declaran EAGLE 9.6.2. Revisa su correspondencia con el firmware actual antes de fabricar.
- [`.vscode/`](.vscode/): configuración local de Arduino y del editor.
- [`build/`](build/): artefactos de una compilación anterior, con rutas locales y componentes de terceros. Compila desde `domo.ino` para obtener firmware actualizado.
- [`LICENSE`](LICENSE) y [`LICENSES/`](LICENSES/): alcance de las licencias y textos completos.

## Licencias

| Componente | Licencia | Alcance |
| --- | --- | --- |
| Hardware | **CERN-OHL-S-2.0** | Diseños propios y documentación de hardware en `dataloggerDomo/`. |
| Software | **GPL-3.0-only** | Software propio y documentación general del proyecto. |
| Firmware | **GPL-3.0-only** | `domo.ino` y código de firmware propio derivado. |

Consulta [LICENSE](LICENSE) para el alcance por componente y los textos completos de [CERN-OHL-S-2.0](LICENSES/CERN-OHL-S-2.0.txt) y [GPL-3.0-only](LICENSES/GPL-3.0-only.txt). GPL se aplica **solo en su versión 3**, sin la opción «o cualquier versión posterior».

Los componentes de terceros conservan sus licencias originales. La biblioteca EAGLE `diy-modules`, incorporada en los diseños, incluye un aviso **CC BY 4.0** de Miroslaw Brudnowski que se conserva. Las bibliotecas, el core ESP32 y sus artefactos compilados conservan sus respectivos términos.

Fuentes de los textos: [Open Hardware Repository / CERN](https://ohwr.org/licences/) y [GNU / Free Software Foundation](https://www.gnu.org/licenses/gpl-3.0.html).

El hardware, el software y el firmware se proporcionan sin garantías, conforme a las licencias respectivas.

## Autor

**Alejandro Rebolledo** — arebolledo@udd.cl
