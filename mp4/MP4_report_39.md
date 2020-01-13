# MP4 Report 39

# Table of Contents
[toc]

# Part I.
> Understanding the NachOS File System

## Question 1
> Explain how the NachOS FS manages and finds free block space. On which sector is the information of free block space stored?

Information of free block space is stored in `freeMap`, which is implemented by the data structure `PersistentBitmap`, a subclass of `Bitmap`.

### Initialization
1. Create a free map and a header for it.
   ```cpp
   PersistentBitmap *freeMap = new PersistentBitmap(NumSectors);
   FileHeader *mapHdr = new FileHeader;
   ```
2. Mark the free map sector as used.
   ```cpp
   freeMap->Mark(FreeMapSector);	    
   ```
3. Allocate data space for free map and write the header back to disk.
   ```cpp
   ASSERT(mapHdr->Allocate(freeMap, FreeMapFileSize));
   mapHdr->WriteBack(FreeMapSector);
   ```
4. The free map file is opened and written back with the newly-created free map.
   ```cpp
   freeMapFile = new OpenFile(FreeMapSector);
   freeMap->WriteBack(freeMapFile);
   ```

### Create Files
1. Load free map from free map file.
   ```cpp
   freeMap = new PersistentBitmap(freeMapFile,NumSectors);
   ```
2. Find the file's sector number and perform validity checking:
   1. Check if there are free sectors for the file header.
   2. Check if there is space for the file header in the directory.
   3. Check if allocating space for the file succeeded.
   ```cpp=
   sector = freeMap->FindAndSet();
   if (sector == -1)
       success = FALSE;
   else if (!directory->Add(name, sector))
            success = FALSE;
   else {
      hdr = new FileHeader;
      if (!hdr->Allocate(freeMap, initialSize))
          success = FALSE;
   }
   ```
3. If all went well, write back to the disk the header file, the directory file, and free map file.
   ```cpp=
   hdr->WriteBack(sector); 		
   directory->WriteBack(directoryFile);
   freeMap->WriteBack(freeMapFile);
   ```

### Remove Files
1. Load the directory and find the sector of the file.
```cpp=
directory = new Directory(NumDirEntries);
directory->FetchFrom(directoryFile);
sector = directory->Find(name);
```
2. Load the free map and the file header from the sector.
```cpp=
fileHdr = new FileHeader;
fileHdr->FetchFrom(sector);

freeMap = new PersistentBitmap(freeMapFile,NumSectors);
```
3. Deallocate the space for the file, clear the sector in free map, and
   remove the file from the directory.
```cpp=
fileHdr->Deallocate(freeMap);
freeMap->Clear(sector);
directory->Remove(name);
```
4. Write back the free map and directory structure.
```cpp=
freeMap->WriteBack(freeMapFile);
directory->WriteBack(directoryFile);
```

## Question 2
> What is the maximum disk size that can be handled by the current implementation? Explain why.

