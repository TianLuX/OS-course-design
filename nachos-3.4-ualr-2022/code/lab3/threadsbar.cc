#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "copyright.h"
#include "system.h"

#include "synch.h"


#define N_THREADS  10    // the number of threads
#define N_TICKS    1000  // 提前模拟时间的节拍数
#define MAX_NAME  16 // 名称的最大长度

//创建n个线程
Thread *threads[N_THREADS];

//线程名称的字符串数组
char threads_name[N_THREADS][MAX_NAME];


Semaphore *mutex;//互斥信号
Semaphore *barrier;//屏障信号量
Semaphore *s;//用于增加上下文切换的信号量

int count;


void MakeTicks(int n)  // 进行了n个模拟时间
{
    //...
    //模拟时间
    //空循环
//   while(n>0){
//       n--;
//   }
    //linux的sleep函数
//   sleep(n);
     s = new Semaphore("s", 1);
     for(int i=n;i>0;i--){
         s->P();
         s->V();
     }

   //...
}


void BarThread(_int which)
{
    MakeTicks(N_TICKS);
    printf("Thread %d rendezvous\n", which);

    //...
    mutex->P();

    count = count + 1;
    if(count == N_THREADS){
        printf("Thread %d is the last\n", which);
        barrier->V();
    }

    mutex->V();

    barrier->P();
    barrier->V();
    //...

    printf("Thread %d critical point\n", which);
}


void ThreadsBarrier()
{
    //...
    //初始化信号量
    mutex = new Semaphore("mutex", 1);
    barrier = new Semaphore("barrier", 0);

    count = 0;
    //...

    // 创建并分叉N_THEADS线程
    for(int i = 0; i < N_THREADS; i++) {
        //...
        // 形成一个字符串，用作线程的名称。
        sprintf(threads_name[i], "Thread %d", i);
        //创建并分叉一个新的生产者线程
        threads[i] = new Thread(threads_name[i]);
        //...
        threads[i]->Fork(BarThread, i);
    };
}
