// scheduler.cc 
//	Routines to choose the next thread to run, and to dispatch to
//	that thread.
//
// 	These routines assume that interrupts are already disabled.
//	If interrupts are disabled, we can assume mutual exclusion
//	(since we are on a uniprocessor).
//
// 	NOTE: We can't use Locks to provide mutual exclusion here, since
// 	if we needed to wait for a lock, and the lock was busy, we would 
//	end up calling FindNextToRun(), and that would put us in an 
//	infinite loop.
//
// 	Very simple implementation -- no priorities, straight FIFO.
//	Might need to be improved in later assignments.
//
// Copyright (c) 1992-1996 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "debug.h"
#include "scheduler.h"
#include "main.h"
int SJFCompare(Thread *t1, Thread *t2){
    if(t1->gett() == t2->gett())
        return 0;
    return (t1->gett() < t2->gett()) ? -1 : 1 ;
}

int PriorityCmp(Thread *t1, Thread *t2){ // -1: t1 precedes t2
    if(t1->getPriority() == t2->getPriority())
        return 0;
    return (t1->getPriority() > t2->getPriority()) ? -1 : 1 ;
}

//----------------------------------------------------------------------
// Scheduler::Scheduler
// 	Initialize the list of ready but not running threads.
//	Initially, no ready threads.
//----------------------------------------------------------------------

Scheduler::Scheduler()
{ 
    readyList1 = new SortedList<Thread *>(SJFCompare); 
    readyList2 = new SortedList<Thread *>(PriorityCmp); 
    readyList3 = new List<Thread *>; 
    toBeDestroyed = NULL;
} 

//----------------------------------------------------------------------
// Scheduler::~Scheduler
// 	De-allocate the list of ready threads.
//----------------------------------------------------------------------

Scheduler::~Scheduler()
{ 
    delete readyList1; 
    delete readyList2; 
    delete readyList3; 
} 

//----------------------------------------------------------------------
// Scheduler::ReadyToRun
// 	Mark a thread as ready, but not running.
//	Put it on the ready list, for later scheduling onto the CPU.
//
//	"thread" is the thread to be put on the ready list.
//----------------------------------------------------------------------

void
Scheduler::ReadyToRun (Thread *thread)
{
    ASSERT(kernel->interrupt->getLevel() == IntOff);
    DEBUG(dbgThread, "Putting thread on ready list: " << thread->getName());
	//cout << "Putting thread on ready list: " << thread->getName() << endl ;
    thread->setStatus(READY);
    
    if(thread->getPriority() >= 100){
        DEBUG(dbgSchedule, "[A] Tick [" << kernel->stats->totalTicks << "]: Thread [" << thread->getID()
                << "] is inserted into queue L[1]");
        readyList1->Insert(thread);
    }
    else if(thread->getPriority() >= 50){
        DEBUG(dbgSchedule, "[A] Tick [" << kernel->stats->totalTicks << "]: Thread [" << thread->getID()
                << "] is inserted into queue L[2]");
        readyList2->Insert(thread);
    }
    else{
        DEBUG(dbgSchedule, "[A] Tick [" << kernel->stats->totalTicks << "]: Thread [" << thread->getID()
                << "] is inserted into queue L[3]");
        readyList3->Append(thread);
    }
    //readyList->Append(thread);
}

//----------------------------------------------------------------------
// Scheduler::FindNextToRun
// 	Return the next thread to be scheduled onto the CPU.
//	If there are no ready threads, return NULL.
// Side effect:
//	Thread is removed from the ready list.
//----------------------------------------------------------------------

Thread *
Scheduler::FindNextToRun () // implement multi-queue selection
{
    ASSERT(kernel->interrupt->getLevel() == IntOff);
    Thread* nextThread;

    if(kernel->currentThread->getStatus() == BLOCKED || kernel->currentThread->getStatus() == READY){ // call from Thread::Sleep()
       if(!readyList1->IsEmpty()){
           nextThread = readyList1->RemoveFront();
           DEBUG(dbgSchedule, "[B] Tick [" << kernel->stats->totalTicks << "]: Thread [" << nextThread->getID()
                << "] is removed from queue L[1]");
       }
       else if(!readyList2->IsEmpty()){
           nextThread = readyList2->RemoveFront();
           DEBUG(dbgSchedule, "[B] Tick [" << kernel->stats->totalTicks << "]: Thread [" << nextThread->getID()
                << "] is removed from queue L[2]");
       }
       else if(!readyList3->IsEmpty()){
           nextThread = readyList3->RemoveFront();
           DEBUG(dbgSchedule, "[B] Tick [" << kernel->stats->totalTicks << "]: Thread [" << nextThread->getID()
                << "] is removed from queue L[3]");
       }
       else return NULL;
    }
    else{ // call from Thread::Yield()
        if(!readyList1->IsEmpty()){
            if( (kernel->currentThread->getPriority() > 99) && 
                (kernel->currentThread->gett() < readyList1->Front()->gett()) ){
                return NULL;
            }
            else{
                nextThread = readyList1->RemoveFront();
                DEBUG(dbgSchedule, "[B] Tick [" << kernel->stats->totalTicks << "]: Thread [" << nextThread->getID()
                        << "] is removed from queue L[1]");
            }
        }
        else if(kernel->currentThread->getPriority() > 49){ // current thread is L1 or L2
            return NULL;
        }
        else{
            if(!readyList2->IsEmpty()){
                nextThread = readyList2->RemoveFront();
                DEBUG(dbgSchedule, "[B] Tick [" << kernel->stats->totalTicks << "]: Thread [" << nextThread->getID()
                        << "] is removed from queue L[2]");
            }
            else if(!readyList3->IsEmpty()){
                nextThread = readyList3->RemoveFront();
                DEBUG(dbgSchedule, "[B] Tick [" << kernel->stats->totalTicks << "]: Thread [" << nextThread->getID()
                        << "] is removed from queue L[3]");
            }
            else 
                return NULL;
        }
    }

    
    nextThread->setenter_running_tick(kernel->stats->totalTicks);
    return nextThread;

    /*if (readyList->IsEmpty()) {
		return NULL;
    } else {
        Thread* nextThread = readyList->RemoveFront();
        nextThread->enter_running_tick = kernel->stats->totalTicks;
    	return nextThread;
    }*/
}

