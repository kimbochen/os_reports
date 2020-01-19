// filesys.cc 
//	Routines to manage the overall operation of the file system.
//	Implements routines to map from textual file names to files.
//
//	Each file in the file system has:
//	   A file header, stored in a sector on disk 
//		(the size of the file header data structure is arranged
//		to be precisely the size of 1 disk sector)
//	   A number of data blocks
//	   An entry in the file system directory
//
// 	The file system consists of several data structures:
//	   A bitmap of free disk sectors (cf. bitmap.h)
//	   A directory of file names and file headers
//
//      Both the bitmap and the directory are represented as normal
//	files.  Their file headers are located in specific sectors
//	(sector 0 and sector 1), so that the file system can find them 
//	on bootup.
//
//	The file system assumes that the bitmap and directory files are
//	kept "open" continuously while Nachos is running.
//
//	For those operations (such as Create, Remove) that modify the
//	directory and/or bitmap, if the operation succeeds, the changes
//	are written immediately back to disk (the two files are kept
//	open during all this time).  If the operation fails, and we have
//	modified part of the directory and/or bitmap, we simply discard
//	the changed version, without writing it back to disk.
//
// 	Our implementation at this point has the following restrictions:
//
//	   there is no synchronization for concurrent accesses
//	   files have a fixed size, set when the file is created
//	   files cannot be bigger than about 3KB in size
//	   there is no hierarchical directory structure, and only a limited
//	     number of files can be added to the system
//	   there is no attempt to make the system robust to failures
//	    (if Nachos exits in the middle of an operation that modifies
//	    the file system, it may corrupt the disk)
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.
#ifndef FILESYS_STUB

#include "copyright.h"
#include "debug.h"
#include "disk.h"
#include "pbitmap.h"
#include "directory.h"
#include "filehdr.h"
#include "filesys.h"

#include "string.h"

// Sectors containing the file headers for the bitmap of free sectors,
// and the directory of files.  These file headers are placed in well-known 
// sectors, so that they can be located on boot-up.
#define FreeMapSector 		0
#define DirectorySector 	1

// Initial file sizes for the bitmap and directory; until the file system
// supports extensible files, the directory size sets the maximum number 
// of files that can be loaded onto the disk.
#define FreeMapFileSize 	(NumSectors / BitsInByte)
#define NumDirEntries 		64
#define DirectoryFileSize 	(sizeof(DirectoryEntry) * NumDirEntries)

//----------------------------------------------------------------------
// FileSystem::FileSystem
// 	Initialize the file system.  If format = TRUE, the disk has
//	nothing on it, and we need to initialize the disk to contain
//	an empty directory, and a bitmap of free sectors (with almost but
//	not all of the sectors marked as free).  
//
//	If format = FALSE, we just have to open the files
//	representing the bitmap and the directory.
//
//	"format" -- should we initialize the disk?
//----------------------------------------------------------------------

FileSystem::FileSystem(bool format)
{ 
    DEBUG(dbgFile, "Initializing the file system.");
    if (format) {
        PersistentBitmap *freeMap = new PersistentBitmap(NumSectors);
        Directory *directory = new Directory(NumDirEntries);
		FileHeader *mapHdr = new FileHeader;
		FileHeader *dirHdr = new FileHeader;

        DEBUG(dbgFile, "Formatting the file system.");

		// First, allocate space for FileHeaders for the directory and bitmap
		// (make sure no one else grabs these!)
		freeMap->Mark(FreeMapSector);	    
		freeMap->Mark(DirectorySector);

		// Second, allocate space for the data blocks containing the contents
		// of the directory and bitmap files.  There better be enough space!

		ASSERT(mapHdr->Allocate(freeMap, FreeMapFileSize));
		ASSERT(dirHdr->Allocate(freeMap, DirectoryFileSize));

		// Flush the bitmap and directory FileHeaders back to disk
		// We need to do this before we can "Open" the file, since open
		// reads the file header off of disk (and currently the disk has garbage
		// on it!).

        DEBUG(dbgFile, "Writing headers back to disk.");
		mapHdr->WriteBack(FreeMapSector);    
		dirHdr->WriteBack(DirectorySector);

		// OK to open the bitmap and directory files now
		// The file system operations assume these two files are left open
		// while Nachos is running.

        freeMapFile = new OpenFile(FreeMapSector);
        directoryFile = new OpenFile(DirectorySector);
     
		// Once we have the files "open", we can write the initial version
		// of each file back to disk.  The directory at this point is completely
		// empty; but the bitmap has been changed to reflect the fact that
		// sectors on the disk have been allocated for the file headers and
		// to hold the file data for the directory and bitmap.

        DEBUG(dbgFile, "Writing bitmap and directory back to disk.");
		freeMap->WriteBack(freeMapFile);	 // flush changes to disk
		directory->WriteBack(directoryFile);

		if (debug->IsEnabled('f')) {
			freeMap->Print();
			directory->Print();
        }
        delete freeMap; 
		delete directory; 
		delete mapHdr; 
		delete dirHdr;
    } else {
		// if we are not formatting the disk, just open the files representing
		// the bitmap and directory; these are left open while Nachos is running
        freeMapFile = new OpenFile(FreeMapSector);
        directoryFile = new OpenFile(DirectorySector);
    }
}

