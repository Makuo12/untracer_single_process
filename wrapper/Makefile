DIR := $(CURDIR)


filer.bc: filer.c
	/opt/homebrew/opt/llvm/bin/clang -I $(DIR)/../include -c -emit-llvm $^ -o $@

global.bc: global.c
	/opt/homebrew/opt/llvm/bin/clang -I $(DIR)/../include -c -emit-llvm $^ -o $@

heap.bc: heap.c
	/opt/homebrew/opt/llvm/bin/clang -I $(DIR)/../include -c -emit-llvm $^ -o $@

mutate.bc: mutate.c
	/opt/homebrew/opt/llvm/bin/clang -I $(DIR)/../include -c -emit-llvm $^ -o $@

runtime.bc: runtime.c
	/opt/homebrew/opt/llvm/bin/clang -I $(DIR)/../include -c -emit-llvm $^ -o $@

untracer.bc: untracer.c
	/opt/homebrew/opt/llvm/bin/clang -I $(DIR)/../include -c -emit-llvm $^ -o $@

fuzzer.bc: filer.bc global.bc heap.bc mutate.bc runtime.bc untracer.bc
	llvm-link $^ -o $@

