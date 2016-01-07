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
#include "util/SerialFlash_directwrite.h"

#define CSASSERT()  DIRECT_WRITE_LOW(cspin_basereg, cspin_bitmask)
#define CSRELEASE() DIRECT_WRITE_HIGH(cspin_basereg, cspin_bitmask)
#define SPICONFIG   SPISettings(50000000, MSBFIRST, SPI_MODE0)

uint16_t SerialFlashChip::dirindex = 0;
uint8_t SerialFlashChip::flags = 0;
uint8_t SerialFlashChip::busy = 0;

static volatile IO_REG_TYPE *cspin_basereg;
static IO_REG_TYPE cspin_bitmask;

#define FLAG_32BIT_ADDR		0x01	// larger than 16 MByte address
#define FLAG_STATUS_CMD70	0x02	// requires special busy flag check
#define FLAG_DIFF_SUSPEND	0x04	// uses 2 different suspend commands
#define FLAG_MULTI_DIE		0x08	// multiple die, don't read cross 32M barrier
#define FLAG_256K_BLOCKS	0x10	// has 256K erase blocks
#define FLAG_DIE_MASK		0xC0	// top 2 bits count during multi-die erase

void SerialFlashChip::wait(void)
{
	uint32_t status;
	//Serial.print("wait-");
	while (1) {
		SPI.beginTransaction(SPICONFIG);
		CSASSERT();
		if (flags & FLAG_STATUS_CMD70) {
			// some Micron chips require this different
			// command to detect program and erase completion
			SPI.transfer(0x70);
			status = SPI.transfer(0);
			CSRELEASE();
			SPI.endTransaction();
			//Serial.printf("b=%02x.", status & 0xFF);
			if ((status & 0x80)) break;
		} else {
			// all others work by simply reading the status reg
			SPI.transfer(0x05);
			status = SPI.transfer(0);
			CSRELEASE();
			SPI.endTransaction();
			//Serial.printf("b=%02x.", status & 0xFF);
			if (!(status & 1)) break;
		}
	}
	busy = 0;
	//Serial.println();
}

void SerialFlashChip::read(uint32_t addr, void *buf, uint32_t len)
{
	uint8_t *p = (uint8_t *)buf;
	uint8_t b, f, status, cmd;

	memset(p, 0, len);
	f = flags;
	SPI.beginTransaction(SPICONFIG);
	b = busy;
	if (b) {
		// read status register ... chip may no longer be busy
		CSASSERT();
		if (flags & FLAG_STATUS_CMD70) {
			SPI.transfer(0x70);
			status = SPI.transfer(0);
			if ((status & 0x80)) b = 0;
		} else {
			SPI.transfer(0x05);
			status = SPI.transfer(0);
			if (!(status & 1)) b = 0;
		}
		CSRELEASE();
		if (b == 0) {
			// chip is no longer busy :-)
			busy = 0;
		} else if (b < 3) {
			// TODO: this may not work on Spansion chips
			// which apparently have 2 different suspend
			// commands, for program vs erase
			CSASSERT();
			SPI.transfer(0x06); // write enable (Micron req'd)
			CSRELEASE();
			delayMicroseconds(1);
			cmd = 0x75; //Suspend program/erase for almost all chips
			// but Spansion just has to be different for program suspend!
			if ((f & FLAG_DIFF_SUSPEND) && (b == 1)) cmd = 0x85;
			CSASSERT();
			SPI.transfer(cmd); // Suspend command
			CSRELEASE();
			if (f & FLAG_STATUS_CMD70) {
				// Micron chips don't actually suspend until flags read
				CSASSERT();
				SPI.transfer(0x70);
				do {
					status = SPI.transfer(0);
				} while (!(status & 0x80));
				CSRELEASE();
			} else {
				CSASSERT();
				SPI.transfer(0x05);
				do {
					status = SPI.transfer(0);
				} while ((status & 0x01));
				CSRELEASE();
			}
		} else {
			// chip is busy with an operation that can not suspend
			SPI.endTransaction();	// is this a good idea?
			wait();			// should we wait without ending
			b = 0;			// the transaction??
			SPI.beginTransaction(SPICONFIG);
		}
	}
	do {
		uint32_t rdlen = len;
		if (f & FLAG_MULTI_DIE) {
			if ((addr & 0xFE000000) != ((addr + len - 1) & 0xFE000000)) {
				rdlen = 0x2000000 - (addr & 0x1FFFFFF);
			}
		}
		CSASSERT();
		// TODO: FIFO optimize....
		if (f & FLAG_32BIT_ADDR) {
			SPI.transfer(0x03);
			SPI.transfer16(addr >> 16);
			SPI.transfer16(addr);
		} else {
			SPI.transfer16(0x0300 | ((addr >> 16) & 255));
			SPI.transfer16(addr);
		}
		SPI.transfer(p, rdlen);
		CSRELEASE();
		p += rdlen;
		addr += rdlen;
		len -= rdlen;
	} while (len > 0);
	if (b) {
		CSASSERT();
		SPI.transfer(0x06); // write enable (Micron req'd)
		CSRELEASE();
		delayMicroseconds(1);
		cmd = 0x7A;
		if ((f & FLAG_DIFF_SUSPEND) && (b == 1)) cmd = 0x8A;
		CSASSERT();
		SPI.transfer(cmd); // Resume program/erase
		CSRELEASE();
	}
	SPI.endTransaction();
}

