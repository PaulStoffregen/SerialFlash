#include <SerialFlash.h>
#include <SPI.h>

SerialFlashFile file;

void setup() {
  char filename[40];
  uint32_t len;

  while (!Serial) ;
  delay(10);

  SPI.setSCK(14);  // Audio shield has SCK on pin 14
  SPI.setMOSI(7);  // Audio shield has MOSI on pin 7

  Serial.println("Test Hardware");
  SerialFlash.begin();
#if 0
  Serial.println("erase");
  SerialFlash.eraseAll();
  while (!SerialFlash.ready()) {
  }
  Serial.println("erase done");
#endif

  Serial.println("Directory:");
  while (SerialFlash.readdir(filename, sizeof(filename), len)) {
    Serial.print("  file: ");
    Serial.print(filename);
    Serial.print("     bytes: ");
    Serial.print(len);
    Serial.println();
  }
  Serial.println();

  Serial.println("simple.txt test");
  file = SerialFlash.open("simple.txt");
  if (file) {
    Serial.println("  file opened");
    Serial.print("    length = ");
    Serial.println(file.size());
    Serial.print("    addr on chip = ");
    Serial.println(file.getFlashAddress());
    file.close();
  } else {
    Serial.println("  create file");
    SerialFlash.create("simple.txt", 516);
  }

  Serial.println("soundfile.wav test");
  file = SerialFlash.open("soundfile.wav");
  if (file) {
    Serial.println("  file opened");
    Serial.print("    length = ");
    Serial.println(file.size());
    Serial.print("    addr on chip = ");
    Serial.println(file.getFlashAddress());
    file.close();
  } else {
    Serial.println("  create file");
    SerialFlash.createErasable("soundfile.wav", 3081000);
  }

  Serial.println("wavetable1 test");
  file = SerialFlash.open("wavetable1");
  if (file) {
    Serial.println("  file opened");
    Serial.print("    length = ");
    Serial.println(file.size());
    Serial.print("    addr on chip = ");
    Serial.println(file.getFlashAddress());
    file.close();
  } else {
    Serial.println("  create file");
    SerialFlash.create("wavetable1", 181003);
  }

  Serial.println("end");
}


void loop() {

}


void printbuf(const void *buf, uint32_t len)
{
  const uint8_t *p = (const uint8_t *)buf;
  do {
    Serial.print(*p++);
    Serial.print(" ");
  } while (--len > 0);
  Serial.println();
}

