# Firmware de teste — Missão #11

Firmware PlatformIO (Arduino) para o **ESP32-C3 DevKitM-1** que lê os sensores e
**transmite a telemetria via LoRa** (RFM95W, 915 MHz). Sem WiFi, sem serial para
o webui — somente transmissão LoRa.

## Hardware

| Componente | Função |
|---|---|
| ESP32-C3 DevKitM-1 | MCU (USB via CH340) |
| GY-GPS6MV2 (u-blox NEO-6M) | GPS, UART 9600 |
| BMP280 | pressão/temperatura, I2C |
| ICM-20602 | accel +16g / giro ±500°/s, I2C |
| RFM95W | rádio LoRa 915 MHz, SPI |
| Buzzer | GPIO1, ativo em nível alto (firmware mantém desligado) |

Pinout completo: [PINOUT.md](PINOUT.md)

## Payload (CSV, 1 Hz)

```
#11,millis,count,altp,temp,umi,p,gp,gr,gy,ap,ar,ay,hora,data,alt,lat,lon,sat,pqd,rssi
```

Mesmo formato do webui (`recovery-webui/app/app.py`). `umi` e `rssi` são `nan`
(sem sensor de umidade nesta missão; RSSI só existe no receptor). `pqd` = 0.

## Rádio

Parâmetros equivalentes ao padrão da lib LoRa.h do receptor:
915 MHz, SF7, BW 125 kHz, CR 4/5, sync word 0x12, CRC on, preamble 8, 17 dBm.

## Como usar

```bash
pio run -t upload && pio device monitor
```

O monitor serial (115200) mostra cada pacote antes de transmitir.
