// interrupt.cc 
//	Routines to simulate hardware interrupts.
//
//	The hardware provides a routine (SetLevel) to enable or disable
//	interrupts.
//
//	In order to emulate the hardware, we need to keep track of all
//	interrupts the hardware devices would cause, and when they
//	are supposed to occur.  
//
//	This module also keeps track of simulated time.  Time advances
//	only when the following occur: 
//		interrupts are re-enabled
//		a user instruction is executed
//		there is nothing in the ready queue
//
//  DO NOT CHANGE -- part of the machine emulation
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "interrupt.h"
#include "system.h"
#include "addrspace.h"//导入需要的addrspace.h文件，用于创建新线程分配空间

// String definitions for debugging messages

static const char *intLevelNames[] = { "off", "on"};
static const char *intTypeNames[] = { "timer", "disk", "console write", 
			"console read", "network send", "network recv"};

//----------------------------------------------------------------------
// PendingInterrupt::PendingInterrupt
// 	Initialize a hardware device interrupt that is to be scheduled 
//	to occur in the near future.
//
//	"func" is the procedure to call when the interrupt occurs
//	"param" is the argument to pass to the procedure
//	"time" is when (in simulated time) the interrupt is to occur
//	"kind" is the hardware device that generated the interrupt
//----------------------------------------------------------------------

PendingInterrupt::PendingInterrupt(VoidFunctionPtr func, _int param, int time, 
				IntType kind)
{
    handler = func;
    arg = param;
    when = time;
    type = kind;
}

//----------------------------------------------------------------------
// Interrupt::Interrupt
// 	Initialize the simulation of hardware device interrupts.
//	
//	Interrupts start disabled, with no interrupts pending, etc.
//----------------------------------------------------------------------

Interrupt::Interrupt()
{
    level = IntOff;
    pending = new List();
    inHandler = FALSE;
    yieldOnReturn = FALSE;
    status = SystemMode;
}

//----------------------------------------------------------------------
// Interrupt::~Interrupt
// 	De-allocate the data structures needed by the interrupt simulation.
//----------------------------------------------------------------------

Interrupt::~Interrupt()
{
    while (!pending->IsEmpty())
	    delete (ListElement *)(pending->Remove());
    delete pending;
}

//----------------------------------------------------------------------
// Interrupt::ChangeLevel
// 	Change interrupts to be enabled or disabled, without advancing 
//	the simulated time (normally, enabling interrupts advances the time).

//----------------------------------------------------------------------
// Interrupt::ChangeLevel
// 	Change interrupts to be enabled or disabled, without advancing 
//	the simulated time (normally, enabling interrupts advances the time).
//
//	Used internally.
//
//	"old" -- the old interrupt status
//	"now" -- the new interrupt status
//----------------------------------------------------------------------
void
Interrupt::ChangeLevel(IntStatus old, IntStatus now)
{
    level = now;
    DEBUG('i',"\tinterrupts: %s -> %s\n",intLevelNames[old],intLevelNames[now]);
}

//----------------------------------------------------------------------
// Interrupt::SetLevel
// 	Change interrupts to be enabled or disabled, and if interrupts
//	are being enabled, advance simulated time by calling OneTick().
//
// Returns:
//	The old interrupt status.
// Parameters:
//	"now" -- the new interrupt status
//----------------------------------------------------------------------

IntStatus
Interrupt::SetLevel(IntStatus now)
{
    IntStatus old = level;
    
    ASSERT((now == IntOff) || (inHandler == FALSE));// interrupt handlers are 
						// prohibited from enabling 
						// interrupts

    ChangeLevel(old, now);			// change to new state
    if ((now == IntOn) && (old == IntOff))
	OneTick();				// advance simulated time
    return old;
}

//----------------------------------------------------------------------
// Interrupt::Enable
// 	Turn interrupts on.  Who cares what they used to be? 
//	Used in ThreadRoot, to turn interrupts on when first starting up
//	a thread.
//----------------------------------------------------------------------
void
Interrupt::Enable()
{ 
    (void) SetLevel(IntOn); 
}

