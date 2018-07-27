#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
using namespace llvm;

const auto TM_START_FUNCTION_NAME = "__start_transaction";
const auto TM_END_FUNCTION_NAME = "__end_transaction";
const auto TM_GET_HASH_FUNCTION_NAME = "__get_hash";

namespace {
    struct SamPass : public FunctionPass {
        static char ID;
        SamPass() : FunctionPass(ID) {}

        virtual bool runOnFunction(Function &F) {
            bool modified = false;
            std::vector<Instruction*> to_be_deleted;
            for (auto &B: F) {
                bool in_transaction = false;
                Value *hash_variable_stack = nullptr;
                for (auto &I: B) {
                    auto &context = I.getContext();
                    IRBuilder<> builder(&I);
                    builder.SetInsertPoint(&B, builder.GetInsertPoint());
                    if (auto *inst = dyn_cast<CallInst>(&I)) {
                        auto function_name = inst->getOperand(0)->getName();
                        if (function_name.equals(TM_START_FUNCTION_NAME)) {
                            if (hash_variable_stack != nullptr) {
                                context.emitError("Function tried to start transaction that was already started!\n");
                                return modified;
                            }
                            hash_variable_stack = builder.CreateAlloca(IntegerType::getInt32Ty(context));
                            builder.CreateStore(ConstantInt::get(IntegerType::getInt32Ty(context), 10, true), hash_variable_stack);
                            modified = true;
                            in_transaction = true;
                        } else if (function_name.equals(TM_END_FUNCTION_NAME)) {
                            if (hash_variable_stack == nullptr) {
                                context.emitError("Function tried to end transaction that is non-existent!\n");
                                return modified;
                            }
                            in_transaction = false;
                        } else {
                            continue;
                        }
                    } else if (auto *inst = dyn_cast<StoreInst>(&I)) {
                        if (!in_transaction) {
                            continue;
                        }

                        auto *value_operand = inst->getValueOperand();
                        auto *value_type = value_operand->getType();
                        if (!value_type->isIntegerTy()) {
                            continue;
                        }

                        if (auto *int_type_of_value = dyn_cast<IntegerType>(value_type)) {
                            if (int_type_of_value->getBitWidth() != 32) {
                                continue;
                            }
                        } else {
                            continue;
                        }

                        Value *hash_variable = builder.CreateLoad(hash_variable_stack);
                        auto *hash_add_result = builder.CreateAdd(hash_variable, value_operand);
                        builder.CreateStore(hash_add_result, hash_variable_stack);
                        modified = true;
                    }
                }
            }

            for (auto *I: to_be_deleted) {
                I->removeFromParent();
            }
            outs() << F;
            return modified;
        }
    };
}

char SamPass::ID = 0;

static void registerSkeletonPass(const PassManagerBuilder &, legacy::PassManagerBase &PM) {
    PM.add(new SamPass());
}
static RegisterStandardPasses RegisterMyPass(PassManagerBuilder::EP_EarlyAsPossible, registerSkeletonPass);