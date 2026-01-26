#line 1 "C:\\Users\\Ale\\Documents\\Arduino\\domo\\README.md"
# Multi-Sensor Logger for ESP32

Este proyecto consiste en un sistema de monitoreo y registro de datos (Data Logger) basado en un **ESP32**. El sistema integra múltiples sensores industriales y de consumo para medir condiciones ambientales, visualizarlas en tiempo real y almacenarlas en una tarjeta MicroSD.

## Características Principales

- **Sensor SHT40**: Lectura de temperatura y humedad ambiental de alta precisión vía I2C.
- **Sensor HMP60 (vía ADS1115)**: Integración de un sensor de humedad y temperatura industrial HMP60. Las señales analógicas (0-1V) son convertidas a digital mediante un ADC ADS1115 de 16 bits.
- **Reloj de Tiempo Real (RTC DS3231)**: Mantiene la fecha y hora exacta para el estampado de tiempo (timestamping) de los datos, incluso tras cortes de energía.
- **Pantalla OLED SSD1306 (128x64)**: Visualización local de los datos en tiempo real y estado del sistema.
- **Registro en MicroSD**: Almacenamiento de datos en formato CSV. El sistema genera un archivo nuevo cada día (`YYYYMMDD.csv`) y registra las lecturas cada minuto.
- **Lógica No Bloqueante**: El sistema intenta inicializar todos los componentes, pero si alguno falla o no está conectado, continúa operando con los sensores disponibles.

## Conexiones (Pinout)

### I2C (OLED, SHT40, ADS1115, RTC)
- **SDA**: Pin por defecto (GPIO 21)
- **SCL**: Pin por defecto (GPIO 22)

### SPI (MicroSD Card - HSPI)
- **MISO**: GPIO 2
- **MOSI**: GPIO 15
- **CLK**: GPIO 14
- **CS**: GPIO 13

## Requisitos de Software
- Arduino IDE o VS Code + PlatformIO.
- Librerías necesarias:
    - `Wire.h`
    - `SPI.h`
    - `SD.h`
    - `RTClib.h` (Adafruit)
    - `U8g2lib.h` (oliver)
    - `SHT4x.h` (Rob Tillaart)
    - `Adafruit_ADS1X15.h` (Adafruit)

## Autor
**Alejandro Rebolledo**
- Email: arebolledo@udd.cl

## Licencia
Este proyecto está bajo la licencia **Creative Commons Attribution-NonCommercial 4.0 International (CC BY-NC 4.0)**. Puedes ver más detalles en el archivo [LICENSE](LICENSE).

## Descargo de Responsabilidad (Disclaimer)
El código se ofrece "tal cual", sin garantías de ningún tipo, ya sean expresas o implícitas. El autor no se hace responsable de cualquier daño, pérdida de datos o mal funcionamiento derivado del uso de este software. El usuario utiliza este código bajo su propio riesgo.