//----------------------------------------------------------------------
// Interrupt::OneTick
// 	Advance simulated time and check if there are any pending 
//	interrupts to be called. 
//
//	Two things can cause OneTick to be called:
//		interrupts are re-enabled
//		a user instruction is executed
//----------------------------------------------------------------------
void
Interrupt::OneTick()
{
    MachineStatus old = status;

// advance simulated time
    if (status == SystemMode) {
        stats->totalTicks += SystemTick;
	stats->systemTicks += SystemTick;
    } else {					// USER_PROGRAM
	stats->totalTicks += UserTick;
	stats->userTicks += UserTick;
    }
    DEBUG('i', "\n== Tick %d ==\n", stats->totalTicks);

// check any pending interrupts are now ready to fire
    ChangeLevel(IntOn, IntOff);		// first, turn off interrupts
					// (interrupt handlers run with
					// interrupts disabled)
    while (CheckIfDue(FALSE))		// check for pending interrupts
	;
    ChangeLevel(IntOff, IntOn);		// re-enable interrupts
    if (yieldOnReturn) {		// if the timer device handler asked 
					// for a context switch, ok to do it now
	yieldOnReturn = FALSE;
 	status = SystemMode;		// yield is a kernel routine
	currentThread->Yield();
	status = old;
    }
}

//----------------------------------------------------------------------
// Interrupt::YieldOnReturn
// 	Called from within an interrupt handler, to cause a context switch
//	(for example, on a time slice) in the interrupted thread,
//	when the handler returns.
//
//	We can't do the context switch here, because that would switch
//	out the interrupt handler, and we want to switch out the 
//	interrupted thread.
//----------------------------------------------------------------------

void
Interrupt::YieldOnReturn()
{ 
    ASSERT(inHandler == TRUE);  
    yieldOnReturn = TRUE; 
}

//----------------------------------------------------------------------
// Interrupt::Idle
// 	Routine called when there is nothing in the ready queue.
//
//	Since something has to be running in order to put a thread
//	on the ready queue, the only thing to do is to advance 
//	simulated time until the next scheduled hardware interrupt.
//
//	If there are no pending interrupts, stop.  There's nothing
//	more for us to do.
//----------------------------------------------------------------------
void
Interrupt::Idle()
{
    DEBUG('i', "Machine idling; checking for interrupts.\n");
    status = IdleMode;
    if (CheckIfDue(TRUE)) {		// check for any pending interrupts
    	while (CheckIfDue(FALSE))	// check for any other pending 
	    ;				// interrupts
        yieldOnReturn = FALSE;		// since there's nothing in the
					// ready queue, the yield is automatic
        status = SystemMode;
	return;				// return in case there's now
					// a runnable thread
    }

    // if there are no pending interrupts, and nothing is on the ready
    // queue, it is time to stop.   If the console or the network is 
    // operating, there are *always* pending interrupts, so this code
    // is not reached.  Instead, the halt must be invoked by the user program.

    DEBUG('i', "Machine idle.  No interrupts to do.\n");
    printf("No threads ready or runnable, and no pending interrupts.\n");
    printf("Assuming the program completed.\n");
    Halt();
}

//----------------------------------------------------------------------
// Interrupt::Halt
// 	Shut down Nachos cleanly, printing out performance statistics.
//----------------------------------------------------------------------
void
Interrupt::Halt()
{
    printf("Machine halting!\n\n");
    stats->Print();
    Cleanup();     // Never returns.
}

//----------------------------------------------------------------------
// Interrupt::Schedule
// 	Arrange for the CPU to be interrupted when simulated time
//	reaches "now + when".
//
//	Implementation: just put it on a sorted list.
//
//	NOTE: the Nachos kernel should not call this routine directly.
//	Instead, it is only called by the hardware device simulators.
//
//	"handler" is the procedure to call when the interrupt occurs
//	"arg" is the argument to pass to the procedure
//	"fromNow" is how far in the future (in simulated time) the 
//		 interrupt is to occur
//	"type" is the hardware device that generated the interrupt
//----------------------------------------------------------------------
void
Interrupt::Schedule(VoidFunctionPtr handler, _int arg, int fromNow, IntType type)
{
    int when = stats->totalTicks + fromNow;
    PendingInterrupt *toOccur = new PendingInterrupt(handler, arg, when, type);

    DEBUG('i', "Scheduling interrupt handler the %s at time = %d\n", 
					intTypeNames[type], when);
    ASSERT(fromNow > 0);

    pending->SortedInsert(toOccur, when);
}

