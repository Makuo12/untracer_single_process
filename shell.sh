# 1. Compile test.cpp to IR
# Clean everything
# rm -f test.ll out.bc test_bin

# # Rebuild from scratch
# # Compile to bitcode directly (.bc instead of .ll)
# /opt/homebrew/opt/llvm/bin/clang -c -emit-llvm -g -O0 main.cc -o main.bc

# # Run your pass on the .bc
# /opt/homebrew/opt/llvm/bin/opt \
#     -load-pass-plugin=./PCTablePass.so \
#     -passes="pctable" \
#     main.bc -o out.bc

# # Link and compile final binary

export LLVM_COMPILER=clang
export LLVM_CC_NAME=clang
export LLVM_CXX_NAME=clang++

wrap=$(pwd)

echo "Building xpdf with custom coverage tracer..."

wllvm++ -c -g -o $wrap/runtime.o $wrap/runtime.cc
/opt/homebrew/opt/llvm/bin/clang++ $wrap/pass.cc \
    -I /opt/homebrew/Cellar/llvm/22.1.1/include \
    -shared \
    -fPIC \
    -fno-rtti \
    -std=c++17 \
    -L /opt/homebrew/Cellar/llvm/22.1.1/lib \
    -lLLVM \
    -o PCTablePass.so

cd $wrap/xpdf-4.06_2
rm -rf build
mkdir -p build 
cd build


# 1. Define the coverage flags we want applied to EVERY source file
COVERAGE_FLAGS="-g"

cmake -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_C_COMPILER=wllvm \
        -DCMAKE_CXX_COMPILER=wllvm++ \
        -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY \
        -DCMAKE_C_FLAGS="$COVERAGE_FLAGS" \
        -DCMAKE_CXX_FLAGS="$COVERAGE_FLAGS" \
        -DCMAKE_EXE_LINKER_FLAGS="$wrap/runtime.o" \
        ../

make -j$(nproc)

# # Extract the whole-program bitcode from the final binary
extract-bc ./xpdf/pdftotext -o whole_program.bc

cd ../..

/opt/homebrew/opt/llvm/bin/opt \
    -load-pass-plugin=./PCTablePass.so \
    -passes="pctable" \
    ./xpdf-4.06_2/build/whole_program.bc -o out.bc

/opt/homebrew/opt/llvm/bin/clang -g out.bc -o main.bin -lstdc++
# 2. Build the pdftotext utility
# make pdftotext

# # 3. Copy the compiled binary back to your output directory
# cp ./xpdf/pdftotext $wrap/output/pdftotext
# cd $wrap

# # 4. Setup basic blocks (gotten from __sanitizer_cov_pcs_init)
# export WRITE_OUT="$wrap/output/text"
# export COVERAGE="$wrap/output/coverage_log.txt"
# ./output/pdftotext > /dev/null 2>&1 || true