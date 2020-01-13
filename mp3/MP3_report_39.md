# Team Members and Contributions
- 106062202 陳騰鴻: Report
- 106062129 ***梁瑜軒***: Implementation

# Trace Code

## New $\to$ Ready

### Purpose
1. Allocate memory space for every thread.
2. Load data of the programs into allocated memory.

### Details

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

#### `Scheduler::ReadyToRun()`
1. Check if the interrupt level is `IntOff` with `ASSERT`. That is, interrupts are disabled.
2. Call `thread->setStatus` to set the thread status to `READY`.
3. Append the thread to `readyList` using `readyList->Append()`.

## Running $\to$ Ready

### Purpose
* Make the current thread yield the CPU to others by periodically triggering interrupts and perform context switch.

### Details

#### `Machine::Run()`
1. It sets the OS status to user mode. 
2. It uses a for loop to keep calling `OneInstruction(instr)`. 
3. After each instruction, it calls `kernel->interrupt->OneTick()` to check whether there is any interrupt to be handled and then adds simulation tick time. 

#### `Interrupt::OneTick()`
1. Updates the statistics of `totalTicks` and `userTicks` by adding `UserTick`.
2. Disables interrupts and re-enables it after calling `CheckIfDue()`.
3. If a context switch is available, sets status to system mode and calls `kernel->currentThread->Yield()` to do context switch.

#### `Thread::Yield()`
1. After turning off interrupts(`kernel->interrupt->SetLevel()`), assign the next thread(`kernel->scheduler->FindNextToRun()`) to `nextThread`.
2. If `nextThread` is not `null`, call `kernel->scheduler->ReadyToRun()` to put `nextThread` into ready queue and call `kernel->scheduler->Run()` to perform context switch.
3. Set the interrupt status back to the original status.

#### `Scheduler::FindNextToRun()`
* Return the popped thread using `readyList->RemoveFront()` if `readyList` is non-empty. Otherwise, return `NULL`.

#### `Scheduler::ReadyToRun()`
1. Check if the interrupt level is `IntOff` with `ASSERT`. That is, interrupts are disabled.
2. Call `thread->setStatus` to set the thread status to `READY`.
3. Append the thread to `readyList` using `readyList->Append()`.

#### `Scheduler::Run()`
1. Store the current thread to `oldThread`.
2. If `oldThread` is to be switched out(`finishing` is the flag), assign `oldThread` to `toBeDestroyed`.
3. If `oldThread` is a user program, save the user's CPU registers by calling `oldThread->SaveUserState()` and the address space by calling `oldThread->space->SaveState()`.
4. Using `oldThread->checkOverflow()`, check if a stack overflow has occured.
5. Switch to the next thread(`kernel->currentThread = nextThread`) and set the thread status to `RUNNING`.
6. Call `SWITCH()` to move the correspoding register data into the CPU registers.

## Running $\to$ Waiting

### Purpose
* Append the thread executing `ConsoleOutput::PutChar()` to the queue of the function's semaphore. The thread is then put to sleep, and the scheduler finds the next thread to run.

### Details

#### `ConsoleOutput::PutChar()` 
1. If the I/O device is not busy(`putBusy == False`), writes the output character to buffer by calling `Writefile()`, which puts the I/O device to busy(`putBusy = True`).
2. Calls `kernel->interrupt->Schedule()` to schedule an interrupt for I/O.

#### `Semaphore::P()`
1. While the semaphore is not available, append `currentThread` to the waiting queue
2. Call `currentThread->Sleep()` to set the status of `currentThread` and perform context switch.
3. When the semaphore is available, consume `value` and set the interrupt level back to `oldLevel`.

#### `SynchList<T>::Append(T)`
1. Lock up access to the list by calling `lock->Acquire()`.
2. Append object `item` to `list` with `list->Append()`.
3. Call `listEmpty->Signal()` to wake up a waiting thread, if any.
4. Unlock access to the list by calling `lock->Release()`.

#### `Thread::Sleep()`
1. Set the thread status to `BLOCKED`.
2. While there is no thread in the ready queue, call `kernel->interrupt->Idle()`.
3. If there are threads in the ready queue, call `kernel->scheduler->Run()` to switch threads.

