/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) Opera Software ASA  2002 - 2006
 *
 * Code analyzer.
 *
 * @author Jens Lindstrom
 */

#ifndef ES_ANALYZER_H
#define ES_ANALYZER_H

#include "modules/ecmascript/carakan/src/compiler/es_code.h"

class ES_Analyzer
{
public:
    static BOOL NextInstruction(ES_CodeStatic *code, ES_CodeWord *&word);

#ifdef ES_NATIVE_SUPPORT
    ES_Analyzer(ES_Context *context, ES_Code *code, BOOL loop_dispatcher = FALSE);
    ~ES_Analyzer();

    BOOL AssumeNoIntegerOverflows();

    void SetLoopIORange(unsigned start, unsigned end) { OP_ASSERT(loop_dispatcher); loop_io_start = start; loop_io_end = end; }

    ES_CodeOptimizationData *AnalyzeL();

private:
    ES_Context *context;
    ES_Code *code;
    ES_CodeWord *word;
    ES_CodeOptimizationData *data;

    BOOL loop_dispatcher;
    unsigned loop_io_start, loop_io_end;

    BOOL IsTemporary(ES_CodeWord::Index index) { return code->data->first_temporary_register <= index; }

    class Input
    {
    public:
        Input() {}
        Input(ES_CodeWord::Index index)
            : index(index),
              has_type(FALSE),
              has_forced_type(FALSE)
        {
        }
        Input(ES_CodeWord::Index index, BOOL has_type, BOOL has_forced_type, ES_ValueType type)
            : index(index),
              has_type(has_type),
              has_forced_type(has_forced_type),
              type(type)
        {
        }

        ES_CodeWord::Index index;
        BOOL has_type, has_forced_type;
        ES_ValueType type;
    };
    class Output
    {
    public:
        Output() {}
        Output(ES_CodeWord::Index index, BOOL optional = FALSE)
            : index(index & 0x7fffffffu),
              has_type(FALSE),
              has_value(FALSE),
              optional(optional)
        {
        }
        Output(ES_CodeWord::Index index, BOOL has_type, const ES_ValueType type, BOOL has_value = FALSE, unsigned value_cw_index = 0, BOOL optional = FALSE)
            : index(index & 0x7fffffffu),
              has_type(has_type),
              has_value(has_value),
              optional(optional),
              type(type),
              value_cw_index(value_cw_index)
        {
        }

        ES_CodeWord::Index index;
        BOOL has_type, has_value, optional;
        ES_ValueType type;
        unsigned value_cw_index;
    };

    ES_CodeOptimizationData::RegisterAccess **register_accesses;
    unsigned *register_access_counts;

    ES_CodeOptimizationData::JumpTarget *previous_jump_target;

    static unsigned FindRegisterAccessIndex(unsigned cw_index, ES_CodeOptimizationData::RegisterAccess *accesses, unsigned count);

    void AppendAccess(unsigned register_index, const ES_CodeOptimizationData::RegisterAccess &access, BOOL final = FALSE);
    void InsertAccess(unsigned register_index, const ES_CodeOptimizationData::RegisterAccess &access);
    void PropagateWriteInformation(unsigned register_index, ES_CodeOptimizationData::RegisterAccess *from);

    ES_CodeOptimizationData::RegisterAccess *ExplicitWriteAt(unsigned register_index, unsigned cw_index);
    ES_CodeOptimizationData::RegisterAccess *ImplicitWriteAt(unsigned register_index, unsigned cw_index);
    ES_CodeOptimizationData::RegisterAccess *ExplicitReadAt(unsigned register_index, unsigned cw_index);
    ES_CodeOptimizationData::RegisterAccess *ImplicitReadAt(unsigned register_index, unsigned cw_index);
    ES_CodeOptimizationData::RegisterAccess *WriteReadAt(unsigned register_index, unsigned cw_index);

    BOOL IsTrampled(unsigned register_index);
    BOOL KnownType(ES_ValueType &type, unsigned register_index, unsigned cw_index);
    BOOL KnownValue(ES_Value_Internal &value, unsigned &value_cw_index, unsigned register_index, unsigned cw_index);

    Output OutputFromInput(unsigned register_index, unsigned cw_index, const Input &input);

    void ProcessInstruction(Input *&input, Output *&output);
    void ProcessInput(unsigned cw_index, const Input &input);
    void ProcessOutput(unsigned cw_index, const Output &output, BOOL is_implicit = FALSE);
    BOOL ProcessJumps();
    BOOL ProcessJump(unsigned from, unsigned to);
    BOOL ReprocessInferredTypes();
    BOOL ReprocessInferredType(unsigned cw_index, unsigned inferred_type, BOOL inferred_value, unsigned value_write_index, ES_CodeWord::Index target);
    BOOL ReprocessCopy(unsigned cw_index, ES_CodeWord::Index from, ES_CodeWord::Index to);

    void ProcessRShiftUnsigned(unsigned cw_index, ES_CodeWord *word, unsigned &inferred_type);
#endif // ES_NATIVE_SUPPORT
};

#endif // ES_ANALYZER_H