//----------------------------------------------------------------------
// MP4 mod tag
// FileSystem::~FileSystem
//----------------------------------------------------------------------
FileSystem::~FileSystem()
{
	delete freeMapFile;
	delete directoryFile;
}

//----------------------------------------------------------------------
// FileSystem::Create
// 	Create a file in the Nachos file system (similar to UNIX create).
//	Since we can't increase the size of files dynamically, we have
//	to give Create the initial size of the file.
//
//	The steps to create a file are:
//	  Make sure the file doesn't already exist
//        Allocate a sector for the file header
// 	  Allocate space on disk for the data blocks for the file
//	  Add the name to the directory
//	  Store the new file header on disk 
//	  Flush the changes to the bitmap and the directory back to disk
//
//	Return TRUE if everything goes ok, otherwise, return FALSE.
//
// 	Create fails if:
//   		file is already in directory
//	 	no free space for file header
//	 	no free entry for file in directory
//	 	no free space for data blocks for the file 
//
// 	Note that this implementation assumes there is no concurrent access
//	to the file system!
//
//	"name" -- name of file to be created
//	"initialSize" -- size of file to be created
//----------------------------------------------------------------------

bool
FileSystem::Create(char *absolutePath, int initialSize)
{
    Directory *directory;
    PersistentBitmap *freeMap;
    FileHeader *hdr;
    int sector;
    bool success;
    char * name = new char[256];
    strcpy(name, absolutePath);

    DEBUG(dbgFile, "Creating file " << name << " size " << initialSize);

    directory = new Directory(NumDirEntries);
    directory->FetchFrom(directoryFile);

    // MP4I
    // Pharse path
    OpenFile *tmp_dir = NULL;
    char *path = strtok(name, "/");
    char *lastPath = path;
    path = strtok(NULL, "/");

    while(path != NULL){// traverse through directory
        if( (sector = directory->Find(lastPath)) == -1 )// name not found in directory
            break;
        
        // Open the directory and go through
        if(tmp_dir != NULL){ 
            delete tmp_dir;
            tmp_dir = NULL;
        }
        tmp_dir = new OpenFile(sector);
        directory->FetchFrom(tmp_dir);
        lastPath = path;
        path = strtok(NULL, "/");
    }

    if (directory->Find(lastPath) != -1)
      success = FALSE;			// file is already in directory
    else {	
        freeMap = new PersistentBitmap(freeMapFile,NumSectors);
        sector = freeMap->FindAndSet();	// find a sector to hold the file header
    	if (sector == -1){ 		
            success = FALSE;		// no free block for file header 
            cout << "No free space\n";
        }
        else if (!directory->Add(lastPath, sector, 0)){
            success = FALSE;	// no space in directory
            cout << "Can't create in directory\n";
        }
        else {
                hdr = new FileHeader;
            if (!hdr->Allocate(freeMap, initialSize))
                    success = FALSE;	// no space on disk for data
            else {	
                success = TRUE;
            // everthing worked, flush all changes back to disk
                    hdr->WriteBack(sector); 		
                    if(tmp_dir == NULL)
                        directory->WriteBack(directoryFile);
                    else
                        directory->WriteBack(tmp_dir);
                    freeMap->WriteBack(freeMapFile);
            }
                delete hdr;
        }
        delete freeMap;
    }
    delete directory;
    delete [] name;
    return success;
}

