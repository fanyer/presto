/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) Opera Software ASA  2008
 *
 * @author Daniel Spang
 */

#ifndef ES_BCLOGGER_H
#define ES_BCLOGGER_H

#include "modules/util/simset.h"

#ifdef ES_BYTECODE_LOGGER

class ES_BytecodeLogger
{
public:
    ES_BytecodeLogger();
    void LogInstruction(ES_Instruction instruction);
    void PrintFrequentInstructionSequences(int rows);

private:
    ES_Instruction previous_instruction;
    unsigned instruction_map[ESI_LAST_INSTRUCTION][ESI_LAST_INSTRUCTION];
    Head instruction_pairs;
};

#endif // ES_BYTECODE_LOGGER
#endif // ES_BCLOGGER_H
