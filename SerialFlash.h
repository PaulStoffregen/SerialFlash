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

// supports auto growing files when writing
#define SF_FEATURE_AUTO_GROW

#define SF_OK                      0
#define SF_ERROR_FAILED            1
#define SF_ERROR_WRITING           2
#define SF_ERROR_DISK_FULL         3
#define SF_ERROR_OVERFLOW          4
#define SF_ERROR_CLOSED            5

class SerialFlashFile;

class SerialFlashChip
{
public:
	static bool begin(SPIClass& device, uint8_t pin = 6);
	static bool begin(uint8_t pin = 6);
	static uint32_t capacity(const uint8_t *id);
	static uint32_t blockSize();
	static void sleep();
	static void wakeup();
	static void readID(uint8_t *buf);
	static void readSerialNumber(uint8_t *buf);
	static void read(uint32_t addr, void *buf, uint32_t len);
	static bool ready();
	static void wait();
	static void write(uint32_t addr, const void *buf, uint32_t len);
	static void eraseAll();
	static void eraseBlock(uint32_t addr);
	static void unprotectAll();

	static SerialFlashFile open(const char *filename, char mode = 'r');
	static SerialFlashFile create(const char *filename, uint32_t length, uint32_t align = 0);
	static SerialFlashFile createErasable(const char *filename, uint32_t length);
	static bool exists(const char *filename);
	static bool remove(const char *filename);
	static bool remove(SerialFlashFile &file);
	static void opendir() { dirindex = 0; }
	static bool readdir(char *filename, uint32_t strsize, uint32_t &filesize);

	static uint8_t lastErr; //last error
private:
	friend class SerialFlashFile; 
	static uint32_t totalCapacity; //flash total capacity
	static bool writing; // file opened for incremental writing
	static uint16_t dirindex; // current position for readdir()
	static uint8_t flags;	// chip features
	static uint8_t busy;	// 0 = ready
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
		if (!address) {
			SerialFlashChip::lastErr = SF_ERROR_CLOSED;
			return 0;
		}
		if (offset + rdlen > length) {
			if (offset >= length) {
				SerialFlashChip::lastErr = SF_ERROR_OVERFLOW;
				return 0;
			}
			rdlen = length - offset;
		}
		SerialFlash.read(address + offset, buf, rdlen);
		offset += rdlen;
		SerialFlashChip::lastErr = SF_OK;
		return rdlen;
	}
	char read() {
		char b = -1;
		read(&b, 1);
		return b;
	}
	uint32_t write(const void *buf, uint32_t wrlen) {
		if (!address) {
			SerialFlashChip::lastErr = SF_ERROR_CLOSED;
			return 0;
		}
		if (length > 0) {
			if (offset + wrlen > length) {
				if (offset >= length) {
					SerialFlashChip::lastErr = SF_ERROR_OVERFLOW;
					return 0;
				}
				wrlen = length - offset;
			}
		} else {
			// handle auto growing
			if (address + offset <  SerialFlashChip::totalCapacity) {
				if (address + offset + wrlen > SerialFlashChip::totalCapacity) {
					wrlen = SerialFlashChip::totalCapacity - address + offset; 
				}
			} else {
				SerialFlashChip::lastErr = SF_ERROR_DISK_FULL;
				return 0;
			}
		}

		SerialFlash.write(address + offset, buf, wrlen);
		offset += wrlen;
		SerialFlashChip::lastErr = SF_OK;
		return wrlen;
	}
	void seek(uint32_t n) {
		// seeking is not allowed while writing to an auto growing file
		if (length == 0) {
			SerialFlashChip::lastErr = SF_ERROR_WRITING;
			return;
		}
		offset = n;
	}
	uint32_t position() {
		return offset;
	}
	uint32_t size() {
		return length ? length : offset;
	}
	uint32_t available() {
		if (offset >= length) return 0;
		return length - offset;
	}
	void erase();
	void flush() {
	}
	void close();
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
