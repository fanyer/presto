/* -*- Mode: c; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*- */

#ifdef NATIVE_DISASSEMBLER_SUPPORT
#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

enum ES_NativeDisassembleMode
{
    ES_NDM_DEBUGGING,
    ES_NDM_TRAMPOLINE
};

enum ES_NativeDisassembleRangeType
{
    ES_NDRT_CODE,
    ES_NDRT_DATA
};

struct ES_NativeDisassembleRange
{
    enum ES_NativeDisassembleRangeType type;

    unsigned start;
    unsigned end;

    struct ES_NativeDisassembleRange *next;
};

struct ES_NativeDisassembleAnnotation
{
    unsigned offset;
    char *value;

    struct ES_NativeDisassembleAnnotation *next;
};

struct ES_NativeDisassembleData
{
    struct ES_NativeDisassembleRange *ranges;
    struct ES_NativeDisassembleAnnotation *annotations;
};

extern void
ES_NativeDisassemble(void *memory, unsigned length, enum ES_NativeDisassembleMode mode, struct ES_NativeDisassembleData *data);

#ifdef __cplusplus
}
#endif // __cplusplus
#endif // NATIVE_DISASSEMBLER_SUPPORT
