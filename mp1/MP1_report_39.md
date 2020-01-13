---
geometry: margin=3cm
header-includes:
    - \usepackage{setspace}
    - \onehalfspacing
    - \usepackage{fontspec}
    - \setmainfont{Noto Sans CJK TC}
    - \setmonofont{Hack}
---

# MP1 Report 39
## Team Member List
* 106062202 陳騰鴻
* 106062129 梁瑜軒

## Team Member Contributions
* Code tracing: Done together.
* Implementation: Done by 梁瑜軒.
* Report and layout: Done by 陳騰鴻, with the help of 梁瑜軒.

## Trace Code

### `SC_Halt`

1. `Machine::Run`:
    1. It sets the OS status to user mode. 
    2. It uses a for loop to keep calling `OneInstruction(instr)`. 
    3. After each instruction, it calls `kernel->interrupt->OneTick()` to check 
       whether there is any interrupt to be handled and then adds simulation tick time. 

2. `Machine::OneInstruction()`:
    1. Fetch the instruction to which the program counter points.
    2. Decode the instruction to find its opcode.
    3. Switch between cases of opcode.
    4. Since `Halt` is a system call, it’ll switch to `OP_SYSCALL` case, which raises an exception.

3. `Machine::RaiseException()`:
    1. Store the trap-causing virtual address to register.
    2. Set OS status to system mode.
    3. Call `ExceptionHandler()` to handle the exception.
    4. Set OS status back to user mode. 

4. `ExceptionHandler()`:
    1. A switch case is used to detect whether the instruction is a system call.
    2. In the `SyscallException` case, the condition is switched to the `SC_Halt` case.
    3. In the `SC_Halt` case, `SysHalt()` is called.

5. `SysHalt()`:  
    `kernel->interrupt->Halt()` is called.

6. `Interrupt::Halt()`:  
    After printing out performance statistics, the function deletes the kernel object.

### `SC_Create`

1. `ExceptionHandler()`:
    1. A switch case is used to detect whether the instruction is a system call.
    2. In the `SyscallException` case, the condition is switched to the `SC_Create` case.
    3. In the `SC_Create` case, the filename is accessed through main memory by the address, 
       which is read from the register.
    4. Function `SysCreate(filename)` is called, which returns a status of success or failure. 
       The status is then written back to register 2.
    5. The program counter register is incremented.

2. `SysCreate()`:  
    Calls `kernel->fileSystem->Create()` while passing the filename as an argument.  
    Returns the function’s status of success or failure. 

3. `Create()`:
    Variable `fileDescriptor` stores the return value of the function `OpenForWrite()`.  
    `fileDescriptor` equals to –1 if the function fails to create a file.
    Otherwise, the file is closed.  
    The status of success or failure is returned.

### `SC_PrintInt`

1. `ExceptionHandler()`:
   1. A switch case is used to detect whether the instruction is a system call.
   2. In the `SyscallException` case, the condition is switched to the `SC_PrintInt` case.
   3. In the `SC_PrintInt` case, a value read from register 4 is passed into `SysPrintInt()`. 
   4. The program counter register is incremented. 

2. `SysPrintInt()`:  
   Calls `kernel->synchConsoleOut->PutInt()` while passing variable `val` into it.

3. `SynchConsoleOutput::PutInt()`:
   1. Converts variable `value` from an integer to string with `sprintf`.
   2. Locks up I/O to prevent access from other programs.
   3. Repeatedly calls `consoleOutput->PutChar()`, which outputs one character of the string.
   4. Calls `waitFor->P()` to check whether the semaphore is available.
   5. Release the lock. ~~讓我的心，Unlock！~~

4. `SynchConsoleOutput::PutChar()`:
   1. Locks up I/O to prevent access from other programs.
   2. Calls `consoleOutput->PutChar()`  to output the character.
   3. Calls `waitFor->P()` to check whether the semaphore is available.
   4. Release the lock. ~~讓我的心，Unlock！~~

5. `ConsoleOutput::PutChar()` :
   1. If the I/O device is not busy(`putBusy == False`), writes the output character to buffer 
      by calling `Writefile()`, which puts the I/O device to busy(`putBusy = True`).
   2. Calls `kernel->interrupt->Schedule()` to schedule an interrupt for I/O.

6. `Interrupt::Schedule()`:
   1. Calculates the simulated time that the interrupt takes place and store it into variable `when`.
   2. Constructs a `PendingInterrupt` object with the argument `when`, 
      and inserts the object into pending queue.

7. `Machine::Run()`:
   1. It sets the OS status to user mode.
   2. It uses a for loop to keep calling `OneInstruction(instr)`.
   3. After each instruction, it calls `kernel->interrupt->OneTick()` 
      to check whether there is any interrupt to be handled and then adds simulation tick time. 