//----------------------------------------------------------------------
// FileSystem::CreateDirectory
// 	Create a directory.  
//
//	"name" -- the directory path name to be created
//----------------------------------------------------------------------

bool 
FileSystem::CreateDirectory(char *absolutePath)
{
    Directory *directory;
    PersistentBitmap *freeMap;
    FileHeader *hdr;
    int sector;
    bool success;
    char * name = new char[256];
    strcpy(name, absolutePath);

    DEBUG(dbgFile, "Creating directory " << name );

    directory = new Directory(NumDirEntries);
    directory->FetchFrom(directoryFile);

    // MP4I
    // Pharse path
    OpenFile *tmp_dir = NULL;
    char *path = strtok(name, "/");
    char *lastPath = path;
    path = strtok(NULL, "/");

    while(path != NULL){// traverse through directory
        if( (sector = directory->Find(lastPath)) == -1 )// name not found in directory
            break;
        
        // Open the directory and go through
        if(tmp_dir != NULL){ 
            delete tmp_dir;
            tmp_dir = NULL;
        }
        tmp_dir = new OpenFile(sector);
        directory->FetchFrom(tmp_dir);
        lastPath = path;
        path = strtok(NULL, "/");
    }

    if (directory->Find(lastPath) != -1)
      success = FALSE;			// file is already in directory
    else {
        Directory *new_dir = new Directory(NumDirEntries);
        freeMap = new PersistentBitmap(freeMapFile,NumSectors);
        sector = freeMap->FindAndSet();	// find a sector to hold the file header
    	if (sector == -1) 		
            success = FALSE;		// no free block for file header 
        else if (!directory->Add(lastPath, sector, 1))
            success = FALSE;	// no space in directory
        else {
                hdr = new FileHeader;
            if (!hdr->Allocate(freeMap, DirectoryFileSize))
                    success = FALSE;	// no space on disk for data
            else {	
                success = TRUE;
            // everthing worked, flush all changes back to disk
                    hdr->WriteBack(sector); 		
                    if(tmp_dir == NULL)
                        directory->WriteBack(directoryFile);
                    else
                        directory->WriteBack(tmp_dir);
                    freeMap->WriteBack(freeMapFile);
                    OpenFile *newDirFile = new OpenFile(sector);
                    new_dir->WriteBack(newDirFile);
                    delete newDirFile;
            }
                delete hdr;
        }
        delete new_dir;
        delete freeMap;
    }
    delete directory;
    delete [] name;
    return success;
}


//----------------------------------------------------------------------
// FileSystem::Open
// 	Open a file for reading and writing.  
//	To open a file:
//	  Find the location of the file's header, using the directory 
//	  Bring the header into memory
//
//	"name" -- the text name of the file to be opened
//----------------------------------------------------------------------

OpenFile *
FileSystem::Open(char *absolutePath)
{
    Directory *directory = new Directory(NumDirEntries);
    int sector;
    char * name = new char[256];
    strcpy(name, absolutePath);

    DEBUG(dbgFile, "Opening file" << name);
    /*
    directory->FetchFrom(directoryFile);
    sector = directory->Find(name); 
    if (sector >= 0) 		
	    openFile = new OpenFile(sector);	// name was found in directory 
    delete directory;
    return openFile;				// return NULL if not found
    */
    // MP4I
    
    directory->FetchFrom(directoryFile);
    // Pharse path
    OpenFile *tmp_dir = NULL;
    char *path = strtok(name, "/");
    char *lastPath = path;
    path = strtok(NULL, "/");

    while(path != NULL){// traverse through directory
        if( (sector = directory->Find(lastPath)) == -1)// name not found in directory
            break;
        
        // Open the directory and go through
        if(tmp_dir != NULL){ 
            delete tmp_dir;
            tmp_dir = NULL;
        }
        tmp_dir = new OpenFile(sector);
        directory->FetchFrom(tmp_dir);
        lastPath = path;
        path = strtok(NULL, "/");
    }
    // file name is stored in lastPath
    sector = directory->Find(lastPath);
    if(sector >= 0)
        currentFile = new OpenFile(sector);
    delete directory;
    if(tmp_dir != NULL) 
        delete tmp_dir;
    delete [] name;
    return currentFile;

}