void SerialFlashChip::write(uint32_t addr, const void *buf, uint32_t len)
{
	const uint8_t *p = (const uint8_t *)buf;
	uint32_t max, pagelen;

	 //Serial.printf("WR: addr %08X, len %d\n", addr, len);
	do {
		if (busy) wait();
		SPI.beginTransaction(SPICONFIG);
		CSASSERT();
		// write enable command
		SPI.transfer(0x06);
		CSRELEASE();
		max = 256 - (addr & 0xFF);
		pagelen = (len <= max) ? len : max;
		 //Serial.printf("WR: addr %08X, pagelen %d\n", addr, pagelen);
		CSASSERT();
		if (flags & FLAG_32BIT_ADDR) {
			SPI.transfer(0x02); // program page command
			SPI.transfer16(addr >> 16);
			SPI.transfer16(addr);
		} else {
			SPI.transfer16(0x0200 | ((addr >> 16) & 255));
			SPI.transfer16(addr);
		}
		addr += pagelen;
		len -= pagelen;
		do {
			SPI.transfer(*p++);
		} while (--pagelen > 0);
		CSRELEASE();
		busy = 1;
		SPI.endTransaction();
	} while (len > 0);
}

void SerialFlashChip::eraseAll()
{
	if (busy) wait();
	uint8_t id[3];
	readID(id);
	//Serial.printf("ID: %02X %02X %02X\n", id[0], id[1], id[2]);
	if (id[0] == 0x20 && id[2] >= 0x20 && id[2] <= 0x22) {
		// Micron's multi-die chips require special die erase commands
		//  N25Q512A	20 BA 20  2 dies  32 Mbyte/die   65 nm transitors
		//  N25Q00AA	20 BA 21  4 dies  32 Mbyte/die   65 nm transitors
		//  MT25QL02GC	20 BA 22  2 dies  128 Mbyte/die  45 nm transitors
		uint8_t die_count = 2;
		if (id[2] == 0x21) die_count = 4;
		uint8_t die_index = flags >> 6;
		 //Serial.printf("Micron die erase %d\n", die_index);
		flags &= 0x3F;
		if (die_index >= die_count) return; // all dies erased :-)
		uint8_t die_size = 2;  // in 16 Mbyte units
		if (id[2] == 0x22) die_size = 8;
		SPI.beginTransaction(SPICONFIG);
		CSASSERT();
		SPI.transfer(0x06); // write enable command
		CSRELEASE();
		 delayMicroseconds(1);
		CSASSERT();
		// die erase command
		SPI.transfer(0xC4);
		SPI.transfer16((die_index * die_size) << 8);
		SPI.transfer16(0x0000);
		CSRELEASE();
		 //Serial.printf("Micron erase begin\n");
		flags |= (die_index + 1) << 6;
	} else {
		// All other chips support the bulk erase command
		SPI.beginTransaction(SPICONFIG);
		CSASSERT();
		// write enable command
		SPI.transfer(0x06);
		CSRELEASE();
		 delayMicroseconds(1);
		CSASSERT();
		// bulk erase command
		SPI.transfer(0xC7);
		CSRELEASE();
		SPI.endTransaction();
	}
	busy = 3;
}

