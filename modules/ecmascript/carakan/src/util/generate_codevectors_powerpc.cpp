#include "core/pch.h"

#include "modules/ecmascript/carakan/src/util/es_codegenerator_powerpc.h"

void Output(ES_CodeGenerator &cg, const char *name)
{
  unsigned *code = cg.GetBuffer();

  printf("const unsigned cv_%s[] =\n{\n", name);

  for (unsigned index = 0; index < cg.GetBufferUsed(); ++index)
    printf("    0x%08x%s\n", code[index], index + 1 == cg.GetBufferUsed() ? "" : ",");

  printf("};\n");
}

static void Store(ES_CodeGenerator &cg, ES_CodeGenerator::Register store_from = ES_CodeGenerator::REG_R14)
{
  unsigned stored_words = 5 + (ES_CodeGenerator::REG_R31 - store_from);
  stored_words += stored_words & 1;

  cg.MFLR(ES_CodeGenerator::REG_R0);
  cg.STW(ES_CodeGenerator::REG_SP, ES_CodeGenerator::Operand(ES_CodeGenerator::REG_SP, -static_cast<int>(stored_words * 4)), TRUE);
  cg.STW(ES_CodeGenerator::REG_R0, ES_CodeGenerator::Operand(ES_CodeGenerator::REG_SP, stored_words * 4 + 8));
  /* Save away the condition register */
  cg.MFCR(ES_CodeGenerator::REG_R7);
  cg.STW(ES_CodeGenerator::REG_R7, ES_CodeGenerator::Operand(ES_CodeGenerator::REG_SP, stored_words * 4 + 4));
  cg.SMW(store_from, ES_CodeGenerator::Operand(ES_CodeGenerator::REG_SP, 8));
}

static void Restore(ES_CodeGenerator &cg, ES_CodeGenerator::Register restore_from = ES_CodeGenerator::REG_R14)
{
  unsigned stored_words = 5 + (ES_CodeGenerator::REG_R31 - restore_from);
  stored_words += stored_words & 1;

  cg.LMW(ES_CodeGenerator::Operand(ES_CodeGenerator::REG_SP, 8), restore_from);
  cg.LDW(ES_CodeGenerator::Operand(ES_CodeGenerator::REG_SP, stored_words * 4 + 4), ES_CodeGenerator::REG_R7);
  cg.MTCRF(0xff, ES_CodeGenerator::REG_R7);
  /* Restore condition register */
  cg.LDW(ES_CodeGenerator::Operand(ES_CodeGenerator::REG_SP, stored_words * 4 + 8), ES_CodeGenerator::REG_R0);
  cg.MTLR(ES_CodeGenerator::REG_R0);
  cg.ADDI(ES_CodeGenerator::REG_SP, stored_words * 4, ES_CodeGenerator::REG_SP);
}

/* The PowerPC ABI has a linkage area + spilling area for callee-saves registers, above SP, of 24 + 32 bytes in length.
   Leave room for this in the freshly allocated stack area by adjusting SP. */
#define CALLEE_SAVE_SPILL_WORDS 8
#define LINKAGE_AREA_WORDS      6

#define FROM_STACK_ALLOCATED_WORDS (CALLEE_SAVE_SPILL_WORDS + LINKAGE_AREA_WORDS + 2)
#define TO_STACK_ALLOCATED_WORDS   (CALLEE_SAVE_SPILL_WORDS + LINKAGE_AREA_WORDS)

