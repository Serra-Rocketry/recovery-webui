/*
 * Missao #11 - Telemetria via LoRa (somente transmissao)
 *
 * ESP32-C3 DevKitM-1 + GY-GPS6MV2 (NEO-6M) + BMP280 + ICM-20602 + RFM95W 915 MHz
 *
 * Envia 1 pacote/segundo no mesmo formato CSV do webui (recovery-webui):
 * #11,millis,count,altp,temp,umi,p,gp,gr,gy,ap,ar,ay,hora,data,alt,lat,lon,sat,pqd,rssi
 *
 * Radio: parametros equivalentes ao padrao da lib LoRa.h (SF7, BW125, CR4/5,
 * sync word 0x12, CRC, 915 MHz, 17 dBm).
 */

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h>
#include <TinyGPSPlus.h>
#include <RadioLib.h>

// ---------- Pinos (ver PINOUT.md) ----------
static const int PIN_GPS_RX  = 0;   // GPS TX -> GPIO0 (UART1 RX, somente leitura)
static const int PIN_BUZZER  = 1;   // buzzer ativo em nivel ALTO (LOW = desligado)
static const int PIN_I2C_SDA = 3;   // BMP280 + ICM-20602
static const int PIN_I2C_SCL = 18;

static const int PIN_LORA_CS   = 7;
static const int PIN_LORA_DIO0 = -1;  // nao ligado (transmissao por polling)
static const int PIN_LORA_RST  = 19;
static const int PIN_LORA_SCK  = 4;
static const int PIN_LORA_MISO = 5;
static const int PIN_LORA_MOSI = 6;

static const char  TEAM_ID[]   = "#11";
static const uint32_t TX_MS    = 1000;  // periodo de transmissao

TinyGPSPlus      gps;
HardwareSerial   gpsSerial(1);
Adafruit_BMP280  bmp;
SX1276           radio = new Module(PIN_LORA_CS, RADIOLIB_NC, PIN_LORA_RST);

bool bmpOk = false;
bool imuOk = false;
uint8_t imuAddr = 0;
uint32_t count = 0;
uint32_t lastTx = 0;

static bool imuWriteReg(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(imuAddr);
  Wire.write(reg);
  Wire.write(val);
  return Wire.endTransmission() == 0;
}

static bool imuInit() {
  // ICM-20602: AD0=0 -> 0x69, AD0=1 -> 0x68 (WHO_AM_I = 0x12)
  const uint8_t addrs[] = {0x69, 0x68};
  for (uint8_t i = 0; i < 2; i++) {
    Wire.beginTransmission(addrs[i]);
    Wire.write(0x75);  // WHO_AM_I
    if (Wire.endTransmission(false) != 0) continue;
    if (Wire.requestFrom(addrs[i], (uint8_t)1) != 1) continue;
    if (Wire.read() != 0x12) continue;
    imuAddr = addrs[i];
    break;
  }
  if (!imuAddr) return false;

  imuWriteReg(0x6B, 0x01);   // PWR_MGMT_1: acorda, clock = PLL
  delay(10);
  imuWriteReg(0x6C, 0x00);   // PWR_MGMT_2: accel + gyro ligados
  imuWriteReg(0x1A, 0x03);   // GYRO_LPF ~41 Hz
  imuWriteReg(0x1B, 0x23);   // gyro +-500 dps (FCHOICE_B=11 -> LPF via LPF_CFG)
  imuWriteReg(0x1C, 0x60);   // accel +-16g
  imuWriteReg(0x1D, 0x03);   // ACCEL_LPF ~44.8 Hz
  imuWriteReg(0x19, 0x04);   // sample rate divider
  return true;
}

void setup() {
  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);  // buzzer desligado (ativo em nivel alto)

  Serial.begin(115200);
  // GPS somente leitura: RX no GPIO0, sem TX (nao usamos GPIO1, que e o buzzer)
  gpsSerial.begin(9600, SERIAL_8N1, PIN_GPS_RX, -1);

  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  Wire.setClock(100000);

  // BMP280: tenta 0x76 e 0x77
  bmpOk = bmp.begin(BMP280_ADDRESS_ALT) || bmp.begin(BMP280_ADDRESS);
  bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,
                  Adafruit_BMP280::SAMPLING_X2,
                  Adafruit_BMP280::SAMPLING_X16,
                  Adafruit_BMP280::FILTER_X16,
                  Adafruit_BMP280::STANDBY_MS_63);
  if (!bmpOk) Serial.println("[ERRO] BMP280 nao encontrado");

  imuOk = imuInit();
  if (!imuOk) Serial.println("[ERRO] ICM-20602 nao encontrado");
  else {
    Serial.print("[OK] ICM-20602 em 0x");
    Serial.println(imuAddr, HEX);
  }

  // RFM95W: SPI com pinos customizados
  SPI.begin(PIN_LORA_SCK, PIN_LORA_MISO, PIN_LORA_MOSI, PIN_LORA_CS);
  int16_t st = radio.begin(915.0, 125.0, 7, 5, 0x12, 17);
  if (st == RADIOLIB_ERR_NONE) {
    radio.setPreambleLength(8);
    Serial.println("[OK] RFM95W iniciado (915 MHz, SF7, BW125, CR4/5, sync 0x12)");
  } else {
    Serial.print("[ERRO] RFM95W: ");
    Serial.println(st);
  }

  Serial.println("[FIM DO SETUP]");
}

