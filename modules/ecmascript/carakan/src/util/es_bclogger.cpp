/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) Opera Software ASA  2008
 *
 * @author Daniel Spang
 */

#include "core/pch.h"

#include "modules/ecmascript/carakan/src/es_pch.h"

#ifdef ES_BYTECODE_LOGGER

#include "modules/ecmascript/carakan/src/util/es_bclogger.h"
#include "modules/ecmascript/carakan/src/util/es_instruction_string.h"
#include "modules/util/opstring.h"

ES_BytecodeLogger::ES_BytecodeLogger()
    : previous_instruction(ESI_LAST_INSTRUCTION)
{
    for (int i = 0; i < ESI_LAST_INSTRUCTION; ++i)
        for (int j = 0; j < ESI_LAST_INSTRUCTION; ++j)
            instruction_map[i][j] = 0;
}

void
ES_BytecodeLogger::LogInstruction(ES_Instruction instruction)
{
    if (previous_instruction != ESI_LAST_INSTRUCTION)
    {
        OP_ASSERT(previous_instruction < ESI_LAST_INSTRUCTION && instruction < ESI_LAST_INSTRUCTION);

        instruction_map[previous_instruction][instruction]++;
    }
    previous_instruction = instruction;
}

void
ES_BytecodeLogger::PrintFrequentInstructionSequences(int rows)
{
    // Have you forgot to update es_instruction_string.h?
    OP_ASSERT(sizeof(g_instruction_strings)/sizeof(g_instruction_strings[0]) == ESI_LAST_INSTRUCTION+1);
    fprintf(stdout, "Most frequent instruction sequences\n");
    for (int p = 0; p < rows; ++p)
    {
        unsigned max = 0;
        int first, second;

        for (int i = 0; i < ESI_LAST_INSTRUCTION; ++i)
            for (int j = 0; j < ESI_LAST_INSTRUCTION; ++j)
                if (instruction_map[i][j] > max)
                {
                    max = instruction_map[i][j];
                    first = i;
                    second = j;
                }
        if (max > 0)
        {
            OP_ASSERT(first < ESI_LAST_INSTRUCTION && second < ESI_LAST_INSTRUCTION);
            fprintf(stdout, "%8u: %-25s -> %-25s\n", max, g_instruction_strings[first], g_instruction_strings[second]);
            instruction_map[first][second] = 0;
        }
    }
}

#endif // ES_BYTECODE_LOGGER
