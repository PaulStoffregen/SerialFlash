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

#ifndef SerialFlash_h_
#define SerialFlash_h_

#include <Arduino.h>
#include <SPI.h>
#include "util/SerialFlash_directwrite.h"

class SerialFlashFile;

class SerialFlashChip
{
public:
	SerialFlashChip();
	bool begin(SPIClass& device, uint8_t pin = 6);
	bool begin(uint8_t pin = 6);
	void changeSettings(SPISettings& settings);
	uint32_t capacity(const uint8_t *id);
	uint32_t blockSize();
	void sleep();
	void wakeup();
	void readID(uint8_t *buf);
	void readSerialNumber(uint8_t *buf);
	void read(uint32_t addr, void *buf, uint32_t len);
	bool ready();
	void wait();
	void write(uint32_t addr, const void *buf, uint32_t len);
	void eraseAll();
	void eraseSector(uint32_t addr);
	void eraseBlock(uint32_t addr);

	SerialFlashFile open(const char *filename);
	bool create(const char *filename, uint32_t length, uint32_t align = 0);
	bool createErasable(const char *filename, uint32_t length) {
		return create(filename, length, blockSize());
	}
	bool exists(const char *filename);
	bool remove(const char *filename);
	bool remove(SerialFlashFile &file);
	void opendir() { dirindex = 0; }
	bool readdir(char *filename, uint32_t strsize, uint32_t &filesize);
protected:
	SPISettings SPICONFIG;

	volatile IO_REG_TYPE *cspin_basereg;
	IO_REG_TYPE cspin_bitmask;

	SPIClass* SPIPORT;

private:
	uint16_t dirindex; // current position for readdir()
	uint8_t flags;	// chip features
	uint8_t busy;	// 0 = ready
				// 1 = suspendable program operation
				// 2 = suspendable erase operation
				// 3 = busy for realz!!
};

extern SerialFlashChip SerialFlash;


class SerialFlashFile
{
public:
	constexpr SerialFlashFile() { }
	operator bool() {
		if (address > 0) return true;
		return false;
	}
	uint32_t read(void *buf, uint32_t rdlen) {
		if (offset + rdlen > length) {
			if (offset >= length) return 0;
			rdlen = length - offset;
		}
		SerialFlash.read(address + offset, buf, rdlen);
		offset += rdlen;
		return rdlen;
	}
	uint32_t write(const void *buf, uint32_t wrlen) {
		if (offset + wrlen > length) {
			if (offset >= length) return 0;
			wrlen = length - offset;
		}
		SerialFlash.write(address + offset, buf, wrlen);
		offset += wrlen;
		return wrlen;
	}
	void seek(uint32_t n) {
		offset = n;
	}
	uint32_t position() {
		return offset;
	}
	uint32_t size() {
		return length;
	}
	uint32_t available() {
		if (offset >= length) return 0;
		return length - offset;
	}
	void erase();
	void flush() {
	}
	void close() {
	}
	uint32_t getFlashAddress() {
		return address;
	}
protected:
	friend class SerialFlashChip;
	uint32_t address = 0;  // where this file's data begins in the Flash, or zero
	uint32_t length = 0;   // total length of the data in the Flash chip
	uint32_t offset = 0; // current read/write offset in the file
	uint16_t dirindex = 0;
};


#endif
