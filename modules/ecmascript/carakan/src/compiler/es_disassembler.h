/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) Opera Software ASA  2002 - 2006
 *
 * Bytecode compiler for ECMAScript -- overview, common data and functions
 *
 * @author Jens Lindstrom
 */

#ifndef ES_DISASSEMBLER_H
#define ES_DISASSEMBLER_H

#ifdef ES_DISASSEMBLER_SUPPORT
#include "modules/ecmascript/carakan/src/compiler/es_code.h"

class ES_Disassembler
{
public:
    ES_Disassembler(ES_Context *context);

    void Disassemble(ES_ProgramCode *program, BOOL local_only = FALSE);
    void Disassemble(ES_FunctionCode *function, BOOL local_only = FALSE, BOOL force_include = FALSE);
    void Disassemble(ES_Code *code, ES_CodeWord *codewords, unsigned codewords_count);

    const uni_char *GetOutput() { return output.GetStorage(); }
    unsigned GetLength() { return output.Length(); }

    unsigned *GetInstructionLineNumbers() { unsigned *result = line_numbers; line_numbers = NULL; return result; }

    void SetOptimizationData(ES_CodeOptimizationData *data) { optimization_data = data; }

    TempBuffer &DisassembleInstruction(ES_Code *code, ES_CodeWord *word);
    const char *GetInstructionName(ES_Instruction instruction);

private:
    void DisassembleCode(ES_Code *code, ES_CodeWord *codewords, unsigned codewords_count);
    void DisassembleInstruction(ES_Code *code, ES_CodeWord *&word, ES_CodeWord *codewords);
    void OutputFunctions(ES_Code *code, BOOL force_include = FALSE);

    void OutputInstruction(const char *string);
    void OutputRegister(unsigned index, const char *key = NULL);
    void OutputRegisters(unsigned count, ES_CodeWord *&word);
    void OutputImmediate(int immediate, const uni_char *key = NULL);
    void OutputOffset(int offset, unsigned base);
    void OutputIdentifier(ES_Code *code, ES_CodeWord &word);
    void OutputString(ES_Context *context, ES_Code *code, unsigned index);
    void OutputDouble(ES_Context *context, ES_Code *code, unsigned index);
    void OutputFunction(ES_Code *code, ES_CodeWord &word);
    void OutputPropertyOffset(unsigned index);
    void OutputPropertyCache(ES_Code *code, unsigned index);
    void OutputGlobalCache(ES_Code *code, unsigned index);
#ifdef ES_NATIVE_SUPPORT
    void OutputProfilingData(ES_Code *code, ES_CodeWord *word, unsigned index);
#endif // ES_NATIVE_SUPPORT

    TempBuffer output;
    ES_Code *primary;
    ES_Code *code;
    ES_CodeWord *word;
    unsigned *line_numbers, operand_index;
    ES_Context *context;
    ES_CodeOptimizationData *optimization_data;
};

#endif // ES_DISASSEMBLER_SUPPORT
#endif // ES_DISASSEMBLER_H
