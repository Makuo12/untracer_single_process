#define _GNU_SOURCE
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <sys/ucontext.h>
#include <sys/mman.h>
#include <unistd.h>
#include "uthash.h"

extern int target_main(int argc, char **argv);



typedef struct {
    uintptr_t addr_value;
    uintptr_t addr_offset;
    unsigned char original;
    size_t block_index;
} breakpoint;

typedef struct  {
    uintptr_t key;
    breakpoint value;
    UT_hash_handle hh;
} map;

map *hash_map = NULL; // global or scoped hash table pointer

map * __untracer_find_breakpoint(map *hash_map, uintptr_t key)
{
    map *entry = NULL;
    HASH_FIND(hh, hash_map, &key, sizeof(uintptr_t), entry);
    return entry; // NULL if not found
}

void __untracer_add_breakpoint(map **hash_map, uintptr_t key, breakpoint bp)
{
    map *entry = (map *)malloc(sizeof(map));

    entry->key = key;
    entry->value = bp;

    HASH_ADD(hh, *hash_map, key, sizeof(uintptr_t), entry);
}

static void __untracer_make_writable(uint64_t addr)
{
    // align down to page boundary
    long page_size = sysconf(_SC_PAGESIZE);
    uint64_t page = addr & ~(page_size - 1);
    mprotect((void *)page, page_size, PROT_READ | PROT_WRITE | PROT_EXEC);
}

void __untracer_sigtrap_handler(int sig, siginfo_t *info, void *ctx) {
    ucontext_t *uc = (ucontext_t *)ctx;
#ifdef __linux__
    uintptr_t rip = uc->uc_mcontext.gregs[REG_RIP];
#elif defined(__APPLE__) && defined(__x86_64__)
    uintptr_t rip = uc->uc_mcontext->__ss.__rip;
#elif defined(__APPLE__) && defined(__aarch64__)
    uintptr_t rip = uc->uc_mcontext->__ss.__pc;
#endif
    uintptr_t addr = rip - 1;
    fprintf(stderr, "TRAP at RIP=0x%lx addr=0x%lx\n", rip, addr); // ← add this
    map * found = __untracer_find_breakpoint(hash_map, addr);
    if (found == NULL) {
        fprintf(stderr, "could not find breakpoint at: %ld\n", addr);
        exit(1);
    }
    __untracer_make_writable(addr);
    unsigned char * ptr = (unsigned char *)addr;
    *ptr = found->value.original;
#ifdef __linux__
    uc->uc_mcontext.gregs[REG_RIP] = addr;
#elif defined(__APPLE__) && defined(__x86_64__)
    uc->uc_mcontext->__ss.__rip = addr;
#elif defined(__APPLE__) && defined(__aarch64__)
    uc->uc_mcontext->__ss.__pc = addr;
#endif
}

void __untracer_crash_handler(int sig) {
    printf("crash %d\n", sig);
    exit(1);
}

void __untracer_setup_handler(void) {
    struct sigaction sa;
    sa.sa_sigaction = __untracer_sigtrap_handler;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGTRAP, &sa, NULL);
    // Second handler
    sa.sa_sigaction = NULL;
    sa.sa_handler = __untracer_crash_handler;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGABRT, &sa, NULL);
    sigaction(SIGSEGV, &sa, NULL);
    sigaction(SIGBUS, &sa, NULL);
}

int __untracer_read_csv(const char * csv) {
    // value(0x400000+offsetUL), addr(value-0x400000UL),original(char),block_index(int)
    FILE * fp = fopen(csv, "r");
    if (fp == NULL) {
        fprintf(stderr, "failed to open csv file:%s\n", csv);
        exit(1);
    }
    char line[1024];
    uintptr_t addr_value;
    uintptr_t addr_offset;
    unsigned char original;
    size_t block_index;
    while (fgets(line, sizeof(line), fp))
    {
        if (sscanf(line, "%lx,%lx,%c,%zu", &addr_value, &addr_offset, &original, &block_index) != 4)
            continue;
        breakpoint bp = {
            .addr_value = addr_value,
            .addr_offset = addr_offset,
            .original = original,
            .block_index = block_index
        };
        __untracer_add_breakpoint(&hash_map, addr_value, bp);
    }
    fclose(fp);
    return 0;
}

int main(int argc, char **argv) {
    __untracer_setup_handler();
    const char * csv = "text.csv";
    __untracer_read_csv(csv);
    while (1) {
        int result = target_main(argc, argv);
        printf("result: %d", result);
    }
}