/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*- */

#ifndef ES_REGISTER_BLOCKS_H
#define ES_REGISTER_BLOCKS_H

#include "modules/ecmascript/carakan/src/vm/es_block.h"
#include "modules/ecmascript/carakan/src/kernel/es_value.h"

#ifdef ES_NATIVE_SUPPORT

class ES_RegisterBlocksAdjustment
{
public:
    unsigned allocation_depth;
    unsigned original_used;
    unsigned new_used;
    ES_Block<ES_Value_Internal> *block;
};

#endif // ES_NATIVE_SUPPORT

class ES_RegisterBlocks
    : public ES_BlockHead<ES_Value_Internal>
{
public:
    ES_RegisterBlocks(ES_Execution_Context *context);

#ifdef ES_NATIVE_SUPPORT
    OP_STATUS Initialize(ES_Execution_Context *context);

    OP_STATUS Allocate(ES_Execution_Context *context, ES_Value_Internal *&item, unsigned size = 1);
    OP_STATUS Allocate(ES_Execution_Context *context, BOOL &first_in_block, ES_Value_Internal *&item, unsigned size, unsigned overlap, unsigned copy);

    void Free(unsigned size = 1);
    void Free(unsigned size, unsigned overlap, unsigned copy, BOOL first_in_block = FALSE);

    OP_STATUS AdjustUsed();
    void ReadjustUsed();

private:
    ES_Execution_Context *context;
    ES_BlockHead<ES_RegisterBlocksAdjustment> adjustments;
    unsigned current_depth;
#endif // ES_NATIVE_SUPPORT
};

#endif // ES_FRAME_H
