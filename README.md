Inspired on  chat64 cart by Bart Venneker and Theo van den Beld (https://github.com/bvenneker/Chat64), same hardware with some midifications:
- SD card
- 27C512 EEPROM
- add jumpers to select 8k memory inside EEPROM
- add transistor to control EXROM line
- standart or ultiMax mode jumper
  
 About the code:
  The arduino skeck and ACME assembly code are a working progress, there are many bugs to fix, expecialy SD directory code.
  Not all SIDs file are playable, same as PRG file.
  
Feaures:
- Run PRG file
- play SID files and show SID info
- show koala images
- show current time
- show current weather
- show news by using 2x2 charset

  Video to show the cart in action: https://www.youtube.com/watch?v=Q0KYe2STK2w

## EPROM 8K bootstrap

L'EPROM puo' contenere solo un piccolo bootstrap invece di tutto il menu:

```bash
acme bootstrap.asm
# -> c64esp8kBOOT.bin (8192 byte)
```

Il bootstrap copia un loader a `$C000`, invia il comando ESP32 `112` per alzare
EXROM (GPIO2/CLED tramite transistor), carica `espnewsauto.prg` dalla root della
SD dell'ESP32 a `$0801` e salta all'entry point. Il menu completo puo' quindi
essere aggiornato sulla SD senza riprogrammare la 27C512.
  
Any help is appreciate,
Have fun!

Matteo

![sideUp1](https://github.com/matt199394/c64esp32cart/assets/65487240/38e022f3-9dd2-4feb-adc3-4f767da10d91)

![sideUp2](https://github.com/matt199394/c64esp32cart/assets/65487240/eeed5ad3-ee54-4731-9f9f-a0043d92e576)

![sideBottom](https://github.com/matt199394/c64esp32cart/assets/65487240/b464b87a-7bbd-49e4-91c8-636ed578510b)

## Credenziali WiFi e API

Il file `c64esp32Cart/secrets.h` non viene versionato.

Per compilare:

```bash
cp c64esp32Cart/secrets.example.h c64esp32Cart/secrets.h
```

Poi inserire in `secrets.h` SSID, password WiFi, chiave OpenWeather e chiave NewsAPI.
