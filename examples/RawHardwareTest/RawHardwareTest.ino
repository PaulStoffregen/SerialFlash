#include <SerialFlash.h>
#include <SPI.h>

SerialFlashFile file;

//const unsigned long testIncrement = 4096;
const unsigned long testIncrement = 4194304;

void setup() {

  //uncomment these if using Teensy audio shield
  SPI.setSCK(14);  // Audio shield has SCK on pin 14
  SPI.setMOSI(7);  // Audio shield has MOSI on pin 7

  while (!Serial) ;
  delay(100);

  Serial.println("Raw SerialFlash Hardware Test");
  SerialFlash.begin();

  test();
}

void print_signature(const unsigned char *data)
{
	Serial.print("data=");
	for (unsigned char i=0; i < 8; i++) {
		Serial.print(data[i]);
		Serial.print(" ");
	}
	Serial.println();
}

void create_signature(unsigned long address, unsigned char *data)
{
	data[0] = address >> 24;
	data[1] = address >> 16;
	data[2] = address >> 8;
	data[3] = address;
	unsigned long hash = 2166136261ul;
	for (unsigned char i=0; i < 4; i++) {
		hash ^= data[i];
		hash *= 16777619ul;
	}
	data[4] = hash;
	data[5] = hash >> 8;
	data[6] = hash >> 16;
	data[7] = hash >> 24;
}

bool equal_signatures(const unsigned char *data1, const unsigned char *data2)
{
	for (unsigned char i=0; i < 8; i++) {
		if (data1[i] != data2[i]) return false;
	}
	return true;
}


bool test() {
  unsigned char buf[16], sig[16], erased[16];
  unsigned long address, count, chipsize;
  bool first;

  // Read the chip identification
  Serial.println();
  Serial.println("Read Chip Identification:");
  SerialFlash.readID(buf);
  Serial.print("  JEDEC ID:     ");
  Serial.print(buf[0], HEX);
  Serial.print(" ");
  Serial.print(buf[1], HEX);
  Serial.print(" ");
  Serial.println(buf[2], HEX);
  Serial.print("  Part Nummber: ");
  Serial.println(id2chip(buf));
  Serial.print("  Memory Size:  ");
  chipsize = SerialFlash.capacity(buf);
  Serial.print(chipsize);
  Serial.println(" bytes");
  if (chipsize == 0) return false;

  //chipsize = 0x20000;
  //Serial.println("erasing a couple small blocks....");
  //SerialFlash.eraseBlock(0x00000000);
  //SerialFlash.eraseBlock(0x00010000);
  //while (!SerialFlash.ready()); // wait

  //Serial.println();
  //Serial.println("read locations in memory...");
  //SerialFlash.read(0x0000000, buf, 12);
  //printbuf(buf, 12);
  //SerialFlash.read(0x1000000, buf, 12);
  //printbuf(buf, 12);
  //SerialFlash.read(0x2000000, buf, 12);
  //printbuf(buf, 12);
  //SerialFlash.read(0x3000000, buf, 12);
  //printbuf(buf, 12);


  // Read the entire chip.  Every test location must be
  // erased, or have a previously tested signature
  for (unsigned char i=0; i < 8; i++) {
    erased[i] = 255;
  }
  Serial.println();
  Serial.println("Reading Chip...");
  address = 0;
  count = 0;
  first = true;
  while (address < chipsize) {
    SerialFlash.read(address, buf, 8);
    //Serial.print("  addr = ");
    //Serial.print(address, HEX);
    //Serial.print(", data = ");
    //printbuf(buf, 8);
    create_signature(address, sig);
    if (equal_signatures(buf, erased) == false) {
      if (equal_signatures(buf, sig) == false) {
        Serial.print("  Previous data found at address ");
        Serial.println(address);
        Serial.println("  You must fully erase the chip before this test");
        Serial.print("  found this: ");
        printbuf(buf, 8);
        Serial.print("     correct: ");
        printbuf(sig, 8);
        Serial.print("      erased: ");
        printbuf(erased, 8);
        return false;
      }
    } else {
      count = count + 1; // number of blank signatures
    }
    if (first) {
      address = address + (testIncrement - 8);
      first = false;
    } else {
      address = address + 8;
      first = true;
    }
  }

  // Write any signatures that were blank on the original check
  if (count > 0) {
    Serial.println();
    Serial.print("Writing ");
    Serial.print(count);
    Serial.println(" signatures");
    address = 0;
    first = true;
    while (address < chipsize) {
      SerialFlash.read(address, buf, 8);
      if (equal_signatures(buf, erased)) {
        create_signature(address, sig);
        Serial.printf("write %08X: data: ", address);
        printbuf(sig, 8);
        SerialFlash.write(address, sig, 8);
        while (!SerialFlash.ready()) ; // wait
        SerialFlash.read(address, buf, 8);
        if (equal_signatures(buf, sig) == false) {
          Serial.print("  error writing signature at ");
          Serial.println(address);
          Serial.print("  Read this: ");
          printbuf(buf, 8);
          Serial.print("  Expected:  ");
          printbuf(sig, 8);
          return false;
        }
      }
      if (first) {
        address = address + (testIncrement - 8);
        first = false;
      } else {
        address = address + 8;
        first = true;
      }
    }
  } else {
    Serial.println("  all signatures present from prior tests");
  }

  // Read all the signatures again, just to be sure
  Serial.println();
  Serial.println("Double Checking All Signatures:");
  count = 0;
  address = 0;
  first = true;
  while (address < chipsize) {
    SerialFlash.read(address, buf, 8);
    create_signature(address, sig);
    if (equal_signatures(buf, sig) == false) {
      Serial.print("  error in signature at ");
      Serial.println(address);
      Serial.print("  Read this: ");
      printbuf(buf, 8);
      Serial.print("  Expected:  ");
      printbuf(sig, 8);
      return false;
    }
    count = count + 1;
    if (first) {
      address = address + (testIncrement - 8);
      first = false;
    } else {
      address = address + 8;
      first = true;
    }
  }
  Serial.print("  all ");
  Serial.print(count);
  Serial.println(" signatures read ok");

  // Read pairs of adjacent signatures
  Serial.println();
  Serial.println("Checking Signature Pairs");
  count = 0;
  address = testIncrement - 8;
  first = true;
  while (address < chipsize - 8) {
    SerialFlash.read(address, buf, 16);
    create_signature(address, sig);
    create_signature(address + 8, sig + 8);
    if (memcmp(buf, sig, 16) != 0) {
      Serial.print("  error in signature pair at ");
      Serial.println(address);
      Serial.print("  Read this: ");
      printbuf(buf, 16);
      Serial.print("  Expected:  ");
      printbuf(sig, 16);
      return false;
    }
    count = count + 1;
    address = address + testIncrement;
  }
  Serial.print("  all ");
  Serial.print(count);
  Serial.println(" signature pairs read ok");





  Serial.println();
  Serial.println("All Tests Passed  :-)");
  return true;
}


