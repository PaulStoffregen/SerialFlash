#include <SerialFlash.h>
#include <SD.h>
#include <SPI.h>

const int SDchipSelect = 4;
const int FlashChipSelect = 6;

void setup() {
  //uncomment these if using Teensy audio shield
  SPI.setSCK(14);  // Audio shield has SCK on pin 14
  SPI.setMOSI(7);  // Audio shield has MOSI on pin 7

  // wait up to 10 seconds for Arduino Serial Monitor
  unsigned long startMillis = millis();
  while (!Serial && (millis() - startMillis < 10000)) ;
  delay(100);
  Serial.println("Copy all files from SD Card to SPI Flash");

  if (!SD.begin(SDchipSelect)) {
    error("Unable to access SD card");
  }
  if (!SerialFlash.begin()) {
    error("Unable to access SPI Flash chip");
  }

  int count = 0;
  File rootdir = SD.open("/");
  while (1) {
    Serial.println();
    File f = rootdir.openNextFile();
    if (!f) break;
    const char *filename = f.name();
    Serial.print(filename);
    Serial.print("    ");
    unsigned long length = f.size();
    Serial.println(length);
    if (SerialFlash.create(filename, length)) {
      SerialFlashFile ff = SerialFlash.open(filename);
      if (ff) {
        Serial.print("  copying");
        // copy data.
        unsigned long count = 0;
        while (count < length) {
          char buf[256];
          unsigned int n;
          n = f.read(buf, 256);
          ff.write(buf, n);
          count = count + n;
          Serial.print(".");
        }
        ff.close();
        Serial.println();
      } else {
        Serial.println("  error opening freshly created file!");
      }
    } else {
      Serial.println("  unable to create file");
    }
    if (++count > 12) break;  // testing, only do first 12 files
    f.close();
  }
  rootdir.close();


}

void loop() {
}

void error(const char *message) {
  while (1) {
    Serial.println(message);
    delay(2500);
  }
}
