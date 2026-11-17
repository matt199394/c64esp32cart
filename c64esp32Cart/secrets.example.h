#pragma once

// Copiare questo file in secrets.h e inserire i propri dati.
// secrets.h NON va committato (e' in .gitignore).

// --- WiFi (opzionale: senza WiFi il menu funziona, mancano solo NEWS/meteo)
const char *ssid = "YOUR_WIFI_SSID";
const char *password = "YOUR_WIFI_PASSWORD";

// --- OpenWeather: https://openweathermap.org/api (piano Current Weather)
String openweatherApiKey = "YOUR_OPENWEATHER_API_KEY";
// Coordinate della localita' in gradi decimali (es. Roma: 41.9028, 12.4964)
String openweatherLat = "xxxxx";
String openweatherLon = "xxxxx";

// --- NewsAPI: https://newsapi.org (serve solo per la fonte "NewsAPI")
String newsApiKey = "YOUR_NEWSAPI_KEY";