void loop() {
  // do nothing after the test
}

const char * id2chip(const unsigned char *id)
{
	if (id[0] == 0xEF) {
		// Winbond
		if (id[1] == 0x40) {
			if (id[2] == 0x17) return "W25Q64FV";
			if (id[2] == 0x18) return "W25Q128FV";
			if (id[2] == 0x19) return "W25Q256FV";
		}
	}
	if (id[0] == 0x01) {
		// Spansion
		if (id[1] == 0x02) {
			if (id[2] == 0x16) return "S25FL064A";
			if (id[2] == 0x19) return "S25FL256S";
			if (id[2] == 0x20) return "S25FL512S";
		}
		if (id[1] == 0x20) {
			if (id[2] == 0x18) return "S25FL127S";
		}
	}
	if (id[0] == 0xC2) {
		// Macronix
		if (id[1] == 0x20) {
			if (id[2] == 0x18) return "MX25L12805D";
		}
	}
	if (id[0] == 0x20) {
		// Micron
		if (id[1] == 0xBA) {
			if (id[2] == 0x20) return "N25Q512A";
			if (id[2] == 0x21) return "N25Q00AA";
		}
		if (id[1] == 0xBB) {
			if (id[2] == 0x22) return "MT25QL02GC";
		}
	}
	if (id[0] == 0xBF) {
		// SST
		if (id[1] == 0x25) {
			if (id[2] == 0x02) return "SST25WF010";
			if (id[2] == 0x03) return "SST25WF020";
			if (id[2] == 0x04) return "SST25WF040";
			if (id[2] == 0x41) return "SST25VF016B";
			if (id[2] == 0x4A) return "SST25VF032";
		}
		if (id[1] == 0x25) {
			if (id[2] == 0x01) return "SST26VF016";
			if (id[2] == 0x02) return "SST26VF032";
			if (id[2] == 0x43) return "SST26VF064";
		}
	}
	return "(unknown chip)";
}


void printbuf(const void *buf, uint32_t len)
{
  const uint8_t *p = (const uint8_t *)buf;
  do {
    unsigned char b = *p++;
    Serial.print(b >> 4, HEX);
    Serial.print(b & 15, HEX);
    //Serial.printf("%02X", *p++);
    Serial.print(" ");
  } while (--len > 0);
  Serial.println();
}