void loop() {
  while (gpsSerial.available()) gps.encode(gpsSerial.read());

  uint32_t now = millis();
  if (now - lastTx < TX_MS) return;
  lastTx = now;

  // ----- BMP280 -----
  float temp = bmpOk ? bmp.readTemperature() : NAN;
  float press = bmpOk ? bmp.readPressure() / 100.0F : NAN;           // hPa
  float altp = NAN;
  if (bmpOk && !isnan(press)) {
    altp = 44330.0F * (1.0F - powf(press / 1013.25F, 0.1902949572F)); // altitude ASL (m)
  }

  // ----- ICM-20602 (I2C cru) -----
  float gx = NAN, gy = NAN, gz = NAN;
  float ax = NAN, ay = NAN, az = NAN;
  uint8_t raw[14];
  if (imuOk) {
    Wire.beginTransmission(imuAddr);
    Wire.write(0x3B);  // ACCEL_XOUT_H: accel(6), temp(2), gyro(6)
  }
  if (imuOk && Wire.endTransmission(false) == 0 &&
      Wire.requestFrom(imuAddr, (uint8_t)14) == 14) {
    for (int i = 0; i < 14; i++) raw[i] = Wire.read();
    int16_t axr = (int16_t)((raw[0] << 8) | raw[1]);
    int16_t ayr = (int16_t)((raw[2] << 8) | raw[3]);
    int16_t azr = (int16_t)((raw[4] << 8) | raw[5]);
    int16_t gxr = (int16_t)((raw[8] << 8) | raw[9]);
    int16_t gyr = (int16_t)((raw[10] << 8) | raw[11]);
    int16_t gzr = (int16_t)((raw[12] << 8) | raw[13]);
    ax = axr / 2048.0F * 9.80665F;   // +/-16g -> 2048 LSB/g
    ay = ayr / 2048.0F * 9.80665F;
    az = azr / 2048.0F * 9.80665F;
    gx = gxr / 65.5F;                // +/-500 deg/s -> 65.5 LSB/dps
    gy = gyr / 65.5F;
    gz = gzr / 65.5F;
  }

  // ----- GPS -----
  char hora[12], data[12];
  if (gps.time.isValid() && gps.date.isValid()) {
    snprintf(hora, sizeof(hora), "%02d:%02d:%02d",
             gps.time.hour(), gps.time.minute(), gps.time.second());
    snprintf(data, sizeof(data), "%04d/%02d/%02d",
             gps.date.year(), gps.date.month(), gps.date.day());
  } else {
    strcpy(hora, "nan");
    strcpy(data, "nan");
  }

  bool fix = gps.location.isValid();
  float lat = fix ? gps.location.lat() : NAN;
  float lon = fix ? gps.location.lng() : NAN;
  float alt = (fix && gps.altitude.isValid()) ? gps.altitude.meters() : NAN;
  int  sat = fix ? (int)gps.satellites.value() : 0;

  // ----- Pacote CSV (mesmo formato do webui) -----
  char pkt[200];
  int n = snprintf(pkt, sizeof(pkt),
      "%s,%lu,%lu,%.2f,%.2f,nan,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%s,%s,%.2f,%.8f,%.8f,%d,0,nan",
      TEAM_ID, (unsigned long)now, (unsigned long)count,
      altp, temp, press,
      gx, gy, gz, ax, ay, az,
      hora, data,
      alt, lat, lon, sat);
  count++;

  Serial.println(pkt);

  if (n > 0 && (size_t)n < sizeof(pkt)) {
    int16_t st = radio.transmit((uint8_t*)pkt, n);
    if (st != RADIOLIB_ERR_NONE) {
      Serial.print("[ERRO TX LoRa] ");
      Serial.println(st);
    }
  }
}
