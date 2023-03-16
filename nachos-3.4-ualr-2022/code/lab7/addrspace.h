// addrspace.h 
//跟踪执行用户程序的数据结构(地址空间).
//
//目前，我们不保留任何关于地址空间的信息。
//用户级CPU状态在执行用户程序的线程中保存和恢复（参见thread.h)。


#ifndef ADDRSPACE_H
#define ADDRSPACE_H

#include "copyright.h"
#include "filesys.h"
#include "bitmap.h"
#include "noff.h"
#include "translate.h"
#include <queue>

//+++++++++++++++++++#include "system.h"不能有这个包否则会编译错误，可以在addrspace.cc中导入

#define UserStackSize		1024 	// 必要时增加此值！

class AddrSpace {

  public:
    AddrSpace(OpenFile *executable);	//创建一个地址空间，用存储在“可执行文件”中的程序初始化它
    ~AddrSpace();			//取消分配地址空间

    void InitRegisters();		//在跳转到用户代码之前，初始化用户级CPU寄存器

    void SaveState();			// 保存/还原特定地址空间
    void RestoreState();		// 上下文切换的信息

    //+++++++++在这里定义的原因是在addrspace.cc中定义显示spaceID非法
    int getSpaceID(){
        return spaceID;
    }//获取spaceID
    //+++++++++++++++++++++++++++++++++++++++++++
    //++++++++在这里定义的原因是在addrspace.cc中定义显示numPages、pageTable非法
    //多用户程序驻留内存的情况
    //+++++++++++++++++++++++++

    //++++++++++++cl add++++++++++++
    // 将逻辑地址，转化为页号和偏移量
    void addrToPageNumAndOffset(int addr, unsigned int &pageNum, unsigned int &offset);

    // 缺页错误后进行请求调页
    void demandPaging(int needPage);

    // 纯请求调页
    void pureDemandPaging(int needPage, int newFrame);

    // 页面置换算法
    void FIFO(int needPage);
    void LRU(int needPage);
    void SecondChance(int demandPage);
    void enhanceSecondChance(int  needPage);
    void randPage(int needPage);
    void processInQueue(int needPage);
    void addLRUtime();
    // SWAP
    void swap(int oldPage, int victim);
    void ReadIn(int page);
    void WriteBack(int page);

    // 输出页表
    void Print();

    TranslationEntry *pageTable;	// 页表，现在假设线性页表交换！
    //++++++++++++cl add++++++++++++


  private:
    unsigned int numPages;		// 虚拟地址空间中的页数

    //++++++++++++++++++++++++
    unsigned int spaceID;//空间编号
    static BitMap *physMap;//设置管理物理页表位图。
    //++++++++++++++++++++++++物理页表位图由addrspace进行管理，线程编号位图由system进行管理

    //++++++++++++cl add++++++++++++
    OpenFile *executable;   // code segment & initData segment
    OpenFile *swapFile;     // uninitData segment & 
    NoffHeader noffH;

    int usedFrame;        // 已经使用的帧数
    int maxFrame;         // 一个进程最大可以用的帧数

    int* pageTime;        //LRU use
    int* pageChance;     //secondChange Need
    int  oldestPage;
    int *pageType;
    void initPage();
    enum {CODE, INITDATA, UNINITDATA, STACK};

    // FIFO queue
    std::queue<int> comeQueue;
    //++++++++++++cl add++++++++++++


};

#endif // ADDRSPACE_H
