/* -*- mode: c++; indent-tabs-mode: nil; tab-width: 4; c-file-style: "stroustrup" -*- */

#ifndef ES_CODEGENERATOR_BASE_FUNCTIONS_H
#define ES_CODEGENERATOR_BASE_FUNCTIONS_H

#include "modules/ecmascript/carakan/src/compiler/es_native.h"

ES_CodeGenerator_Base::ES_CodeGenerator_Base(OpMemGroup *arena)
    : arena(arena ? arena : &local_arena),
      free_blocks(NULL),
      free_blocks_end(NULL),
      free_targets(NULL),
      free_targets_end(NULL),
      buffer_base(NULL),
      buffer(NULL),
      buffer_top(NULL)
{
    Reset();
}

ES_CodeGenerator_Base::~ES_CodeGenerator_Base()
{
    OP_DELETEA(buffer_base);
}

void
ES_CodeGenerator_Base::Reset()
{
    first_block = NULL;
    last_block = NULL;
    current_block = NULL;
    prologue_point = NULL;
    first_ooo_block = NULL;
    last_ooo_block = NULL;
    current_ooo_block = NULL;
    first_target = NULL;
    last_target = NULL;
    jump_target_id_counter = 0;
    first_constant = NULL;
    in_prologue = FALSE;
    buffer = buffer_base;
}

void
ES_CodeGenerator_Base::Finalize(OpExecMemoryManager *execmem, const OpExecMemory *code)
{
    execmem->FinalizeL(code);
}

#ifdef ES_FUNCTION_CONSTANT_DATA_SECTION
OP_STATUS ES_CodeGenerator_Base::CreateDataSection(ES_NativeCodeBlock *ncblock)
{
    if (!first_constant)
        return OpStatus::OK;

    Constant *constant = first_constant;

    unsigned size = 0;
    int base_register_offset = 0;

    char *data_section;

#ifdef DEBUG_ENABLE_OPASSERT
    unsigned initial_aligment = constant->alignment;
#endif

    while (constant)
    {
        if (constant->first_use || constant->address_of)
        {
            // It's assumed that constants are naturally aligned and sorted largest first.
            OP_ASSERT(!(size & (constant->alignment - 1)));

            constant->offset = size;
            size += constant->size;
        }
        constant = constant->next;
    }

    if (!size)
        return OpStatus::OK;

    if (size > ES_DATA_SECTION_MAX_SIZE)
        return OpStatus::ERR_NOT_SUPPORTED;

    if (size > ES_DATA_SECTION_MAX_POSITIVE_OFFSET)
        base_register_offset = size / 2;

    data_section = (char *) op_malloc(size);

    if (!data_section)
        return OpStatus::ERR_NO_MEMORY;

    OP_ASSERT(((INTPTR) data_section & (initial_aligment - 1)) == 0);

    ncblock->SetDataSection(data_section, base_register_offset);

    constant = first_constant;
    while (constant)
    {
        if (constant->first_use || constant->address_of)
        {
            switch (constant->type)
            {
            case Constant::TYPE_INT:
                reinterpret_cast<int *>(data_section)[0] = constant->value.i;
                break;

            case Constant::TYPE_DOUBLE:
                reinterpret_cast<double *>(data_section)[0] = constant->value.d;
                break;

            case Constant::TYPE_2DOUBLE:
                reinterpret_cast<double *>(data_section)[0] = constant->value.d2.d1;
                reinterpret_cast<double *>(data_section)[1] = constant->value.d2.d2;
                break;

            case Constant::TYPE_POINTER:
                reinterpret_cast<const void **>(data_section)[0] = constant->value.p;
                break;
            }

            constant->instance = data_section;
            constant->offset -= base_register_offset;

            data_section += constant->size;
        }
        constant = constant->next;
    }

    constant = first_constant;
    while (constant)
    {
        if (constant->address_of)
        {
            OP_ASSERT(constant->address_of->type == Constant::TYPE_POINTER);
            *((void **) constant->address_of->instance) = constant->instance;
        }

        constant = constant->next;
    }

    return OpStatus::OK;
}
#endif // ES_FUNCTION_CONSTANT_DATA_SECTION

void
ES_CodeGenerator_Base::StartPrologue()
{
    if (current_block && current_block->end == UINT_MAX)
        current_block->end = buffer - buffer_base;

    AddBlock(TRUE);

    in_prologue = TRUE;
}

void
ES_CodeGenerator_Base::EndPrologue()
{
    in_prologue = FALSE;

    if (current_block->end == UINT_MAX)
        current_block->end = buffer - buffer_base;

    current_block = last_block;
}