8. `Interrupt::Onetick()`:
   1. Updates the statistics of `totalTicks` and `userTicks` by adding `UserTick`.
   2. Disables interrupts and re-enables it after calling `CheckIfDue()`.
   3. If a context switch is available, sets status to system mode and calls 
      `kernel->currentThread->Yield()` to do context switch.

9. `Interrupt::CheckIfDue()`:
   1. If there are no pending interrupts, return `False`.
   2. If the first interrupt in the pending queue will occur in the future, 
      it checks if the ready queue is empty or not. Return `False` if not empty.
   3. If the ready queue is empty, advance the clock to the next interrupt.
   4. Set the `inHandler` flag to `True`.
   5. Pop the first interrupt from the pending queue and call its callback function. 
      Then, delete the interrupt object.
   6. Keeps dealing with interrupts until pending queue is emptied or 
      the first pending queue task is scheduled in the future.
   7. Set the `inHandler` flag to `False` and return `True`.

10. `ConsoleOutput::Callback()`:
    1. Sets the flag `putBusy` to `False`.
    2. Updates the statistics of `numConsoleCharsWritten`.
    3. Calls the callback function when done.

11. `SynchConsoleOutput::Callback()`:  
    Calls `waitFor->V()` to increase the number of available threads.

## Implementation

### test/start.S
1. `Open`, `Read`, `Write`, `Close` are implemented as assembly code in this file.
2. `addiu $2, $0, SC_Function`: Add unsigned immediate `SC_Function` with value in register 0 and 
   store the result in register 2.
3. `syscall`: Execute system call.
4. Each function has its own macro `SC_Function` defined in syscall.h.

### userprog/syscall.h
The macros used in start.S are uncommented.

### userprog/exception.cc
1. In `ExceptionHandler()`, switch cases for the previously uncommented macros are added.
2. `SC_Open`: 
    1. Memory address is read from register 4.
    2. The pointer of the filename is read from the main memory, using the memory address.
    3. `SysOpen()` is called and a status is returned, which is then written to register 2.
    5. Increment the program counter.

3. `SC_Read`:
    1. Memory address is read from register 4.
    2. The pointer of the buffer is read from the main memory, using the memory address.
    3. The number of bytes read (`size`) is read from register 5.
    4. The file ID is read from register 6.
    5. `SysRead()` is called and a status is returned, which is then written to register 2.
    6. Increment the program counter.

4. `SC_Write`:
    1. Memory address is read from register 4.
    2. The pointer of the buffer is read from the main memory, using the memory address.
    3. The number of bytes read (`size`) is read from register 5.
    4. The file ID is read from register 6.
    5. `SysWrite()` is called and a status is returned, which is then written to register 2.
    6. Increment the program counter.

5. `SC_Close`:
    1. The file ID is read from register 6.
    2. `SysClose()` is called and a status is returned, which is then written to register 2.
    3. Increment the program counter.

### userprog/ksyscall.h
1. `SysOpen`: Returns a call of `kernel->fileSystem->OpenAFile()`.
2. `SysRead`: Returns a call of `kernel->fileSystem->ReadFile()`.
3. `SysWrite`: Returns a call of `kernel->fileSystem->WriteFile()`.
4. `SysClose`: Returns a call of `kernel->fileSystem->CloseFile()`.

### filesys/filesys.h
1. `OpenAFile()`:
    1. Return -1 if filename is null.
    2. Loop through `fileDescriptorTable` to find an empty space for the file about to be opened. 
       Return -1 if not found.
    3. Call `Open()`, which returns an `OpenFile` object. The object is then stored into 
       the given space in `fileDescriptorTable`.
    4. Return -1 if the object is null. Otherwise, return the index where the object is stored.

2. `WriteFile()`:
	1. Return -1 if the buffer does not exist.
	2. Return -1 if the size is invalid.
	3. Return -1 if the index is out of bounds.
	4. Return -1 if the object in `fileDescriptorTable` does not exist.
	5. If the testings are successful, return a call of `fileDescriptorTable[index]->Write()`.

3. `CloseFile()`:
	1. Return -1 if the index is out of bounds.
	2. Return -1 if the object in fileDescriptorTable does not exist.
	3. If the testings are successful, the object in `fileDescriptorTable[index]` will be deleted, 
       which the destructor closes the file. `fileDescriptorTable[index]` is then set to null.
	4. Return 1.

## Difficulties Encountered
1. While most hints are useful, we had difficulties understanding some of them.
2. It is challenging for us to trace code of such amount.

## Feedback
TA time would be helpful.
