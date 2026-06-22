DIR := $(CURDIR)
OS := $(shell uname -s)
ifeq ($(OS), Linux)
	CLANG := /usr/lib/llvm-20/bin/clang
else
	CLANG := /opt/homebrew/opt/llvm/bin/clang
endif

filer.bc: filer.c
	$(CLANG) -I $(DIR)/../include -c -emit-llvm $^ -o $(DIR)/build/$@

global.bc: global.c
	$(CLANG) -I $(DIR)/../include -c -emit-llvm $^ -o $(DIR)/build/$@

heap.bc: heap.c
	$(CLANG) -I $(DIR)/../include -c -emit-llvm $^ -o $(DIR)/build/$@

mutate.bc: mutate.c
	$(CLANG) -I $(DIR)/../include -c -emit-llvm $^ -o $(DIR)/build/$@

runtime.bc: runtime.c
	$(CLANG) -I $(DIR)/../include -c -emit-llvm $^ -o $(DIR)/build/$@

untracer.bc: untracer.c
	$(CLANG) -I $(DIR)/../include -c -emit-llvm $^ -o $(DIR)/build/$@

fuzzer.bc: filer.bc global.bc heap.bc mutate.bc runtime.bc untracer.bc
	llvm-link $(addprefix $(DIR)/build/, $^) -o $(DIR)/build/$@