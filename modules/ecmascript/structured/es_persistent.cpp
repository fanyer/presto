/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) Opera Software ASA  1995-2011
 */

#include "core/pch.h"

#ifdef ES_PERSISTENT_SUPPORT

#include "modules/ecmascript/structured/es_persistent.h"
#include "modules/ecmascript/ecmascript.h"

enum ES_PersistentTag
{
    ES_PTAG_OBJECT_ARRAYBUFFER = 0x7fffffed,
    ES_PTAG_OBJECT_TYPEDARRAY = 0x7fffffee,
    ES_PTAG_NUMBER         = 0x7fffffef,
    ES_PTAG_NULL           = 0x7ffffff0,
    ES_PTAG_UNDEFINED      = 0x7ffffff1,
    ES_PTAG_BOOLEAN        = 0x7ffffff2,
    ES_PTAG_STRING         = 0x7ffffff3,
    ES_PTAG_STRING_AGAIN   = 0x7ffffff4,
    ES_PTAG_OBJECT         = 0x7ffffff5,
    ES_PTAG_OBJECT_BOOLEAN = 0x7ffffff6,
    ES_PTAG_OBJECT_NUMBER  = 0x7ffffff7,
    ES_PTAG_OBJECT_STRING  = 0x7ffffff8,
    ES_PTAG_OBJECT_ARRAY   = 0x7ffffff9,
    ES_PTAG_OBJECT_REGEXP  = 0x7ffffffa,
    ES_PTAG_OBJECT_DATE    = 0x7ffffffb,
    ES_PTAG_OBJECT_AGAIN   = 0x7ffffffc,
    ES_PTAG_PROPERTY       = 0x7ffffffd,
    ES_PTAG_HOST_OBJECT    = 0x7ffffffe,
    ES_PTAG_STOP           = 0x7fffffff
};

#define GROW(n) do { if (buffer_used + (n) > buffer_size) GrowBufferL(n); } while (0)
#define PUT(x) buffer[buffer_used++] = (x)
#define PUT2(tag, value) do { PUT(tag); PUT(value); } while (0)
#define BUFFER() &(buffer[buffer_used])
#define ADVANCE(x) buffer_used += x

ES_CloneToPersistent::ES_CloneToPersistent(unsigned size_limit)
    : size_limit(size_limit),
      buffer(NULL),
      buffer_used(0),
      buffer_size(0)
{
}

/* virtual */
ES_CloneToPersistent::~ES_CloneToPersistent()
{
    OP_DELETEA(buffer);

    /* Items will be left in the vector only if the clone was aborted.
       GetResultL() clears it as it transfers the items to the
       ES_PersistentValue object. */
    items.DeleteAll();
}

/* virtual */ void
ES_CloneToPersistent::PushNullL()
{
    GROW(1);
    PUT(ES_PTAG_NULL);
}

/* virtual */ void
ES_CloneToPersistent::PushUndefinedL()
{
    GROW(1);
    PUT(ES_PTAG_UNDEFINED);
}

/* virtual */ void
ES_CloneToPersistent::PushBooleanL(BOOL value)
{
    GROW(2);
    PUT2(ES_PTAG_BOOLEAN, value);
}

/* virtual */ void
ES_CloneToPersistent::PushNumberL(double value)
{
    GROW(2);

    if (op_isnan(value))
        PUT2(ES_PTAG_NUMBER, 0);
    else
        PUT2(op_double_high(value), op_double_low(value));
}

/* virtual */ void
ES_CloneToPersistent::PushStringL(const uni_char *value, unsigned length)
{
    GROW(2 + (length + 1) / 2);
    PUT(ES_PTAG_STRING);
    PUT(length);

    for (unsigned index = 0, stop = length & ~1u; index != stop; index += 2)
#ifdef OPERA_BIG_ENDIAN
        PUT(value[index + 1] | (static_cast<unsigned>(value[index]) << 16));
#else
        PUT(value[index] | (static_cast<unsigned>(value[index + 1]) << 16));
#endif // OPERA_BIG_ENDIAN

    if (length & 1u)
#ifdef OPERA_BIG_ENDIAN
        PUT(static_cast<unsigned>(value[length - 1]) << 16);
#else
        PUT(value[length - 1]);
#endif // OPERA_BIG_ENDIAN
}

