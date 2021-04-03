#include <cstdio>



int main(int argc, char** argv)
{
    pid_t pid;
    int i = 0;
    for(i; i < 3; ++i)
    {
        if((pid = fork()) == 0)
        {
            print("I am child process, my pid is: %ld\n", (unsigned long)pid);
            return;
        }
        else
            print("I am father process, my pid is: %ld\n", (unsigned long)getpid());
    }


    return 0;
}