// MP4I
//----------------------------------------------------------------------
// FileSystem::OpenByName
// Open the file by name and return openfileid
//----------------------------------------------------------------------

OpenFileId
FileSystem::OpenByName(char *absolutePath)
{
    Directory *directory = new Directory(NumDirEntries);
    int sector;
    char * name = new char[256];
    strcpy(name, absolutePath);

    DEBUG(dbgFile, "Opening file" << name);
    
    directory->FetchFrom(directoryFile);

    // Pharse path
    OpenFile *tmp_dir = NULL;
    char *path = strtok(name, "/");
    char *lastPath = path;
    path = strtok(NULL, "/");

    while(path != NULL){// traverse through directory
        if( (sector = directory->Find(lastPath)) == -1)// name not found in directory
            break;
        
        // Open the directory and go through
        if(tmp_dir != NULL){ 
            delete tmp_dir;
            tmp_dir = NULL;
        }
        tmp_dir = new OpenFile(sector);
        directory->FetchFrom(tmp_dir);
        lastPath = path;
        path = strtok(NULL, "/");
    }
    // file name is stored in lastPath
    sector = directory->Find(lastPath);
    if(sector >= 0)
        currentFile = new OpenFile(sector);
    delete directory;
    if(tmp_dir != NULL) 
        delete tmp_dir;
    delete [] name;
    return sector;
}

//----------------------------------------------------------------------
// FileSystem::Close
// Close the file by id
//----------------------------------------------------------------------

bool
FileSystem::Close(OpenFileId id)
{
    delete currentFile;
    currentFile = NULL;
    return 1;
}

//----------------------------------------------------------------------
// FileSystem::Remove
// 	Delete a file from the file system.  This requires:
//	    Remove it from the directory
//	    Delete the space for its header
//	    Delete the space for its data blocks
//	    Write changes to directory, bitmap back to disk
//
//	Return TRUE if the file was deleted, FALSE if the file wasn't
//	in the file system.
//
//	"name" -- the text name of the file to be removed
//----------------------------------------------------------------------

bool
FileSystem::Remove(char *absolutePath)
{ 
    Directory *directory;
    PersistentBitmap *freeMap;
    FileHeader *fileHdr;
    int sector;
    char * name = new char[256];
    strcpy(name, absolutePath);
    
    directory = new Directory(NumDirEntries);
    directory->FetchFrom(directoryFile);

    // MP4I
    // Pharse path
    OpenFile *tmp_dir = NULL;
    char *path = strtok(name, "/");
    char *lastPath = path;
    path = strtok(NULL, "/");

    while(path != NULL){// traverse through directory
        if( (sector = directory->Find(lastPath)) == -1)// name not found in directory
            break;
        
        // Open the directory and go through
        if(tmp_dir != NULL){ 
            delete tmp_dir;
            tmp_dir = NULL;
        }
        tmp_dir = new OpenFile(sector);
        directory->FetchFrom(tmp_dir);
        lastPath = path;
        path = strtok(NULL, "/");
    }
    
    // remove
    sector = directory->Find(lastPath);
    if (sector == -1) {
       delete directory;
       return FALSE;			 // file not found 
    }
    fileHdr = new FileHeader;
    fileHdr->FetchFrom(sector);

    freeMap = new PersistentBitmap(freeMapFile,NumSectors);

    fileHdr->Deallocate(freeMap);  		// remove data blocks
    freeMap->Clear(sector);			// remove header block
    directory->Remove(lastPath);

    freeMap->WriteBack(freeMapFile);		// flush to disk
    if(tmp_dir != NULL){
        directory->WriteBack(tmp_dir); // flush to disk
        delete tmp_dir;
    }
    else{
        directory->WriteBack(directoryFile); // flush to disk
    }
    delete fileHdr;
    delete directory;
    delete freeMap;
    delete [] name;
    return TRUE;
} 

// MP4I
//----------------------------------------------------------------------
// FileSystem::RecursiveRemove
// 	Recursively remove files and directory.
//----------------------------------------------------------------------

