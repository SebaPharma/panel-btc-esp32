# Precio Bitcoin por Bluetooth (modo auto)

El ESP32 aparece como **`BTC-Auto`**. El celular consulta Binance y **escribe** el precio en BLE.

## UUIDs (para nRF Connect, MacroDroid, etc.)

| Rol | UUID |
|-----|------|
| Servicio | `4fafc201-1fb5-459e-8fcc-c5c9c331914b` |
| Precio (escritura) | `beb5483e-36e1-4688-b7f5-ea07361b26a8` |

## Formato del dato

Texto UTF-8, cualquiera de estos:

- `64271.94`
- `$64271.94`
- `{"price":"64271.94"}`

## Prueba manual (nRF Connect)

1. Escanea y conecta a **BTC-Auto**.
2. Abre el servicio de la tabla.
3. En la característica de precio, **Write** el valor (texto).
4. La pantalla debe actualizarse al instante.

## Automatizar en Android (idea)

Cada 1–5 minutos (o al conectar Bluetooth del auto):

1. HTTP GET: `https://api.binance.com/api/v3/ticker/price?symbol=BTCUSDT`
2. Extraer `price` del JSON.
3. BLE write a la característica de precio (MacroDroid, Tasker + plugin BLE, o app propia).

En iOS, Shortcuts puede hacer el HTTP pero **enviar BLE en segundo plano** es más limitado; suele hacer falta app dedicada o usar el hotspot + WiFi del ESP como respaldo.

## WiFi

Sigue intentando redes de `secrets.h` cada ~2 min si no hay WiFi (casa/hotspot). En el auto, lo normal es que el precio llegue solo por **BLE** desde el celular.
