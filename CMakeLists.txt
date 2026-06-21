project(Untracer)

cmake_minimum_required(VERSION 3.20)

find_package(LLVM REQUIRED CONFIG)

list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(AddLLVM)

include_directories(${LLVM_INCLUDE_DIRS} include/)
add_definitions(${LLVM_DEFINITIONS})

include_directories(./include)

add_library(Untracer MODULE
    ./src/Pass.cc
    ./src/RenameMainPass.cpp
    ./src/HeapPass.cpp
    ./src/ExitPass.cpp
    ./src/PCTable.cc
    ./src/GlobalPass.cpp
    ./src/FilePass.cpp
)

llvm_map_components_to_libnames(LLVM_LIBS
    Core
    Support
    IRReader
    Passes
    TransformUtils
)

set_target_properties(Untracer PROPERTIES 
    PREFIX ""
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    )

target_compile_options(Untracer PRIVATE -fno-rtti)

target_link_libraries(Untracer PRIVATE ${LLVM_LIBS})