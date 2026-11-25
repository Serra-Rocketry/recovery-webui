# Pinout — Missão #11 (ESP32-C3 DevKitM-1)

> Conecte o USB do devkit na porta **COM/UART** (a do chip CH340), não na porta USB nativa.

## GPS GY-GPS6MV2 (NEO-6M, UART 9600, somente leitura)

| GY-GPS6MV2 | ESP32-C3 |
|---|---|
| TX  | GPIO0 (RX1) |
| RX  | **não ligar** (o NEO-6M transmite sozinho; GPIO1 é do buzzer) |
| VCC | 5V (ou 3V3 — o módulo aceita ambos) |
| GND | GND |

## Buzzer (ativo em nível ALTO, no GPIO1)

O firmware mantém o **GPIO1 em LOW** (buzzer desligado). Se quiser usá-lo como
beeper de localização depois, é só pedir.

## BMP280 + ICM-20602 (I2C, mesmo barramento)

| Sensor | ESP32-C3 |
|---|---|
| SDA | GPIO3 |
| SCL | GPIO18 |
| VCC | 3V3 |
| GND | GND |

O firmware procura o ICM-20602 nos endereços 0x69 (AD0=0) e 0x68 (AD0=1) e o
BMP280 em 0x76 e 0x77 — a ordem dos fios de endereço não importa.

## RFM95W (LoRa 915 MHz, SPI)

| RFM95W | ESP32-C3 |
|---|---|
| MOSI | GPIO6 |
| MISO | GPIO5 |
| SCK  | GPIO4 |
| NSS/CS | GPIO7 |
| RST  | GPIO19 |
| DIO0 | não ligar (transmissão por polling) |
| VCC | 3V3 (**não ligue em 5V!**) |
| GND | GND |

## Parâmetros LoRa (devem casar com o receptor)

- Frequência: 915 MHz
- Spreading factor: SF7
- Bandwidth: 125 kHz
- Coding rate: 4/5
- Sync word: 0x12 (padrão da lib LoRa.h)
- CRC: ativado
- Preamble: 8

## Payload transmitido (CSV, 1 pacote/segundo)

```
#11,millis,count,altp,temp,umi,p,gp,gr,gy,ap,ar,ay,hora,data,alt,lat,lon,sat,pqd,rssi
```

- `altp` — altitude barométrica (m)
- `alt` — altitude GPS (m)
- `lat`, `lon` — graus decimais
- `p` — pressão (hPa)
- `gp/gr/gy` — giroscópio (deg/s) | `ap/ar/ay` — acelerômetro (m/s²)
- `umi` — `nan` (não temos sensor de umidade nesta missão)
- `pqd` — `0` (sem mecanismo de paraquedas ligado)
- `rssi` — `nan` (firmware só transmite)
