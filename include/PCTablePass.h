#pragma once
#include "common.h"

class PCTablePass : public llvm::PassInfoMixin<PCTablePass> {
    public:
        llvm::PreservedAnalyses run(llvm::Module &M, llvm::ModuleAnalysisManager &MAM);
        void emitRegistrationCtor(llvm::Module &M, llvm::GlobalVariable *PCTable, llvm::Function &F);
        llvm::GlobalVariable *buildPCTable(llvm::Function &F);
        static bool foundInSkipFunctions(llvm::StringRef &fName);
};
