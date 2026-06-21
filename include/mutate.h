#ifndef MUTATE_H_
#define MUTATE_H_
#include <stdio.h>
#include <stdlib.h>
#include "data.h"
typedef unsigned char u8;
void __untracer_setup_dir(const char *output);
void __untracer_mutate(u8 *mem, int position);
void __untracer_add_file(Entry * all_entries, int * capacity, const char *filename, const char *file_path, size_t size);
void __untracer_files(Entry **entries, int *capacity, const char *in_dir, size_t *entry_count);
void __untracer_setup_std_outputs(void);
void __untracer_suppress_output(void);
void __untracer_restore_output(void);
void __untracer_write_testcase(u8 *mem, Entry *entry, const char *input_file);
u8 *__untracer_read_file(Entry *entry);
typedef enum {
    CRASH,
    TRAP
} Result ;
void __untracer_write_to_file(Entry *all_entries, int *capacity, size_t *entry_count,
                              const char *input, size_t file_size, Result result);
#endif