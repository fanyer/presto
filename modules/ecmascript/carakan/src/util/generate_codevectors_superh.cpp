#include "core/pch.h"

#include "modules/ecmascript/carakan/src/util/es_codegenerator_superh.h"

void Output(ES_CodeGenerator &cg, const char *name)
{
  unsigned char *code = cg.GetBuffer();

  printf("const MachineInstruction cv_%s[] =\n{\n", name);

  for (unsigned index = 0; index < cg.GetBufferUsed(); index += 2)
    printf("    0x%x, 0x%x%s\n", code[index], code[index + 1], index + 2 == cg.GetBufferUsed() ? "" : ",");

  printf("};\n");
}

static void Store(ES_CodeGenerator &cg)
{
  cg.PushPR();

  for (unsigned r = ES_CodeGenerator::REG_R8; r <= ES_CodeGenerator::REG_R14; ++r)
    cg.MOV(static_cast<ES_CodeGenerator::Register>(r), ES_CodeGenerator::Memory(ES_CodeGenerator::REG_SP, ES_CodeGenerator::Memory::TYPE_AUTO_ADJUST));
}

static void Restore(ES_CodeGenerator &cg)
{
  for (unsigned r = ES_CodeGenerator::REG_R14; r >= ES_CodeGenerator::REG_R8; --r)
    cg.MOV(ES_CodeGenerator::Memory(ES_CodeGenerator::REG_SP, ES_CodeGenerator::Memory::TYPE_AUTO_ADJUST), static_cast<ES_CodeGenerator::Register>(r));

  cg.PopPR();
}

void generate_StartImpl()
{
  ES_CodeGenerator cg;

  Store(cg);

  cg.MOV(ES_CodeGenerator::REG_R6, ES_CodeGenerator::REG_R8);
  cg.MOV(ES_CodeGenerator::REG_R15, ES_CodeGenerator::Memory(ES_CodeGenerator::REG_R6));
  cg.JSR(ES_CodeGenerator::REG_R5);
  cg.MOV(ES_CodeGenerator::REG_R7, ES_CodeGenerator::REG_R15); // note: this instruction is executed before the above jump takes place
  cg.MOV(ES_CodeGenerator::Memory(ES_CodeGenerator::REG_R8), ES_CodeGenerator::REG_R15);

  Restore(cg);

  cg.RTS();
  cg.MOV(1, ES_CodeGenerator::REG_R0); // return value: this instruction does execute before RTS takes effect

  Output(cg, "StartImpl");
}

void generate_ResumeImpl()
{
  ES_CodeGenerator cg;

  Store(cg);

  cg.MOV(ES_CodeGenerator::REG_R15, ES_CodeGenerator::Memory(ES_CodeGenerator::REG_R4));
  cg.MOV(ES_CodeGenerator::REG_R5, ES_CodeGenerator::REG_R15);

  Restore(cg);

  cg.RTS();
  cg.NOP();

  Output(cg, "ResumeImpl");
}

void generate_YieldImpl()
{
  ES_CodeGenerator cg;

  Store(cg);

  cg.MOV(ES_CodeGenerator::REG_R15, ES_CodeGenerator::Memory(ES_CodeGenerator::REG_R4));
  cg.MOV(ES_CodeGenerator::REG_R5, ES_CodeGenerator::REG_R15);

  Restore(cg);

  cg.RTS();
  cg.MOV(0, ES_CodeGenerator::REG_R0); // return value: this instruction does execute before RTS takes effect

  Output(cg, "YieldImpl");
}

void generate_SuspendImpl()
{
  ES_CodeGenerator cg;

  cg.MOV(ES_CodeGenerator::REG_R8, ES_CodeGenerator::Memory(ES_CodeGenerator::REG_SP, ES_CodeGenerator::Memory::TYPE_AUTO_ADJUST));
  cg.PushPR();

  cg.MOV(ES_CodeGenerator::REG_R15, ES_CodeGenerator::REG_R8);
  cg.JSR(ES_CodeGenerator::REG_R5);
  cg.MOV(ES_CodeGenerator::REG_R6, ES_CodeGenerator::REG_R15); // note: this instruction is executed before the above jump takes place
  cg.MOV(ES_CodeGenerator::REG_R8, ES_CodeGenerator::REG_R15);

  cg.PopPR();
  cg.RTS();
  cg.MOV(ES_CodeGenerator::Memory(ES_CodeGenerator::REG_SP, ES_CodeGenerator::Memory::TYPE_AUTO_ADJUST), ES_CodeGenerator::REG_R8);

  Output(cg, "SuspendImpl");
}

void generate_ReserveImpl()
{
  ES_CodeGenerator cg;

  cg.MOV(ES_CodeGenerator::REG_R8, ES_CodeGenerator::Memory(ES_CodeGenerator::REG_SP, ES_CodeGenerator::Memory::TYPE_AUTO_ADJUST));
  cg.PushPR();

  cg.MOV(ES_CodeGenerator::REG_R15, ES_CodeGenerator::REG_R8);
  cg.JSR(ES_CodeGenerator::REG_R5);
  cg.MOV(ES_CodeGenerator::REG_R6, ES_CodeGenerator::REG_R15); // note: this instruction is executed before the above jump takes place
  cg.MOV(ES_CodeGenerator::REG_R8, ES_CodeGenerator::REG_R15);

  cg.PopPR();
  cg.RTS();
  cg.MOV(ES_CodeGenerator::Memory(ES_CodeGenerator::REG_SP, ES_CodeGenerator::Memory::TYPE_AUTO_ADJUST), ES_CodeGenerator::REG_R8);

  Output(cg, "ReserveImpl");
}

void generate_StackSpaceRemainingImpl()
{
  ES_CodeGenerator cg;

  cg.MOV(ES_CodeGenerator::REG_SP, ES_CodeGenerator::REG_R0);
  cg.RTS();
  cg.SUB(ES_CodeGenerator::REG_R4, ES_CodeGenerator::REG_R0);

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
