// test.cpp
#include <stdio.h>

// Declare the runtime API
extern "C" void __runtime_dump_pctables();

int add(int a, int b)
{
    if (a > 0)
    {
        return a + b;
    }
    else if (a == 0)
    {
        return b;
    } else {
        return b;
    }
}

int main()
{
    // By the time main() runs, all ctors have already fired
    // and all tables are registered — safe to query now

    __runtime_dump_pctables();

    return add(-1, 2);
}