ES_CodeGenerator_Base::Block *
ES_CodeGenerator_Base::SetProloguePoint()
{
    OP_ASSERT(current_block);

    if (current_block->end == UINT_MAX)
        current_block->end = buffer - buffer_base;

    Block *previous_prologue_point = prologue_point;
    prologue_point = current_block;
    return previous_prologue_point;
}

void
ES_CodeGenerator_Base::ResetProloguePoint(Block *reset_prologue_point)
{
    prologue_point = reset_prologue_point;
}

ES_CodeGenerator_Base::OutOfOrderBlock *
ES_CodeGenerator_Base::StartOutOfOrderBlock(BOOL align_block)
{
    OutOfOrderBlock *interrupted = current_ooo_block;

    if (current_block && current_block->end == UINT_MAX)
        current_block->end = buffer - buffer_base;

    current_ooo_block = OP_NEWGRO_L(OutOfOrderBlock, (), arena);
    current_ooo_block->first_block = current_ooo_block->last_block = NULL;
    current_ooo_block->previous_current_block = current_block;
    current_ooo_block->target = ForwardJump();
    current_ooo_block->continuation = ForwardJump();
    current_ooo_block->align_block = align_block;
    current_ooo_block->finalized = FALSE;
    if ((current_ooo_block->previous = last_ooo_block) != NULL)
        last_ooo_block->next = current_ooo_block;
    current_ooo_block->next = NULL;
    current_ooo_block->interrupted = interrupted;

    last_ooo_block = current_ooo_block;

    if (!first_ooo_block)
        first_ooo_block = current_ooo_block;

    return current_ooo_block;
}

BOOL
ES_CodeGenerator_Base::EndOutOfOrderBlock(BOOL jump_to_continuation_point)
{
    OP_ASSERT(current_ooo_block != NULL);

    BOOL empty = !current_ooo_block->first_block;

    if (!empty)
    {
        if (current_ooo_block->last_block->end == UINT_MAX)
            current_ooo_block->last_block->end = buffer - buffer_base;

        if (jump_to_continuation_point)
            if (current_ooo_block->last_block->jump_condition != ES_NATIVE_UNCONDITIONAL)
                Jump(current_ooo_block->continuation);
    }

    current_block = current_ooo_block->previous_current_block;
    current_ooo_block = current_ooo_block->interrupted;

    return !empty;
}

void
ES_CodeGenerator_Base::SetOutOfOrderContinuationPoint(OutOfOrderBlock *ooo_block, BOOL align_target)
{
    if (!ooo_block->finalized)
    {
        SetJumpTarget(ooo_block->continuation);
        ooo_block->finalized = TRUE;
    }
}

ES_CodeGenerator_Base::JumpTarget *
ES_CodeGenerator_Base::Here(BOOL align_target)
{
    if (!current_block || current_block->start != UINT_MAX || current_block->jump_condition != ES_NATIVE_NOT_A_JUMP || current_block->ooo_block != current_ooo_block)
    {
        Block *block = AddBlock();

        block->is_jump_target = TRUE;
        block->align_block = align_target;
    }

    if (free_targets == free_targets_end)
        free_targets_end = (free_targets = OP_NEWGROA_L(JumpTarget, 64, arena)) + 64;

    JumpTarget *jt = free_targets++;

    jt->next = NULL;
    jt->previous = last_target;
    jt->id = jump_target_id_counter++;
    jt->uses = 0;
    jt->block = current_block;
    jt->first_with_jump = NULL;

    if (last_target)
        last_target->next = jt;

    last_target = jt;
    return jt;
}

ES_CodeGenerator_Base::JumpTarget *
ES_CodeGenerator_Base::ForwardJump()
{
    if (free_targets == free_targets_end)
        free_targets_end = (free_targets = OP_NEWGROA_L(JumpTarget, 64, arena)) + 64;

    JumpTarget *jt = free_targets++;

    jt->next = NULL;
    jt->previous = last_target;
    jt->id = jump_target_id_counter++;
    jt->uses = 0;
    jt->block = NULL;
    jt->first_with_jump = NULL;

    if (last_target)
        last_target->next = jt;

    last_target = jt;
    return jt;
}

