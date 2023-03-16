// addrspace.cc 
//	管理地址空间的例程（执行用户程序）.
//
//	要运行用户程序，需要：
//
//	1. 与-N-T 0选项链接
//	2. 运行coff2noff将对象文件转换为Nachos格式
//	3. 将NOFF文件加载到Nachos文件系统中
//


#include "copyright.h"
#include "system.h"
#include "addrspace.h"
#include "time.h"
// #include "noff.h"

//+++++++++对物理页表位图进行初始化，大小为NumPhysPages，物理页允许大小
BitMap *AddrSpace::physMap = new BitMap(NumPhysPages);
//++++++++++++++++++++++++++++++++

//----------------------------------------------------------------------
// SwapHeader
// 对对象文件头中的字节进行小端到大端的转换，以防文件是在小端机器上生成的，而我们现在在大端机器上运行.
//----------------------------------------------------------------------

static void 
SwapHeader (NoffHeader *noffH)
{
	noffH->noffMagic = WordToHost(noffH->noffMagic);
	noffH->code.size = WordToHost(noffH->code.size);
	noffH->code.virtualAddr = WordToHost(noffH->code.virtualAddr);
	noffH->code.inFileAddr = WordToHost(noffH->code.inFileAddr);
	noffH->initData.size = WordToHost(noffH->initData.size);
	noffH->initData.virtualAddr = WordToHost(noffH->initData.virtualAddr);
	noffH->initData.inFileAddr = WordToHost(noffH->initData.inFileAddr);
	noffH->uninitData.size = WordToHost(noffH->uninitData.size);
	noffH->uninitData.virtualAddr = WordToHost(noffH->uninitData.virtualAddr);
	noffH->uninitData.inFileAddr = WordToHost(noffH->uninitData.inFileAddr);
}

//----------------------------------------------------------------------
// AddrSpace::AddrSpace
// 创建一个地址空间来运行一个用户程序。从一个“可执行”文件中加载程序，并设置好一切，以便我们可以开始执行用户指令。
//
//	假定目标代码文件为NOFF格式。
//
//	首先，设置从程序内存到物理内存的转换。现在，这真的很简单（1:1），
//	因为我们只进行uniprogramming，并且我们有一个未分段的页表
//
//	“可执行文件”是包含要加载到内存中的目标代码的文件
//----------------------------------------------------------------------

AddrSpace::AddrSpace(OpenFile *executable)
{
    //++++++++++++cl add++++++++++++
    this->executable = executable;
    usedFrame = 0;
    maxFrame = 6;
    //++++++++++++cl add++++++++++++

    //+++++++++++++++++++++分配对应spaceID
    ASSERT(ThreadMap->NumClear() >= 1);
    spaceID = ThreadMap->Find();
    //+++++++++++++++++++++++++++++++++

    //++++++++++++cl add+++++++++++++++++
    printf("SpaceId: %d, Memory size: %d\n", spaceID, maxFrame);
    printf("Max frames per user process: %d, Swap file: SWAP, Page replacement algorithm: FIFO\n", NumPhysPages);
    //++++++++++++cl add+++++++++++++++++

    //目标代码文件，为该文件分配空间
    unsigned int i, size;
    
    //读取该文件，描述文件
    executable->ReadAt((char *)&noffH, sizeof(noffH), 0);
    if ((noffH.noffMagic != NOFFMAGIC) && 
		(WordToHost(noffH.noffMagic) == NOFFMAGIC))
    	SwapHeader(&noffH);
    ASSERT(noffH.noffMagic == NOFFMAGIC);

    //+++++++++++++cl add+++++++++++++
    int swapSize = divRoundUp(noffH.uninitData.size, PageSize) 
            + divRoundUp(UserStackSize, PageSize);
    numPages = divRoundUp(noffH.code.size, PageSize)
        + divRoundUp(noffH.initData.size, PageSize)
        + swapSize;
    size = numPages * PageSize;
    swapSize = swapSize * PageSize;

    // 创建交换区，uninitdata和stack
    fileSystem->Create("SWAP", swapSize);
    this->swapFile = fileSystem->Open("SWAP");
    char temp[swapSize];
    memset(temp, 0, sizeof(temp));
    this->swapFile->WriteAt(temp, swapSize, 0);

    DEBUG('a', "Initializing address space, num pages %d, size %d\n", numPages, size);

    // 初始化页表相关
    pageTable = new TranslationEntry[numPages];
    pageType = new int[numPages];
    initPage();

    // 输出页表
    Print();

    // 将整个地址空间归零，将单位化数据段和堆栈段归零
    bzero(machine->mainMemory, NumPhysPages * PageSize);

    //+++++++++++++cl add+++++++++++++
}



