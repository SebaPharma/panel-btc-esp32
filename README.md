# Panel BTC para ESP32

Pantalla de 1,3" que muestra el precio de Bitcoin cada 5 segundos, el dólar, el euro y la UF en pesos chilenos, y el clima de dos ciudades. La armé para el auto: se alimenta del USB y se conecta al hotspot del celular.

Es la Herramienta MIFI #1 de [Ruta MIFI](https://rutamifi.com).

## Qué muestra

| Dato | Fuente | Frecuencia |
|---|---|---|
| BTC/USDT | API pública de Binance | cada 5 s (el número parpadea al cambiar) |
| Dólar, euro y UF en CLP | [mindicador.cl](https://mindicador.cl) | cada 10 min |
| Clima de 2 ciudades | [wttr.in](https://wttr.in) | cada 15 min |

Además tiene GIF animados (por defecto, la moneda K del WiseKhameleon), caritas kaomoji que cambian de color, varias redes WiFi en orden de prioridad, Bluetooth para recibir el precio desde el celular ([docs/BLE-AUTO.md](docs/BLE-AUTO.md)) y actualización por WiFi (OTA) protegida con clave.

## Materiales

| Componente | Detalle |
|---|---|
| Placa | ESP32-S3 N16R8 DevKitC (16 MB flash, 8 MB PSRAM) |
| Pantalla | IPS 1,3" 240×240, controlador ST7789, módulo GMT130 de 7 pines SPI (sin pin CS) |
| Otros | Cables dupont hembra-hembra y un cable USB-C |

## Conexiones

| Pin de la pantalla | ESP32-S3 |
|---|---|
| GND | GND |
| VCC | 3V3 |
| SCL | GPIO 12 |
| SDA | GPIO 11 |
| RES | GPIO 13 |
| DC | GPIO 14 |
| BLK | 3V3 |

El código define CS en el GPIO 10, pero este módulo no tiene ese pin: déjalo sin conectar. Los pines se cambian en `platformio.ini`.

## Instalación

1. Instala [VS Code](https://code.visualstudio.com/) con la extensión **PlatformIO IDE**.
2. Descarga este repositorio y ábrelo en VS Code.
3. Copia `include/secrets.example.h` como `include/secrets.h` y completa tus redes WiFi, la clave OTA y tus ciudades. Ese archivo nunca se sube a GitHub.
4. Conecta la placa por USB y toca **Upload** (flecha en la barra inferior de PlatformIO).

### Tus propios GIF

Copia GIF de 64×64 px en la carpeta `data/` y vuelve a compilar: se convierten solos y rotan cada 10 segundos. Solo `data/k_coin.gif` se publica en el repositorio; los demás quedan en tu PC. Usa GIF propios o con licencia que permita su uso.

## Notas

- Las tres APIs son gratuitas y no piden clave. Si haces un uso comercial, revisa sus condiciones.
- La API de Binance no responde en algunos países. En ese caso, cambia la URL en `fetchBTC()`.
- No manipules la pantalla mientras manejas.

## Licencia

[MIT](LICENSE)