void
ES_CodeGenerator_Base::Jump(JumpTarget *target, ES_NativeJumpCondition condition, BOOL hint, BOOL branch_taken)
{
    if (!current_block || current_block->jump_condition != ES_NATIVE_NOT_A_JUMP || current_block->ooo_block != current_ooo_block)
        AddBlock();

    if (current_block->end == UINT_MAX)
        current_block->end = GetBufferUsed();

    current_block->jump_condition = condition;
    current_block->jump_target = target->block;
    current_block->jump_target_id = target->id;
    current_block->jump_size = UINT_MAX;
    current_block->jump_is_call = FALSE;

    ++target->uses;

    if ((current_block->hint = hint) == TRUE)
        current_block->branch_taken = branch_taken;

    current_block->next_with_jump = target->first_with_jump;
    target->first_with_jump = current_block;
}

void
ES_CodeGenerator_Base::Call(JumpTarget *target, ES_NativeJumpCondition condition, BOOL hint, BOOL branch_taken)
{
    if (!current_block || current_block->jump_condition != ES_NATIVE_NOT_A_JUMP || current_block->ooo_block != current_ooo_block)
        AddBlock();

    if (current_block->end == UINT_MAX)
        current_block->end = GetBufferUsed();

    current_block->jump_condition = condition;
    current_block->jump_target = target->block;
    current_block->jump_target_id = target->id;
    current_block->jump_size = UINT_MAX;
    current_block->jump_is_call = TRUE;

    ++target->uses;

    if ((current_block->hint = hint) == TRUE)
        current_block->branch_taken = branch_taken;

    current_block->next_with_jump = target->first_with_jump;
    target->first_with_jump = current_block;
}

void
ES_CodeGenerator_Base::SetJumpTarget(JumpTarget *target, BOOL align_target, Block *target_block)
{
    OP_ASSERT(!target->block);

    if (!target_block)
    {
        if (!current_block || static_cast<int>(current_block->start) != (buffer - buffer_base) || current_block->jump_condition != ES_NATIVE_NOT_A_JUMP || current_block->ooo_block != current_ooo_block)
        {
            Block *block = AddBlock();

            block->is_jump_target = TRUE;
            block->align_block = align_target;
        }

        target_block = current_block;
    }

    target->block = target_block;

    if (target->uses)
    {
        Block *block = target->first_with_jump;

        while (block)
        {
            if (!block->jump_is_call && block->next == target_block)
                block->jump_condition = ES_NATIVE_NOT_A_JUMP;
            else
                block->jump_target = target_block;

            block = block->next_with_jump;
        }
    }
}

void
ES_CodeGenerator_Base::Constant::AddUse(ES_CodeGenerator_Base::Block *block, unsigned offset, OpMemGroup *arena)
{
    Use *use = OP_NEWGRO_L(Use, (), arena);

    use->constant = this;
    use->block = block;
    use->offset = offset;

    use->next_per_constant = first_use;
    first_use = use;

    use->next_per_block = NULL;
    *block->constant_use = use;
    block->constant_use = &use->next_per_block;
}

#ifndef ES_FUNCTION_CONSTANT_DATA_SECTION
void
ES_CodeGenerator_Base::Constant::AddInstance(unsigned offset, OpMemGroup *arena)
{
    Instance *instance = OP_NEWGRO_L(Instance, (), arena);

    instance->offset = offset;
    instance->next = first_instance;
    first_instance = instance;
}
#endif // !ES_FUNCTION_CONSTANT_DATA_SECTION

ES_CodeGenerator_Base::Constant *
ES_CodeGenerator_Base::NewConstant(int i, unsigned alignment)
{
    Constant *constant = OP_NEWGRO_L(Constant, (), arena);
    constant->size = sizeof i;
    constant->alignment = alignment;
    constant->type = Constant::TYPE_INT;
    constant->value.i = i;
    Constant **ptr = &first_constant;
    while (*ptr && (*ptr)->alignment > alignment)
        ptr = &(*ptr)->next;
    constant->next = *ptr;
    *ptr = constant;
    return constant;
}

ES_CodeGenerator_Base::Constant *
ES_CodeGenerator_Base::NewConstant(double d, unsigned alignment)
{
    Constant *constant = OP_NEWGRO_L(Constant, (), arena);
    constant->size = sizeof d;
    constant->alignment = alignment;
    constant->type = Constant::TYPE_DOUBLE;
    constant->value.d = d;
    Constant **ptr = &first_constant;
    while (*ptr && (*ptr)->alignment > alignment)
        ptr = &(*ptr)->next;
    constant->next = *ptr;
    *ptr = constant;
    return constant;
}