void SerialFlashChip::eraseBlock(uint32_t addr)
{
	uint8_t f = flags;
	if (busy) wait();
	SPI.beginTransaction(SPICONFIG);
	CSASSERT();
	SPI.transfer(0x06); // write enable command
	CSRELEASE();
	 delayMicroseconds(1);
	CSASSERT();
	if (f & FLAG_32BIT_ADDR) {
		SPI.transfer(0xD8);
		SPI.transfer16(addr >> 16);
		SPI.transfer16(addr);
	} else {
		SPI.transfer16(0xD800 | ((addr >> 16) & 255));
		SPI.transfer16(addr);
	}
	CSRELEASE();
	SPI.endTransaction();
	busy = 2;
}


bool SerialFlashChip::ready()
{
	uint32_t status;
	if (!busy) return true;
	SPI.beginTransaction(SPICONFIG);
	CSASSERT();
	if (flags & FLAG_STATUS_CMD70) {
		// some Micron chips require this different
		// command to detect program and erase completion
		SPI.transfer(0x70);
		status = SPI.transfer(0);
		CSRELEASE();
		SPI.endTransaction();
		//Serial.printf("ready=%02x\n", status & 0xFF);
		if ((status & 0x80) == 0) return false;
	} else {
		// all others work by simply reading the status reg
		SPI.transfer(0x05);
		status = SPI.transfer(0);
		CSRELEASE();
		SPI.endTransaction();
		//Serial.printf("ready=%02x\n", status & 0xFF);
		if ((status & 1)) return false;
	}
	busy = 0;
	if (flags & 0xC0) {
		// continue a multi-die erase
		eraseAll();
		return false;
	}
	return true;
}


#define ID0_WINBOND	0xEF
#define ID0_SPANSION	0x01
#define ID0_MICRON	0x20
#define ID0_MACRONIX	0xC2
#define ID0_SST		0xBF

//#define FLAG_32BIT_ADDR	0x01	// larger than 16 MByte address
//#define FLAG_STATUS_CMD70	0x02	// requires special busy flag check
//#define FLAG_DIFF_SUSPEND	0x04	// uses 2 different suspend commands
//#define FLAG_256K_BLOCKS	0x10	// has 256K erase blocks

