# SerialFlash

Use SPI Flash memory with a filesystem-like interface.

## Accessing Files

### Open A File

    SerialFlashFile file;
    file = SerialFlash.open("filename.bin");
    if (file) {  // true if the file exists

### Read Data

    char buffer[256];
    file.read(buffer, 256);
    
### File Size & Positon

    file.size();
    file.position()
    file.seek();
    
### Write Data

    file.write(buffer, 256);
    
Several limitations apply to writing.  Only previously unwritten portions of the file may be written.  Files sizes can never change.  Writes may only be done within the file's original size.

    file.erase();  // not yet implemented
    
Only files created for erasing can be erased.  The entire file is erased to all 255 (0xFF) bytes.
    
## Managing Files

### Create New Files

    SerialFlash.create(filename, size);
    SerialFlash.createErasable(filename, size);
    
New files must be created using these funtions.  Each returns true if the file is successfully created, or false if not enough space is available.

Once created, files can never be renamed or deleted.  The file's size can never change.  Writing additional data can NOT grow the size of file.

Files created for erasing automatically increase in size to the nearest number of erasable blocks, resulting in a file that may be 4K to 128K larger than requested.

### Directory Listing

    SerialFlash.opendir();
    SerialFlash.readdir(buffer, buflen, filelen);
    
A list of files stored in the Flash can be accessed with readdir(), which returns true for each file, or false to indicate no more files.

## Full Erase

    SerialFlash.erase();
    
    while (SerialFlash.ready() == false) {
       // wait, 30 seconds to 2 minutes for most chips
    }