ES_CodeGenerator_Base::Constant *
ES_CodeGenerator_Base::NewConstant(double d1, double d2, unsigned alignment)
{
    Constant *constant = OP_NEWGRO_L(Constant, (), arena);
    constant->size = sizeof d1 + sizeof d2;
    constant->alignment = alignment;
    constant->type = Constant::TYPE_2DOUBLE;
    constant->value.d2.d1 = d1;
    constant->value.d2.d2 = d2;
    Constant **ptr = &first_constant;
    while (*ptr && (*ptr)->alignment > alignment)
        ptr = &(*ptr)->next;
    constant->next = *ptr;
    *ptr = constant;
    return constant;
}

ES_CodeGenerator_Base::Constant *
ES_CodeGenerator_Base::NewConstant(const void *p, unsigned alignment)
{
    Constant *constant = OP_NEWGRO_L(Constant, (), arena);
    constant->size = sizeof p;
    constant->alignment = alignment;
    constant->type = Constant::TYPE_POINTER;
    constant->value.p = p;
    Constant **ptr = &first_constant;
    while (*ptr && (*ptr)->alignment > alignment)
        ptr = &(*ptr)->next;
    constant->next = *ptr;
    *ptr = constant;
    return constant;
}

ES_CodeGenerator_Base::Constant *
ES_CodeGenerator_Base::NewBlob(unsigned size, unsigned alignment)
{
    Constant *constant = OP_NEWGRO_L(Constant, (), arena);
    constant->size = size;
    constant->alignment = alignment;
    constant->type = Constant::TYPE_BLOB;

    Constant **ptr = &first_constant;
    while (*ptr && (*ptr)->alignment > alignment)
        ptr = &(*ptr)->next;
    constant->next = *ptr;
    *ptr = constant;
    return constant;
}

ES_CodeGenerator_Base::Constant *
ES_CodeGenerator_Base::AddressOf(Constant *constant)
{
    if (!constant->address_of)
        constant->address_of = NewConstant(static_cast<const void *>(NULL));

    return constant->address_of;
}

void *
ES_CodeGenerator_Base::GetBlobStorage(void *base, Constant *constant)
{
#ifdef ES_FUNCTION_CONSTANT_DATA_SECTION
        OP_ASSERT(constant->type == Constant::TYPE_BLOB);
        return constant->instance;
#else // ES_FUNCTION_CONSTANT_DATA_SECTION
        return (constant->first_use || constant->address_of) ? static_cast<char *>(base) + constant->first_instance->offset : NULL;
#endif // ES_FUNCTION_CONSTANT_DATA_SECTION
}

#ifdef NATIVE_DISASSEMBLER_ANNOTATION_SUPPORT

void
ES_CodeGenerator_Base::Annotate(const uni_char *string, unsigned length)
{
    if (string)
    {
        if (length == UINT_MAX)
            length = uni_strlen(string);

        (current_ooo_block ? current_ooo_block->annotation_buffer : annotation_buffer).AppendL(string, length);
    }
}

#endif // NATIVE_DISASSEMBLER_ANNOTATION_SUPPORT

#ifdef ES_FUNCTION_CONSTANT_DATA_SECTION
void
ES_CodeGenerator_Base::Block::AddDataSectionPointerLoad(unsigned offset, OpMemGroup *arena)
{
    DataSectionPointer *load = OP_NEWGRO_L(DataSectionPointer, (), arena);

    load->offset = offset;

    load->next = first_data_section_pointer;
    first_data_section_pointer = load;
}
#endif // ES_FUNCTION_CONSTANT_DATA_SECTION

