#include <Arduino.h>
#include <SPI.h>
#include <LoRa.h>
#include <TinyGPSPlus.h>

// LoRa pins
#define LORA_SS   5
#define LORA_RST  14
#define LORA_DIO0 26

// NEO-8M pins (Serial2)
#define RXD2 16
#define TXD2 17

TinyGPSPlus gps;

unsigned long lastSend = 0;
const unsigned long SEND_INTERVAL = 5000; // send every 5s when GPS valid

void setup() {
  Serial.begin(115200);
  delay(100);

  // Init Serial2 for GPS
  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);
  Serial.println("Serial2 (GPS) started");

  // Setup LoRa
  SPI.begin(18, 19, 23); // SCK, MISO, MOSI (match your wiring)
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);
  if (!LoRa.begin(915E6)) {
    Serial.println("Starting LoRa failed!");
    while (1) delay(1000);
  }
  Serial.println("LoRa initialized @ 915MHz");
}

void loop() {
  // Feed TinyGPS with incoming chars from GPS module
  while (Serial2.available()) {
    char c = Serial2.read();
    gps.encode(c);
  }
  Serial.print("Sending fixed: 14.565387,121.143830\n");
      // send to receiver over UART (Serial2) using literal
      Serial2.println("14.565387,121.143830");
      // send over LoRa as well
      LoRa.beginPacket();
      LoRa.print("14.565387,121.143830");
      LoRa.endPacket();
  // debug: echo raw NMEA to Serial monitor (optional)
  // while (Serial2.available()) Serial.write(Serial2.read());

  if (gps.location.isUpdated() || gps.location.isValid()) {
    double lat = gps.location.lat();
    double lon = gps.location.lng();


    // only send fixed coordinates for now, ignore GPS fix
    if (millis() - lastSend >= SEND_INTERVAL) {
      lastSend = millis();
      Serial.print("Sending fixed: 14.565387,121.143830\n");
      // send to receiver over UART (Serial2) using literal
      Serial2.println("14.565387,121.143830");
      // send over LoRa as well
      LoRa.beginPacket();
      LoRa.print("14.565387,121.143830");
      LoRa.endPacket();
    }
  }

  // small delay
  delay(50);
}
