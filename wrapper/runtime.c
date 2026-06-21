#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

typedef struct
{
    void **start;
    void **end;
} PCTableEntry;

static int current_entry = 0;

static int current_size = 1000;

static PCTableEntry * entries = NULL;

void __runtime_register_pctable(void **start, void **end)
{
    if (entries == NULL)
    { // first call
        entries = (PCTableEntry *)malloc(sizeof(PCTableEntry) * current_size);
        if (entries == NULL)
        {
            fprintf(stderr, "malloc couldn't work\n");
            exit(1);
        }
    }
    else if (current_entry >= current_size)
    { // need to grow
        current_size *= 2;
        PCTableEntry *new_entries = (PCTableEntry *)realloc(entries, sizeof(PCTableEntry) * current_size);
        if (new_entries == NULL)
        {
            fprintf(stderr, "realloc couldn't work\n");
            exit(1);
        }
        entries = new_entries;
    }
    entries[current_entry].start = start;
    entries[current_entry].end = end;
    ++current_entry;
}

void __runtime_dump_pctables(void)
{
    FILE * fp = fopen(".bblist", "w");
    if (fp == NULL) {
        fprintf(stderr, "fopen bblist couldn't work\n");
        exit(1);
    }
    int funcBlk = 0;
    for (int i = 0; i < current_entry; i++) {
        PCTableEntry * entry = &entries[i];
        fprintf(fp, "Function:%d\n", funcBlk++);
        int blockIdx = 0;
        for (void ** start = entry->start; start != entry->end; start++) {
            fprintf(fp, "Block: %d -> %p\n", blockIdx++, *start);
        }
    }
    fclose(fp);
}
