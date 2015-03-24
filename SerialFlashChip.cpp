/* SerialFlash Library - for filesystem-like access to SPI Serial Flash memory
 * https://github.com/PaulStoffregen/SerialFlash
 * Copyright (C) 2015, Paul Stoffregen, paul@pjrc.com
 *
 * Development of this library was funded by PJRC.COM, LLC by sales of Teensy.
 * Please support PJRC's efforts to develop open source software by purchasing
 * Teensy or other genuine PJRC products.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice, development funding notice, and this permission
 * notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "SerialFlash.h"
#include "SPIFIFO.h"

#define CSCONFIG()  pinMode(6, OUTPUT)
#define CSASSERT()  digitalWriteFast(6, LOW)
#define CSRELEASE() digitalWriteFast(6, HIGH)
#define SPICONFIG   SPISettings(50000000, MSBFIRST, SPI_MODE0)

uint16_t SerialFlashChip::dirindex = 0;
uint8_t SerialFlashChip::fourbytemode = 0;
uint8_t SerialFlashChip::busy = 0;
uint8_t SerialFlashChip::blocksize = 1;
uint8_t SerialFlashChip::capacityId = 0;

void SerialFlashChip::wait(void)
{
	uint32_t status;
	do {
		SPI.beginTransaction(SPICONFIG);
		CSASSERT();
		status = SPI.transfer16(0x0500);
		CSRELEASE();
		SPI.endTransaction();
	} while ((status & 1));
	busy = 0;
}

void SerialFlashChip::read(void *buf, uint32_t addr, uint32_t len)
{
	uint8_t *p = (uint8_t *)buf;
	uint8_t b;

	memset(p, 0, len);
	b = busy;
	if (b) {
		if (b == 1) {
			SPI.beginTransaction(SPICONFIG);
			CSASSERT();
			SPI.transfer(0x75); // Suspend program/erase
			CSRELEASE();
			SPI.endTransaction();
			delayMicroseconds(20); // Tsus = 20us
		} else {
			wait();
		}
	}
	SPI.beginTransaction(SPICONFIG);
	CSASSERT();
	// TODO: FIFO optimize....
	if (fourbytemode) {
		SPI.transfer(0x13);
		SPI.transfer16(addr >> 16);
		SPI.transfer16(addr);
	} else {
		SPI.transfer16(0x0300 | ((addr >> 16) & 255));
		SPI.transfer16(addr);
	}
	SPI.transfer(p, len);
	CSRELEASE();
	SPI.endTransaction();
	if (b == 1) {
		SPI.beginTransaction(SPICONFIG);
		CSASSERT();
		SPI.transfer(0x7A); // Resume program/erase
		CSRELEASE();
		SPI.endTransaction();
	}
}

void SerialFlashChip::write(const void *buf, uint32_t addr, uint32_t len)
{
	const uint8_t *p = (const uint8_t *)buf;
	uint32_t max, pagelen;

	do {
		if (busy) wait();
		SPI.beginTransaction(SPICONFIG);
		CSASSERT();
		SPI.transfer(0x06);
		CSRELEASE();
		//delayMicroseconds(1);
		max = 256 - (addr & 0xFF);
		pagelen = (len <= max) ? len : max;
		len -= pagelen;
		CSASSERT();
		if (fourbytemode) {
			SPI.transfer(0x12);
			SPI.transfer16(addr >> 16);
			SPI.transfer16(addr);
		} else {
			SPI.transfer16(0x0200 | ((addr >> 16) & 255));
			SPI.transfer16(addr);
		}
		do {
			SPI.transfer(*p++);
		} while (--pagelen > 0);
		CSRELEASE();
		SPI.endTransaction();
		busy = 1;
	} while (len > 0);
}

void SerialFlashChip::eraseAll()
{
	if (busy) wait();
	SPI.beginTransaction(SPICONFIG);
	CSASSERT();
	SPI.transfer(0x06);
	CSRELEASE();
	CSASSERT();
	SPI.transfer(0xC7);
	CSRELEASE();
	busy = 2;
}

bool SerialFlashChip::ready()
{
	uint32_t status;
	if (!busy) return true;
	SPI.beginTransaction(SPICONFIG);
	CSASSERT();
	status = SPI.transfer16(0x0500);
	CSRELEASE();
	SPI.endTransaction();
	if ((status & 1)) return false;
	busy = 0;
	return true;
}


bool SerialFlashChip::begin()
{
	SPI.begin();
	if (busy) wait();
	CSCONFIG();
	SPI.beginTransaction(SPICONFIG);
	CSASSERT();
	SPI.transfer(0x9F);
	SPI.transfer(0); // manufacturer ID
	SPI.transfer(0); // memory type
	capacityId = SPI.transfer(0); // capacity
	CSRELEASE();
	SPI.endTransaction();
	//Serial.print("capacity = ");
	//Serial.println(capacityId, HEX);
	if ((capacityId & 0xF0) == 0x20) {
		fourbytemode = 1;  // chip larger than 16 MByte
	} else {
		fourbytemode = 0;
	}
	// TODO: how to detect the uniform sector erase size?
	blocksize = 1;
	return true;
}

uint32_t SerialFlashChip::capacity()
{
	return 16777216; // TODO: compute this from capacityId...
}

uint32_t SerialFlashChip::blockSize()
{
	return 4096; // TODO: how to discover this?
}



//			size	sector
// Part			Mbit	kbyte	ID bytes	Digikey
// ----			----	-----	--------	-------
// Winbond W25Q128FV	128		EF 40 18	W25Q128FVSIG-ND
// Winbond W25Q256FV	256	64	EF 40 19	
// SST SST25VF016B	16		BF 25 41
// Spansion S25FL127S	128	64?	01 20 18	1274-1045-ND
// Spansion S25FL128P	128		01 20 18
// Spansion S25FL064A	64		01 02 16
// Macronix MX25L12805D 128		C2 20 18
// Micron M25P80	8		20 20 14
// Numonyx M25P128	128		20 20 18
// SST SST25WF512	0.5		BF 25 01
// SST SST25WF010	1		BF 25 02
// SST SST25WF020	2		BF 25 03
// SST SST25WF040	4		BF 25 04
// Spansion FL127S	128		01 20 18  ?
// Spansion S25FL512S	512		01 02 20  ?
// Micron N25Q512A	512	4	20 BA 20	557-1569-ND
// Micron N25Q00AA	1024	4/64	20 BA 21	557-1571-5-ND
// Micron MT25QL02GC	2048	4/64	20 BB 22

SerialFlashChip SerialFlash;
