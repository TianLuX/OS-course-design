// threadtest.cc 
//	Simple test case for the threads assignment.
//
//	Create two threads, and have them context switch
//	back and forth between themselves by calling Thread::Yield, 
//	to illustratethe inner workings of the thread system.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"
#include <unistd.h>


//----------------------------------------------------------------------
// SimpleThread
// 	Loop 5 times, yielding the CPU to another ready thread 
//	each iteration.
//
//	"which" is simply a number identifying the thread, for debugging
//	purposes.
//----------------------------------------------------------------------
// =======================================4.测试============================================

//全局变量
Thread *t1;
Thread *t2;
Thread *t3;

void
SimpleThread(_int which)
{
    int num;  
    for (num = 0; num < 5; num++) {
        printf("\n*** thread %d looped %d times, priority=%d\n", 
          (int) which, num, currentThread->getPriority());//调整打印格式
               
// =======================================(10)============================================
        int ticks = stats->systemTicks-scheduler->GetLastSwitchTick();
        DEBUG('t', "last=%d, total=%d, p1=%d, p2=%d, p3=%d\n", 
          scheduler->GetLastSwitchTick(), stats->systemTicks, t1->getPriority(), t2->getPriority(), t3->getPriority());
// =======================================(10)============================================

        if(num!=4) currentThread->Yield(); 
        else currentThread->Finish();   
        //虽然只需5个时间片完成循环，但是需要第6个时间片来判断线程已结束，导致3个线程共多了3次上下文切换
        //为了减少最后一次的上下文切换，令线程在for循环中通过循环次数直接判断自身是否结束，通过finish提前本线程的终止时间
        
    }
}

//----------------------------------------------------------------------
// ThreadTest
// 	Set up a ping-pong between two threads, by forking a thread 
//	to call SimpleThread, and then calling SimpleThread ourselves.
//----------------------------------------------------------------------

void
ThreadTest()
{
    DEBUG('t', "Entering SimpleTest");
    
    t1 = new Thread("t1", 6);//JUST_CREATED
    t2 = new Thread("t2", 7);
    t3 = new Thread("t3", 8);
    
    t1->Fork(SimpleThread, 1); //READY
    t2->Fork(SimpleThread, 2);
    t3->Fork(SimpleThread, 3);
    
    //SimpleThread(0);
}
// =======================================4.测试============================================

