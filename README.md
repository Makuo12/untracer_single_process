# Untracer: Single-Process Fuzzing with Hardware Trap-Based Coverage

A fuzzing framework combining techniques from the **Untracer** and **ClosureX** papers. Uses hardware traps to track basic block execution, eliminating traditional instrumentation overhead.

## Overview

Traditional coverage-guided fuzzers instrument binaries to track executed code paths. This project replaces the first byte of each basic block with a hardware trap instruction (INT3/SIGTRAP). When execution hits a trap, a signal handler records the coverage and restores the original byte, leaving no permanent instrumentation in the binary.

 **Runtime Coverage Tracking** (`wrapper/untracer.c`)

The signal handler implements the trap-and-restore mechanism:

```c
// Signal handler receives SIGTRAP when execution hits a basic block
// 1. Extracts RIP (instruction pointer) from signal context
// 2. Looks up trap address in hash map (full_address → breakpoint struct)
// 3. Restores the original byte at that address
// 4. Sets virgin_blocks[block_index] = 1 to mark block as executed
// 5. Returns control to execute the restored original instruction
// 6. Next execution of that block will hit the trap again
```

The key data structure:

```c
typedef struct {
    uintptr_t addr_value;      // Full address where trap is placed
    uintptr_t addr_offset;     // Relative offset from base
    unsigned char original;    // Original byte before trap
    size_t block_index;        // Index into virgin_blocks bitmap
} breakpoint;

u8 virgin_blocks[MAP_SIZE];    // Bitmap of executed blocks
```

### State Reset Mechanism

### State Reset

After each input, the framework resets process state to allow repeated execution within a single process:

- **Heap**: Frees allocated memory and restores heap boundaries
- **Globals**: Resets cloned global variables to their initial values
- **File Descriptors**: Closes and frees any descriptors opened during execution
- **Control Flow**: Returns to the fuzzer loop via `siglongjmp` instead of calling `exit()`

---

### Build Process

`shell.sh` orchestrates a four-stage build pipeline.

**Stage 1 — Compile LLVM Passes**
```bash
mkdir build && cd build
cmake -DLLVM_DIR=/usr/lib/llvm-20/lib/cmake/llvm ...
make -j$(nproc)
```
Output: `build/Untracer.so`

**Stage 2 — Build Target and Extract Bitcode**
```bash
cd xpdf-4.06_2/build
CC=wllvm CXX=wllvm++ cmake .. && make
extract-bc xpdf/pdftotext -o whole_program.bc
```
Output: `whole_program.bc` — full program LLVM bitcode before any passes run

**Stage 3 — Apply LLVM Passes**
```bash
opt -load-pass-plugin=./build/Untracer.so -passes="pctable" \
    ./xpdf-4.06_2/build/whole_program.bc -o out.bc
```
Output: `out.bc` — bitcode with the all passes applied.

**Stage 4 — Link and Dump Block List**
```bash
clang++ out.bc wrapper/build/fuzzer.bc -no-pie -o ./main.bin
./main.bin drop_pctable
```
`main.bin` is a clean, unmodified binary. Running it with `drop_pctable` dumps the embedded block addresses — the list of every basic block's virtual address.

**Stage 5 — Patch Traps and Generate Coverage Map**
```bash
clang++ tools/utils.cc -o tools/utils
./tools/utils
chmod +x oracle.bin
```
`utils` performs two steps:

- Copies `main.bin` → `oracle.bin`
- For each address in `.bblist`, seeks to `address - 0x400000` in `oracle.bin`, saves the original byte, and overwrites it with `0xCC` (INT3)
- Writes `text.csv`: `virtual_addr, file_offset, original_byte, block_index`

Output: `oracle.bin` (trap-patched binary that runs under the fuzzer), `text.csv` (coverage map used by the signal handler to restore bytes and record hits)

---

### Usage

```bash
# Build (requires LLVM 20, wllvm, clang++)
# Only works with linux for now (can run with docker)
./shell.sh linux

# Run the fuzzer
./oracle.bin -o output -i input_dir(pdf_test)

# Inspect coverage mapping
cat text.csv
```
