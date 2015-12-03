#include <SerialFlash.h>
#include <SD.h>
#include <SPI.h>

const int SDchipSelect = 4; // Audio Shield has SD card CS on pin 10
const int FlashChipSelect = 6;

void setup() {
  //uncomment these if using Teensy audio shield
  //SPI.setSCK(14);  // Audio shield has SCK on pin 14
  //SPI.setMOSI(7);  // Audio shield has MOSI on pin 7

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
    // open a file from the SD card
    Serial.println();
    File f = rootdir.openNextFile();
    if (!f) break;
    const char *filename = f.name();
    Serial.print(filename);
    Serial.print("    ");
    unsigned long length = f.size();
    Serial.println(length);

    // check if this file is already on the Flash chip
    if (SerialFlash.exists(filename)) {
      Serial.println("  already exists on the Flash chip");
      SerialFlashFile ff = SerialFlash.open(filename);
      if (ff && ff.size() == f.size()) {
        Serial.println("  size is the same, comparing data...");
        if (compareFiles(f, ff) == true) {
          Serial.println("  files are identical :)");
          f.close();
          ff.close();
          continue;  // advance to next file
        } else {
          Serial.println("  files are different");
        }
      } else {
        Serial.print("  size is different, ");
        Serial.print(ff.size());
        Serial.println(" bytes");
      }
      // delete the copy on the Flash chip, if different
      Serial.println("  delete file from Flash chip");
      SerialFlash.remove(filename);
    }

    // create the file on the Flash chip and copy data
    if (SerialFlash.create(filename, length)) {
      SerialFlashFile ff = SerialFlash.open(filename);
      if (ff) {
        Serial.print("  copying");
        // copy data loop
        unsigned long count = 0;
        unsigned char dotcount = 9;
        while (count < length) {
          char buf[256];
          unsigned int n;
          n = f.read(buf, 256);
          ff.write(buf, n);
          count = count + n;
          Serial.print(".");
          if (++dotcount > 100) {
             Serial.println();
             dotcount = 0;
          }
        }
        ff.close();
        if (dotcount > 0) Serial.println();
      } else {
        Serial.println("  error opening freshly created file!");
      }
    } else {
      Serial.println("  unable to create file");
    }
    f.close();
  }
  rootdir.close();
  delay(10);
  Serial.println("Finished All Files");
}


bool compareFiles(File &file, SerialFlashFile &ffile) {
  file.seek(0);
  ffile.seek(0);
  unsigned long count = file.size();
  while (count > 0) {
    char buf1[128], buf2[128];
    unsigned long n = count;
    if (n > 128) n = 128;
    file.read(buf1, n);
    ffile.read(buf2, n);
    if (memcmp(buf1, buf2, n) != 0) return false; // differ
    count = count - n;
  }
  return true;  // all data identical
}


void loop() {
}

void error(const char *message) {
  while (1) {
    Serial.println(message);
    delay(2500);
  }
}
