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
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "scheduler.h"
#include "system.h"

//----------------------------------------------------------------------
// Scheduler::Scheduler
// 	Initialize the list of ready but not running threads to empty.
//----------------------------------------------------------------------

Scheduler::Scheduler()
{ 
    readyList = new List;    
}

//----------------------------------------------------------------------
// Scheduler::~Scheduler
// 	De-allocate the list of ready threads.
//----------------------------------------------------------------------

Scheduler::~Scheduler()
{ 
    delete readyList; 
} 

//----------------------------------------------------------------------
// Scheduler::ReadyToRun
// 	Mark a thread as ready, but not running.
//	Put it on the ready list, for later scheduling onto the CPU.
//
//	"thread" is the thread to be put on the ready list.
//----------------------------------------------------------------------
//因为优先级调度本质上是把线程被创建后插入就绪队列的位置从时间顺序改成了优先级顺序，所以只需修改readytorun函数
void
Scheduler::ReadyToRun (Thread *thread)
{
    DEBUG('t', "Putting thread \"%s\" on ready list.\n", thread->getName());

    thread->setStatus(READY);

// =======================================PREEMPTIVE============================================
#ifdef PREEMPTIVE
    if(currentThread==NULL || thread->getPriority() < currentThread->getPriority()){ 
        readyList->SortedInsert((void *)currentThread,currentThread->getPriority());
        Run(thread);
    }
    else readyList->SortedInsert((void *)thread,thread->getPriority()); 
#endif 
// =======================================PREEMPTIVE============================================
    
// =======================================3.实现带有优先级的线程调度============================================
    #ifndef PREEMPTIVE
    //readyList->Append((void *)thread);//附加插入
    readyList->SortedInsert((void *)thread,thread->getPriority());//按优先级插入 
    #endif 
// =======================================3.实现带有优先级的线程调度============================================
}

//----------------------------------------------------------------------
// Scheduler::FindNextToRun
// 	Return the next thread to be scheduled onto the CPU.
//	If there are no ready threads, return NULL.
// Side effect:
//	Thread is removed from the ready list.
//----------------------------------------------------------------------

Thread *
Scheduler::FindNextToRun ()
{
// =======================================(6)============================================
#ifdef AGING
    int newP;
    if(currentThread->getPriority()<(stats->systemTicks-lastSwitchTick)/10) newP=0;
    else newP=currentThread->getPriority()+(int)((stats->systemTicks-lastSwitchTick)/10);
    currentThread->setPriority(newP);
#endif 
// =======================================(6)============================================      
    return (Thread *)readyList->Remove();
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
//	The global variable currentThread becomes nextThread.
//
//	"nextThread" is the thread to be put into the CPU.
//----------------------------------------------------------------------

void
Scheduler::Run (Thread *nextThread)
{
    Thread *oldThread = currentThread;
    
#ifdef USER_PROGRAM			// ignore until running user programs 
    if (currentThread->space != NULL) {	// if this thread is a user program,
        currentThread->SaveUserState(); // save the user's CPU registers
	currentThread->space->SaveState();
    }
#endif
    
    oldThread->CheckOverflow();		    // check if the old thread
					    // had an undetected stack overflow

    currentThread = nextThread;		    // switch to the next thread
    currentThread->setStatus(RUNNING);      // nextThread is now running
    
    DEBUG('t', "*** Switching from thread \"%s\" to thread \"%s\"\n",
	  oldThread->getName(), nextThread->getName());
    
    // This is a machine-dependent assembly language routine defined 
    // in switch.s.  You may have to think
    // a bit to figure out what happens after this, both from the point
    // of view of the thread and from the perspective of the "outside world".

// =======================================(7)============================================
    lastSwitchTick=stats->systemTicks;
// =======================================(7)============================================

    SWITCH(oldThread, nextThread);
    
    DEBUG('t', "Now in thread \"%s\"\n", currentThread->getName());

    // If the old thread gave up the processor because it was finishing,
    // we need to delete its carcass.  Note we cannot delete the thread
    // before now (for example, in Thread::Finish()), because up to this
    // point, we were still running on the old thread's stack!
    if (threadToBeDestroyed != NULL) {
        delete threadToBeDestroyed;
	threadToBeDestroyed = NULL;
    }
    
#ifdef USER_PROGRAM
    if (currentThread->space != NULL) {		// if there is an address space
        currentThread->RestoreUserState();     // to restore, do it.
	currentThread->space->RestoreState();
    }
#endif
}

// =======================================(8)============================================
void 
Younger(Thread *thread)
{
    thread->setPriority(thread->getPriority()-AGING_PACE);
}

void 
Scheduler::FlushPriority()
{
    DEBUG('t', "---------------------> Flushing priority of readyList <------------------ \n");
    readyList->Mapcar((VoidFunctionPtr) Younger); //参考Print()实现
}

// =======================================(8)============================================


//----------------------------------------------------------------------
// Scheduler::Print
// 	Print the scheduler state -- in other words, the contents of
//	the ready list.  For debugging.
//----------------------------------------------------------------------
void
Scheduler::Print()
{
    printf("Ready list contents:\n");
    readyList->Mapcar((VoidFunctionPtr) ThreadPrint);
}