//----------------------------------------------------------------------
// AddrSpace::~AddrSpace
// 	释放地址空间。
//----------------------------------------------------------------------

AddrSpace::~AddrSpace()
{
    //++++++++++++释放spaceID对应的进程号
    ThreadMap->Clear(spaceID);
    //++++++++++++释放物理页，物理页对应编号为pageTable[i].physicalPage
    for(int i=0;i<numPages; i++){
        physMap->Clear(pageTable[i].physicalPage);
    }
    //+++++++++++++++++++
   delete [] pageTable;
   
}


//----------------------------------------------------------------------
// AddrSpace::InitRegisters
// 	设置用户级寄存器集的初始值。
//
// 我们将这些直接写入“机器”寄存器，这样我们就可以立即跳转到用户代码。
// 请注意，当这个线程被上下文切换时，这些将被保存/恢复到currentThread->userRegisters中。
//----------------------------------------------------------------------

void
AddrSpace::InitRegisters()
{
    int i;

    for (i = 0; i < NumTotalRegs; i++){
        machine->WriteRegister(i, 0);
    }

    // 初始程序计数器--必须是“Start”的位置
    machine->WriteRegister(PCReg, 0);	

    // 由于分支延迟的可能性，还需要告诉MIPS下一条指令在哪里
    machine->WriteRegister(NextPCReg, 4);

   // 将堆栈寄存器设置到地址空间的末尾，在那里我们分配了堆栈；但是减去一点，以确保我们不会意外地引用结尾！
    machine->WriteRegister(StackReg, numPages * PageSize - 16);
    DEBUG('a', "Initializing stack register to %d\n", numPages * PageSize - 16);
}

//----------------------------------------------------------------------
// AddrSpace::SaveState
// 	在上下文开关上，保存特定于此地址空间的任何需要保存的机器状态。
//
//----------------------------------------------------------------------

void AddrSpace::SaveState() 
{
}

//----------------------------------------------------------------------
// AddrSpace::RestoreState
// 	在上下文开关上，恢复计算机状态，以便此地址空间可以运行。
//
//  现在，告诉机器在哪里可以找到页面表。
//----------------------------------------------------------------------

void AddrSpace::RestoreState() 
{
    machine->pageTable = pageTable;
    machine->pageTableSize = numPages;
}


//++++++++++++cl add++++++++++++
void
AddrSpace::addrToPageNumAndOffset(int addr, unsigned int &pageNum, unsigned int &offset) {
    // code segment
    int pos = 0;
    int prePage = 0;
    if (addr >= pos && addr < pos + noffH.code.size) {
        pageNum = prePage + (addr - pos) / PageSize;
        offset = (addr - pos) % PageSize;
        return;
    }
    // initData segment
    prePage += divRoundUp(noffH.code.size, PageSize);
    pos += noffH.code.size;
    if (addr >= pos && addr < pos + noffH.initData.size) {
        pageNum = prePage + (addr - pos) / PageSize;
        offset = (addr - pos) % PageSize;
        return;
    }
    // uninitData segment
    prePage += divRoundUp(noffH.initData.size, PageSize);
    pos += noffH.initData.size;
    if (addr >= pos && addr < pos + noffH.uninitData.size) {
        pageNum = prePage + (addr - pos) / PageSize;
        offset = (addr - pos) % PageSize;
        return;
    }
    // stack segment
    if (addr >= pos && addr < numPages * PageSize) {
        pageNum = numPages - (numPages * PageSize - addr) / PageSize - 1;
        offset = PageSize - (numPages * PageSize - addr) % PageSize;
        return;
    }
    // overflow
    ASSERT(false);
    return;
}


void
AddrSpace::demandPaging(int demandPage) {
    // 如果页面有空闲进行纯请求调页，否则进行页面置换
    comeQueue.push(demandPage);
    stats->pagingFaultsNum++;
    int newFrame = -1;
    if (usedFrame < maxFrame) {
        newFrame = physMap->Find();
        ++usedFrame;
    }
    if (newFrame != - 1) {
        pureDemandPaging(demandPage, newFrame);
    } else {
	    FIFO(demandPage);
    }
}

