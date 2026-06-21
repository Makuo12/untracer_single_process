#include "PCTablePass.h"

using namespace llvm;
using namespace std;

std::vector<std::string> skipFunctions = {
    "__runtime",
    "pctable",
    "PCTableEntry",
    "__untracer",
    "malloc",
    "realloc",
    "calloc"
    "HASH_FIND",
    "HASH_ADD",
    "sysconf",
    "sscanf",
    "mprotect",
    "printf",
    "fopen",
    "fclose",
    "sigemptyset",
    "sigaction",
    "fgets",
    "sscanf",
    "sizeof",
};

// Step 2 — emit a constructor that registers the PC table with the runtime
void PCTablePass::emitRegistrationCtor(llvm::Module &M, llvm::GlobalVariable *PCTable, llvm::Function &F)
{
    LLVMContext &Ctx = M.getContext();
    Type *VoidTy = Type::getVoidTy(Ctx);
    Type *PtrTy = PointerType::getUnqual(Ctx);

    // -------------------------------------------------------
    // Declare __runtime_register_pctable(i8* start, i8* end)
    // This is your runtime function defined in runtime.cpp
    // -------------------------------------------------------
    FunctionCallee RegFn = M.getOrInsertFunction(
        "__runtime_register_pctable",
        FunctionType::get(VoidTy, {PtrTy, PtrTy}, /*isVarArg=*/false));

    // -------------------------------------------------------
    // Create the constructor function
    // e.g. @__pctable_ctor__Z3addii
    // -------------------------------------------------------
    std::string CtorName = "__pctable_ctor_" + F.getName().str();
    Function *Ctor = Function::Create(
        FunctionType::get(VoidTy, false),
        GlobalValue::InternalLinkage,
        CtorName,
        M);

    // -------------------------------------------------------
    // Build the constructor body
    // -------------------------------------------------------
    BasicBlock *Entry = BasicBlock::Create(Ctx, "entry", Ctor);
    IRBuilder<> IRB(Entry);

    // Get pointer to first element — start of table
    Value *Start = IRB.CreateConstGEP2_32(
        PCTable->getValueType(),
        PCTable,
        0, 0, // [0][0]
        "pctable_start");

    // Get pointer past last element — end of table
    uint64_t NumEntries = cast<ArrayType>(
                              PCTable->getValueType())
                              ->getNumElements();

    Value *End = IRB.CreateConstGEP2_32(
        PCTable->getValueType(),
        PCTable,
        0, NumEntries, // [0][N] — one past the end
        "pctable_end");

    // Call __runtime_register_pctable(start, end)
    IRB.CreateCall(RegFn, {Start, End});
    IRB.CreateRetVoid();

    // -------------------------------------------------------
    // Register as a global constructor
    // Runs after relocations are patched, before main()
    // Priority 65535 = runs last among ctors (after C++ global ctors)
    // -------------------------------------------------------
    appendToGlobalCtors(M, Ctor, /*Priority=*/65535);
}

// Builds the PC table for a single function
// Returns the created global, or nullptr if function has no blocks
GlobalVariable * PCTablePass::buildPCTable(Function &F) {
    // Skip declarations — no body to build a table from
    if (F.isDeclaration())
        return nullptr;

    LLVMContext &Ctx = F.getContext();
    Module *M = F.getParent();

    // i8* type — standard for block addresses
    Type *PtrTy = PointerType::getUnqual(Ctx);

    // -------------------------------------------------------
    // Collect a blockaddress constant for every basic block
    // -------------------------------------------------------
    std::vector<Constant *> Entries;
    for (BasicBlock &BB : F)
    {
        // Skip the entry block — blockaddress is not allowed on it
        if (&BB == &F.getEntryBlock())
            continue;

        BlockAddress *BA = BlockAddress::get(&F, &BB);
        Constant *Entry = ConstantExpr::getBitCast(BA, PtrTy);
        Entries.push_back(Entry);
    }

    if (Entries.empty())
        return nullptr;

    // -------------------------------------------------------
    // Build the global constant array
    // -------------------------------------------------------
    ArrayType *TableTy = ArrayType::get(PtrTy, Entries.size());
    Constant *TableInit = ConstantArray::get(TableTy, Entries);

    // Name it per-function so multiple functions don't collide
    std::string TableName = "__my_pctable_" + F.getName().str();

    GlobalVariable *PCTable = new GlobalVariable(
        *M,
        TableTy,
        /*isConstant=*/true,                   // lives in .rodata
        GlobalValue::InternalLinkage,          // not visible outside TU
        TableInit,
        TableName
    );

    // Ensure natural pointer alignment
    PCTable->setAlignment(MaybeAlign(8));

    return PCTable;
}


bool PCTablePass::foundInSkipFunctions(StringRef &fName) {
    for (auto func : skipFunctions) {
        if (fName.contains(func)) {
            return true;
        }
    }
    return false;
}

PreservedAnalyses PCTablePass::run(Module &M, ModuleAnalysisManager &MAM)
{
    bool Changed = false;
    for (Function &F : M)
    {
        StringRef fName = F.getName();
        if (fName == "main" || foundInSkipFunctions(fName))
        {
            cout << "skipping function name" << " " << fName.str() << endl;
            continue;
        }
        GlobalVariable *Table = buildPCTable(F);
        if (!Table)
            continue;
        // Step 2 — emit the registration constructor
        emitRegistrationCtor(M, Table, F);

        // errs() << "[PCTablePass] Built table for: " << F.getName()
        //        << " with " << F.size() << " blocks"
        //        << " → " << Table->getName() << "\n";

        Changed = true;
    }

    // We only added globals — function bodies are untouched
    // so we preserve all existing analyses
    return Changed
               ? PreservedAnalyses::none()
               : PreservedAnalyses::all();
}