#include "core/pch.h"

#include "modules/ecmascript/carakan/src/util/es_codegenerator_arm.h"

void Output(ES_CodeGenerator &cg, const char *name)
{
  ES_CodeGenerator::Instruction *code = cg.GetBuffer();

  printf("const MachineInstruction cv_%s[] =\n{\n", name);

  for (unsigned index = 0; index < cg.GetBufferUsed(); ++index)
    printf("    0x%x%s\n", code[index], index + 1 == cg.GetBufferUsed() ? "" : ",");

  printf("};\n");
}

void Store(ES_CodeGenerator &cg)
{
  cg.PUSH(0x50f0); // PUSH { R4-R7, LR }

  cg.MOV(ES_CodeGenerator::REG_R8, ES_CodeGenerator::REG_R4);
  cg.MOV(ES_CodeGenerator::REG_R9, ES_CodeGenerator::REG_R5);
  cg.MOV(ES_CodeGenerator::REG_R10, ES_CodeGenerator::REG_R6);
  cg.MOV(ES_CodeGenerator::REG_R11, ES_CodeGenerator::REG_R7);
  cg.PUSH(0x00f0); // PUSH { R4-R7 }

  cg.MOV(ES_CodeGenerator::REG_R12, ES_CodeGenerator::REG_R4);
  cg.PUSH(0x0010); // PUSH { R4 }
}

void RestoreAndReturn(ES_CodeGenerator &cg, int return_value = -1)
{
  cg.POP(0x0010); // POP { R4 }
  cg.MOV(ES_CodeGenerator::REG_R4, ES_CodeGenerator::REG_R12);

  cg.POP(0x00f0); // POP { R4-R7 }
  cg.MOV(ES_CodeGenerator::REG_R7, ES_CodeGenerator::REG_R11);
  cg.MOV(ES_CodeGenerator::REG_R6, ES_CodeGenerator::REG_R10);
  cg.MOV(ES_CodeGenerator::REG_R5, ES_CodeGenerator::REG_R9);
  cg.MOV(ES_CodeGenerator::REG_R4, ES_CodeGenerator::REG_R8);

  if (return_value != -1)
    cg.MOV(return_value, ES_CodeGenerator::REG_R0L);

  cg.POP(0x80f0); // PUSH { R4-R7, PC }
}

// BOOL StartImpl(OpPseudoThread *thread, OpPseudoThread::Callback callback, unsigned char **original_stack, unsigned char *thread_stack)

void generate_StartImpl()
{
  ES_CodeGenerator cg;

  Store(cg);

  cg.MOV(ES_CodeGenerator::REG_R2, ES_CodeGenerator::REG_R4);
  cg.MOV(ES_CodeGenerator::REG_SP, ES_CodeGenerator::REG_R5);
  cg.STR(ES_CodeGenerator::REG_R5L, ES_CodeGenerator::REG_R4L, 0);
  cg.MOV(ES_CodeGenerator::REG_R3, ES_CodeGenerator::REG_SP);
  cg.BLX(ES_CodeGenerator::REG_R1);
  cg.LDR(ES_CodeGenerator::REG_R4L, 0, ES_CodeGenerator::REG_R5L);
  cg.MOV(ES_CodeGenerator::REG_R5, ES_CodeGenerator::REG_SP);

  RestoreAndReturn(cg, 1);

  Output(cg, "StartImpl");
}

// BOOL ResumeImpl(OpPseudoThread *thread, unsigned char **original_stack, unsigned char *thread_stack)

void generate_ResumeImpl()
{
  ES_CodeGenerator cg;

  Store(cg);

  cg.MOV(ES_CodeGenerator::REG_SP, ES_CodeGenerator::REG_R3);
  cg.STR(ES_CodeGenerator::REG_R3L, ES_CodeGenerator::REG_R1L, 0); // *original_stack = <SP>;
  cg.MOV(ES_CodeGenerator::REG_R2, ES_CodeGenerator::REG_SP);    // <SP> = thread_stack;

  RestoreAndReturn(cg);

  Output(cg, "ResumeImpl");
}

// BOOL YieldImpl(OpPseudoThread *thread, unsigned char **thread_stack, unsigned char *original_stack)

void generate_YieldImpl()
{
  ES_CodeGenerator cg;

  Store(cg);

  cg.MOV(ES_CodeGenerator::REG_SP, ES_CodeGenerator::REG_R3);
  cg.STR(ES_CodeGenerator::REG_R3L, ES_CodeGenerator::REG_R1L, 0); // *thread_stack = <SP>;
  cg.MOV(ES_CodeGenerator::REG_R2, ES_CodeGenerator::REG_SP);    // <SP> = original_stack;

  RestoreAndReturn(cg, 0);

  Output(cg, "YieldImpl");
}

// BOOL SuspendImpl(OpPseudoThread *thread, OpPseudoThread::Callback callback, unsigned char *original_stack)

void generate_SuspendImpl()
{
  ES_CodeGenerator cg;

  cg.PUSH(0x4010); // PUSH { R4, LR }

  cg.MOV(ES_CodeGenerator::REG_SP, ES_CodeGenerator::REG_R4); // store SP in R4 for later use
  cg.MOV(ES_CodeGenerator::REG_R2, ES_CodeGenerator::REG_SP); // <SP> = original_stack;
  cg.BLX(ES_CodeGenerator::REG_R1);                           // callback(thread)
  cg.MOV(ES_CodeGenerator::REG_R4, ES_CodeGenerator::REG_SP); // restore SP from R4

  cg.POP(0x8010); // POP { R4, PC }

  Output(cg, "SuspendImpl");
}

// BOOL ReserveImpl(OpPseudoThread *thread, OpPseudoThread::Callback callback, unsigned char *new_stack_ptr)

void generate_ReserveImpl()
{
  ES_CodeGenerator cg;

  cg.PUSH(0x4010); // PUSH { R4, LR }

  cg.MOV(ES_CodeGenerator::REG_SP, ES_CodeGenerator::REG_R4); // store SP in R4 for later use
  cg.MOV(ES_CodeGenerator::REG_R2, ES_CodeGenerator::REG_SP); // <SP> = new_stack;
  cg.BLX(ES_CodeGenerator::REG_R1);                           // callback(thread)
  cg.MOV(ES_CodeGenerator::REG_R4, ES_CodeGenerator::REG_SP); // restore SP from R4

  cg.POP(0x8010); // POP { R4, PC }

  Output(cg, "ReserveImpl");
}

// BOOL StackSpaceRemainingImpl(unsigned char *bottom)

void generate_StackSpaceRemainingImpl()
{
  ES_CodeGenerator cg;

  cg.MOV(ES_CodeGenerator::REG_SP, ES_CodeGenerator::REG_R1);
  cg.SUB(ES_CodeGenerator::REG_R1L, ES_CodeGenerator::REG_R0L, ES_CodeGenerator::REG_R0L);
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
