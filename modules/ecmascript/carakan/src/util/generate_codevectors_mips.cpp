#include "core/pch.h"

#include "modules/ecmascript/carakan/src/util/es_codegenerator_mips.h"

void Output(ES_CodeGenerator &cg, const char *name)
{
  unsigned *code = cg.GetBuffer();

  printf("const MachineInstruction cv_%s[] =\n{\n", name);

  for (unsigned index = 0; index < cg.GetBufferUsed(); ++index)
    printf("    0x%08x%s\n", code[index], index + 1 == cg.GetBufferUsed() ? "" : ",");

  printf("};\n");
}

#define FRAME_SIZE 64

static void Store(ES_CodeGenerator &cg)
{
  cg.SUB(ES_CodeGenerator::REG_SP, FRAME_SIZE, ES_CodeGenerator::REG_SP);

  unsigned offset = FRAME_SIZE;

  cg.SW(ES_CodeGenerator::REG_RA, ES_CodeGenerator::REG_SP, offset -= 4);
  cg.SW(ES_CodeGenerator::REG_GP, ES_CodeGenerator::REG_SP, offset -= 4);
  cg.SW(ES_CodeGenerator::REG_FP, ES_CodeGenerator::REG_SP, offset -= 4);

  for (unsigned r = ES_CodeGenerator::REG_S0; r <= ES_CodeGenerator::REG_S7; ++r)
    cg.SW(static_cast<ES_CodeGenerator::Register>(r), ES_CodeGenerator::REG_SP, offset -= 4);
}

static void Restore(ES_CodeGenerator &cg, BOOL and_return = FALSE)
{
  unsigned offset = FRAME_SIZE;

  cg.LW(ES_CodeGenerator::REG_SP, offset -= 4, ES_CodeGenerator::REG_RA);
  cg.LW(ES_CodeGenerator::REG_SP, offset -= 4, ES_CodeGenerator::REG_GP);
  cg.LW(ES_CodeGenerator::REG_SP, offset -= 4, ES_CodeGenerator::REG_FP);

  for (unsigned r = ES_CodeGenerator::REG_S0; r <= ES_CodeGenerator::REG_S7; ++r)
    cg.LW(ES_CodeGenerator::REG_SP, offset -= 4, static_cast<ES_CodeGenerator::Register>(r));

  if (and_return)
    cg.Return();

  cg.ADD(ES_CodeGenerator::REG_SP, FRAME_SIZE, ES_CodeGenerator::REG_SP);
}

void generate_StartImpl()
{
  ES_CodeGenerator cg;

  Store(cg);

  cg.Move(ES_CodeGenerator::REG_A2, ES_CodeGenerator::REG_S0);
  cg.SW(ES_CodeGenerator::REG_SP, ES_CodeGenerator::REG_A2, 0);
  cg.Move(ES_CodeGenerator::REG_A1, ES_CodeGenerator::REG_T9);
  cg.Call(ES_CodeGenerator::REG_T9);
  cg.Move(ES_CodeGenerator::REG_A3, ES_CodeGenerator::REG_SP);
  cg.LW(ES_CodeGenerator::REG_S0, 0, ES_CodeGenerator::REG_SP);

  Restore(cg);

  cg.Return();
  cg.ADD(ES_CodeGenerator::REG_ZERO, 1, ES_CodeGenerator::REG_V0);

  Output(cg, "StartImpl");
}

void generate_ResumeImpl()
{
  ES_CodeGenerator cg;

  Store(cg);

  cg.SW(ES_CodeGenerator::REG_SP, ES_CodeGenerator::REG_A0, 0);
  cg.Move(ES_CodeGenerator::REG_A1, ES_CodeGenerator::REG_SP);

  Restore(cg, TRUE);

  Output(cg, "ResumeImpl");
}

void generate_YieldImpl()
{
  ES_CodeGenerator cg;

  Store(cg);

  cg.SW(ES_CodeGenerator::REG_SP, ES_CodeGenerator::REG_A0, 0);
  cg.Move(ES_CodeGenerator::REG_A1, ES_CodeGenerator::REG_SP);

  Restore(cg);

  cg.Return();
  cg.Move(ES_CodeGenerator::REG_ZERO, ES_CodeGenerator::REG_V0);

  Output(cg, "YieldImpl");
}

void generate_SuspendImpl()
{
  ES_CodeGenerator cg;

  cg.SUB(ES_CodeGenerator::REG_SP, 32, ES_CodeGenerator::REG_SP);
  cg.SW(ES_CodeGenerator::REG_RA, ES_CodeGenerator::REG_SP, 24);
  cg.SW(ES_CodeGenerator::REG_S0, ES_CodeGenerator::REG_SP, 16);

  cg.Move(ES_CodeGenerator::REG_SP, ES_CodeGenerator::REG_S0);
  cg.Move(ES_CodeGenerator::REG_A1, ES_CodeGenerator::REG_T9);
  cg.Call(ES_CodeGenerator::REG_T9);
  cg.Move(ES_CodeGenerator::REG_A2, ES_CodeGenerator::REG_SP);
  cg.Move(ES_CodeGenerator::REG_S0, ES_CodeGenerator::REG_SP);

  cg.LW(ES_CodeGenerator::REG_SP, 24, ES_CodeGenerator::REG_RA);
  cg.LW(ES_CodeGenerator::REG_SP, 16, ES_CodeGenerator::REG_S0);

  cg.Return();
  cg.ADD(ES_CodeGenerator::REG_SP, 32, ES_CodeGenerator::REG_SP);

  Output(cg, "SuspendImpl");
}

void generate_ReserveImpl()
{
  ES_CodeGenerator cg;

  cg.SUB(ES_CodeGenerator::REG_SP, 32, ES_CodeGenerator::REG_SP);
  cg.SW(ES_CodeGenerator::REG_RA, ES_CodeGenerator::REG_SP, 24);
  cg.SW(ES_CodeGenerator::REG_S0, ES_CodeGenerator::REG_SP, 16);

  cg.Move(ES_CodeGenerator::REG_SP, ES_CodeGenerator::REG_S0);
  cg.Move(ES_CodeGenerator::REG_A1, ES_CodeGenerator::REG_T9);
  cg.Call(ES_CodeGenerator::REG_T9);
  cg.Move(ES_CodeGenerator::REG_A2, ES_CodeGenerator::REG_SP);
  cg.Move(ES_CodeGenerator::REG_S0, ES_CodeGenerator::REG_SP);

  cg.LW(ES_CodeGenerator::REG_SP, 24, ES_CodeGenerator::REG_RA);
  cg.LW(ES_CodeGenerator::REG_SP, 16, ES_CodeGenerator::REG_S0);

  cg.Return();
  cg.ADD(ES_CodeGenerator::REG_SP, 32, ES_CodeGenerator::REG_SP);

  Output(cg, "ReserveImpl");
}

void generate_StackSpaceRemainingImpl()
{
  ES_CodeGenerator cg;

  cg.Return();
  cg.SUB(ES_CodeGenerator::REG_SP, ES_CodeGenerator::REG_A0, ES_CodeGenerator::REG_V0);

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