bool
FileSystem::RecursiveRemove(char *name)
{
    return FALSE;
}


//----------------------------------------------------------------------
// FileSystem::List
// 	List all the files in the file system directory.
//----------------------------------------------------------------------

void
FileSystem::List(char *absolutePath)
{
    Directory *directory = new Directory(NumDirEntries);

    directory->FetchFrom(directoryFile);
    char *name = new char[256];
    strcpy(name, absolutePath);
    // MP4I

    // Pharse path
    OpenFile *tmp_dir = NULL;
    char *path = strtok(name, "/");
    char *lastPath = path;
    path = strtok(NULL, "/");
    int sector;

    while(path != NULL){// traverse through directory
        if( (sector = directory->Find(lastPath)) == -1)// name not found in directory
            break;
        
        // Open the directory and go through
        if(tmp_dir != NULL){ 
            delete tmp_dir;
            tmp_dir = NULL;
        }
        tmp_dir = new OpenFile(sector);
        directory->FetchFrom(tmp_dir);
        lastPath = path;
        path = strtok(NULL, "/");
    }
    
    if(lastPath != NULL  && (sector = directory->Find(lastPath)) != -1){// case of /t0  lastPath:t0 Path:NULL but directory point to root
        if(tmp_dir != NULL){
            delete tmp_dir;
            tmp_dir = NULL;
        }
        tmp_dir = new OpenFile(sector);
        directory->FetchFrom(tmp_dir);
    }

    directory->List();
    if(tmp_dir != NULL)
        delete tmp_dir;
    delete [] name;
    delete directory;
}

// MP4I
//----------------------------------------------------------------------
// FileSystem::RecursiveList
// 	List all the files in the file system directory.
//----------------------------------------------------------------------

void
FileSystem::RecursiveList(char *absolutePath)
{
    Directory *directory = new Directory(NumDirEntries);

    directory->FetchFrom(directoryFile);

    // MP4I
    
    char *name = new char[256];
    strcpy(name, absolutePath);
    // Pharse path
    OpenFile *tmp_dir = NULL;
    char *path = strtok(name, "/");
    char *lastPath = path;
    path = strtok(NULL, "/");
    int sector;

    while(path != NULL){// traverse through directory
        if( (sector = directory->Find(lastPath)) == -1)// name not found in directory
            break;
        
        // Open the directory and go through
        if(tmp_dir != NULL){ 
            delete tmp_dir;
            tmp_dir = NULL;
        }
        tmp_dir = new OpenFile(sector);
        directory->FetchFrom(tmp_dir);
        lastPath = path;
        path = strtok(NULL, "/");
    }
   
    if(lastPath != NULL  && (sector = directory->Find(lastPath)) != -1){// case of /t0  lastPath:t0 Path:NULL but directory point to root
        if(tmp_dir != NULL){
            delete tmp_dir;
            tmp_dir = NULL;
        }
        tmp_dir = new OpenFile(sector);
        directory->FetchFrom(tmp_dir);
    }

    directory->RecursiveList();
    if(tmp_dir != NULL)
        delete tmp_dir;
    delete [] name;
    delete directory;
}


//----------------------------------------------------------------------
// FileSystem::Print
// 	Print everything about the file system:
//	  the contents of the bitmap
//	  the contents of the directory
//	  for each file in the directory,
//	      the contents of the file header
//	      the data in the file
//----------------------------------------------------------------------

void
FileSystem::Print()
{
    FileHeader *bitHdr = new FileHeader;
    FileHeader *dirHdr = new FileHeader;
    PersistentBitmap *freeMap = new PersistentBitmap(freeMapFile,NumSectors);
    Directory *directory = new Directory(NumDirEntries);

    printf("Bit map file header:\n");
    bitHdr->FetchFrom(FreeMapSector);
    bitHdr->Print();

    printf("Directory file header:\n");
    dirHdr->FetchFrom(DirectorySector);
    dirHdr->Print();

    freeMap->Print();

    directory->FetchFrom(directoryFile);
    directory->Print();

    delete bitHdr;
    delete dirHdr;
    delete freeMap;
    delete directory;
} 

#endif // FILESYS_STUB