#### `Scheduler::FindNextToRun()`
See [`Scheduler::FindNextToRun()`](#`Scheduler::FindNextToRun()`)

#### `Scheduler::Run()`*
See [`Scheduler::Run()`](#`Scheduler::Run()`)

## Waiting $\to$ Ready

### Purpose
* After an interrupt of calling the callback functions, the thread executing `ConsoleOutput::PutChar()` is removed from the waiting queue and pushed into the ready queue.

### Details

#### `Semaphore::V()`
1. Disable interrupts with `interrupt->SetLevel(IntOff)`.
2. If `queue` is not empty, put the first thread in `queue` into ready queue (`kernel->scheduler->ReadyToRun(queue->RemoveFront())`).
3. Release the semaphore resource (`value++`) and re-enable interrupts.

#### `Scheduler::ReadyToRun()`
See [`Scheduler::ReadyToRun()`](#`Scheduler::ReadyToRun()`)

## Running $\to$ Terminated

### Purpose
* After a thread finishes execution, it is put to sleep for later being destroyed.

### Detail

#### `ExceptionHandler(): case SC_Exit`
1. Read the program's return value from the register to `val` and output `val`.
2. Call `kernel->CurrentThread->Finish()` after executing the thread.

#### `Thread::Finish()`
1. Disable interrupts.
2. Put the thread to sleep and set `finishing` to `TRUE` for the thread to be destroyed later.

#### `Thread::Sleep()`
See [`Thread::Sleep()`](#`Thread::Sleep()`)

#### `Scheduler::FindNextToRun()`
See [`Scheduler::FindNextToRun()`](#`Scheduler::FindNextToRun()`)

#### `Scheduler::Run()`*
See [`Scheduler::Run()`](#`Scheduler::Run()`)

## Ready $\to$ Running

### Purpose

### Detail

#### `Scheduler::FindNextToRun()`
See [`Scheduler::FindNextToRun()`](#`Scheduler::FindNextToRun()`)

#### `Scheduler::Run()`*
See [`Scheduler::Run()`](#`Scheduler::Run()`)

#### `SWITCH()`
1. `.comm _eax_save,4`: Allocate a 4-byte memory storage called `_eax_save`.
2. Store data of `t1`:
    1. `movl %eax,_eax_save`: Save the value of `eax` to `_eax_save`.
    2. `movl 4(%esp),%eax`: Move the pointer to t1 into `eax`.
    3. Save registers, which contain data of the thread, to its `machineState`, e.g. `movl %ebx,_EBX(%eax)`.
    4. `movl %esp,_ESP(%eax)`: Save the stack pointer.
    5. Save data in `_eax_save` to address `_EAX(%eax)` by first saving data in `_eax_save` to `%ebx`, then saving that in `%ebx` to address `_EAX(%eax)`.
    6. Save the return address to `_PC(%eax)` by first saving data in `%esp` into `%ebx`, then saving that in `%ebx` to `_PC(%eax)`.
3. Load data of `t2`:
    1. `movl 8(%esp),%eax`: Load the pointer to t2 into `eax`.
    2. Save the new value for `eax` to `_eax_save` by first saving value in `_EAX(%eax)` to `%ebx`, then saving that in `%ebx` to `_eax_save`.
    3. Load registers with data of `t2` using base address `%eax`, e.g. `movl _EBX(%eax),%ebx`.
    4. Load the return address of `t2` into `%eax` and save it to `4(%esp)`.
    5. `movl _eax_save,%eax`: Save the value of `_eax_save` to `eax`.

#### `Machine::Run()`*
See [`Machine::Run()`](#`Machine::Run()`)

# Implementation

## Multilevel Feedback Queue

### Declaration
* L1, L2, and L3 are declared in `scheduler.h`.
```cpp=
SortedList<Thread *> *readyList1;
SortedList<Thread *> *readyList2;
List<Thread *> *readyList3;
```

### Thread Insertion
* Thread insertion is implemented in `scheduler.cc/Scheduler::ReadyToRun()`.
* After being set to `READY`, the thread is put into a queue, based on its priority.
* Debug message [A] is implemented here.
```cpp=
thread->setStatus(READY);

if(thread->getPriority() >= 100){
    DEBUG(dbgSchedule, "Tick " << kernel->status->totalTicks
    << ": Thread " << thread->getID() << " is insert into queue L1");
    readyList1->Insert(thread);
}
else if(thread->getPriority() >= 50){
    DEBUG(dbgSchedule, "Tick " << kernel->status->totalTicks
    << ": Thread " << thread->getID() << " is insert into queue L2");
    readyList2->Insert(thread);
}
else{
    DEBUG(dbgSchedule, "Tick " << kernel->status->totalTicks
    << ": Thread " << thread->getID() << " is insert into queue L3");
    readyList3->Append(thread);
}
```

### Thread Selection
* Implemented in `scheduler.cc/Scheduler::FindNextToRun()`.
* `FindNextToRun()` is called by 2 functions: `Thread::Sleep` and `Thread::Yield`.
    * `Thread::Sleep()`: When there is no thread running (`currentThread` status is `BLOCKED` or `READY`, a new thread is needed.
    * Select a thread from queue based on the order of priority. Return `NULL` when ~~all is lost, all is found~~ none is found.
    * Debug message [B] is implemented here.
    ```cpp=
    if(!readyList1->IsEmpty()){
       nextThread = readyList1->RemoveFront();
       DEBUG(dbgSchedule, "Tick " << kernel->stats->totalTicks
       << ": Thread " << nextThread->getID() << " is removed from queue L1");
    }
    else if(!readyList2->IsEmpty()){
        nextThread = readyList2->RemoveFront();
        DEBUG(dbgSchedule, "Tick " << kernel->stats->totalTicks
        << ": Thread " << nextThread->getID() << " is removed from queue L2");
    }
    else if(!readyList3->IsEmpty()){
        nextThread = readyList3->RemoveFront();
        DEBUG(dbgSchedule, "Tick " << kernel->stats->totalTicks
        << ": Thread " << nextThread->getID() << " is removed from queue L3");
    }
    else return NULL;
    ```
    * `Thread::Yield()`: When an alarm interrupt triggers, whether a thread should be swtiched out is based on the priority of the current process and the states of L1, L2, and L3. Relations are shown as below.

    | Current Process |  L1   |  L2   | L3  |    Context Swtich to     |
    |:---------------:|:-----:|:-----:|:---:|:------------------------:|
    |       L1        |  job  |   x   |  x  | L1 if job is shorter job |
    |       L1        | empty |   x   |  x  |          False           |
    |       L2        |  job  |   x   |  x  |            L1            |
    |       L2        | empty |   x   |  x  |          False           |
    |       L3        |  job  |   x   |  x  |            L1            |
    |       L3        | empty |  job  |  x  |            L2            |
    |       L3        | empty | empty | job |            L3            |

    ```cpp=
    if(!readyList1->IsEmpty()){
        if( (kernel->currentThread->getPriority() > 99) && 
            (kernel->currentThread->gett() < readyList1->Front()->gett()) ){
            return NULL;
        }
        else{
            nextThread = readyList1->RemoveFront();
            DEBUG(dbgSchedule, "Tick " << kernel->stats->totalTicks
            << ": Thread " << nextThread->getID() << " is removed from queue L1");
        }
    }
    else if(kernel->currentThread->getPriority() > 49){ // current thread is L1 or L2
        return NULL;
    }
    else{
        if(!readyList2->IsEmpty()){
            nextThread = readyList2->RemoveFront();
            DEBUG(dbgSchedule, "Tick " << kernel->stats->totalTicks
            << ": Thread " << nextThread->getID() << " is removed from queue L2");
        }
        else if(!readyList3->IsEmpty()){
            nextThread = readyList3->RemoveFront();
            DEBUG(dbgSchedule, "Tick " << kernel->stats->totalTicks
            << ": Thread " << nextThread->getID() << " is removed from queue L3");
        }
        else 
            return NULL;
    }
    ```
* Enter running tick is set here: `nextThread->setenter_running_tick(kernel->stats->totalTicks);`

### Aging
* Implemented in `alarm.cc/Alarm::CallBack()` and `alarm.cc/GetOld()`.
* Aging is performed on every thread in the queues by calling `List::Apply()`.
  ```cpp=
  L1->Apply(GetOld);
  L2->Apply(GetOld);
  L3->Apply(GetOld);
  ```
* `GetOld` increments `waiting_tick` of thread `t` by 100 .
* Priority is increased by 10 for every 1500 ticks (to maximum of 149).
* Debug message [C] is implemented here.
```cpp=
t->setWaiting_tick(t->getWaiting_tick() + 100);

if (t->getWaiting_tick() % 1500 == 0)
{
    int new_pri = ((t->getPriority() + 10)>149)? 149: t->getPriority()+10;
    
    DEBUG(dbgSchedule, "Tick " << kernel->stats->totalTicks
    << ": Thread " << t->getID() << " changes its priority from "
    << t->getPriority() << " to " << new_pri);

    t->setPriority( new_pri );
}
```

### Changing Queue
* Implemented in `alarm.cc/Alarm::CallBack()`, `alarm.cc/GetOld()`, and `alarm.cc/ChangeQueue()`.
* Changing queue is performed on every thread in the queues by calling `List::Apply()`.
```cpp=
 // Insertion
L1->Apply(GetOld);
L2->Apply(GetOld);
L3->Apply(GetOld);

// Deletion
L1->Apply(ChangeQueue);
L2->Apply(ChangeQueue);
```
* Changing queue is broke down into 2 steps: insertion and deletion.
    * Insertion: Implemented in `alarm.cc/GetOld()`. Debug message [A] is implemented here.
    ```cpp=
     if(t->getPriority() > 99 && !L1->IsInList(t)) {
         DEBUG(dbgSchedule, "Tick " << kernel->stats->totalTicks
         << ": Thread " << t->getID() << " is insert into queue L1");
            L1->Insert(t);
     }
     else if(t->getPriority() > 49 && t->getPriority() <= 99 && !L2->IsInList(t) ) {
         DEBUG(dbgSchedule, "Tick " << kernel->stats->totalTicks
         << ": Thread " << t->getID() << " is insert into queue L2");
         L2->Insert(t);
     }
    ```
    * Deletion: Implemented in `alarm.cc/ChangeQueue`. Debug message [B] is implemented here.
    ```cpp=
    if(t->getPriority() > 99 && !L1->IsInList(t)){
        DEBUG(dbgSchedule, "Tick " << kernel->stats->totalTicks
        << ": Thread " << t->getID() << " is insert into queue L1");
        L1->Insert(t);
    }
    else if(t->getPriority() > 49 && t->getPriority() <= 99 && !L2->IsInList(t) ){
        DEBUG(dbgSchedule, "Tick " << kernel->stats->totalTicks
        << ": Thread " << t->getID() << " is insert into queue L2");
        L2->Insert(t);
    }
    ```
