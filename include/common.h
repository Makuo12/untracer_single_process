#pragma once
#include "llvm/IR/PassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/Type.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Transforms/Utils/ModuleUtils.h" // appendToGlobalCtors
#ifdef __linux__
#include "llvm/Passes/PassPlugin.h"
#elif defined(__APPLE__) && defined(__aarch64__)
#include "llvm/Plugins/PassPlugin.h"
#endif
#include <string>
#include <vector>
#include <iostream>