void generate_StartImpl()
{
  ES_CodeGenerator cg;

  Store(cg);

  /* Store SP and switch to thread stack. */
  cg.Move(ES_CodeGenerator::REG_R5, ES_CodeGenerator::REG_R31);

  cg.Move(ES_CodeGenerator::REG_SP, ES_CodeGenerator::REG_R7);
  cg.ADDI(ES_CodeGenerator::REG_R7, -(FROM_STACK_ALLOCATED_WORDS * 4), ES_CodeGenerator::REG_R7);
  cg.STW(ES_CodeGenerator::REG_R7, ES_CodeGenerator::Operand(ES_CodeGenerator::REG_R31, 0), FALSE);

  cg.ADDI(ES_CodeGenerator::REG_R6, -(TO_STACK_ALLOCATED_WORDS * 4), ES_CodeGenerator::REG_R6);
  cg.STW(ES_CodeGenerator::REG_SP, ES_CodeGenerator::Operand(ES_CodeGenerator::REG_R6, -8), TRUE);
  cg.Move(ES_CodeGenerator::REG_R6, ES_CodeGenerator::REG_SP);

  /* Call callback. */
  cg.MTCTR(ES_CodeGenerator::REG_R4);
  cg.BCTRL();

  /* Restore SP. */
  cg.LDW(ES_CodeGenerator::REG_R31, ES_CodeGenerator::REG_R7);
  cg.ADDI(ES_CodeGenerator::REG_R7, FROM_STACK_ALLOCATED_WORDS * 4, ES_CodeGenerator::REG_R7);
  cg.Move(ES_CodeGenerator::REG_R7, ES_CodeGenerator::REG_SP);

  Restore(cg);

  cg.Move(1, ES_CodeGenerator::REG_R3);
  cg.BLR();

  Output(cg, "StartImpl");
}

void generate_ResumeImpl()
{
  ES_CodeGenerator cg;

  Store(cg);

  cg.Move(ES_CodeGenerator::REG_SP, ES_CodeGenerator::REG_R7);
  cg.ADDI(ES_CodeGenerator::REG_R7, -(FROM_STACK_ALLOCATED_WORDS * 4), ES_CodeGenerator::REG_R7);
  cg.STW(ES_CodeGenerator::REG_R7, ES_CodeGenerator::REG_R3);

  cg.Move(ES_CodeGenerator::REG_R4, ES_CodeGenerator::REG_SP);

  Restore(cg);

  cg.BLR();

  Output(cg, "ResumeImpl");
}

void generate_YieldImpl()
{
  ES_CodeGenerator cg;

  Store(cg);

  cg.STW(ES_CodeGenerator::REG_SP, ES_CodeGenerator::REG_R3);

  cg.Move(ES_CodeGenerator::REG_R4, ES_CodeGenerator::REG_SP);
  cg.ADDI(ES_CodeGenerator::REG_SP, FROM_STACK_ALLOCATED_WORDS * 4, ES_CodeGenerator::REG_SP);

  Restore(cg);

  cg.Move(0, ES_CodeGenerator::REG_R3);
  cg.BLR();

  Output(cg, "YieldImpl");
}

void generate_SuspendImpl()
{
  ES_CodeGenerator cg;

  Store(cg, ES_CodeGenerator::REG_R31);

  cg.Move(ES_CodeGenerator::REG_SP, ES_CodeGenerator::REG_R31);
  cg.Move(ES_CodeGenerator::REG_R5, ES_CodeGenerator::REG_SP);

  cg.MTCTR(ES_CodeGenerator::REG_R4);
  cg.BCTRL();

  cg.Move(ES_CodeGenerator::REG_R31, ES_CodeGenerator::REG_SP);

  Restore(cg, ES_CodeGenerator::REG_R31);

  cg.BLR();

  Output(cg, "SuspendImpl");
}

void generate_ReserveImpl()
{
  ES_CodeGenerator cg;

  Store(cg, ES_CodeGenerator::REG_R31);

  cg.Move(ES_CodeGenerator::REG_SP, ES_CodeGenerator::REG_R31);
  cg.Move(ES_CodeGenerator::REG_R5, ES_CodeGenerator::REG_SP);
  cg.ADDI(ES_CodeGenerator::REG_SP, -(FROM_STACK_ALLOCATED_WORDS * 4), ES_CodeGenerator::REG_SP);

  cg.MTCTR(ES_CodeGenerator::REG_R4);
  cg.BCTRL();

  cg.Move(ES_CodeGenerator::REG_R31, ES_CodeGenerator::REG_SP);

  Restore(cg, ES_CodeGenerator::REG_R31);

  cg.BLR();

  Output(cg, "ReserveImpl");
}

void generate_StackSpaceRemainingImpl()
{
  ES_CodeGenerator cg;

  cg.SUB(ES_CodeGenerator::REG_SP, ES_CodeGenerator::REG_R3, ES_CodeGenerator::REG_R3);
  cg.BLR();

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
