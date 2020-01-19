## Team Member List
* 106062202 陳騰鴻
* 106062129 梁瑜軒

## Team Member Contributions
* Code tracing: Done together.
* Implementation: Done by 梁瑜軒.
* Report and layout: Done by 陳騰鴻, with the help of 梁瑜軒.

## Trace Code

### Report Questions
- **How does Nachos allocate the memory space for new thread(process)?**  
At `Thread::Fork()`, `StackAllocate()` is called to allocate memory for a stack.  
At `Kernel::Exec()`, the initialization of an `AddrSpace` object allocates a page table.

- **How does Nachos initialize the memory content of a thread(process), including loading the user binary code in the memory?**  
The memory is zeroed out when initialized in the `AddrSpace` constructor. In `AddrSpace::Load()`, the code and data segments of the executable are loaded into the main memory by calling `executable->ReadAt()`.

- **How does Nachos create and manage the page table?**  
In the `AddrSpace` constructor, a page table is created by allocating an array of `TranslationEntry` objects. The attributes of each `TranslationEntry` object are then initialized with a `for` loop. Finally, the page table is zeroed out by calling `bzero()`. Every page has attributes `use` and `dirty`, which are set whenever it is referenced or modified.

* **How does Nachos translate addresses?**  
[See `Machine::Translate()`.](#`Machine::Translate()`)
\newpage

* **How does Nachos initialize the machine status (registers, etc) before running a thread(process)?**  
When object `machine` is allocated in `Kernel::Initialize()`, `Machine::Machine()` zeros out `register` and `mainMemory`. Before running a thread, `SWITCH` in `switch.S` is executed. [See below for more explanation.](#`switch.S/SWITCH`) If an old thread is switched back, `oldThread->RestoreUserState()` and `oldThread->RestoreState()` is executed.

* **Which type of objects in Nachos serves as the process control block?**  
`Thread` objects serve as PCBs.

* **When and how does a thread get added into the `ReadyToRun` queue of Nachos CPU scheduler?**  
In `Thread::Fork`, a thread is pushed into the `ReadyToRun` queue after the interrupt is turned off. [See below for more explanation on how `ReadyToRun` is implemented.](#`Thread::Fork()`)

### Function Explanations

#### `Kernel::ExecAll()`
1. Loops through array `execfile` to execute the files by calling `Exec()`.
2. Closes the current thread (The main thread), which is allocated at the 
   initialization of the kernel.

#### `Kernel::Exec()`
1. In array `t`, a new `Thread` and `AddrSpace` is and assigned to `t[threadNum]`.
2. Call `t[threadNum]->Fork` to allocate memory space for `t[threadNum]`.
3. Increment `threadNum` and return the value of current thread number.

#### `Thread::Fork()`
1. Call `StackAllocate()` to allocate a stack for the function `func` points to.
2. Turn off interrupt with `interrupt->SetLevel()` and store the previous level in `oldLevel`.
3. Push this thread to the ready queue with `scheduler->ReadyToRun()`.
4. Set the interrupt level back to `oldLevel`.

#### `Thread::StackAllocate()`
1. Allocate a memory protected array to `stack` using `AllocaBoundedArray()`.
2. Store the pointers of functions and arguments of all states in the array `machineState`, which is used when switching threads.
\newpage

#### `Scheduler::ReadyToRun()`
1. Check if the interrupt level is `IntOff` with `ASSERT`. That is, interrupts are disabled.
2. Call `thread->setStatus` to set the thread status to `READY`.
3. Append the thread to `readyList` using `readyList->Append()`.

#### `Thread::Finish()`
1. Call `interrupt->SetLevel()` to turn off interrupts for the next step.
2. Call `Sleep()` before invoking a context switch.

#### `Thread::Sleep()`
1. Set the thread status to `BLOCKED`.
2. While there is no thread in the ready queue, call `kernel->interrupt->Idle()`.
3. If there are threads in the ready queue, call `kernel->scheduler->Run()` to switch threads.

#### `Interrupt::Idle()`
1. Set the machine status to `IdleMode`.
2. Check if there are any pending interrupts with `CheckIfDue()`.
3. If true, set the machine status to `SystemMode`.
4. If there are no pending interrupts, call `Halt()`.

#### `Scheduler::Run()`
1. Store the current thread to `oldThread`.
2. If `oldThread` is to be switched out(`finishing` is the flag), assign `oldThread` to `toBeDestroyed`.
3. If `oldThread` is a user program, save the user's CPU registers by calling `oldThread->SaveUserState()` and the address space by calling `oldThread->space->SaveState()`.
4. Using `oldThread->checkOverflow()`, check if a stack overflow has occured.
5. Switch to the next thread(`kernel->currentThread = nextThread`) and set the thread status to `RUNNING`.
6. Call `SWITCH()` to move the correspoding register data into the CPU registers.
\newpage

#### `switch.S/SWITCH`
1. Execute `sw rd, offset(a0)` to save the register data of the old thread, which pointer `a0` points to .
2. Exectue `lw rd, offset(a1)` to load the new thread's register data to the CPU register.
3. Instruction `lw ra, PC(a1)` loads the return address, which points to the new thread's `ThreadRoot`.
4. Exectue `j ra` to jump to `ThreadRoot`.

#### `switch.S/ThreadRoot`
1. Execute `jal StartUpPC` to call the startup procedure before linking back to the next instruction.
2. Execute `move a0, InitialArg` to move the pointer to the user program and its arguments to the argument register `a0`.
3. Exectue `jal InitialPC` to call the main procedure(execute the user program).
4. Execute `jal WhenDonePC` to call the cleanup procedure.

#### `Thread::ThreadBegin()`
* Call `kernel->currentThread->Begin()`.

#### `Thread::Begin()`
1. Call `kernel->scheduler->CheckToBeDestroyed()` to deallocate the old thread if it finished its procedure.
2. Call `kernel->interrupt->Enable()` to enable interrupts.

#### `ForkExecute()`
1. Load the program code by calling `t->space->Load()`, which returns `false` when an executable does not exist.
2. If `t->space->Load()` returns `true`, call `t->space->Execute()` to execute the program.

#### `AddrSpace::Load()`
1. Call `kernel->fileSystem->Open()` to get the program file and store it in `*executable`.
2. Load the NachOS File Format Header to `noffh` using `executable->ReadAt()`.
3. Check if `noffH` is a NachOS object code file in little Endian format. If so, convert it to big Endian by calling `SwapHeader()`.
4. Check if `noffH` is a NachOS object code file in big Endian format with `ASSERT(noffH.noffMagic == NOFFMAGIC)`.
5. Calculate the address space size, which is the sum of `noffH`'s data size and the user stack size.
6. Calculate the number of pages the address space takes up using `divRoundUp`. The total size of all pages should be no less than the address space size actually takes up.
7. Load the code and data segments of `noffH` into memory with `executable->ReadAt()`.
8. Close file with `delete executable`.

#### `AddrSpace::Execute()`
1. Switch address spaces by assigning `this` to `kernel->currentThread->space`.
2. Initialize register values using `this->InitRegisters()`.
3. Load the page table registers using `this->RestoreState()`.
4. Execute the program by calling `kernel->machine->Run()`.

#### `AddrSpace::InitRegisters()`
* Using `machine->WriteRegister()`, do the following:
1. Zero out all the registers.
2. Initialize `PCReg` and `NextPCReg`.
3. Initialize `StackReg` with the address space size.

#### `Machine::Translate()`
1. Check for aligment errors, that is, whether `virtAddr` is a multiple of 4 or 2.
2. If `tlb` is null, calculate the virtual page number(`virtAddr / PageSize`) and assign it to `vpn`. If `vpn` is smaller than `pageTableSize` and `pageTable[vpn]` is valid, assign `&pageTable[vpn]` to `entry`.
3. Check if we are trying to write a read-only page. If not, assign the physical page of the entry to `pageFrame`.
4. Check if `pageFrame` is out of bounds(`NumPhysPages`). If not, set `entry->use` to `TRUE` and `entry->dirty` to `TRUE` if it is currently being written.
5. Calculate the physical address and store it in `*physAddr`.
6. Return `NoException` if all steps above are done.

## Implementation

### `kernel.h / kernel.cc`

#### `Class Kernel`
1. `PhysPagesStatus`: Stores physical page status. 0: not used, 1: used.
2. `FreePagesNum`: Stores the number of unused pages.
\newpage

#### `Kernel::Initialize()`
1. Set all elements in `PhysPagesStatus` to zero.
2. Initialize `FreePagesNum` by assigning `NumPhysPages` to it.

### `addrspace.cc`

#### `AddrSpace::Load()`
1. Allocate an array of `TranslationEntry` objects and store it in `pageTable`.
2. Loop through the pages address space needs, for each iteration:
   1. Set the virtual page index `pageTable.virtualPage` to `pageTable` index.
   2. Loop through `PhyPagesStatus` until an unused physical page is found. Assign its index to `pageTable.physicalPage`.
   3. Set the founded physical page status to 'used'(`kernel->PhysPagesStatus[index] = 1`) and decrement `FreePagesNum`.
   4. Zero out its corresponding main memory space with `bzero()`.
   5. Zero out the page space.
3. Copy code and data segments into memory by repeatedly calling `executable->ReadAt()`.
   1. `fraction`: The size of the first unused page, considering the offset that the previous segment produced.
   2. `readsize`: The actual size of the memory to copy data into.
   3. `noffH.code.virtualAddr`: The memory address currently writing to. Increments by `readsize` after an iteration.
   4. `noffH.code.size`: The size of segment to be written. Decrements by `readsize` after an iteration.
   5. `noffH.code.inFileAddr`: The file address currently read. Increments by `readsize` after an iteration.

#### `AddrSpace::AddrSpace()`
* Everything is deleted.

#### `AddrSpace::~AddrSpace()`
* Release the physical pages by doing the following:
1. Set the physical page status to zero for each entry in `pageTable`.
2. Increment `FreePagesNum` by the size of `pageTable`.
3. Deallocate `pageTable`.