//----------------------------------------------------------------------
// Scheduler::Run
// 	Dispatch the CPU to nextThread.  Save the state of the old thread,
//	and load the state of the new thread, by calling the machine
//	dependent context switch routine, SWITCH.
//
//      Note: we assume the state of the previously running thread has
//	already been changed from running to blocked or ready (depending).
// Side effect:
//	The global variable kernel->currentThread becomes nextThread.
//
//	"nextThread" is the thread to be put into the CPU.
//	"finishing" is set if the current thread is to be deleted
//		once we're no longer running on its stack
//		(when the next thread starts running)
//----------------------------------------------------------------------

void
Scheduler::Run (Thread *nextThread, bool finishing)
{
    Thread *oldThread = kernel->currentThread;
    
    ASSERT(kernel->interrupt->getLevel() == IntOff);

    if (finishing) {	// mark that we need to delete current thread
         ASSERT(toBeDestroyed == NULL);
	 toBeDestroyed = oldThread;
    }
    
    if (oldThread->space != NULL) {	// if this thread is a user program,
        oldThread->SaveUserState(); 	// save the user's CPU registers
	oldThread->space->SaveState();
    }
    
    oldThread->CheckOverflow();		    // check if the old thread
					    // had an undetected stack overflow

    kernel->currentThread = nextThread;  // switch to the next thread
    nextThread->setStatus(RUNNING);      // nextThread is now running
    
    DEBUG(dbgThread, "Switching from: " << oldThread->getName() << " to: " << nextThread->getName());
    
    DEBUG(dbgSchedule, "[E] Tick [" << kernel->stats->totalTicks << "]: Thread [" << nextThread->getID()
                << "] is now selected for execution, thread [" << oldThread->getID()
                << "] is replaced, and it has executed [" << oldThread->getT() << "] ticks");
    // This is a machine-dependent assembly language routine defined 
    // in switch.s.  You may have to think
    // a bit to figure out what happens after this, both from the point
    // of view of the thread and from the perspective of the "outside world".

    SWITCH(oldThread, nextThread);

    // we're back, running oldThread
      
    // interrupts are off when we return from switch!
    ASSERT(kernel->interrupt->getLevel() == IntOff);

    DEBUG(dbgThread, "Now in thread: " << oldThread->getName());

    CheckToBeDestroyed();		// check if thread we were running
					// before this one has finished
					// and needs to be cleaned up
    
    if (oldThread->space != NULL) {	    // if there is an address space
        oldThread->RestoreUserState();     // to restore, do it.
	oldThread->space->RestoreState();
    }
}

//----------------------------------------------------------------------
// Scheduler::CheckToBeDestroyed
// 	If the old thread gave up the processor because it was finishing,
// 	we need to delete its carcass.  Note we cannot delete the thread
// 	before now (for example, in Thread::Finish()), because up to this
// 	point, we were still running on the old thread's stack!
//----------------------------------------------------------------------

void
Scheduler::CheckToBeDestroyed()
{
    if (toBeDestroyed != NULL) {
        delete toBeDestroyed;
	toBeDestroyed = NULL;
    }
}
 
//----------------------------------------------------------------------
// Scheduler::Print
// 	Print the scheduler state -- in other words, the contents of
//	the ready list.  For debugging.
//----------------------------------------------------------------------
void
Scheduler::Print()
{
    cout << "Ready list 1 contents:\n";
    readyList1->Apply(ThreadPrint);
    cout << "Ready list 2 contents:\n";
    readyList2->Apply(ThreadPrint);
    cout << "Ready list 3 contents:\n";
    readyList3->Apply(ThreadPrint);
}