/* virtual */ void
ES_CloneToPersistent::PushStringL(unsigned index)
{
    GROW(2);
    PUT2(ES_PTAG_STRING_AGAIN, index);
}

/* virtual */ void
ES_CloneToPersistent::PushObjectL(unsigned index)
{
    GROW(2);
    PUT2(ES_PTAG_OBJECT_AGAIN, index);
}

/* virtual */ void
ES_CloneToPersistent::StartObjectL()
{
    GROW(1);
    PUT(ES_PTAG_OBJECT);
}

/* virtual */ void
ES_CloneToPersistent::StartObjectBooleanL()
{
    GROW(1);
    PUT(ES_PTAG_OBJECT_BOOLEAN);
}

/* virtual */ void
ES_CloneToPersistent::StartObjectNumberL()
{
    GROW(1);
    PUT(ES_PTAG_OBJECT_NUMBER);
}

/* virtual */ void
ES_CloneToPersistent::StartObjectStringL()
{
    GROW(1);
    PUT(ES_PTAG_OBJECT_STRING);
}

/* virtual */ void
ES_CloneToPersistent::StartObjectArrayL()
{
    GROW(1);
    PUT(ES_PTAG_OBJECT_ARRAY);
}

/* virtual */ void
ES_CloneToPersistent::StartObjectRegExpL()
{
    GROW(1);
    PUT(ES_PTAG_OBJECT_REGEXP);
}

/* virtual */ void
ES_CloneToPersistent::StartObjectDateL()
{
    GROW(1);
    PUT(ES_PTAG_OBJECT_DATE);
}

/* virtual */ void
ES_CloneToPersistent::StartObjectArrayBufferL(const unsigned char *value, unsigned length)
{
    GROW(2 + (length + 3) / 4);
    PUT(ES_PTAG_OBJECT_ARRAYBUFFER);
    PUT(length);

    op_memcpy(BUFFER(), value, length);
    ADVANCE((length + 3) / 4);
}

/* virtual */ void
ES_CloneToPersistent::StartObjectTypedArrayL(unsigned kind, unsigned offset, unsigned size)
{
    GROW(4);
    PUT(ES_PTAG_OBJECT_TYPEDARRAY);
    PUT(kind);
    PUT(offset);
    PUT(size);
}

/* virtual */ void
ES_CloneToPersistent::AddPropertyL(int attributes)
{
    GROW(2);
    PUT2(ES_PTAG_PROPERTY, attributes);
}

/* virtual */ BOOL
ES_CloneToPersistent::HostObjectL(EcmaScript_Object *source_object)
{
    ES_HostObjectCloneHandler *handler = g_ecmaManager->GetFirstHostObjectCloneHandler();

    while (handler)
    {
        ES_PersistentItem *target_item;
        OP_STATUS status = handler->Clone(source_object, target_item);

        if (status == OpStatus::OK)
        {
            PUT2(ES_PTAG_HOST_OBJECT, items.GetCount());

            if (OpStatus::IsMemoryError(items.Add(target_item)))
            {
                OP_DELETE(target_item);
                LEAVE(OpStatus::ERR_NO_MEMORY);
            }

            return TRUE;
        }
        else if (status != OpStatus::ERR)
            LEAVE(status);

        handler = g_ecmaManager->GetNextHostObjectCloneHandler(handler);
    }

    return FALSE;
}

/* virtual */ BOOL
ES_CloneToPersistent::HostObjectL(ES_PersistentItem *source_item)
{
    /* ES_HostObjectCloneHandler doesn't support persistent->persistent, and
       there's really no reason to. */
    return FALSE;
}

#undef GROW
#undef PUT
#undef PUT2