void
AddrSpace::pureDemandPaging(int demandPage, int newFrame) {

    printf("Demand page = %d in frame = %d\n", demandPage, newFrame);

    // 更新页表
    pageTable[demandPage].virtualPage = demandPage;
    pageTable[demandPage].physicalPage = newFrame;
    pageTable[demandPage].valid = TRUE;
    pageTable[demandPage].readOnly = FALSE;
    pageTable[demandPage].use = FALSE;
    pageTable[demandPage].dirty = FALSE;

    // 从磁盘中读到内存中
    ReadIn(demandPage);
    // 输出更新信息
	Print();
}

void 
AddrSpace::swap(int demandPage, int victim) {

    // 写回victim
    WriteBack(victim);

    // 修改目前页面的属性
	pageTable[demandPage].physicalPage = pageTable[victim].physicalPage;
	pageTable[demandPage].valid = TRUE;
	pageTable[demandPage].dirty = FALSE;
	pageTable[demandPage].use = TRUE;
	pageTable[demandPage].readOnly = FALSE;

    // 修改victim页面的属性
	pageTable[victim].valid = FALSE;
	pageTable[victim].physicalPage = -1;
	pageTable[victim].dirty = FALSE;
	pageTable[victim].use = FALSE;
	pageTable[demandPage].readOnly = FALSE;


    // 加载页面
    ReadIn(demandPage);
}


void
AddrSpace::FIFO(int demandPage) {
    // 假设每一次都替换1号页面
    int victim = comeQueue.front();
    comeQueue.pop();
	printf("Swap page %d out, demand page %d in frame %d\n", victim, demandPage, pageTable[victim].physicalPage);

    // 交换
    swap(demandPage, victim);
    // 输出更新信息
	Print();
}

void 
AddrSpace::WriteBack(int page) {
    if (!pageTable[page].dirty) 
        return;
    stats->writeBackNum++;
    switch (pageType[page]) {
        case CODE:
            executable->WriteAt(&(machine->mainMemory[pageTable[page].physicalPage * PageSize]), 
                PageSize, noffH.code.inFileAddr + PageSize * page);
            break;
        case INITDATA:
            executable->WriteAt(&(machine->mainMemory[pageTable[page].physicalPage * PageSize]), 
                PageSize, noffH.initData.inFileAddr + 
                    PageSize * (page - divRoundUp(noffH.code.size, PageSize)));
            break;
        case UNINITDATA:
        case STACK:
            swapFile->WriteAt(&(machine->mainMemory[pageTable[page].physicalPage * PageSize]), 
                PageSize, PageSize * (page - divRoundUp(noffH.code.size, PageSize)
                             - divRoundUp(noffH.initData.size, PageSize)));
            break;
    }
}

void
AddrSpace::ReadIn(int page) {
    switch (pageType[page]) {
        case CODE:
	        executable->ReadAt(&(machine->mainMemory[pageTable[page].physicalPage * PageSize]), 
                PageSize, noffH.code.inFileAddr + PageSize * page);
            break;
        case INITDATA:
            executable->ReadAt(&(machine->mainMemory[pageTable[page].physicalPage * PageSize]), 
                PageSize, noffH.initData.inFileAddr + 
                    PageSize * (page - divRoundUp(noffH.code.size, PageSize)));
            break;
        case UNINITDATA:
        case STACK:
            swapFile->ReadAt(&(machine->mainMemory[pageTable[page].physicalPage * PageSize]), 
                PageSize, PageSize * (page - divRoundUp(noffH.code.size, PageSize)
                             - divRoundUp(noffH.initData.size, PageSize)));
        break;
    }
}


void
AddrSpace::Print() {
    printf("SpaceId: %d, page table dump: %d pages in total\n", spaceID, numPages);
    printf("============================================\n");
    printf("Page, \tFrame, \tValid, \tUse, \tDirty\n");
    for (int i=0; i < numPages; i++) {
        printf("  %d, \t%d, \t%d, \t%d, \t%d\n", 
        pageTable[i].virtualPage, pageTable[i].physicalPage,
        pageTable[i].valid, pageTable[i].use, pageTable[i].dirty);
    }
    printf("============================================\n\n");
}