The free map is maintained as a file in the disk.\
Due to the fact that free space management is implemented in **bit vector**, the maximum disk size is bounded by the number of bits in a file.\
By the answer to [question 5](#Question-5), the maximum size of a file is 3840 bytes = 3840 $\times$ 8 bits, representing the number of sectors.\
Each sector is of size 128 bytes, so there are **3840 $\times$ 8 $\times$ 128 bytes in total = 3840 KB.**

## Question 3
> Explain how the NachOS FS manages the directory data structure. Which sector on the raw disk is the information stored?

### Initialization
1. Create a directory with a constant number of entries. Also create a directory header..
```cpp=
#define NumDirEntries 10
Directory *directory = new Directory(NumDirEntries);
FileHeader *dirHdr = new FileHeader;
```
2. Mark the directory sector as used.
   ```cpp
   freeMap->Mark(DirectorySector);
   ```
3. Allocate data space for the directory and write the header back to disk.
   ```cpp
   ASSERT(dirHdr->Allocate(freeMap, DirectoryFileSize));
   dirHdr->WriteBack(DirectorySector);
   ```
4. The directory file is opened and written back with the newly-created directory.
   ```cpp
   directoryFile = new OpenFile(DirectorySector);
   directory->WriteBack(directoryFile);
   ```

### Create Files
1. Load the directory from the directory file.
```cpp=
directory = new Directory(NumDirEntries);
directory->FetchFrom(directoryFile);
```
2. Perform validity checking and write back as described in [question 1](#Create-Files).\
See [below](#DirectoryAdd) for elaboration on `Directory::Add`.

### Remove Files
1. Load the directory from disk and find the file header in directory.
```cpp=
directory = new Directory(NumDirEntries);
directory->FetchFrom(directoryFile);
sector = directory->Find(name);
```
2. If the file header is not found, return `FALSE`.
```cpp=
if (sector == -1) {
   delete directory;
   return FALSE;
}
```
3. Otherwise, remove the file header from the directory and write the directory file back to disk.
```cpp=
directory->Remove(name);
directory->WriteBack(directoryFile);
```

### Open Files
1. Load the directory from disk and find the file header according to the file name.
```cpp=
Directory *directory = new Directory(NumDirEntries);
directory->FetchFrom(directoryFile);
int sector = directory->Find(name);
```
2. If the file header is found, open the file and return it. Otherwise, return `False`.
```cpp=
OpenFile *openFile = NULL;
if (sector >= 0) openFile = new OpenFile(sector);
delete directory;
return openFile;
```

## Question 4
> Explain what information is stored in an inode, and use a figure to illustrate the disk allocation scheme of current implementation.

* The file size, the number of sectors used, and the sector number table.
```cpp=
int numBytes;
int numSectors;
int dataSectors[NumDirect];
```
* Disk Allocation Scheme
![](https://i.imgur.com/n7kjHZd.png)


## Question 5
> Why is a file limited to just under 4KB in the current implementation?

1. A file header is stored in one single sector, which is of size 128 bytes.
```cpp
const int SectorSize = 128; // Defined in disk.h
```
2. A file header consists of 2 integers and data sector addresses of integer, so 30 direct addresses are stored.
```cpp
// Defined in filehdr.h
#define NumDirect ((SectorSize - 2 * sizeof(int)) / sizeof(int))
```
3. The maximum file size contains at most 30 data sectors, which is **3840 bytes and just under 4 KB.**
```cpp
// Defined in filehdr.h
#define MaxFileSize (NumDirect * SectorSize)
```

# Part II.
> Modify the file system code to support file I/O system call and larger file size.

## Requirement 1
> Combine your MP1 file system call interface with NachOS FS.

### System Calls

#### `SysCreate`
Call the corresponding create function of the file system.
```cpp=
int SysCreate(char *filename, int initialSize)
{
	return kernel->fileSystem->Create(filename, initialSize);
}
```

#### `SysOpen`
Call the corresponding open function of the file system.
```cpp=
OpenFileId SysOpen(char *filename)
{
	return kernel->fileSystem->OpenByName(filename);
}
```

#### `SysRead`
Access the currenlty-opened file and call its read function.
```cpp=
int SysRead(char *buffer, int size, OpenFileId id)
{
	return kernel->fileSystem->currentFile->Read(buffer, size);
}
```

#### `SysWrite`
Access the currenlty-opened file and call its write function.
```cpp=
int SysWrite(char *buffer, int size, OpenFileId id)
{
	return kernel->fileSystem->currentFile->Write(buffer, size);
}
```

#### `SysClose`
Call the corresponding close function of the file system.
```cpp=
int SysClose(OpenFileId id)
{
	return kernel->fileSystem->Close(id);
}
```

### Exception Handler

#### `SC_Create`
1. Get the filename from the memory and the file size from the register.
```cpp=
val = kernel->machine->ReadRegister(4);
char *filename = &(kernel->machine->mainMemory[val]);
int initialSize = kernel->machine->ReadRegister(5);
```
2. Make a "Create" system call and write the status to register.
```cpp=
status = SysCreate(filename, initialSize);
kernel->machine->WriteRegister(2, (int) status);
```
3. Update the program counter register.
```cpp=
kernel->machine->WriteRegister(PrevPCReg, kernel->machine->ReadRegister(PCReg));
kernel->machine->WriteRegister(PCReg, kernel->machine->ReadRegister(PCReg) + 4);
kernel->machine->WriteRegister(NextPCReg, kernel->machine->ReadRegister(PCReg)+4);
```

#### `SC_Open`
1. The way of accessing the filename and updating the PC register is identical to that of [`SC_Create`](#`SC_Create`).
2. Make a "Open" system call and write the status to the register.
```cpp=
OpenFileId status = SysOpen(filename);
kernel->machine->WriteRegister(2, (OpenFileId) status);
```

#### `SC_Read`
1. Read the buffer, the buffer size, and the open file ID from the memory and registers.
```cpp=
val = kernel->machine->ReadRegister(4);
char *buffer = &(kernel->machine->mainMemory[val]);
int size = kernel->machine->ReadRegister(5);
OpenFileId id = kernel->machine->ReadRegister(6);
```
2. Make a "Read" system call and write the status to the register.
```cpp=
status = SysRead(buffer, size, id);
kernel->machine->WriteRegister(2, (int) status);
```
3. The way of updating the PC register is identical to that of [`SC_Create`](#`SC_Create`).

#### `SC_Write`
1. The way of reading the buffer, the buffer size and the open file ID is identical to that of [`SC_Read`](#`SC_Read`).
2. Make a "Write" system call and write the status to the register.
```cpp=
status = SysWrite(buffer, size, id);
kernel->machine->WriteRegister(2, (int) status);
```
3. The way of updating the PC register is identical to that of [`SC_Create`](#`SC_Create`).

#### `SC_Close`
1. Read the open file ID from memory and make a "Close" system call before writing the status to the register.
2. The way of updating the PC register is identical to that of [`SC_Create`](#`SC_Create`).

## Requirement 2
> Enhance the FS to support up to 32KB file size.

See the [bonus section](#Bonus).


# Part III.
> Modify the file system code to support subdirectory.

## Requirement 1
> Implement the subdirectory structure.

### `Directory::Add`
1. New member `isdir` is added to class `DirectoryEntry` as a flag.
2. The flag is set according to the given parameter.
   ```cpp=
   if(isdirectory)
       table[i].isdir = TRUE;
   else
       table[i].isdir = FALSE;
   ```

### `FileSystem::CreateDirectory` and `FileSystem::Create`
1. In addition to declaring the directory, the free map, and the file header, copy the path to a new varaible to avoid modification on the original one.
   ```cpp
   char * name = new char[256];
   strcpy(name, absolutePath);
   ```
2. Parse path: Repeat the following steps until the end.
   1. Find the directory and stop repetition if not found.
	  ```cpp
	  if( (sector = directory->Find(lastPath)) == -1 ) break;
	  ```
   2. Load the directory from the given sector.
	  ```cpp=
	  tmp_dir = new OpenFile(sector);
	  directory->FetchFrom(tmp_dir);
	  ```
   3. Extract the directory name from the path.
      ```cpp=
      lastPath = path;
      path = strtok(NULL, "/");
      ```
3. If the directory does not already exist, create a new one, load the free map, and set spaces for the directory.
   ```cpp=
    Directory *new_dir = new Directory(NumDirEntries);
    freeMap = new PersistentBitmap(freeMapFile,NumSectors);
    sector = freeMap->FindAndSet();
   ```
4. Validity checking:
   ```cpp=
   if (sector == -1) 		
       success = FALSE;  // No free blocks for file header 
   else if (!directory->Add(lastPath, sector, 1))
       success = FALSE;  // No space in the parent directory
   else {
       hdr = new FileHeader;
       if (!hdr->Allocate(freeMap, DirectoryFileSize))
           success = FALSE;  // No space on the disk for the data
       else {
           // Success
       }
   }
   ```
5. If every thing worked, write back all changes back to disk.
   ```cpp=
   if(tmp_dir == NULL)  // When the directory is under the root directory
       directory->WriteBack(directoryFile);
   else
       directory->WriteBack(tmp_dir);

   freeMap->WriteBack(freeMapFile);
   OpenFile *newDirFile = new OpenFile(sector);
   new_dir->WriteBack(newDirFile);
   ```
6. `FileSystem::Create` is identical to [`FileSystem::CreateDirectory`](#`FileSystem::CreateDirectory`) except it is specified that a file rather than a directory is added.
   ```cpp
   else if (!directory->Add(lastPath, sector, 0)) // isdir = 0
   ```

### `FileSystem::Open` and `FileSystem::OpenByName`
1. Path parsing is identical to that of [`FileSystem::CreateDirectory`](#`FileSystem::CreateDirectory`).
2. After finding a valid sector number, the file pointer is created and returned.
   ```cpp=
   sector = directory->Find(lastPath);
   if (sector >= 0) currentFile = new OpenFile(sector);
   return currentFile;
   ```
3. `FileSystem::OpenByName` is identical to `FileSystem::Open` except it returns the sector number.
   ```cpp=
   sector = directory->Find(lastPath);
   return sector;
   ```

### `FileSystem::RecursiveList` and `Directory::RecursiveList`
1. Path parsing is done in `FileSystem::RecursiveList`, which is identical to that of [`FileSystem::CreateDirectory`](#`FileSystem::CreateDirectory`).
2. In `Directory::RecursiveList`, loop through the table and recursively list the subdirectories.
   ```cpp=
   for(int i = 0; i < tableSize; i++){
       if (table[i].inUse) {
           print(table[i].name)

           // Recursive call part
           if (table[i].isdir) {
               subdir->RecursiveList();
           }
       }
   }
   ```
3. A prefix is printed to indicate the current object is a directory.
   ```cpp=
   if(table[i].isdir)
       printf("D ");
   else
       printf("  ");
   ```
4. Open, load, and make a recursive call if the current object is a subdirectory.
   ```cpp=
   OpenFile *subdirfile = new OpenFile(table[i].sector);
   Directory *subdir = new Directory(NumDirEntries);
   subdir->FetchFrom(subdirfile);
   subdir->RecursiveList();
   ```
5. `FileSystem::List` is identical to `FileSystem::RecursiveList`, except it calls `Directory::List` rather than `Directory::RecursiveList`.
6. `Directory::List` is basically `Directory::RecursiveList` without the recursive call part.

### `FileSystem::Remove`
1. Initialization and path parsing are identical to that of [`FileSystem::CreateDirectory`](#FileSystemCreateDirectory-and-FileSystemCreate).
2. Removing includes:
   1. Removing data blocks from the file header.
   2. Removing header blocks from the free map.
   3. Removing the directory.
   ```cpp=
   fileHdr->Deallocate(freeMap);
   freeMap->Clear(sector);
   directory->Remove(lastPath);
   ```
3. Write back all the changes to the disk.
```cpp=
freeMap->WriteBack(freeMapFile);
directory->WriteBack(tmp_dir);  // Assume tmp_dir exists
```

## Requirement 2
> Support up to 64 files/subdirectories per directory.

Change the number of entries from 10 to 64. There are 2 places need to be changed:
```cpp=
// filesys/filesys.cc
#define NumDirEntries 64
```
```cpp=
// filesys/directory.h
#define NumDirEntries 64
```

# Bonus

## Bonus 1 + 2
> Enhance the NachOS to support even larger file size with multi-level header files.

A segment of code and a function are added to support large files.

### Multi-level Indexing
1. Multi-level indexing is implemented by recursively allocating spaces for header files.
   ```clike=
   function Allocate(filesize)
   {
       if (filesize > LEVEL4SIZE) {
           Allocate(LEVEL4SIZE)
       }
       else if (filesize > LEVEL3SIZE) {
           Allocate(LEVEL3SIZE)
       }
       else if (filesize > LEVEL2SIZE) {
           Allocate(LEVEL2SIZE)
       }
       else {
           // Level 1 size for level 1 table
       }
   }
   ```
2. When allocating a $n^{th}$ level file header, we allocate multiple $(n-1)^{th}$ level file headers until the file size is covered.
   ```cpp=
   // We take allocating a 4-level file header as an example.
   int index = 0;
   int currentSize = fileSize;
   while(currentSize > 0){
       // Find space in the free map and set it as used.
       dataSectors[index] = freeMap->FindAndSet();
   
       // Create a 3rd level file header and allocate its space.
       FileHeader *nextlevelhdr = new FileHeader;
       if(currentSize > LEVEL3SIZE)
           nextlevelhdr->Allocate(freeMap, LEVEL3SIZE);
       else
           nextlevelhdr->Allocate(freeMap, currentSize);
   
       currentSize -= LEVEL3SIZE;
   
       // Write back the header file to disk.
       nextlevelhdr->WriteBack(dataSectors[index]);
       index++;
       delete nextlevelhdr;
   }
   ```
3. For the base case, we simply find space in the free map and set it as used.
   ```cpp=
   for (int i = 0; i < numSectors; i++) {
       dataSectors[i] = freeMap->FindAndSet();
   }
   ```

### `ByteToSector`
Given the offset of a piece of data in a file, functions like `SynchDisk::ReadSector` needs the sector number. `ByteToSector` takes the offset as input and outputs the sector in which the piece of data resides.

1. The sector number is found by recursively finding the file headers pointing to the sector.
   ```clike=
   function ByteToSector(offset)
   {
       if (numBytes > LEVEL4SIZE) {
           ByteToSector(offset - LEVEL4SIZE * index);
       }
       else if (numBytes > LEVEL3SIZE) {
           ByteToSector(offset - LEVEL3SIZE * index);
       }
       else if (numBytes > LEVEL2SIZE) {
           ByteToSector(offset - LEVEL2SIZE * index);
       }
       else {
           // Base case
       }
   }
   ```
2. In the $n^{th}$ level, the offset for $(n-1)^{th}$ is calculated, which is used to recursively find the sector number.
   ```cpp=
   int index = divRoundDown(offset, LEVEL4SIZE);
   
   FileHeader *nextlevelhdr = new FileHeader;
   nextlevelhdr->FetchFrom(dataSectors[index]);
   int reval = nextlevelhdr->ByteToSector(offset - LEVEL4SIZE * index);
   delete nextlevelhdr;
   
   return reval;
   ```
3. Base case:
   ```cpp
   return (dataSectors[offset / SectorSize]);
   ```