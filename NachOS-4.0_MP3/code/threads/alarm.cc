// alarm.cc
//	Routines to use a hardware timer device to provide a
//	software alarm clock.  For now, we just provide time-slicing.
//
//	Not completely implemented.
//
// Copyright (c) 1992-1996 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "alarm.h"
#include "main.h"

//----------------------------------------------------------------------
// Alarm::Alarm
//      Initialize a software alarm clock.  Start up a timer device
//
//      "doRandom" -- if true, arrange for the hardware interrupts to 
//		occur at random, instead of fixed, intervals.
//----------------------------------------------------------------------

Alarm::Alarm(bool doRandom)
{
    timer = new Timer(doRandom, this);
}

// Aging
void GetOld(Thread *t)
{

    SortedList<Thread *> *L1 = kernel->scheduler->readyList1; 
    SortedList<Thread *> *L2 = kernel->scheduler->readyList2; 
    List<Thread *> *L3 = kernel->scheduler->readyList3;
    
    t->setWaiting_tick(t->getWaiting_tick() + 100);
    //t->waiting_tick += 100;

    if (t->getWaiting_tick() % 1500 == 0)
    {
        DEBUG(dbgSchedule, "[C] Tick [" << kernel->stats->totalTicks << "]: Thread [" << t->getID()
                << "] changes its priority from [" << t->getPriority() << "] to ["
                << (((t->getPriority()+10) > 149)? 149: t->getPriority()+10) << "]");

        int new_pri = ((t->getPriority() + 10)>149)? 149: t->getPriority()+10;
        t->setPriority( new_pri );
        //t-> priority = (new_pri > 149) ? 149 : new_pri;
        
        if(t->getPriority() > 99 && !L1->IsInList(t)){ // has L1 priority but not in L1    
            DEBUG(dbgSchedule, "[A] Tick [" << kernel->stats->totalTicks << "]: Thread [" << t->getID()
                    << "] is insert into queue L[1]");
            L1->Insert(t);
        }
        else if(t->getPriority() > 49 && t->getPriority() <= 99 && !L2->IsInList(t) ){ // has L2 priority but not in L2
            DEBUG(dbgSchedule, "[A] Tick [" << kernel->stats->totalTicks << "]: Thread [" << t->getID()
                    << "] is insert into queue L[2]");
            L2->Insert(t);
        }
    }
}

// Change queue
void ChangeQueue(Thread *t)
{
    SortedList<Thread *> *L1 = kernel->scheduler->readyList1; 
    SortedList<Thread *> *L2 = kernel->scheduler->readyList2; 
    List<Thread *> *L3 = kernel->scheduler->readyList3;

    // Thread t needs to be taken from L3 to L2
    if (L3->IsInList(t) && t->getPriority() > 49) // t is in L3 but has L2 priority
    {
            DEBUG(dbgSchedule, "[B] Tick [" << kernel->stats->totalTicks << "]: Thread [" << t->getID()
                    << "] is removed from queue L[3]");
            L3->Remove(t);
    }


    // Thread t needs to be taken from L2 to L1
    if (L2->IsInList(t) && t->getPriority() > 99) // t is in L2 but has L1 priority
    { 
            DEBUG(dbgSchedule, "[B] Tick [" << kernel->stats->totalTicks << "]: Thread [" << t->getID()
                    << "] is removed from queue L[2]");
            L2->Remove(t);
    }

    // This thread is normal.
}

//----------------------------------------------------------------------
// Alarm::CallBack
//	Software interrupt handler for the timer device. The timer device is
//	set up to interrupt the CPU periodically (once every TimerTicks).
//	This routine is called each time there is a timer interrupt,
//	with interrupts disabled.
//
//	Note that instead of calling Yield() directly (which would
//	suspend the interrupt handler, not the interrupted thread
//	which is what we wanted to context switch), we set a flag
//	so that once the interrupt handler is done, it will appear as 
//	if the interrupted thread called Yield at the point it is 
//	was interrupted.
//
//	For now, just provide time-slicing.  Only need to time slice 
//      if we're currently running something (in other words, not idle).
//----------------------------------------------------------------------

void 
Alarm::CallBack() 
{
    Interrupt *interrupt = kernel->interrupt;
    MachineStatus status = interrupt->getStatus();
    
    if (status != IdleMode) {
	interrupt->YieldOnReturn();
    }

    SortedList<Thread *> *L1 = kernel->scheduler->readyList1; 
    SortedList<Thread *> *L2 = kernel->scheduler->readyList2; 
    List<Thread *> *L3 = kernel->scheduler->readyList3; 

    // Aging
    L1->Apply(GetOld);
    L2->Apply(GetOld);
    L3->Apply(GetOld);

    // Change queue
    L1->Apply(ChangeQueue);
    L2->Apply(ChangeQueue);
}