//----------------------------------------------------------------------
// Interrupt::CheckIfDue
// 	Check if an interrupt is scheduled to occur, and if so, fire it off.
//
// Returns:
//	TRUE, if we fired off any interrupt handlers
// Params:
//	"advanceClock" -- if TRUE, there is nothing in the ready queue,
//		so we should simply advance the clock to when the next 
//		pending interrupt would occur (if any).  If the pending
//		interrupt is just the time-slice daemon, however, then 
//		we're done!
//----------------------------------------------------------------------
bool
Interrupt::CheckIfDue(bool advanceClock)
{
    MachineStatus old = status;
    int when;

    ASSERT(level == IntOff);		// interrupts need to be disabled,
					// to invoke an interrupt handler
    if (DebugIsEnabled('i'))
	DumpState();
    PendingInterrupt *toOccur = 
		(PendingInterrupt *)pending->SortedRemove(&when);

    if (toOccur == NULL)		// no pending interrupts
	return FALSE;			

    if (advanceClock && when > stats->totalTicks) {	// advance the clock
	stats->idleTicks += (when - stats->totalTicks);
	stats->totalTicks = when;
    } else if (when > stats->totalTicks) {	// not time yet, put it back
	pending->SortedInsert(toOccur, when);
	return FALSE;
    }

// Check if there is nothing more to do, and if so, quit
    if ((status == IdleMode) && (toOccur->type == TimerInt) 
				&& pending->IsEmpty()) {
	 pending->SortedInsert(toOccur, when);
	 return FALSE;
    }

    DEBUG('i', "Invoking interrupt handler for the %s at time %d\n", 
			intTypeNames[toOccur->type], toOccur->when);
#ifdef USER_PROGRAM
    if (machine != NULL)
    	machine->DelayedLoad(0, 0);
#endif
    inHandler = TRUE;
    status = SystemMode;			// whatever we were doing,
						// we are now going to be
						// running in the kernel
    (*(toOccur->handler))(toOccur->arg);	// call the interrupt handler
    status = old;				// restore the machine status
    inHandler = FALSE;
    delete toOccur;
    return TRUE;
}

//----------------------------------------------------------------------
// PrintPending
// 	Print information about an interrupt that is scheduled to occur.
//	When, where, why, etc.
//----------------------------------------------------------------------

static void
PrintPending(_int arg)
{
    PendingInterrupt *pend = (PendingInterrupt *)arg;

    printf("Interrupt handler %s, scheduled at %d\n", 
	intTypeNames[pend->type], pend->when);
}

//----------------------------------------------------------------------
// DumpState
// 	Print the complete interrupt state - the status, and all interrupts
//	that are scheduled to occur in the future.
//----------------------------------------------------------------------

void
Interrupt::DumpState()
{
    printf("Time: %d, interrupts %s\n", stats->totalTicks, 
					intLevelNames[level]);
    printf("Pending interrupts:\n");
    fflush(stdout);
    pending->Mapcar(PrintPending);
    printf("End of pending interrupts\n");
    fflush(stdout);
}

//+++++++++++实现Exec()
int Interrupt::Exec(){
    //输出信息，有一个Exec()的系统调用
        printf("Execute system call of Exec()\n");

        //从寄存器r4中读取文件名,r4中存放的实际为文件地址
        char filename[50];
        int address = machine->ReadRegister(4);
        //需要将文件地址转换为文件名称
        for(int i=0; ;i++){
            machine->ReadMem(address+i,1,(int *)&filename[i]);
            if(filename[i] == '\0'){//读到文件名称结尾
                break;
            }
        }

        //输出文件名称
        printf("Exec(%s):\n",filename);

        //为该文件创建线程，需要分配地址空间
        //addrspace有参数,打开文件
        OpenFile *executable = fileSystem->Open(filename);
        if(executable == NULL){//处理无法打开文件情况
            printf("can't open the %s!\n",filename);
            return -1;
        }

        //使用addrspace分配地址空间
        AddrSpace *addrspace = new AddrSpace(executable);

        //为当前文件创建线程
        Thread* thread = new Thread(filename);
        //原本使用fork实现，但由于获取当前线程startProcess(参考userprog/progtest.c文件)一直编译显示非法使用，所以改为直接调度
        //先将当前进程保存至原有队列，参考synch.cc中v（）的实现
        IntStatus oldLevel = interrupt->SetLevel(IntOff);
        scheduler->ReadyToRun(currentThread);
        (void) interrupt->SetLevel(oldLevel);
        //再调度当前进程,参考progtest.cc中startprogress的实现
        currentThread = thread;
        thread->space = addrspace;
        addrspace->InitRegisters();		// 设置寄存器值
        addrspace->RestoreState();		// 获取页表


        //系统调用完成后关闭文件
        delete executable;
        //由于Exec()系统调用有返回值spaceID，因此，使用r2寄存器将SpaceId返回
        machine->WriteRegister(2,addrspace->getSpaceID());

        machine->Run();//事实上machine->Run()后不会再执行下面语句，所以return可有可无
        ASSERT(FALSE);
        return addrspace->getSpaceID();
}


//++++++++++++实现PrintInt()
void Interrupt::PrintInt(){
    //获取寄存器的值
    int t = machine->ReadRegister(4);
    //输出寄存器的值
    printf("%d\n",t);
}