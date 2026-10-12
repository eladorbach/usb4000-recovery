// Read-only I2C EEPROM diagnostics via ESP-WROVER-KIT accessory bus.
// Actual board tested with GPIO21 SDA, GPIO22 SCL and independent 3.3V DMM check.
// NEVER attach an unmeasured 5V bus directly to an ESP32 GPIO.
// This firmware does not contain EEPROM writes, ARM commands or a patcher.
#include <Arduino.h>
#include <Wire.h>

static constexpr int SDA_PIN = 21;
static constexpr int SCL_PIN = 22;
static constexpr uint8_t READ_CHUNK = 16;
static constexpr uint16_t EEPROM_BYTES = 32768;

bool read_chunk(uint8_t i2c_address, uint16_t offset, uint8_t *dest) {
  if (i2c_address != 0x51 && i2c_address != 0x53) return false;
  Wire.beginTransmission(i2c_address);
  Wire.write(uint8_t(offset >> 8));
  Wire.write(uint8_t(offset & 0xff));
  if (Wire.endTransmission(false) != 0) return false; // repeated-start
  if (Wire.requestFrom(uint16_t(i2c_address), uint8_t(READ_CHUNK), true) != READ_CHUNK) return false;
  for (uint8_t i = 0; i < READ_CHUNK; i++) {
    if (!Wire.available()) return false;
    dest[i] = Wire.read();
  }
  return true;
}

void scan() {
  for (uint8_t addr : {uint8_t(0x4f), uint8_t(0x51), uint8_t(0x53)}) {
    Wire.beginTransmission(addr);
    uint8_t rc = Wire.endTransmission(true);
    Serial.printf("ADDR 0x%02X %s STATUS=%u\n", addr, rc == 0 ? "ACK" : "NOACK", rc);
  }
}

void dump(uint8_t addr) {
  uint8_t block[READ_CHUNK];
  Serial.printf("BEGIN 0x%02X 32768\n", addr);
  for (uint32_t offset = 0; offset < EEPROM_BYTES; offset += READ_CHUNK) {
    if (!read_chunk(addr, uint16_t(offset), block)) {
      Serial.printf("ERROR address=0x%02X offset=0x%04lX\n", addr, offset);
      return;
    }
    Serial.printf("DATA %04lX ", offset);
    for (uint8_t j = 0; j < READ_CHUNK; j++) Serial.printf("%02X", block[j]);
    Serial.println();
  }
  Serial.printf("END 0x%02X 32768\n", addr);
}

void setup() {
  Serial.begin(115200);
  Wire.begin(SDA_PIN, SCL_PIN, 100000);
  Serial.println("USB4000 READ-ONLY I2C bridge ready. Commands: SCAN, DUMP51, DUMP53");
}

void loop() {
  if (!Serial.available()) { delay(2); return; }
  String c = Serial.readStringUntil('\n'); c.trim(); c.toUpperCase();
  if (c == "SCAN") scan();
  else if (c == "DUMP51") dump(0x51);
  else if (c == "DUMP53") dump(0x53);
  else Serial.println("Unsupported command. READ-ONLY firmware.");
}
