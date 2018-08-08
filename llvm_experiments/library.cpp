#include <string>
#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

using namespace llvm;

const auto INSTRUMENTATION_START_FUNCTION_NAME = "__start_store_instrumentation";
const auto INSTRUMENTATION_END_FUNCTION_NAME = "__end_store_instrumentation";
const auto INSTRUMENTATION_GET_HASH_FUNCTION_NAME = "__get_instrumentation_hash";
const auto TM_CRC32_FUNCTION_NAME = "__crc32";

namespace {
    struct SamPass : public FunctionPass {
        static char ID;

        SamPass() : FunctionPass(ID) {}

        Value *castToLong(Value *value, IRBuilder<> &builder) {
            Value *rawIntValue;
            auto *valueType = value->getType();
            auto &context = value->getContext();
            if (valueType->isIntegerTy()) {
                if (valueType->getPrimitiveSizeInBits() < 64) {
                    rawIntValue = builder.CreateZExt(value, IntegerType::getInt64Ty(context));
                } else if (valueType->getPrimitiveSizeInBits() > 64) {
                    rawIntValue = builder.CreateTrunc(value, IntegerType::getInt64Ty(context));
                } else {
                    rawIntValue = value;
                }
            } else if (valueType->isPointerTy()) {
                rawIntValue = builder.CreatePtrToInt(value, IntegerType::getInt64Ty(context));
            } else if (valueType->isFloatTy()) {
                rawIntValue = builder.CreateBitCast(value, IntegerType::get(context, valueType->getPrimitiveSizeInBits()));
                return castToLong(rawIntValue, builder);
            }
            return rawIntValue;
        }

        virtual bool runOnFunction(Function &F) {
            auto &context = F.getContext();
            auto *module = F.getParent();
            auto *crc32_function = module->getFunction(TM_CRC32_FUNCTION_NAME);
            if (!crc32_function) {
                errs() << "I can't do this.\n";
                return false;
            }

            bool modified = false;
            std::vector<Instruction *> to_be_deleted;
            bool in_transaction = false;
            Value *hash_variable_stack = nullptr;
            Value *hash_variable_reg = nullptr;
            for (auto &B: F) {
                for (auto &I: B) {
                    auto &context = I.getContext();
                    IRBuilder<> builder(&I);
                    builder.SetInsertPoint(&B, builder.GetInsertPoint());
                    if (auto *inst = dyn_cast<CallInst>(&I)) {
                        auto function_name = inst->getOperand(0)->getName();
                        if (function_name.equals(INSTRUMENTATION_START_FUNCTION_NAME)) {
                            if (hash_variable_stack != nullptr) {
                                context.emitError("Function tried to start transaction that was already started!\n");
                                return modified;
                            }
                            hash_variable_stack = builder.CreateAlloca(IntegerType::getInt32Ty(context));
                            builder.CreateStore(ConstantInt::get(IntegerType::getInt32Ty(context), 0, true),
                                                hash_variable_stack);
                            hash_variable_reg = builder.CreateLoad(hash_variable_stack);
                            modified = true;
                            in_transaction = true;
                        } else if (function_name.equals(INSTRUMENTATION_END_FUNCTION_NAME)) {
                            if (hash_variable_stack == nullptr) {
                                context.emitError("Function tried to end transaction that is non-existent!\n");
                                return modified;
                            }

                            in_transaction = false;
                        } else if (function_name.equals(INSTRUMENTATION_GET_HASH_FUNCTION_NAME)) {
                            if (hash_variable_stack == nullptr) {
                                context.emitError("Function tried to get hash without creating a transaction!\n");
                                return modified;
                            }

                            for (auto& U : inst->uses()) {
                                User* user = U.getUser();
                                user->setOperand(U.getOperandNo(), hash_variable_reg);
                            }
                        } else {
                            continue;
                        }
                    } else if (auto *inst = dyn_cast<StoreInst>(&I)) {
                        if (!in_transaction) {
                            continue;
                        }

                        auto *value_operand = inst->getValueOperand();
                        Value *args[] = {hash_variable_reg, castToLong(value_operand, builder)};
                        hash_variable_reg = builder.CreateCall(crc32_function, args);
                        modified = true;
                    } else if (auto *inst = dyn_cast<ReturnInst>(&I)) {
                        if (hash_variable_stack == nullptr) {
                            continue;
                        }

                        if (auto *ret_value = inst->getReturnValue()) {
                            if (ret_value->getType()->isIntegerTy(32)) {
                                inst->setOperand(0, hash_variable_reg);
                            }
                        }
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