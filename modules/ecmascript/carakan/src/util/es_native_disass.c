/* -*- Mode: c; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*- */

#ifdef NATIVE_DISASSEMBLER_SUPPORT
#include "modules/ecmascript/carakan/src/util/es_native_disass.h"

#include <dis-asm.h>
#include <stdio.h>

int
ES_NativeDisassemble1(void *ptr, struct disassemble_info *info)
{
#ifdef ARCHITECTURE_IA32
    return print_insn_i386_att((bfd_vma)(long)(ptr), info);
#endif // ARCHITECTURE_IA32

#ifdef ARCHITECTURE_ARM
#  ifdef OPERA_BIG_ENDIAN
    return print_insn_big_arm((bfd_vma)(long)(ptr), info);
#  else // OPERA_BIG_ENDIAN
    return print_insn_little_arm((bfd_vma)(long)(ptr), info);
#  endif // OPERA_BIG_ENDIAN
#endif // ARCHITECTURE_ARM

#ifdef ARCHITECTURE_MIPS
#  ifdef OPERA_BIG_ENDIAN
    return print_insn_big_mips((bfd_vma)(long)(ptr), info);
#  else
    return print_insn_little_mips((bfd_vma)(long)(ptr), info);
#  endif
#endif
}

void
ES_NativeDisassemble(void *memory, unsigned length, enum ES_NativeDisassembleMode mode, struct ES_NativeDisassembleData *data)
{
    extern FILE *g_native_disassembler_file;

    if (g_native_disassembler_file)
    {
        struct disassemble_info disass_info;

        INIT_DISASSEMBLE_INFO(disass_info, g_native_disassembler_file, fprintf);

        disass_info.buffer = memory;
        disass_info.buffer_vma = (bfd_vma)(long)(memory);
        disass_info.buffer_length = length;

#ifdef ARCHITECTURE_IA32
        disass_info.arch = bfd_arch_i386;
#  ifdef ARCHITECTURE_AMD64
        disass_info.mach = bfd_mach_x86_64;
#  else // ARCHITECTURE_AMD64
        disass_info.mach = bfd_mach_i386_i386;
#  endif // ARCHITECTURE_AMD64
#endif // ARCHITECTURE_IA32

#ifdef ARCHITECTURE_ARM
        disass_info.arch = bfd_arch_arm;
        disass_info.mach = bfd_mach_arm_unknown;
#endif // ARCHITECTURE_ARM

#ifdef ARCHITECTURE_MIPS
        disass_info.arch = bfd_arch_mips;
        disass_info.mach = bfd_mach_mipsisa32;
#endif // ARCHITECTURE_ARM

        struct ES_NativeDisassembleRange *range = data->ranges;
        struct ES_NativeDisassembleAnnotation *annotation = data->annotations;

        while (range)
        {
            unsigned char *ptr = (unsigned char *) memory + range->start;
            unsigned char *end = (unsigned char *) memory + range->end;
            unsigned offset = range->start;

            if (range->type == ES_NDRT_CODE)
            {
                if (mode == ES_NDM_DEBUGGING)
                {
                    fputs("--------------------------------------------------------------------------------\n", g_native_disassembler_file);
                    fprintf(g_native_disassembler_file, "Code: %u bytes\n", (unsigned) (end - ptr));
                    fputs("--------------------------------------------------------------------------------\n", g_native_disassembler_file);
                }

                while (ptr < end)
                {
                    while (annotation && annotation->offset <= offset)
                    {
                        fputs(annotation->value, g_native_disassembler_file);
                        annotation = annotation->next;
                    }

                    switch (mode)
                    {
                    case ES_NDM_DEBUGGING:
                        generic_print_address((bfd_vma)(long)(ptr), &disass_info);
#if defined ARCHITECTURE_ARM && defined OUTPUT_INSTRUCTION_WORDS
                        fprintf(g_native_disassembler_file, "    [0x%08x]    ", *(unsigned *)(ptr));
#else // ARCHITECTURE_ARM
                        fputs("\t", g_native_disassembler_file);
#endif // ARCHITECTURE_ARM
                        break;

                    case ES_NDM_TRAMPOLINE:
                        {
#ifdef ARCHITECTURE_ARM
                            unsigned *uptr = (unsigned *)(ptr);
                            fprintf(g_native_disassembler_file, "    0x%08x%s    // ", *uptr, ptr + sizeof(unsigned) != end || range->next ? "," : " ");
#else // ARCHITECTURE_ARM
                            fputs("    // ", g_native_disassembler_file);
#endif // ARCHITECTURE_ARM
                        }
                        break;
                    }

                    int result = ES_NativeDisassemble1(ptr, &disass_info);

#ifdef ARCHITECTURE_IA32
                    switch (mode)
                    {
                    case ES_NDM_DEBUGGING:
                        fputs("\n", g_native_disassembler_file);
                        break;

                    case ES_NDM_TRAMPOLINE:
                        fputs("\n    ", g_native_disassembler_file);

                        unsigned i, l;

                        for (i = 0, l = result <= 0 ? end - ptr : result; i < l; ++i)
                            fprintf(g_native_disassembler_file, "%s0x%02x,", i == 0 ? "" : " ", ptr[i]);

                        fputs("\n", g_native_disassembler_file);
                    }
#else // ARCHITECTURE_IA32
                    fputs("\n", g_native_disassembler_file);
#endif // ARCHITECTURE_IA32

                    if (result <= 0)
                        break;

                    ptr += result;
                    offset += result;

#if 0
                    unsigned char *rest = ptr;

                    while (rest != end && *rest == 0) ++rest;

                    if (rest == end)
                        break;
#endif // 0
                }
            }

            if (range->type == ES_NDRT_DATA && mode == ES_NDM_DEBUGGING)
            {
                fputs("--------------------------------------------------------------------------------\n", g_native_disassembler_file);
                fprintf(g_native_disassembler_file, "Data: %u bytes\n", (unsigned) (end - ptr));
                fputs("--------------------------------------------------------------------------------\n", g_native_disassembler_file);

                while (ptr < end)
                {
                    generic_print_address((bfd_vma)(long)(ptr), &disass_info);
                    fprintf(g_native_disassembler_file, "    [0x%08x]\n", *(unsigned *)(ptr));
                    ptr += sizeof(unsigned);
                }
            }

            range = range->next;
        }

        if (mode == ES_NDM_DEBUGGING)
            fputs("--------------------------------------------------------------------------------\n", g_native_disassembler_file);

        fflush(g_native_disassembler_file);
    }
}

#endif // NATIVE_DISASSEMBLER_SUPPORT
