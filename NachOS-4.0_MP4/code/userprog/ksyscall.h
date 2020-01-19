/**************************************************************
 *
 * userprog/ksyscall.h
 *
 * Kernel interface for systemcalls 
 *
 * by Marcus Voelp  (c) Universitaet Karlsruhe
 *
 **************************************************************/

#ifndef __USERPROG_KSYSCALL_H__ 
#define __USERPROG_KSYSCALL_H__ 

#include "kernel.h"

#include "synchconsole.h"


void SysHalt()
{
  kernel->interrupt->Halt();
}

int SysAdd(int op1, int op2)
{
  return op1 + op2;
}

#ifdef FILESYS_STUB
int SysCreate(char *filename)
{
	// return value
	// 1: success
	// 0: failed
	return kernel->interrupt->CreateFile(filename);
}
#endif
#ifndef FILESYS_STUB // MP4I
int SysCreate(char *filename, int initialSize)
{
	// return value
	// 1: success
	// 0: failed
	return kernel->fileSystem->Create(filename, initialSize);
}

OpenFileId SysOpen(char *filename)
{
	// return value
	// OpenFileId : success
	// -1 : failed
	DEBUG(dbgSys, "SysOpen" << filename << "\n");
	return kernel->fileSystem->OpenByName(filename);
}

int SysRead(char *buffer, int size, OpenFileId id)
{
	// return value
	// number of characters read from file
	// -1 : failed	
	DEBUG(dbgSys, "SysRead" << id << "\n");
	return kernel->fileSystem->currentFile->Read(buffer, size);
}

int SysWrite(char *buffer, int size, OpenFileId id)
{
	// return value
	// number of characters write from file
	// -1 : failed	
	DEBUG(dbgSys, "SysWrite" << id << "\n");
	return kernel->fileSystem->currentFile->Write(buffer, size);
}

int SysClose(OpenFileId id)
{
	// return value
	// 1 : success
	// -1 : failed	
	DEBUG(dbgSys, "SysClose" << id << "\n");
	return kernel->fileSystem->Close(id);
}
#endif
#endif /* ! __USERPROG_KSYSCALL_H__ */
