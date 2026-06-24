#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <dirent.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include "mutate.h"

static char *crash_dir = NULL;
static char *trace_dir = NULL;
static int std_out_ref = -1;
static int std_err_ref = -1;
// FIX #2: open /dev/null once instead of on every suppress call
static int dev_null_fd = -1;
size_t number_execs = 0;

void __untracer_setup_dir(const char *output)
{
    size_t crash_name_size = snprintf(NULL, 0, "%s/%s", output, "crash");
    size_t trace_name_size = snprintf(NULL, 0, "%s/%s", output, "trace");
    crash_dir = (char *)malloc(crash_name_size + 1);
    trace_dir = (char *)malloc(trace_name_size + 1);
    if (crash_dir == NULL || trace_dir == NULL)
    {
        fprintf(stderr, "could not setup crash and trace dir\n");
        exit(1);
    }
    snprintf(crash_dir, crash_name_size + 1, "%s/%s", output, "crash");
    snprintf(trace_dir, trace_name_size + 1, "%s/%s", output, "trace");
    mkdir(crash_dir, 0777);
    mkdir(trace_dir, 0777);
}

static void copy_binary(const char *src_path, const char *dst_path)
{
    struct stat st = {0};
    if (stat(src_path, &st) < 0)
    {
        perror(src_path);
        exit(1);
    }
    char *data = (char *)malloc(st.st_size);
    if (data == NULL)
    {
        perror("malloc");
        exit(1);
    }
    FILE *src_file = fopen(src_path, "rb");
    if (src_file == NULL)
    {
        perror(src_path);
        free(data);
        exit(1);
    }
    FILE *dst_file = fopen(dst_path, "wb");
    if (dst_file == NULL)
    {
        perror(dst_path);
        fclose(src_file);
        free(data);
        exit(1);
    }
    if (fread(data, 1, st.st_size, src_file) != (size_t)st.st_size)
    {
        perror(src_path);
        fclose(src_file);
        fclose(dst_file);
        free(data);
        exit(1);
    }
    if (fwrite(data, 1, st.st_size, dst_file) != (size_t)st.st_size)
    {
        perror(dst_path);
        fclose(src_file);
        fclose(dst_file);
        free(data);
        exit(1);
    }
    fclose(src_file);
    fclose(dst_file);
    free(data);
    if (chmod(dst_path, 0777) < 0)
    {
        perror(dst_path);
        exit(1);
    }
}

void __untracer_mutate(u8 *mem, int position)
{
    mem[position >> 3] ^= (128 >> (position & 7));
}

static void add_file(Entry *all_entries, int *capacity, size_t *entry_count, const char *filename, const char *file_path, size_t size)
{
    if (filename == NULL || file_path == NULL)
    {
        fprintf(stderr, "Error: null filename or file_path passed to add_file");
        return;
    }

    if (*entry_count >= (size_t)(*capacity))
    {
        size_t new_capacity = (*capacity == 0) ? 1 : *capacity * 2;
        Entry *temp = (Entry *)realloc(all_entries, new_capacity * sizeof(Entry));
        if (temp == NULL)
        {
            fprintf(stderr, "Memory reallocation failed while scanning entries");
            exit(1);
        }
        all_entries = temp;
        *capacity = new_capacity;
    }

    Entry *current_entry = &all_entries[*entry_count];
    snprintf(current_entry->d_name, sizeof(current_entry->d_name), "%s", filename);
    snprintf(current_entry->file_path, sizeof(current_entry->file_path), "%s", file_path);
    current_entry->st_size = size;
    current_entry->has_issues = 0;
    current_entry->single_pass = 0;
    current_entry->path_found = 0;
    current_entry->trace_count = 0;
    current_entry->full_pass = 0;
    (*entry_count)++;
}

static void generateTimestampFilename(char *buffer, size_t buf_size)
{
    time_t now = time(NULL);
    struct tm *localTime = localtime(&now);
    size_t size = strftime(buffer, buf_size, "%Y%m%d_%H%M%S", localTime);
    buffer[size] = '\0';
}

void __untracer_write_to_file(Entry *all_entries, int *capacity, size_t *entry_count,
                              const char *input, size_t file_size, Result result)
{
    char timestamp[1024];
    generateTimestampFilename(timestamp, sizeof(timestamp));
    char buf[1024];
    switch (result)
    {
    case CRASH:
    {
        size_t size = snprintf(NULL, 0, "%s/%s", crash_dir, timestamp);
        snprintf(buf, size + 1, "%s/%s", crash_dir, timestamp);
        copy_binary(input, buf);
        break;
    }
    case TRAP:
    {
        size_t size = snprintf(NULL, 0, "%s/%s", trace_dir, timestamp);
        snprintf(buf, size + 1, "%s/%s", trace_dir, timestamp);
        copy_binary(input, buf);
        add_file(all_entries, capacity, entry_count, timestamp, buf, file_size);
        break;
    }
    }
}

