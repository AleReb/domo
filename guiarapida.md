# Domo · Guía rápida

Domo es un registrador ambiental basado en ESP32. Al encenderse, mide temperatura y humedad con cuatro sensores, mide presión con el BME280 y muestra las lecturas en una pantalla OLED. El reloj DS3231 pone fecha y hora a cada fila que se guarda en la MicroSD.

## Qué mide y dónde verlo

| Sensor | Medición | Dónde verla |
| --- | --- | --- |
| SHT40 y DHT22 | Temperatura (°C) y humedad relativa (%) | Primera vista de la OLED y CSV |
| HMP60 | Temperatura (°C) y humedad relativa (%) mediante el ADS1115 | Segunda vista de la OLED y CSV |
| BME280 | Temperatura (°C), humedad relativa (%) y presión (hPa) | Segunda vista de la OLED y CSV. La presión se consulta en el CSV. |

El equipo lee los sensores cada **2 s**. La OLED se actualiza cada **0,5 s**, cambia de vista cada **20 s** y muestra la hora del reloj y el tiempo transcurrido desde el encendido.

## Cómo iniciar el registro

1. **Enciende el Domo con la MicroSD insertada.** Abre el monitor serie a **115200 baudios**. Selecciona un final de línea (CR, LF o CRLF) y envía `h` o `help` para consultar el estado de los componentes.
2. **Comprueba la hora.** Cuando aparezca `[OK] RTC Found`, revisa la hora con `h`. Para ajustarla, envía la fecha y hora actuales en formato `DD/MM/AAAA HH:MM:SS`, por ejemplo `25/11/2026 14:35:00`. El reloj conserva la hora introducida sin convertir la zona horaria.
3. **Confirma que se guarda el archivo.** El registro empieza automáticamente cuando el reloj y la tarjeta están inicializados. Busca en el monitor serie un mensaje como `[LOG] Data written: /20261125.csv`.

## Cómo leer el archivo

La MicroSD guarda un CSV por día en su carpeta raíz, con el nombre `YYYYMMDD.csv`. Cada fila contiene la fecha y hora (`Timestamp`), el tiempo encendido (`Timeon`) y las lecturas. El equipo intenta guardar cada **1 s**, así que dos filas consecutivas pueden repetir una medición. Un valor `nan` indica un dato no disponible.

> **Nota de calibración.** Para el HMP60, el firmware convierte la señal de A0 a temperatura con `°C = mV × 0,1 − 40` y la de A1 a humedad con `% HR = mV × 0,1`, limitada a 0–100 %. El CSV contiene estas lecturas antes de cualquier ajuste experimental. Los campos `RefBME_Temp` y `RefBME_RefHum` pertenecen al BME280 del propio Domo.

Fuente del funcionamiento descrito: [domo.ino](domo.ino).