void
AddrSpace::initPage() {
    // 初始化页表
    for (int i = 0; i < numPages; i++) {
        pageTable[i].virtualPage = i;
        pageTable[i].physicalPage = -1;
        pageTable[i].valid = FALSE;
        pageTable[i].use = FALSE;
        pageTable[i].dirty = FALSE;
        pageTable[i].readOnly = FALSE;
    }
    // 初始化页表类型
    int sep[4];
    int pos = 0;
    // Code segment
    pos += divRoundUp(noffH.code.size, PageSize);
    sep[0] = pos;
    for (int i = 0; i < sep[0]; i++) {
        pageType[i] = CODE;
    }
    // initData
    pos += divRoundUp(noffH.initData.size, PageSize);
    sep[1] = pos;
    for (int i = sep[0]; i < sep[1]; i++) {
        pageType[i] = INITDATA;
    }
    // uninitData
    pos += divRoundUp(noffH.uninitData.size, PageSize);
    sep[2] = pos;
    for (int i = sep[1]; i < sep[2]; i++) {
        pageType[i] = UNINITDATA;
    }
    // Stack
    for (int i = sep[2]; i < numPages; i++) {
        pageType[i] = STACK;
    }

}

//++++++++++++cl add++++++++++++
void
AddrSpace::LRU(int demandPage) {
    int max = 0, maxPage = 0;
    for (int i = 0; i < comeQueue.size(); i++)
    {
        int page = comeQueue.front();
        comeQueue.pop();
        if (pageTime[page] > max) {
            maxPage = page;
            max = pageTime[page];
        }
        comeQueue.push(page);
    }
    for (int i = 0; i < comeQueue.size(); i++)
    {
        int page = comeQueue.front();
        comeQueue.pop();
        if (maxPage == page) {
            break;
        }
        comeQueue.push(page);
    }

    int victim = maxPage;
    pageTime[demandPage] = 0;
    printf("Swap page %d out, demand page %d in frame %d\n", victim, demandPage, pageTable[victim].physicalPage);
    // 交换
    swap(demandPage, victim);
    // 输出更新信息
    Print();
}
void
AddrSpace::enhanceSecondChance(int demandPage) {
    int old = comeQueue.front();
    comeQueue.pop();
    int i = 0;
    while (old != oldestPage && i < comeQueue.size()) {
        comeQueue.push(old);
        old = comeQueue.front();
        comeQueue.pop();
        i++;
    }
    while (pageChance[old] != 0) {
        pageChance[old] = 0;
        comeQueue.push(old);
        old = comeQueue.front();
        comeQueue.pop();
    }
    oldestPage = comeQueue.front();
    pageChance[demandPage] = 1;
    int victim = old;
    printf("Swap page %d out, demand page %d in frame %d\n", victim, demandPage, pageTable[victim].physicalPage);
    // 交换
    swap(demandPage, victim);
    // 输出更新信息
    Print();
}

void
AddrSpace::SecondChance(int demandPage) {
    int old = comeQueue.front();
    comeQueue.pop();
    while (pageChance[old] != 0) {
        pageChance[old] = 0;
        comeQueue.push(old);
        old = comeQueue.front();
        comeQueue.pop();
    }

    pageChance[demandPage] = 1;
    int victim = old;
    printf("Swap page %d out, demand page %d in frame %d\n", victim, demandPage, pageTable[victim].physicalPage);
    // 交换
    swap(demandPage, victim);
    // 输出更新信息
    Print();
}
void
AddrSpace::randPage(int demandPage) {
    int r = ((time(0) % 1564) % (comeQueue.size()));

    int victim = comeQueue.front();
    comeQueue.pop();
    for (int i = 0; i < r; i++)
    {
        comeQueue.push(victim);
        victim = comeQueue.front();
        comeQueue.pop();
    }

    printf("Swap page %d out, demand page %d in frame %d\n", victim, demandPage, pageTable[victim].physicalPage);
    // 交换
    swap(demandPage, victim);
    // 输出更新信息
    Print();
}


void
AddrSpace::addLRUtime() {
    int i = comeQueue.size() - 1;
    int n = comeQueue.front();
    comeQueue.pop();

    while (i >= 0) {
        pageTime[n];
    }
}

void
AddrSpace::processInQueue(int demandPage) {
    pageChance[demandPage] = 1;
    pageTime[demandPage] = 0;

}