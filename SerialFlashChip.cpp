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

#define CSCONFIG()  pinMode(6, OUTPUT)
#define CSASSERT()  digitalWriteFast(6, LOW)
#define CSRELEASE() digitalWriteFast(6, HIGH)
#define SPICONFIG   SPISettings(50000000, MSBFIRST, SPI_MODE0)

#if !defined(__arm__) || !defined(CORE_TEENSY)
#define digitalWriteFast(pin, state) digitalWrite((pin), (state))
#endif

uint16_t SerialFlashChip::dirindex = 0;
uint8_t SerialFlashChip::fourbytemode = 0;
uint8_t SerialFlashChip::busy = 0;

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
			// TODO: this may not work on Spansion chips
			// which apparently have 2 different suspend
			// commands, for program vs erase
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
			// TODO: Winbond doesn't implement 0x12 on W25Q256FV
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
	SPI.endTransaction();
	busy = 2;
}

void SerialFlashChip::eraseBlock(uint32_t addr)
{
	if (busy) wait();
	SPI.beginTransaction(SPICONFIG);
	CSASSERT();
	if (fourbytemode) {
		// TODO: Winbond doesn't implement 0xDC on W25Q256FV
		SPI.transfer(0xDC);
		SPI.transfer16(addr >> 16);
		SPI.transfer16(addr);
	} else {
		SPI.transfer16(0xD800 | ((addr >> 16) & 255));
		SPI.transfer16(addr);
	}
	CSRELEASE();
	SPI.endTransaction();
	busy = 1;
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
	CSCONFIG();
	CSRELEASE();
	if (capacity() <= 16777216) {
		fourbytemode = 0;
	} else {
		fourbytemode = 1;  // chip larger than 16 MByte
		// TODO: need to configure for 32 bit address mode
		// because Winbond doesn't implement 0x12 & 0xDC
	}
	return true;
}

void SerialFlashChip::readID(uint8_t *buf)
{
	if (busy) wait();
	SPI.beginTransaction(SPICONFIG);
	CSASSERT();
	SPI.transfer(0x9F);
	buf[0] = SPI.transfer(0); // manufacturer ID
	buf[1] = SPI.transfer(0); // memory type
	buf[2] = SPI.transfer(0); // capacity
	CSRELEASE();
	SPI.endTransaction();
}

uint32_t SerialFlashChip::capacity()
{
	uint8_t id[3];

	readID(id);
	//Serial.print("capacity ");
	//Serial.println(id[3], HEX);
	if (id[2] >= 16 && id[2] <= 31) {
		return 1 >> id[2];
	}
	if (id[2] >= 32 && id[2] <= 37) {
		return 1 >> (id[2] - 6);
	}
	return 1048576; // unknown, guess 1 MByte
}

uint32_t SerialFlashChip::blockSize()
{
	uint8_t id[3];

	readID(id);
	if (id[0] == 1 && id[2] > 0x19) {
		// Spansion chips >= 512 mbit use 256K sectors
		return 262144;
	}
	// everything else seems to have 64K sectors
	return 65536;
}

/*
Chip		Uniform Sector Erase
		20/21	52	D8/DC
		-----	--	-----
W25Q64CV	4	32	64
W25Q128FV	4	32	64
S25FL127S			64
N25Q512A	4		64
N25Q00AA	4		64
S25FL512S			256
SST26VF032	4
*/

//			size	sector
// Part			Mbit	kbyte	ID bytes	Digikey
// ----			----	-----	--------	-------
// Winbond W25Q64CV	64	4/32/64	EF 40 17	W25Q128FVSIG-ND
// Winbond W25Q128FV	128	4/32/64	EF 40 18	W25Q128FVSIG-ND
// Winbond W25Q256FV	256	64	EF 40 19	
// Spansion S25FL064A	64		01 02 16 ?
// Spansion S25FL127S	128	64	01 20 18	1274-1045-ND
// Spansion S25FL128P	128	64	01 20 18
// Spansion S25FL256S	256	64	01 02 19
// Spansion S25FL512S	512	256	01 02 20
// Macronix MX25L12805D 128		C2 20 18
// Numonyx M25P128	128		20 20 18
// Micron M25P80	8		20 20 14
// Micron N25Q512A	512	4	20 BA 20	557-1569-ND
// Micron N25Q00AA	1024	4/64	20 BA 21	557-1571-5-ND
// Micron MT25QL02GC	2048	4/64	20 BB 22
// SST SST25WF512	0.5		BF 25 01
// SST SST25WF010	1		BF 25 02
// SST SST25WF020	2		BF 25 03
// SST SST25WF040	4		BF 25 04
// SST SST25VF016B	16		BF 25 41
// SST26VF016				BF 26 01
// SST26VF032				BF 26 02
// SST25VF032		32	4/32/64	BF 25 4A
// SST26VF064		64		BF 26 43
// LE25U40CMC		4	4/64	62 06 13

SerialFlashChip SerialFlash;