void __untracer_files(Entry **entries, int *capacity, const char *in_dir, size_t *entry_count)
{
    struct dirent **items;
    int count = scandir(in_dir, &items, NULL, alphasort);
    if (count < 0)
    {
        fprintf(stderr, "Failed to scan input directory");
        exit(1);
    }
    if (count == 0)
    {
        free(items);
        fprintf(stderr, "No input files found to fuzz");
        exit(1);
    }

    *entry_count = 0;
    *entries = (Entry *)malloc(*capacity * sizeof(Entry));
    if (*entries == NULL)
    {
        fprintf(stderr, "Memory allocation failed for entries array");
        exit(1);
    }

    struct stat st;
    for (int i = 0; i < count; ++i)
    {
        if (items[i]->d_type != DT_REG)
        {
            free(items[i]);
            continue;
        }

        char file_path[512];
        snprintf(file_path, sizeof(file_path), "%s/%s", in_dir, items[i]->d_name);

        if (strstr(file_path, ".pdf") == NULL)
        {
            free(items[i]);
            continue;
        }

        if (stat(file_path, &st) == 0)
        {
            if (*entry_count >= (size_t)*capacity)
            {
                *capacity *= 2;
                Entry *temp = (Entry *)realloc(*entries, *capacity * sizeof(Entry));
                if (temp == NULL)
                {
                    fprintf(stderr, "Memory reallocation failed while scanning entries");
                    exit(1);
                }
                *entries = temp;
            }

            Entry *current_entry = &((*entries)[*entry_count]);
            snprintf(current_entry->d_name, sizeof(current_entry->d_name), "%s", items[i]->d_name);
            snprintf(current_entry->file_path, sizeof(current_entry->file_path), "%s", file_path);
            current_entry->st_size = st.st_size;
            current_entry->has_issues = 0;
            current_entry->single_pass = 0;
            current_entry->path_found = 0;
            current_entry->trace_count = 0;
            current_entry->full_pass = 0;
            (*entry_count)++;
        }
        free(items[i]);
    }
    free(items);

    if (*entry_count == 0)
    {
        free(*entries);
        *entries = NULL;
        fprintf(stderr, "No valid input files found to fuzz");
        exit(1);
    }
}

void __untracer_setup_std_outputs(void)
{
    std_out_ref = dup(STDOUT_FILENO);
    std_err_ref = dup(STDERR_FILENO);
    if (std_out_ref == -1 || std_err_ref == -1)
        perror("dup");

    // FIX #2: open /dev/null once here, reuse in suppress/restore
    dev_null_fd = open("/dev/null", O_WRONLY);
    if (dev_null_fd == -1)
        perror("open /dev/null");
}

void __untracer_suppress_output(void)
{
    if (std_out_ref == -1 || std_err_ref == -1 || dev_null_fd == -1)
        return;

    fflush(stdout);
    fflush(stderr);

    // FIX #2: reuse the single persistent fd instead of open()/close() every call
    dup2(dev_null_fd, STDOUT_FILENO);
    dup2(dev_null_fd, STDERR_FILENO);
}

void __untracer_restore_output(void)
{
    if (std_out_ref == -1 || std_err_ref == -1)
        return;

    dup2(std_out_ref, STDOUT_FILENO);
    dup2(std_err_ref, STDERR_FILENO);
}

void __untracer_teardown_std_outputs(void)
{
    if (std_out_ref != -1)
    {
        close(std_out_ref);
        std_out_ref = -1;
    }
    if (std_err_ref != -1)
    {
        close(std_err_ref);
        std_err_ref = -1;
    }
    // FIX #2: close the persistent dev_null fd on teardown
    if (dev_null_fd != -1)
    {
        close(dev_null_fd);
        dev_null_fd = -1;
    }
}

void __untracer_write_testcase(u8 *mem, Entry *entry, const char *input_file)
{
    FILE *f = fopen(input_file, "wb");
    if (f == NULL)
    {
        fprintf(stdout, "failed to open input file\n");
        exit(1);
    }
    fwrite(mem, 1, entry->st_size, f);
    fclose(f);
}

u8 *__untracer_read_file(Entry *entry)
{
    int fd = open(entry->file_path, O_RDONLY);
    if (fd < 0)
    {
        entry->has_issues = 1;
        char err_msg[512];
        snprintf(err_msg, sizeof(err_msg), "Failed to open file: %s", entry->d_name);
        return NULL;
    }
    u8 *mem = (u8 *)mmap(NULL, entry->st_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
    close(fd);

    if (mem == MAP_FAILED)
    {
        entry->has_issues = 1;
        char err_msg[512];
        snprintf(err_msg, sizeof(err_msg), "Failed to map file: %s", entry->d_name);
        return NULL;
    }
    return mem;
}