ES_PersistentValue *
ES_CloneToPersistent::GetResultL()
{
    ES_PersistentValue *value = OP_NEW_L(ES_PersistentValue, ());

    value->data = OP_NEWA(unsigned, buffer_used);

    if (!value->data)
    {
        OP_DELETE(value);
        LEAVE(OpStatus::ERR_NO_MEMORY);
    }

    op_memcpy(value->data, buffer, buffer_used * sizeof *buffer);
    value->length = buffer_used;

    unsigned items_count = items.GetCount();

    if (items_count != 0)
    {
        value->items = OP_NEWA(ES_PersistentItem *, items_count);

        if (!value->items)
        {
            OP_DELETE(value);
            LEAVE(OpStatus::ERR_NO_MEMORY);
        }

        for (unsigned index = 0; index < items_count; ++index)
            value->items[index] = items.Get(index);

        value->items_count = items_count;

        items.Clear();
    }

    return value;
}

void
ES_CloneToPersistent::GrowBufferL(unsigned n)
{
    if (size_limit != 0 && (buffer_used + n) * sizeof *buffer > size_limit)
        LEAVE(OpStatus::ERR);

    unsigned new_buffer_size;

    if (buffer_size == 0)
        new_buffer_size = 64;
    else
        new_buffer_size = buffer_size * 2;

    while (buffer_used + n > new_buffer_size)
        new_buffer_size += new_buffer_size;

    if (size_limit != 0 && new_buffer_size * sizeof *buffer > size_limit)
        new_buffer_size = (size_limit + sizeof *buffer - 1) / sizeof *buffer;

    unsigned *new_buffer = OP_NEWA_L(unsigned, new_buffer_size);

    if (buffer)
    {
        op_memcpy(new_buffer, buffer, buffer_used * sizeof *buffer);
        OP_DELETEA(buffer);
    }

    buffer = new_buffer;
    buffer_size = new_buffer_size;
}

ES_CloneFromPersistent::ES_CloneFromPersistent(ES_PersistentValue *value)
    : value(value)
{
}

/* virtual */ void
ES_CloneFromPersistent::CloneL(ES_CloneBackend *target)
{
    const unsigned *ptr = value->data, *stop = ptr + value->length;

    while (ptr != stop)
    {
        unsigned tag = *ptr++;

        switch (tag)
        {
        case ES_PTAG_NULL:
            target->PushNullL();
            break;

        case ES_PTAG_UNDEFINED:
            target->PushUndefinedL();
            break;

        case ES_PTAG_BOOLEAN:
            target->PushBooleanL(*ptr++ != 0);
            break;

        case ES_PTAG_STRING:
            {
                unsigned length = *ptr++;
                target->PushStringL(reinterpret_cast<const uni_char *>(ptr), length);
                ptr += (length + 1) / 2;
            }
            break;

        case ES_PTAG_STRING_AGAIN:
            target->PushStringL(*ptr++);
            break;

        case ES_PTAG_OBJECT:
            target->StartObjectL();
            break;

        case ES_PTAG_OBJECT_BOOLEAN:
            target->StartObjectBooleanL();
            break;

        case ES_PTAG_OBJECT_NUMBER:
            target->StartObjectNumberL();
            break;

        case ES_PTAG_OBJECT_STRING:
            target->StartObjectStringL();
            break;

        case ES_PTAG_OBJECT_ARRAY:
            target->StartObjectArrayL();
            break;

        case ES_PTAG_OBJECT_REGEXP:
            target->StartObjectRegExpL();
            break;

        case ES_PTAG_OBJECT_DATE:
            target->StartObjectDateL();
            break;

        case ES_PTAG_OBJECT_AGAIN:
            target->PushObjectL(*ptr++);
            break;

        case ES_PTAG_PROPERTY:
            target->AddPropertyL(*ptr++);
            break;

        case ES_PTAG_HOST_OBJECT:
            target->HostObjectL(value->items[*ptr++]);
            break;

        case ES_PTAG_OBJECT_ARRAYBUFFER:
            {
                unsigned length = *ptr++;
                target->StartObjectArrayBufferL(reinterpret_cast<const unsigned char *>(ptr), length);
                ptr += (length + 3) / 4;
            }
            break;

        case ES_PTAG_OBJECT_TYPEDARRAY:
            {
                unsigned kind = *ptr++;
                unsigned offset = *ptr++;
                unsigned size = *ptr++;
                target->StartObjectTypedArrayL(kind, offset, size);
            }
            break;

        default:
            target->PushNumberL(op_implode_double(tag, *ptr++));
            break;
        }
    }
}

#endif // ES_PERSISTENT_SUPPORT
