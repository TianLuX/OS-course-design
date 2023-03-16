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
#include "noff.h"

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
    //+++++++++++++++++++++分配对应spaceID
    //保证有进程号可以分配
    ASSERT(ThreadMap->NumClear() >= 1);
    //进行分配,find函数会找到没有分配的，并进行分配
    spaceID = ThreadMap->Find();
    printf("spaceID:%d\n",spaceID);
    //+++++++++++++++++++++++++++++++++

    NoffHeader noffH;//目标代码文件，为该文件分配空间
    unsigned int i, size;

    executable->ReadAt((char *)&noffH, sizeof(noffH), 0);//读取该文件
    if ((noffH.noffMagic != NOFFMAGIC) && 
		(WordToHost(noffH.noffMagic) == NOFFMAGIC))
    	SwapHeader(&noffH);
    ASSERT(noffH.noffMagic == NOFFMAGIC);

// 地址空间 = 代码大小+初始化数据大小+未初始化数据大小+栈空间大小
    size = noffH.code.size + noffH.initData.size + noffH.uninitData.size 
			+ UserStackSize;	// 我们需要加大尺寸，为栈留出空间
            //根据每一页的大小，计算需要多少页
    numPages = divRoundUp(size, PageSize);
    //总空间大小未页码数*页大小
    size = numPages * PageSize;

    ASSERT(numPages <= NumPhysPages);		//检查我们没有试图运行太大的东西，至少在我们有虚拟内存之前

    DEBUG('a', "Initializing address space, num pages %d, size %d\n", 
					numPages, size);
// 首先设置交换
    pageTable = new TranslationEntry[numPages];
    for (i = 0; i < numPages; i++) {
        pageTable[i].virtualPage = i;
        //+++++++++++++++++++物理页从空闲页管理中进行分配
        pageTable[i].physicalPage = physMap->Find();//在physMap中找到空闲页进行分配
        //++++++++++++++++++++++++++++++++
        pageTable[i].valid = TRUE;//已经放入
        pageTable[i].use = FALSE;//未使用
        pageTable[i].dirty = FALSE;//脏
        pageTable[i].readOnly = FALSE;//只读  //如果代码段完全位于单独的页面上，我们可以将其页面设置为只读
    }
    //++++++++++++++查看页分配情况
    Print();
    //++++++++++++++++
    
// 将整个地址空间归零，将单位化数据段和堆栈段归零
    bzero(machine->mainMemory, size);

// 然后，将代码和数据段复制到内存中
    //代码大小

    //+++++++++++++此处有虚拟内存向物理内存的转化
    if (noffH.code.size > 0) {
        DEBUG('a', "Initializing code segment, at 0x%x, size %d\n", 
			noffH.code.virtualAddr, noffH.code.size);
        //+++++++++++++++++++++++
        int code_page = noffH.code.virtualAddr/PageSize;//含有的页数
        //物理页中按照顺序分配，所以从第code_page开始分配
        int code_phy_addr = pageTable[code_page].physicalPage *PageSize;//代码的物理地址
        
        executable->ReadAt(&(machine->mainMemory[code_phy_addr]),
			noffH.code.size, noffH.code.inFileAddr);
        //+++++++++++++++读取实际物理地址
    }
    //数据大小

    if (noffH.initData.size > 0) {
        DEBUG('a', "Initializing data segment, at 0x%x, size %d\n", 
			noffH.initData.virtualAddr, noffH.initData.size);
        //+++++++++++++++++++++++
        int data_page = noffH.initData.virtualAddr/PageSize;//含有的页数
		int data_offset = noffH.initData.virtualAddr%PageSize;//页内偏移
		int data_phy_addr = pageTable[data_page].physicalPage *PageSize + data_offset;//数据的物理地址

        executable->ReadAt(&(machine->mainMemory[data_phy_addr]),
			noffH.initData.size, noffH.initData.inFileAddr);
        //++++++++++++++++++++++
    }

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
