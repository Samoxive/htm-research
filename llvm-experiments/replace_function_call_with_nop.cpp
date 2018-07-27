#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
using namespace llvm;

namespace {
    struct SamPass : public FunctionPass {
        static char ID;
        SamPass() : FunctionPass(ID) {}

        virtual bool runOnFunction(Function &F) {
            bool modified = false;
            std::vector<Instruction*> to_be_removed;
            for (auto &B: F) {
                bool modified_block = false;
                for (auto &I: B) {
                    if (auto *inst = dyn_cast<CallInst>(&I)) {
                        if (!inst->getCalledFunction()->getName().equals("__test_stuff")) {
                            continue;
                        }
                        IRBuilder<> builder(inst);
                        builder.SetInsertPoint(&B, builder.GetInsertPoint());
                        auto *inline_asm_value = InlineAsm::get(FunctionType::get(Type::getVoidTy(I.getContext()), false), "nop", "", false);
                        auto *call_inst = builder.CreateCall(inline_asm_value, {});
                        to_be_removed.push_back(inst);
                        modified = true;


                    }
                }
            }

            for (auto *I: to_be_removed) {
                I->removeFromParent();
            }

            for (auto &B: F) {
                bool modified_block = false;
                for (auto &I: B) {
                    outs() << "---\n" << I << "\n---\n";
                }
            }
            return modified;
        }
    };
}

char SamPass::ID = 0;

static void registerSkeletonPass(const PassManagerBuilder &, legacy::PassManagerBase &PM) {
    PM.add(new SamPass());
}
static RegisterStandardPasses RegisterMyPass(PassManagerBuilder::EP_EarlyAsPossible, registerSkeletonPass);