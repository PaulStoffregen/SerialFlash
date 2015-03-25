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
#include "util/crc16.h"

/* On-chip SerialFlash file allocation data structures:

  uint32_t signature = 0xFA96554C;
  uint16_t maxfiles          
  uint16_t stringssize  // div by 4
  uint16_t hashes[maxfiles]
  struct {
    uint32_t file_begin
    uint32_t file_length
    uint16_t string_index  // div4
  } fileinfo[maxfiles]
  char strings[stringssize]

A 32 bit signature is stored at the beginning of the flash memory.
If 0xFFFFFFFF is seen, the entire chip should be assumed blank.
If any value other than 0xFA96554C is found, a different data format
is stored.  This could should refuse to access the flash.

The next 4 bytes store number of files and size of the strings
section, which allow the position of every other item to be found.
The string section size is the 16 bit integer times 4, which allows
up to 262140 bytes for string data.

An array of 16 bit filename hashes allows for quick linear search
for potentially matching filenames.  A hash value of 0xFFFF indicates
no file is allocated for the remainder of the array.

Following the hashes, and array of 10 byte structs give the location
and length of the file's actual data, and the offset of its filename
in the strings section.

Strings are null terminated.  The remainder of the chip is file data.
*/

#define DEFAULT_MAXFILES      600
#define DEFAULT_STRINGS_SIZE  25560


static uint32_t check_signature(void)
{
	uint32_t sig[2];

	SerialFlash.read(sig, 0, 8);
	if (sig[0] == 0xFA96554C) return sig[1];
	if (sig[0] == 0xFFFFFFFF) {
		sig[0] = 0xFA96554C;
		sig[1] = ((DEFAULT_STRINGS_SIZE/4) << 16) | DEFAULT_MAXFILES;
		SerialFlash.write(sig, 0, 8);
		while (!SerialFlash.ready()) ; // TODO: timeout
		SerialFlash.read(sig, 0, 8);
		if (sig[0] == 0xFA96554C) return sig[1];
	}
	return 0;
}

static uint16_t filename_hash(const char *filename)
{
	// http://isthe.com/chongo/tech/comp/fnv/
	uint32_t hash = 2166136261;
	const char *p;

	for (p=filename; *p; p++) {
		hash ^= *p;
		hash *= 16777619;
	}
	hash %= (uint32_t)0xFFFF;
	return hash;
}

static bool filename_compare(const char *filename, uint32_t straddr)
{
	unsigned int i;
	const char *p;
	char buf[16];

	p = filename;
	while (1) {
		SerialFlash.read(buf, straddr, sizeof(buf));
		straddr += sizeof(buf);
		for (i=0; i < sizeof(buf); i++) {
			if (*p++ != buf[i]) return false;
			if (buf[i] == 0) return true;
		}
	}
}

SerialFlashFile SerialFlashChip::open(const char *filename)
{
	uint32_t maxfiles, straddr;
	uint16_t hash, hashtable[8];
	uint32_t i, n, index=0;
	uint32_t buf[3];
	SerialFlashFile file;

	maxfiles = check_signature();
	if (!maxfiles) return file;
	maxfiles &= 0xFFFF;
	hash = filename_hash(filename);
	while (index < maxfiles) {
		n = 8;
		if (n > maxfiles - index) n = maxfiles - index;
		SerialFlash.read(hashtable, 8 + index * 2, n * 2);
		for (i=0; i < n; i++) {
			if (hashtable[i] == hash) {
				buf[2] = 0;
				SerialFlash.read(buf, 8 + maxfiles * 2 + (index+i) * 10, 10);
				straddr = 8 + maxfiles * 12 + buf[2] * 4;
				if (filename_compare(filename, straddr)) {
					file.address = buf[0];
					file.length = buf[1];
					file.offset = 0;
					return file;
				}
			} else if (hashtable[i] == 0xFFFF) {
				return file;
			}
		}
		index += n;
	}
	return file;
}

static uint32_t find_first_unallocated_file_index(uint32_t maxfiles)
{
	uint16_t hashtable[8];
	uint32_t i, n, index=0;

	do {
		n = 8;
		if (index + n > maxfiles) n = maxfiles - index;
		SerialFlash.read(hashtable, 8 + index * 2, n * 2);
		for (i=0; i < n; i++) {
			if (hashtable[i] == 0xFFFF) return index + i;
		}
		index += n;
	} while (index < maxfiles);
	return 0xFFFFFFFF;
}