bool SerialFlashChip::begin(uint8_t pin)
{
	uint8_t id[3];
	uint8_t f;
	uint32_t size;

	cspin_basereg = PIN_TO_BASEREG(pin);
	cspin_bitmask = PIN_TO_BITMASK(pin);
	SPI.begin();
	pinMode(pin, OUTPUT);
	CSRELEASE();
	readID(id);
	f = 0;
	size = capacity(id);
	if (size > 16777216) {
		// more than 16 Mbyte requires 32 bit addresses
		f |= FLAG_32BIT_ADDR;
		SPI.beginTransaction(SPICONFIG);
		if (id[0] == ID0_SPANSION) {
			// spansion uses MSB of bank register
			CSASSERT();
			SPI.transfer16(0x1780); // bank register write
			CSRELEASE();
		} else {
			// micron & winbond & macronix use command
			CSASSERT();
			SPI.transfer(0x06); // write enable
			CSRELEASE();
			delayMicroseconds(1);
			CSASSERT();
			SPI.transfer(0xB7); // enter 4 byte addr mode
			CSRELEASE();
		}
		SPI.endTransaction();
		if (id[0] == ID0_MICRON) f |= FLAG_MULTI_DIE;
	}
	if (id[0] == ID0_SPANSION) {
		// Spansion has separate suspend commands
		f |= FLAG_DIFF_SUSPEND;
		if (size >= 67108864) {
			// Spansion chips >= 512 mbit use 256K sectors
			f |= FLAG_256K_BLOCKS;
		}
	}
	if (id[0] == ID0_MICRON) {
		// Micron requires busy checks with a different command
		f |= FLAG_STATUS_CMD70; // TODO: all or just multi-die chips?
	}
	flags = f;
	readID(id);
	return true;
}

// chips tested: https://github.com/PaulStoffregen/SerialFlash/pull/12#issuecomment-169596992
//
void SerialFlashChip::sleep()
{
	if (busy) wait();
	SPI.beginTransaction(SPICONFIG);
	CSASSERT();
	SPI.transfer(0xB9); // Deep power down command
	CSRELEASE();
}

void SerialFlashChip::wakeup()
{
	SPI.beginTransaction(SPICONFIG);
	CSASSERT();
	SPI.transfer(0xAB); // Wake up from deep power down command
	CSRELEASE();
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
	//Serial.printf("ID: %02X %02X %02X\n", buf[0], buf[1], buf[2]);
}

uint32_t SerialFlashChip::capacity(const uint8_t *id)
{
	uint32_t n = 1048576; // unknown chips, default to 1 MByte

	if (id[2] >= 16 && id[2] <= 31) {
		n = 1ul << id[2];
	} else
	if (id[2] >= 32 && id[2] <= 37) {
		n = 1ul << (id[2] - 6);
	}
	//Serial.printf("capacity %lu\n", n);
	return n;
}

uint32_t SerialFlashChip::blockSize()
{
	// Spansion chips >= 512 mbit use 256K sectors
	if (flags & FLAG_256K_BLOCKS) return 262144;
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



//			size	sector			busy	pgm/erase	chip
// Part			Mbyte	kbyte	ID bytes	cmd	suspend		erase
// ----			----	-----	--------	---	-------		-----
// Winbond W25Q64CV	8	64	EF 40 17
// Winbond W25Q128FV	16	64	EF 40 18	05	single		60 & C7
// Winbond W25Q256FV	32	64	EF 40 19	
// Spansion S25FL064A	8	?	01 02 16
// Spansion S25FL127S	16	64	01 20 18	05
// Spansion S25FL128P	16	64	01 20 18
// Spansion S25FL256S	32	64	01 02 19	05			60 & C7
// Spansion S25FL512S	64	256	01 02 20
// Macronix MX25L12805D 16	?	C2 20 18
// Macronix MX66L51235F	64		C2 20 1A
// Numonyx M25P128	16	?	20 20 18
// Micron M25P80	1	?	20 20 14
// Micron N25Q128A	16	64	20 BA 18
// Micron N25Q512A	64	?	20 BA 20	70	single		C4 x2
// Micron N25Q00AA	128	64	20 BA 21		single		C4 x4
// Micron MT25QL02GC	256	64	20 BA 22	70			C4 x2
// SST SST25WF010	1/8	?	BF 25 02
// SST SST25WF020	1/4	?	BF 25 03
// SST SST25WF040	1/2	?	BF 25 04
// SST SST25VF016B	1	?	BF 25 41
// SST26VF016			?	BF 26 01
// SST26VF032			?	BF 26 02
// SST25VF032		4	64	BF 25 4A
// SST26VF064		8	?	BF 26 43
// LE25U40CMC		1/2	64	62 06 13

SerialFlashChip SerialFlash;
