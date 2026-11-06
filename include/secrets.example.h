#pragma once

// 1. Copia este archivo como include/secrets.h
// 2. Completa tus datos. secrets.h está en .gitignore: nunca se sube a GitHub.

// Redes WiFi: el ESP32 prueba en orden y usa la primera disponible.
// Pon el hotspot de tu celular como segunda red para usarlo en el auto.
struct WiFiNetwork {
  const char* ssid;
  const char* password;
};

static const WiFiNetwork WIFI_NETWORKS[] = {
    {"NOMBRE_WIFI_CASA", "CLAVE_WIFI_CASA"},
    {"NOMBRE_HOTSPOT_CELULAR", "CLAVE_HOTSPOT"},
};
static const size_t WIFI_NETWORK_COUNT =
    sizeof(WIFI_NETWORKS) / sizeof(WIFI_NETWORKS[0]);

// Clave para actualizar el ESP32 por WiFi (OTA). Usa una clave larga.
// Si borras esta línea, la actualización por WiFi queda desactivada.
#define OTA_PASSWORD "cambia-esta-clave"

// Ciudades del clima. LABEL es lo que se ve en pantalla (máx. ~9 letras);
// QUERY es el nombre para wttr.in, con + en vez de espacios.
#define CITY_1_LABEL "Santiago"
#define CITY_1_QUERY "Santiago"
#define CITY_2_LABEL "Valpo"
#define CITY_2_QUERY "Valparaiso"

// 1 = muestra el nombre de la red WiFi abajo de la pantalla; 0 = solo "WiFi OK".
// Déjalo en 0 si vas a grabar videos de la pantalla.
#define SHOW_WIFI_NAME 0
