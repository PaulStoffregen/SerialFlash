#ifndef SerialFlash_h_
#define SerialFlash_h_

#include <Arduino.h>
#include <SPI.h>

class SerialFlashFile;

class SerialFlashChip
{
public:
	static bool begin();
	static uint32_t capacity();
	static uint32_t blockSize();
	static void read(void *buf, uint32_t addr, uint32_t len);
	static bool ready();
	static void wait();
	static void write(const void *buf, uint32_t addr, uint32_t len);
	static void eraseAll();

	static SerialFlashFile open(const char *filename);
	static bool create(const char *filename, uint32_t length, uint32_t align = 0);
	static bool createErasable(const char *filename, uint32_t length) {
		return create(filename, length, blockSize());
	}
	static void opendir() { dirindex = 0; }
	static bool readdir(char *filename, uint32_t strsize, uint32_t &filesize);
private:
	static uint16_t dirindex;    // current position for readdir()
	static uint8_t fourbytemode; // 0=use 24 bit address, 1=use 32 bit address
	static uint8_t busy;         // 0 = ready, 1 = suspendable busy, 2 = busy for realz
	static uint8_t blocksize;    // erasable uniform block size, 1=4K, 2=8K, etc
	static uint8_t capacityId;   // 3rd byte from 0x9F identification command
};

extern SerialFlashChip SerialFlash;


class SerialFlashFile
{
public:
	SerialFlashFile() : address(0) {
	}
	operator bool() {
		if (address > 0) return true;
		return false;
	}
	uint32_t read(uint8_t *buf, uint32_t rdlen) {
		if (offset + rdlen > length) {
			if (offset >= length) return 0;
			rdlen = length - offset;
		}
		SerialFlash.read(buf, address + offset, rdlen);
		offset += rdlen;
		return rdlen;
	}
	uint32_t write(const void *buf, uint32_t wrlen) {
		if (offset + wrlen > length) {
			if (offset >= length) return 0;
			wrlen = length - offset;
		}
		SerialFlash.write(buf, address + offset, wrlen);
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
	uint32_t address;  // where this file's data begins in the Flash, or zero
	uint32_t length;   // total length of the data in the Flash chip
	uint32_t offset; // current read/write offset in the file
};


#endif
