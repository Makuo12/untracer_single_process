export LLVM_COMPILER=clang
export LLVM_CC_NAME=clang-20        # ← pin to version 20
export LLVM_CXX_NAME=clang++-20     # ← pin to version 20

wrap=$(pwd)

rm -rf build
rm -rf output


mkdir output

echo "Setting up llvm pass"

mkdir build && cd build
cmake -DLLVM_DIR=/usr/lib/llvm-20/lib/cmake/llvm \
    -DCMAKE_BUILD_TYPE=Release \
    .. 2>&1 | cat
make -j$(nproc)
cd ..

echo "Building xpdf with custom coverage tracer..."
cd $wrap/xpdf-4.06_2
rm -rf build
mkdir -p build 
cd build

if [ "$1" = "linux" ]; then
COVERAGE_FLAGS="-I $wrap/include -g -fno-pie"

cmake -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_C_COMPILER=wllvm \
        -DCMAKE_CXX_COMPILER=wllvm++ \
        -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY \
        -DCMAKE_C_FLAGS="$COVERAGE_FLAGS" \
        -DCMAKE_CXX_FLAGS="$COVERAGE_FLAGS" \
        -DCMAKE_EXE_LINKER_FLAGS="-no-pie" \
        -DCMAKE_SHARED_LINKER_FLAGS="-no-pie" \
        ../

make -j$(nproc)

# # Extract the whole-program bitcode from the final binary
extract-bc ./xpdf/pdftotext -o whole_program.bc

cd ../..

/usr/lib/llvm-20/bin/opt \
    -load-pass-plugin=./build/Untracer.so \
    -passes="pctable" \
    ./xpdf-4.06_2/build/whole_program.bc -o out.bc

echo "Make call on fuzzer"
cd $wrap/wrapper
rm -rf build
mkdir -p build
make fuzzer.bc

cd ..
echo "Building final binary"
/usr/lib/llvm-20/bin/clang++ out.bc $wrap/wrapper/build/fuzzer.bc -no-pie -o ./main.bin
./main.bin drop_pctable
clang++ tools/utils.cc -o tools/utils
./tools/utils
chmod +x oracle.bin

else
COVERAGE_FLAGS=" -g"

cmake -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_C_COMPILER=wllvm \
        -DCMAKE_CXX_COMPILER=wllvm++ \
        -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY \
        -DCMAKE_C_FLAGS="$COVERAGE_FLAGS" \
        -DCMAKE_CXX_FLAGS="$COVERAGE_FLAGS" \
        ../

make -j$(nproc)

# # # Extract the whole-program bitcode from the final binary
extract-bc ./xpdf/pdftotext -o whole_program.bc

cd ../..

/opt/homebrew/opt/llvm/bin/opt \
    -load-pass-plugin=./build/Untracer.so \
    -passes="pctable" \
    ./xpdf-4.06_2/build/whole_program.bc -o out.bc
    

echo "Make call on fuzzer"
cd $wrap/wrapper
rm -rf build
mkdir -p build
make fuzzer.bc

cd ..
/opt/homebrew/opt/llvm/bin/clang++ out.bc $wrap/wrapper/fuzzer.bc -o ./main.bin

fi