static uint32_t string_length(uint32_t addr)
{
	char buf[16];
	const char *p;
	uint32_t len=0;

	while (1) {
		SerialFlash.read(buf, addr, sizeof(buf));
		for (p=buf; p < buf + sizeof(buf); p++) {
			if (*p == 0) return len;
			len++;
		}
		addr += len;
	}
}

//  uint32_t signature = 0xFA96554C;
//  uint16_t maxfiles          
//  uint16_t stringssize  // div by 4
//  uint16_t hashes[maxfiles]
//  struct {
//    uint32_t file_begin
//    uint32_t file_length
//    uint16_t string_index  // div 4
//  } fileinfo[maxfiles]
//  char strings[stringssize]

bool SerialFlashChip::create(const char *filename, uint32_t length, uint32_t align)
{
	uint32_t maxfiles, stringsize;
	uint32_t index, buf[3];
	uint32_t address, straddr, len;
	SerialFlashFile file;

	// first, get the filesystem parameters
	maxfiles = check_signature();
	if (!maxfiles) return false;
	stringsize = (maxfiles & 0xFFFF0000) >> 14;
	maxfiles &= 0xFFFF;
	// TODO: should we check if the file already exists?  Then what?
	// find the first unused slot for this file
	index = find_first_unallocated_file_index(maxfiles);
	if (index >= maxfiles) return false;
	// compute where to store the filename and actual data
	straddr = 8 + maxfiles * 12;
	if (index == 0) {
		address = straddr + stringsize;
	} else {
		buf[2] = 0;
		SerialFlash.read(buf, 8 + maxfiles * 2 + (index-1) * 10, 10);
		address = buf[0] + buf[1];
		straddr += buf[2] * 4;
		straddr += string_length(straddr);
		straddr = (straddr + 3) & 0x0003FFFC;
	}
	if (align > 0) {
		// for files aligned to sectors, adjust addr & len
		address += align - 1;
		address /= align;
		address *= align;
		length += align - 1;
		length /= align;
		length *= align;
	} else {
		// always align every file to a page boundary
		// for predictable write latency and to guarantee
		// write suspend for reading another file can't
		// conflict on the same page (2 files never share
		// a write page).
		address = (address + 255) & 0xFFFFFF00;
	}
	// last check, if enough space exists...
	len = strlen(filename);
	// TODO: check for enough string space for filename
	if (address + length > SerialFlash.capacity()) return false;

	SerialFlash.write(filename, straddr, len+1);
	buf[0] = address;
	buf[1] = length;
	buf[2] = (straddr - (8 + maxfiles * 12)) / 4;
	SerialFlash.write(buf, 8 + maxfiles * 2 + index * 10, 10);
	buf[0] = filename_hash(filename);
	SerialFlash.write(buf, 8 + index * 2, 2);
	while (!SerialFlash.ready()) ;  // TODO: timeout
	return false;
}

bool SerialFlashChip::readdir(char *filename, uint32_t strsize, uint32_t &filesize)
{
	uint32_t maxfiles, index, straddr;
	uint32_t i, n;
	uint32_t buf[2];
	char str[16], *p=filename;

	filename[0] = 0;
	maxfiles = check_signature();
	if (!maxfiles) return false;
	maxfiles &= 0xFFFF; 
	index = dirindex;
	if (index >= maxfiles) return false;
	dirindex = index + 1;

	buf[1] = 0;
	SerialFlash.read(buf, 8 + 4 + maxfiles * 2 + index * 10, 6);
	if (buf[0] == 0xFFFFFFFF) return false;
	filesize = buf[0];
	straddr = 8 + maxfiles * 12 + buf[1] * 4;

	while (strsize) {
		n = strsize;
		if (n > sizeof(str)) n = sizeof(str);
		SerialFlash.read(str, straddr, n);
		for (i=0; i < n; i++) {
			*p++ = str[i];
			if (str[i] == 0) {
				return true;
			}
		}
		strsize -= n;
	}
	*(p - 1) = 0;
	return true;
}


void SerialFlashFile::erase()
{
	uint32_t i, blocksize;

	blocksize = SerialFlash.blockSize();
	if (address & (blocksize - 1)) return; // must begin on a block boundary
	if (length & (blocksize - 1)) return;  // must be exact number of blocks
	for (i=0; i < length; i += blocksize) {
		SerialFlash.eraseBlock(address + i);
	}
}

