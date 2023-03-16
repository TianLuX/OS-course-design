// exception.cc 
//	从用户程序进入Nachos内核的入口点。有两种情况会导致控制从用户代码转移回此处：
//
//	syscall -- 用户代码明确请求调用Nachos内核中的过程。
//	现在，我们支持的唯一功能是"Halt".
//
//	exceptions -- 用户代码做了CPU无法处理的事情。
//	例如，访问不存在的内存、算术错误等.
//
//	中断（也可能导致控制从用户代码转移到Nachos内核）在其他地方处理.
//
// For now, this only handles the Halt() system call.
// 所有其他核心转储.

#include "copyright.h"
#include "system.h"
#include "syscall.h"
//++++++++++++++++++++++导入包
#include "machine.h"
#include "addrspace.h"
#include "synch.h"

//++++++++++++++++++++++++++++++++++++++++++++++++++增加pc值
void AdvancePC(){
    //前一个PC
    machine->WriteRegister(PrevPCReg,machine->ReadRegister(PCReg));
    //当前PC
    machine->WriteRegister(PCReg, machine->ReadRegister(PCReg) + 4);
    //下一个PC
    machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg) + 4);
}
//+++++++++++++++++++++++++++++++++++++++++++++++++++

//----------------------------------------------------------------------
// ExceptionHandler
// 	Nachos内核的入口点. 当用户程序正在执行时调用，并执行系统调用或生成寻址或算术异常。
//
// 	For system calls, the following is the calling convention:
//
//  r为寄存器编号
// 	system call code -- r2
//		arg1 -- r4
//		arg2 -- r5
//		arg3 -- r6
//		arg4 -- r7
//
//	系统调用的结果（如果有）必须放回r2.
//
//  返回前别忘了增加pc。（否则，您将永远循环进行相同的系统调用！
//
//	"which" is the kind of exception.  The list of possible exceptions 
//	are in machine.h.
//----------------------------------------------------------------------

void ExceptionHandler(ExceptionType which)
{
    int type = machine->ReadRegister(2);

    if ((which == SyscallException) && (type == SC_Halt)) {
	DEBUG('a', "Shutdown, initiated by user program.\n");
   	interrupt->Halt();
    }
    //+++++++++++++++++++实现系统调用Exec()的判断
    else if((which == SyscallException) && (type == SC_Exec)) {
        //原本将Exec()内容写在此处，但参考Halt()的实现，应该写在中断处理中
        interrupt->Exec();
        //推进PC值
        AdvancePC();
        return;
    }
    //++++++++++++++++++++++++++需要实现系统调用PrintInt(int t)的判断
    else if((which == SyscallException) && (type == SC_PrintInt)){
        interrupt->PrintInt();
        //推进PC值
        AdvancePC();
        return;
    }
    //+++++++++++++++++++++++++++++++++++++++++++

    else {
	printf("Unexpected user mode exception %d %d\n", which, type);
	ASSERT(FALSE);
    }
}