ES_CodeGenerator_Base::Block *
ES_CodeGenerator_Base::AddBlock(BOOL start_prologue)
{
    if (free_blocks == free_blocks_end)
        free_blocks_end = (free_blocks = OP_NEWGROA_L(Block, 64, arena)) + 64;

    Block *block = free_blocks++;

    block->is_jump_target = FALSE;
    block->align_block = FALSE;
    block->start = buffer - buffer_base;
    block->end = UINT_MAX;
    block->actual_start = UINT_MAX;
    block->jump_condition = ES_NATIVE_NOT_A_JUMP;
#ifdef ES_CODEGENERATOR_RELATIVE_CALLS
    block->relative_calls = NULL;
#endif // ES_CODEGENERATOR_RELATIVE_CALLS
#ifdef NATIVE_DISASSEMBLER_ANNOTATION_SUPPORT
    block->annotations = NULL;
#endif // NATIVE_DISASSEMBLER_ANNOTATION_SUPPORT
#ifdef ES_FUNCTION_CONSTANT_DATA_SECTION
    block->first_data_section_pointer = NULL;
#endif // ES_FUNCTION_CONSTANT_DATA_SECTION
    block->next_with_jump = NULL;
    block->first_constant_use = NULL;
    block->constant_use = &block->first_constant_use;
#ifdef _DEBUG
    block->min_calculated_length = UINT_MAX;
#endif // _DEBUG

    OP_ASSERT(!current_ooo_block || !start_prologue);

    if (start_prologue)
    {
        block->ooo_block = NULL;

        if (prologue_point)
        {
            block->next = prologue_point->next;
            block->previous = prologue_point;

            if (prologue_point->next)
                prologue_point->next->previous = block;

            prologue_point->next = block;
        }
        else
        {
            block->next = first_block;
            block->previous = NULL;

            if (first_block)
                first_block->previous = block;

            first_block = block;
        }
    }
    else
    {
        block->next = NULL;

        if ((block->ooo_block = current_ooo_block) != NULL)
        {
            if ((block->previous = current_ooo_block->last_block) != NULL)
            {
                if (block->previous->end == UINT_MAX)
                    block->previous->end = buffer - buffer_base;
                block->previous->next = block;
            }

            current_ooo_block->last_block = block;
            if (!current_ooo_block->first_block)
                current_ooo_block->first_block = block;

            if (last_block && last_block->end == UINT_MAX)
                last_block->end = buffer - buffer_base;
        }
        else if (in_prologue)
        {
            if (current_block->end == UINT_MAX)
                current_block->end = buffer - buffer_base;

            block->previous = current_block;
            block->next = current_block->next;

            current_block->next = block;

            if (block->next)
                block->next->previous = block;
            else
                last_block = block;
        }
        else
        {
            if ((block->previous = last_block) != NULL)
            {
                if (block->previous->end == UINT_MAX)
                    block->previous->end = buffer - buffer_base;
                block->previous->next = block;
            }

            last_block = block;
            if (!first_block)
                first_block = block;
        }
    }

    return current_block = block;
}

void
ES_CodeGenerator_Base::FinalizeBlockList()
{
    if (current_block && current_block->start != UINT_MAX && current_block->end == UINT_MAX)
        current_block->end = GetBufferUsed();

    for (OutOfOrderBlock *ooo_block = first_ooo_block; ooo_block; ooo_block = ooo_block->next)
        if (ooo_block->first_block)
        {
            SetJumpTarget(ooo_block->target, FALSE, ooo_block->first_block);

            ooo_block->first_block->previous = last_block;
            last_block->next = ooo_block->first_block;
            last_block = ooo_block->last_block;
        }
}

void
ES_CodeGenerator_Base::GrowBuffer()
{
    unsigned new_size = buffer ? (buffer_top - buffer_base) * 2 : 4096;

    /* Note: not using the arena here, since we may be reallocating this array
       several times, and the arena probably couldn't reuse the memory
       anyway. */
    ES_NativeInstruction *new_buffer = OP_NEWA_L(ES_NativeInstruction, new_size);

    for (ES_NativeInstruction *p = buffer_base, *q = new_buffer; p < buffer; ++p, ++q)
        *q = *p;

    buffer = new_buffer + (buffer - buffer_base);
    buffer_top = new_buffer + new_size;

    OP_DELETEA(buffer_base);
    buffer_base = new_buffer;
}

#ifdef NATIVE_DISASSEMBLER_ANNOTATION_SUPPORT

void
ES_CodeGenerator_Base::AddAnnotation()
{
    TempBuffer &current_buffer = current_ooo_block ? current_ooo_block->annotation_buffer : annotation_buffer;

    if (current_buffer.Length() != 0)
    {
        Block::Annotation *annotation = OP_NEWGRO_L(Block::Annotation, (), arena);

        annotation->value = OP_NEWGROA_L(char, current_buffer.Length() + 3, arena);
        annotation->offset = (buffer - buffer_base) - current_block->start;

        Block::Annotation **pointer = &current_block->annotations;
        while (*pointer)
            pointer = &(*pointer)->next;
        *pointer = annotation;

        annotation->value[0] = '\n';
        for (unsigned index = 0; index < current_buffer.Length(); ++index)
            annotation->value[index + 1] = current_buffer.GetStorage()[index];
        annotation->value[current_buffer.Length() + 1] = '\n';
        annotation->value[current_buffer.Length() + 2] = 0;

        current_buffer.Clear();
    }
}

#endif // NATIVE_DISASSEMBLER_ANNOTATION_SUPPORT
#endif // ES_CODEGENERATOR_BASE_FUNCTIONS_H

