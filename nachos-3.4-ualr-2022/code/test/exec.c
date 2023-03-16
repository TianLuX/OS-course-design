//简单程序运行另一个用户程序
#include "syscall.h"
int main()
{
    SpaceId pid;
    PrintInt(12345);
    pid = Exec("../test/halt.noff");//如果打不开这个文件，请检查权限和文件名称
    Halt();
}