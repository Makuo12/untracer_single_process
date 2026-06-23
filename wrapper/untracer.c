#define _GNU_SOURCE
#include <getopt.h>
#include <stdint.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <sys/ucontext.h>
#include <sys/mman.h>
#include <unistd.h>
#include <setjmp.h>
#include <string.h>
#include "uthash.h"
#include "data.h"
#include "global.h"
#include "mutate.h"

extern int target_main(int argc, char **argv);
void __runtime_dump_pctables(void);
int capacity = 100;
Entry *all_entries = NULL;
size_t entry_count = 0;
uint64_t total_exec = 0; 

sigjmp_buf env;

int has_coverage = 0;

#define MAP_SIZE (1 << 16)

u8 virgin_blocks[MAP_SIZE];

typedef struct
{
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
int ret_status = 0;


void exitHook(int status) {
    ret_status = status;
    siglongjmp(env, status);
}

map *__untracer_find_breakpoint(map *hash_map, uintptr_t key)
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

void __untracer_sigtrap_handler(int sig, siginfo_t *info, void *ctx) {
    has_coverage += 1;
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
    } else {
        virgin_blocks[found->value.block_index] = 1;
        fprintf(stdout, "character: %c addr: %lu\n", found->value.original, addr);
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
    siglongjmp(env, sig);
}

void __untracer_signals(void) {
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

void __untracer_setup_stat(const char * file) {
    FILE * fp = fopen(file, "w");
    if (fp == NULL) {
        perror("stat file could not open");
        exit(1);
    }
    const char * header = "Past,Time,execs,coverage\n";
    fputs(header, fp);
    fclose(fp);
}

void __untracer_write_to_stat(const char * file, int past_time, time_t current_time) {
    FILE * fp = fopen(file, "r");
    if (fp == NULL) {
        perror("stat file could not open");
        return;
    }
    int coverage = 0;
    for (int i = 0; i < MAP_SIZE; ++i) {
        if (virgin_blocks[i]) ++coverage;
    }
    char buf[1024];
    struct tm *tm_info = localtime(&current_time);
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", tm_info);
    fprintf(fp, "%dhrs,%s,%lu,%d", past_time, buf, total_exec, coverage);
    fclose(fp);
}

int main(int argc, char **argv)
{
    const char *csv = "text.csv";
    char *out_dir;
    char *in_dir;
    const char *input = "output/cur_input.pdf";
    const char *stats = "output/stat.csv";
    if (strcmp(argv[1], "drop_pctable") == 0) {
        __runtime_dump_pctables();
        return 0;
    }
    char c;
    while ((c = getopt(argc, argv, "o:i:")) > 0) {
        switch (c) {
            case 'o':
                out_dir = optarg;
                break;
            case 'i':
                in_dir = optarg;
                break;
        }
    }
    __untracer_setup_std_outputs();
    __untracer_signals();
    __untracer_read_csv(csv);
    __untracer_setup_global();
    __untracer_setup_stat(stats);
    __untracer_setup_dir(out_dir);
    __untracer_files(&all_entries, &capacity, in_dir, &entry_count);
    memset(virgin_blocks, 0, MAP_SIZE);
    size_t current = 0;
    time_t start_time = time(NULL);
    int last_processed_hour = 1;
    char * new_args[3] = {(char *)argv[0], (char *)input, NULL};
    while (1)
    {
        int sig_result = sigsetjmp(env, 1);
        if (sig_result > 0)
        {
            // crash_handle it here
            size_t previous = current - 1;
            if (previous > 0 && previous < entry_count) {
                Entry * entry = &all_entries[previous];
                // __untracer_write_to_file(all_entries, &capacity, &entry_count, input, entry->st_size, CRASH);
            }
            continue;
        }
        else
        {
            Entry *entry = &all_entries[current++];
            if (entry->has_issues)
            {
                continue;
            }
            u8 *mem = __untracer_read_file(entry);
            if (mem == NULL)
            {
                entry->has_issues = 1;
                continue;
            }
            size_t len = entry->st_size << 3;
            for (size_t i = 0; i < len; ++i) {
                ++total_exec;
                has_coverage = 0;
                __untracer_mutate(mem, i);
                __untracer_write_testcase(mem, entry, input);
                __untracer_suppress_output();
                target_main(2, new_args);
                __untracer_restore_output();
                __untracer_mutate(mem, i);
                if (has_coverage) {
                    entry->trace_count += has_coverage;
                    entry->path_found += 1;
                    // __untracer_write_to_file(all_entries, &capacity, &entry_count, input, entry->st_size, TRAP);
                }
                entry->single_pass += 1;
            }
            entry->full_pass += 1;
        }
        close_open_file_handles();
        free_ptrs();
        __untracer_restore_global();
        time_t current_time = time(NULL);
        int hours_elapsed = (int)difftime(current_time, start_time) / 360;
        if (hours_elapsed > last_processed_hour) {
            last_processed_hour += 1;
            __untracer_write_to_stat(stats, hours_elapsed, current_time);
        }
    }
}