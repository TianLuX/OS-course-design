//完成一个系统调用输出一个数字并且关闭操作系统
#include "syscall.h"
int main()
{
    PrintInt(67890);
    Halt();
}