#include "core/pch.h"

#include "modules/ecmascript/carakan/src/util/es_codegenerator_arm.h"

void Output(ES_CodeGenerator &cg, const char *name)
{
  unsigned *code = cg.GetBuffer();

  printf("const MachineInstruction cv_%s[] =\n{\n", name);

  for (unsigned index = 0; index < cg.GetBufferUsed(); ++index)
    printf("    0x%x%s\n", code[index], index + 1 == cg.GetBufferUsed() ? "" : ",");

  printf("};\n");
}

// BOOL StartImpl(OpPseudoThread *thread, OpPseudoThread::Callback callback, unsigned char **original_stack, unsigned char *thread_stack)

void generate_StartImpl()
{
  ES_CodeGenerator cg;

  cg.PUSH(0x5ff0); // PUSH { R4-R12, LR }

  cg.MOV(ES_CodeGenerator::REG_R2, ES_CodeGenerator::REG_R4);    // store original_stack in R4 for later use
  cg.STR(ES_CodeGenerator::REG_SP, ES_CodeGenerator::REG_R2, 0); // *original_stack = <SP>;
  cg.MOV(ES_CodeGenerator::REG_R3, ES_CodeGenerator::REG_SP);    // <SP> = thread_stack;
  cg.BLX(ES_CodeGenerator::REG_R1);                              // callback(thread)
  cg.LDR(ES_CodeGenerator::REG_R4, 0, ES_CodeGenerator::REG_SP); // restore SP from original_stack (in R4)
  cg.MOV(1, ES_CodeGenerator::REG_R0);

  cg.POP(0x9ff0); // POP { R4-R12, PC }

  Output(cg, "StartImpl");
}

// BOOL ResumeImpl(OpPseudoThread *thread, unsigned char **original_stack, unsigned char *thread_stack)

void generate_ResumeImpl()
{
  ES_CodeGenerator cg;

  cg.PUSH(0x4ff0); // PUSH { R4-R12, LR }

  cg.STR(ES_CodeGenerator::REG_SP, ES_CodeGenerator::REG_R1, 0); // *original_stack = <SP>;
  cg.MOV(ES_CodeGenerator::REG_R2, ES_CodeGenerator::REG_SP);    // <SP> = thread_stack;

  cg.POP(0x8ff0); // POP { R4-R12, PC }

  Output(cg, "ResumeImpl");
}

// BOOL YieldImpl(OpPseudoThread *thread, unsigned char **thread_stack, unsigned char *original_stack)

void generate_YieldImpl()
{
  ES_CodeGenerator cg;

  cg.PUSH(0x4ff0); // PUSH { R4-R12, LR }

  cg.STR(ES_CodeGenerator::REG_SP, ES_CodeGenerator::REG_R1, 0); // *thread_stack = <SP>;
  cg.MOV(ES_CodeGenerator::REG_R2, ES_CodeGenerator::REG_SP);    // <SP> = original_stack;
  cg.MOV(0, ES_CodeGenerator::REG_R0);

  cg.POP(0x8ff0); // POP { R4-R12, PC }

  Output(cg, "YieldImpl");
}

// BOOL SuspendImpl(OpPseudoThread *thread, OpPseudoThread::Callback callback, unsigned char *original_stack)

void generate_SuspendImpl()
{
  ES_CodeGenerator cg;

  cg.PUSH(0x4ff0); // PUSH { R4-R12, LR }

  cg.MOV(ES_CodeGenerator::REG_SP, ES_CodeGenerator::REG_R4); // store SP in R4 for later use
  cg.MOV(ES_CodeGenerator::REG_R2, ES_CodeGenerator::REG_SP); // <SP> = original_stack;
  cg.BLX(ES_CodeGenerator::REG_R1);                           // callback(thread)
  cg.MOV(ES_CodeGenerator::REG_R4, ES_CodeGenerator::REG_SP); // restore SP from R4

  cg.POP(0x8ff0); // POP { R4-R12, PC }

  Output(cg, "SuspendImpl");
}

// BOOL ReserveImpl(OpPseudoThread *thread, OpPseudoThread::Callback callback, unsigned char *new_stack_ptr)

void generate_ReserveImpl()
{
  ES_CodeGenerator cg;

  cg.PUSH(0x4ff0); // PUSH { R4-R12, LR }

  cg.MOV(ES_CodeGenerator::REG_SP, ES_CodeGenerator::REG_R4); // store SP in R4 for later use
  cg.MOV(ES_CodeGenerator::REG_R2, ES_CodeGenerator::REG_SP); // <SP> = new_stack_ptr;
  cg.BLX(ES_CodeGenerator::REG_R1);                           // callback(thread)
  cg.MOV(ES_CodeGenerator::REG_R4, ES_CodeGenerator::REG_SP); // restore SP from R4

  cg.POP(0x8ff0); // POP { R4-R12, PC }

  Output(cg, "ReserveImpl");
}

// BOOL StackSpaceRemainingImpl(unsigned char *bottom)

void generate_StackSpaceRemainingImpl()
{
  ES_CodeGenerator cg;

  cg.SUB(ES_CodeGenerator::REG_SP, ES_CodeGenerator::REG_R0, ES_CodeGenerator::REG_R0);
  cg.BX(ES_CodeGenerator::REG_LR);

  Output(cg, "StackSpaceRemainingImpl");
}

int main(int argc, char **argv)
{
  generate_StartImpl();
  generate_ResumeImpl();
  generate_YieldImpl();
  generate_SuspendImpl();
  generate_ReserveImpl();
  generate_StackSpaceRemainingImpl();

  return 0;
}
