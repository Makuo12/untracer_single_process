#include <stdio.h>
#include <stdint.h>
#include <vector>
#include <fstream>

using namespace std;

struct PCTableEntry
{
    void **start;
    void **end;
};



// Function-local static — constructed on first call, guaranteed safe
static std::vector<PCTableEntry> & __runtime_getAllTables()
{
    static std::vector<PCTableEntry> AllTables;
    return AllTables;
}

extern "C" void __runtime_register_pctable(void **start, void **end)
{
    __runtime_getAllTables().push_back({start, end});
}

extern "C" void __runtime_dump_pctables()
{
    ofstream file(".bblist");
    auto &AllTables = __runtime_getAllTables();
    int funcIdx = 0;
    for (auto &T : AllTables)
    {
        int blockIdx = 0;
        file << "Function " << funcIdx++ << endl;
        // printf("Function %d:\n", funcIdx++);
        for (void **p = T.start; p != T.end; p++, blockIdx++) {
            file << "Block " << blockIdx << " → " << std::hex << *p << endl;
            // printf("  Block %d → %p\n", blockIdx, *p);
        }
        
    }
    file.close();
}

extern "C" void *__runtime_get_block_addr(int funcIdx, int blockIdx)
{
    auto &AllTables = __runtime_getAllTables();
    if (funcIdx < 0 || funcIdx >= (int)AllTables.size())
        return nullptr;
    PCTableEntry &T = AllTables[funcIdx];
    void **ptr = T.start + blockIdx;
    if (ptr >= T.end)
        return nullptr;
    return *ptr;
}

extern "C" int __runtime_num_functions()
{
    return (int)__runtime_getAllTables().size();
}

extern "C" int __runtime_num_blocks(int funcIdx)
{
    auto &AllTables = __runtime_getAllTables();
    if (funcIdx < 0 || funcIdx >= (int)AllTables.size())
        return 0;
    PCTableEntry &T = AllTables[funcIdx];
    return (int)(T.end - T.start);
}