/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*- */

#include "core/pch.h"

#ifdef ES_OPPSEUDOTHREAD

#include "modules/ecmascript/carakan/src/es_pch.h"

#include "modules/ecmascript/oppseudothread/oppseudothread.h"
#include "modules/memory/src/memory_executable.h"

#if !defined OPPSEUDOTHREAD_STACK_SWAPPING && !defined OPPSEUDOTHREAD_THREADED
# warning "////////// OpPseudoThread is disabled! //////////"
#endif // !OPPSEUDOTHREAD_STACK_SWAPPING && !OPPSEUDOTHREAD_THREADED

#ifndef OPPSEUDOTHREAD_THREADED
#ifndef OPPSEUDOTHREAD_STACK_SWAPPING

BOOL
OpPseudoThread_StartImpl(OpPseudoThread *thread, OpPseudoThread::Callback callback, unsigned char **original_stack, unsigned char *thread_stack)
{
    TRAPD(r, callback(thread));
    return OpStatus::IsSuccess(r);
}

BOOL
OpPseudoThread_ResumeImpl(OpPseudoThread *thread, unsigned char **original_stack, unsigned char *thread_stack)
{
    return TRUE;
}

BOOL
OpPseudoThread_YieldImpl(OpPseudoThread *thread, unsigned char **original_stack, unsigned char *thread_stack)
{
    LEAVE(OpStatus::ERR_NO_MEMORY);
    return FALSE;
}

void
OpPseudoThread_SuspendImpl(OpPseudoThread *thread, OpPseudoThread::Callback callback, unsigned char *original_stack)
{
    callback(thread);
}
void
OpPseudoThread_ReserveImpl(OpPseudoThread *thread, OpPseudoThread::Callback callback, unsigned char *new_stack_ptr)
{
}

unsigned
OpPseudoThread_StackSpaceRemainingImpl(void *stack_bottom)
{
    return INT_MAX;
}

#else // OPPSEUDOTHREAD_STACK_SWAPPING

#ifdef ARCHITECTURE_IA32

typedef unsigned char MachineInstruction;

#ifdef ARCHITECTURE_AMD64_UNIX

const MachineInstruction cv_StartImpl[] =
{
    0x55,                               // push %rbp
    0x53,                               // push %rbx
    0x41, 0x54,                         // push %r12
    0x41, 0x55,                         // push %r13
    0x41, 0x56,                         // push %r14
    0x41, 0x57,                         // push %r15
    0x48, 0x89, 0x22,                   // mov %rsp, (%rdx)
    0x48, 0x8b, 0xe1,                   // mov %rcx, %rsp
    0x48, 0x83, 0xe4, 0xf0,             // and $0xf0, %rsp
    0x48, 0x83, 0xec, 0x08,             // sub $0x8, %rsp
    0x52,                               // push %rdx
    0xff, 0xd6,                         // call *%rsi
    0x5a,                               // pop %rdx
    0x48, 0x8b, 0x22,                   // mov (%rdx), %rsp
    0x41, 0x5f,                         // pop %r15
    0x41, 0x5e,                         // pop %r14
    0x41, 0x5d,                         // pop %r13
    0x41, 0x5c,                         // pop %r12
    0x5b,                               // pop %rbx
    0x5d,                               // pop %rbp
    0xb8, 0x01, 0x00, 0x00, 0x00,       // mov $0x1, %rax
    0xc3                                // ret
};
const MachineInstruction cv_ResumeImpl[] =
{
    0x55,                               // push %rbp
    0x53,                               // push %rbx
    0x41, 0x54,                         // push %r12
    0x41, 0x55,                         // push %r13
    0x41, 0x56,                         // push %r14
    0x41, 0x57,                         // push %r15
    0x48, 0x89, 0x27,                   // mov %rsp, (%rdi)
    0x48, 0x8b, 0xe6,                   // mov %rsi, %rsp
    0x41, 0x5f,                         // pop %r15
    0x41, 0x5e,                         // pop %r14
    0x41, 0x5d,                         // pop %r13
    0x41, 0x5c,                         // pop %r12
    0x5b,                               // pop %rbx
    0x5d,                               // pop %rbp
    0xc3                                // ret
};
const MachineInstruction cv_YieldImpl[] =
{
    0x55,                               // push %rbp
    0x53,                               // push %rbx
    0x41, 0x54,                         // push %r12
    0x41, 0x55,                         // push %r13
    0x41, 0x56,                         // push %r14
    0x41, 0x57,                         // push %r15
    0x48, 0x89, 0x27,                   // mov %rsp, (%rdi)
    0x48, 0x8b, 0xe6,                   // mov %rsi, %rsp
    0x41, 0x5f,                         // pop %r15
    0x41, 0x5e,                         // pop %r14
    0x41, 0x5d,                         // pop %r13
    0x41, 0x5c,                         // pop %r12
    0x5b,                               // pop %rbx
    0x5d,                               // pop %rbp
    0x48, 0x31, 0xc0,                   // xor %rax, %rax
    0xc3                                // ret
};
const MachineInstruction cv_SuspendImpl[] =
{
    0x53,                               // push %rbx
    0x48, 0x8b, 0xdc,                   // mov %rsp, %rbx
    0x48, 0x8b, 0xe2,                   // mov %rdx, %rsp
    0x48, 0x83, 0xe4, 0xf0,             // and $fffffffffffffff0, %rsp
    0xff, 0xd6,                         // call *%rsi
    0x48, 0x8b, 0xe3,                   // mov %rbx, %rsp
    0x5b,                               // pop %rbx
    0xc3                                // ret
};
const MachineInstruction cv_ReserveImpl[] =
{
    0x53,                               // push %rbx
    0x48, 0x8b, 0xdc,                   // mov %rsp, %rbx
    0x48, 0x8b, 0xe2,                   // mov %rdx, %rsp
    0xff, 0xd6,                         // call *%rsi
    0x48, 0x8b, 0xe3,                   // mov %rbx, %rsp
    0x5b,                               // pop %rbx
    0xc3                                // ret
};
const MachineInstruction cv_StackSpaceRemainingImpl[] =
{
    0x48, 0x8b, 0xc4,                   // mov %rsp, %rax
    0x48, 0x2b, 0xc7,                   // sub %rdi, %rax
    0xc3                                // ret
};

#elif defined(ARCHITECTURE_AMD64_WINDOWS)

/* If you don't want to pay for saving XMM6-XMM15 across stack
   switches, and is certain that no harm is done, disable. */
#define ARCHITECTURE_AMD64_WINDOWS_SAVE_XMM_REGS

const MachineInstruction cv_StartImpl[] =
{
    0x53,                                     // push %rbx
    0x55,                                     // push %rbp
    0x56,                                     // push %rsi
    0x57,                                     // push %rdi
    0x41, 0x54,                               // push %r12
    0x41, 0x55,                               // push %r13
    0x41, 0x56,                               // push %r14
    0x41, 0x57,                               // push %r15
#ifdef ARCHITECTURE_AMD64_WINDOWS_SAVE_XMM_REGS
    0x48, 0x81, 0xec, 0xa8, 0x00, 0x00, 0x00, // sub $0xa8,%rsp
    0x66, 0x0f, 0x29, 0x34, 0x24,             // movapd %xmm6,(%rsp)
    0x66, 0x0f, 0x29, 0x7c, 0x24, 0x10,       // movapd %xmm7,0x10(%rsp)
    0x66, 0x44, 0x0f, 0x29, 0x44, 0x24, 0x20, // movapd %xmm8,0x20(%rsp)
    0x66, 0x44, 0x0f, 0x29, 0x4c, 0x24, 0x30, // movapd %xmm9,0x30(%rsp)
    0x66, 0x44, 0x0f, 0x29, 0x54, 0x24, 0x40, // movapd %xmm10,0x40(%rsp)
    0x66, 0x44, 0x0f, 0x29, 0x5c, 0x24, 0x50, // movapd %xmm11,0x50(%rsp)
    0x66, 0x44, 0x0f, 0x29, 0x64, 0x24, 0x60, // movapd %xmm12,0x60(%rsp)
    0x66, 0x44, 0x0f, 0x29, 0x6c, 0x24, 0x70, // movapd %xmm13,0x70(%rsp)
    0x66, 0x44, 0x0f, 0x29, 0xb4, 0x24,       // movapd %xmm14,0x80(%rsp)
    0x80, 0x00, 0x00, 0x00,
    0x66, 0x44, 0x0f, 0x29, 0xbc, 0x24,       // movapd %xmm15,0x90(%rsp)
    0x90, 0x00, 0x00, 0x00,
#endif // ARCHITECTURE_AMD64_WINDOWS_SAVE_XMM_REGS
    0x49, 0x89, 0x20,                         // mov %rsp, (%r8)
    0x4c, 0x89, 0xcc,                         // mov %r9, %rsp
    0x48, 0x83, 0xe4, 0xf0,                   // and $0xf0, %rsp
    0x48, 0x83, 0xec, 0x08,                   // sub $0x8, %rsp
    0x41, 0x50,                               // push %r8
    0x48, 0x83, 0xec, 0x20,                   // sub $0x20, %rsp
    0xff, 0xd2,                               // callq *%rdx
    0x48, 0x83, 0xc4, 0x20,                   // add $0x20, %rsp
    0x41, 0x58,                               // pop %r8
    0x49, 0x8b, 0x20,                         // mov (%r8), %rsp
#ifdef ARCHITECTURE_AMD64_WINDOWS_SAVE_XMM_REGS
    0x66, 0x0f, 0x28, 0x34, 0x24,             // movapd (%rsp),%xmm6
    0x66, 0x0f, 0x28, 0x7c, 0x24, 0x10,       // movapd 0x10(%rsp),%xmm7
    0x66, 0x44, 0x0f, 0x28, 0x44, 0x24, 0x20, // movapd 0x20(%rsp),%xmm8
    0x66, 0x44, 0x0f, 0x28, 0x4c, 0x24, 0x30, // movapd 0x30(%rsp),%xmm9
    0x66, 0x44, 0x0f, 0x28, 0x54, 0x24, 0x40, // movapd 0x40(%rsp),%xmm10
    0x66, 0x44, 0x0f, 0x28, 0x5c, 0x24, 0x50, // movapd 0x50(%rsp),%xmm11
    0x66, 0x44, 0x0f, 0x28, 0x64, 0x24, 0x60, // movapd 0x60(%rsp),%xmm12
    0x66, 0x44, 0x0f, 0x28, 0x6c, 0x24, 0x70, // movapd 0x70(%rsp),%xmm13
    0x66, 0x44, 0x0f, 0x28, 0xb4, 0x24, 0x80, // movapd 0x80(%rsp),%xmm14
    0x00, 0x00, 0x00,
    0x66, 0x44, 0x0f, 0x28, 0xbc, 0x24, 0x90, // movapd 0x90(%rsp),%xmm15
    0x00, 0x00, 0x00,
    0x48, 0x81, 0xc4, 0xa8, 0x00, 0x00, 0x00, // add $0xa8,%rsp
#endif // ARCHITECTURE_AMD64_WINDOWS_SAVE_XMM_REGS
    0x41, 0x5f,                               // pop %r15
    0x41, 0x5e,                               // pop %r14
    0x41, 0x5d,                               // pop %r13
    0x41, 0x5c,                               // pop %r12
    0x5f,                                     // pop %rdi
    0x5e,                                     // pop %rsi
    0x5d,                                     // pop %rbp
    0x5b,                                     // pop %rbx
    0xb8, 0x01, 0x00, 0x00, 0x00,             // mov 0x1, %eax
    0xc3                                      // ret
};
const MachineInstruction cv_ResumeImpl[] =
{
    0x53,                                     // push %rbx
    0x55,                                     // push %rbp
    0x56,                                     // push %rsi
    0x57,                                     // push %rdi
    0x41, 0x54,                               // push %r12
    0x41, 0x55,                               // push %r13
    0x41, 0x56,                               // push %r14
    0x41, 0x57,                               // push %r15
#ifdef ARCHITECTURE_AMD64_WINDOWS_SAVE_XMM_REGS
    0x48, 0x81, 0xec, 0xa8, 0x00, 0x00, 0x00, // sub $0xa8,%rsp
    0x66, 0x0f, 0x29, 0x34, 0x24,             // movapd %xmm6,(%rsp)
    0x66, 0x0f, 0x29, 0x7c, 0x24, 0x10,       // movapd %xmm7,0x10(%rsp)
    0x66, 0x44, 0x0f, 0x29, 0x44, 0x24, 0x20, // movapd %xmm8,0x20(%rsp)
    0x66, 0x44, 0x0f, 0x29, 0x4c, 0x24, 0x30, // movapd %xmm9,0x30(%rsp)
    0x66, 0x44, 0x0f, 0x29, 0x54, 0x24, 0x40, // movapd %xmm10,0x40(%rsp)
    0x66, 0x44, 0x0f, 0x29, 0x5c, 0x24, 0x50, // movapd %xmm11,0x50(%rsp)
    0x66, 0x44, 0x0f, 0x29, 0x64, 0x24, 0x60, // movapd %xmm12,0x60(%rsp)
    0x66, 0x44, 0x0f, 0x29, 0x6c, 0x24, 0x70, // movapd %xmm13,0x70(%rsp)
    0x66, 0x44, 0x0f, 0x29, 0xb4, 0x24,       // movapd %xmm14,0x80(%rsp)
    0x80, 0x00, 0x00, 0x00,
    0x66, 0x44, 0x0f, 0x29, 0xbc, 0x24,       // movapd %xmm15,0x90(%rsp)
    0x90, 0x00, 0x00, 0x00,
#endif // ARCHITECTURE_AMD64_WINDOWS_SAVE_XMM_REGS
    0x48, 0x89, 0x21,                         // mov %rsp, (%rcx)
    0x48, 0x89, 0xd4,                         // mov %rdx, %rsp
#ifdef ARCHITECTURE_AMD64_WINDOWS_SAVE_XMM_REGS
    0x66, 0x0f, 0x28, 0x34, 0x24,             // movapd (%rsp),%xmm6
    0x66, 0x0f, 0x28, 0x7c, 0x24, 0x10,       // movapd 0x10(%rsp),%xmm7
    0x66, 0x44, 0x0f, 0x28, 0x44, 0x24, 0x20, // movapd 0x20(%rsp),%xmm8
    0x66, 0x44, 0x0f, 0x28, 0x4c, 0x24, 0x30, // movapd 0x30(%rsp),%xmm9
    0x66, 0x44, 0x0f, 0x28, 0x54, 0x24, 0x40, // movapd 0x40(%rsp),%xmm10
    0x66, 0x44, 0x0f, 0x28, 0x5c, 0x24, 0x50, // movapd 0x50(%rsp),%xmm11
    0x66, 0x44, 0x0f, 0x28, 0x64, 0x24, 0x60, // movapd 0x60(%rsp),%xmm12
    0x66, 0x44, 0x0f, 0x28, 0x6c, 0x24, 0x70, // movapd 0x70(%rsp),%xmm13
    0x66, 0x44, 0x0f, 0x28, 0xb4, 0x24, 0x80, // movapd 0x80(%rsp),%xmm14
    0x00, 0x00, 0x00,
    0x66, 0x44, 0x0f, 0x28, 0xbc, 0x24, 0x90, // movapd 0x90(%rsp),%xmm15
    0x00, 0x00, 0x00,
    0x48, 0x81, 0xc4, 0xa8, 0x00, 0x00, 0x00, // add $0xa8,%rsp
#endif // ARCHITECTURE_AMD64_WINDOWS_SAVE_XMM_REGS
    0x41, 0x5f,                               // pop %r15
    0x41, 0x5e,                               // pop %r14
    0x41, 0x5d,                               // pop %r13
    0x41, 0x5c,                               // pop %r12
    0x5f,                                     // pop %rdi
    0x5e,                                     // pop %rsi
    0x5d,                                     // pop %rbp
    0x5b,                                     // pop %rbx
    0xc3                                      // ret
};
const MachineInstruction cv_YieldImpl[] =
{
    0x53,                                     // push %rbx
    0x55,                                     // push %rbp
    0x56,                                     // push %rsi
    0x57,                                     // push %rdi
    0x41, 0x54,                               // push %r12
    0x41, 0x55,                               // push %r13
    0x41, 0x56,                               // push %r14
    0x41, 0x57,                               // push %r15
#ifdef ARCHITECTURE_AMD64_WINDOWS_SAVE_XMM_REGS
    0x48, 0x81, 0xec, 0xa8, 0x00, 0x00, 0x00, // sub $0xa8,%rsp
    0x66, 0x0f, 0x29, 0x34, 0x24,             // movapd %xmm6,(%rsp)
    0x66, 0x0f, 0x29, 0x7c, 0x24, 0x10,       // movapd %xmm7,0x10(%rsp)
    0x66, 0x44, 0x0f, 0x29, 0x44, 0x24, 0x20, // movapd %xmm8,0x20(%rsp)
    0x66, 0x44, 0x0f, 0x29, 0x4c, 0x24, 0x30, // movapd %xmm9,0x30(%rsp)
    0x66, 0x44, 0x0f, 0x29, 0x54, 0x24, 0x40, // movapd %xmm10,0x40(%rsp)
    0x66, 0x44, 0x0f, 0x29, 0x5c, 0x24, 0x50, // movapd %xmm11,0x50(%rsp)
    0x66, 0x44, 0x0f, 0x29, 0x64, 0x24, 0x60, // movapd %xmm12,0x60(%rsp)
    0x66, 0x44, 0x0f, 0x29, 0x6c, 0x24, 0x70, // movapd %xmm13,0x70(%rsp)
    0x66, 0x44, 0x0f, 0x29, 0xb4, 0x24,       // movapd %xmm14,0x80(%rsp)
    0x80, 0x00, 0x00, 0x00,
    0x66, 0x44, 0x0f, 0x29, 0xbc, 0x24,       // movapd %xmm15,0x90(%rsp)
    0x90, 0x00, 0x00, 0x00,
#endif // ARCHITECTURE_AMD64_WINDOWS_SAVE_XMM_REGS
    0x48, 0x89, 0x21,                         // mov %rsp, (%rcx)
    0x48, 0x89, 0xd4,                         // mov %rdx, %rsp
#ifdef ARCHITECTURE_AMD64_WINDOWS_SAVE_XMM_REGS
    0x66, 0x0f, 0x28, 0x34, 0x24,             // movapd (%rsp),%xmm6
    0x66, 0x0f, 0x28, 0x7c, 0x24, 0x10,       // movapd 0x10(%rsp),%xmm7
    0x66, 0x44, 0x0f, 0x28, 0x44, 0x24, 0x20, // movapd 0x20(%rsp),%xmm8
    0x66, 0x44, 0x0f, 0x28, 0x4c, 0x24, 0x30, // movapd 0x30(%rsp),%xmm9
    0x66, 0x44, 0x0f, 0x28, 0x54, 0x24, 0x40, // movapd 0x40(%rsp),%xmm10
    0x66, 0x44, 0x0f, 0x28, 0x5c, 0x24, 0x50, // movapd 0x50(%rsp),%xmm11
    0x66, 0x44, 0x0f, 0x28, 0x64, 0x24, 0x60, // movapd 0x60(%rsp),%xmm12
    0x66, 0x44, 0x0f, 0x28, 0x6c, 0x24, 0x70, // movapd 0x70(%rsp),%xmm13
    0x66, 0x44, 0x0f, 0x28, 0xb4, 0x24, 0x80, // movapd 0x80(%rsp),%xmm14
    0x00, 0x00, 0x00,
    0x66, 0x44, 0x0f, 0x28, 0xbc, 0x24, 0x90, // movapd 0x90(%rsp),%xmm15
    0x00, 0x00, 0x00,
    0x48, 0x81, 0xc4, 0xa8, 0x00, 0x00, 0x00, // add $0xa8,%rsp
#endif // ARCHITECTURE_AMD64_WINDOWS_SAVE_XMM_REGS
    0x41, 0x5f,                               // pop %r15
    0x41, 0x5e,                               // pop %r14
    0x41, 0x5d,                               // pop %r13
    0x41, 0x5c,                               // pop %r12
    0x5f,                                     // pop %rdi
    0x5e,                                     // pop %rsi
    0x5d,                                     // pop %rbp
    0x5b,                                     // pop %rbx
    0x48, 0x31, 0xc0,                         // xor %rax, %rax
    0xc3                                      // ret
};
const MachineInstruction cv_SuspendImpl[] =
{
    0x53,                                     // push %rbx
    0x48, 0x89, 0xe3,                         // mov %rsp, %rbx
    0x4c, 0x89, 0xc4,                         // mov %r8, %rsp
    0x48, 0x83, 0xe4, 0xf0,                   // and $0xfffffffffffffff0, %rsp
    0x48, 0x83, 0xec, 0x20,                   // sub 0x20, %rsp
    0xff, 0xd2,                               // call *%rdx
    0x48, 0x83, 0xc4, 0x20,                   // add $0x20, %rsp
    0x48, 0x89, 0xdc,                         // mov %rbx, %rsp
    0x5b,                                     // pop %rbx
    0xc3                                      // ret
};
const MachineInstruction cv_ReserveImpl[] =
{
    0x53,                                     // push %rbx
    0x48, 0x89, 0xe3,                         // mov %rsp, %rbx
    0x4c, 0x89, 0xc4,                         // mov %r8, %rsp
    0x48, 0x83, 0xec, 0x20,                   // sub $0x20, %rsp
    0xff, 0xd2,                               // call *%rdx
    0x48, 0x83, 0xc4, 0x20,                   // add $0x20, %rsp
    0x48, 0x89, 0xdc,                         // mov %rbx, %rsp
    0x5b,                                     // pop %rbx
    0xc3                                      // ret
};
const MachineInstruction cv_StackSpaceRemainingImpl[] =
{
    0x48, 0x89, 0xe0,                         // mov %rsp, %rax
    0x48, 0x29, 0xc8,                         // sub %rcx, %rax
    0xc3                                      // ret
};

#else // ARCHITECTURE_AMD64_UNIX || ARCHITECTURE_AMD64_WINDOWS

const MachineInstruction cv_StartImpl[] =
{
    0x55,                               // push %ebp
    0x8b, 0xec,                         // mov %esp, %ebp
    0x53,                               // push %ebx
    0x56,                               // push %esi
    0x57,                               // push %edi
    0x8b, 0x5d, 0x10,                   // mov 0x10(%ebp), %ebx
    0x89, 0x23,                         // mov %esp, (%ebx)
    0x8b, 0x65, 0x14,                   // mov 0x14(%ebp), %esp
    0x8b, 0x45, 0x08,                   // mov 0x8(%ebp), %eax
    0x83, 0xec, 0x08,                   // sub $0x8, %esp
    0x83, 0xe4, 0xf0,                   // and $0xfffffff0, %esp
    0x89, 0x5c, 0x24, 0x4,              // mov %ebx, 0x4(%esp)
    0x89, 0x04, 0x24,                   // mov %eax, (%esp)
    0xff, 0x55, 0x0c,                   // call *0xc(%ebp)
    0x8b, 0x5c, 0x24, 0x4,              // mov 0x4(%esp), %ebx
    0x8b, 0x23,                         // mov (%ebx), %esp
    0x5f,                               // pop %edi
    0x5e,                               // pop %esi
    0x5b,                               // pop %ebx
    0x5d,                               // pop %ebp
    0xb8, 0x01, 0x00, 0x00, 0x00,       // mov $0x1, %eax
    0xc3                                // ret
};
const MachineInstruction cv_ResumeImpl[] =
{
    0x55,                               // push %ebp
    0x8b, 0xec,                         // mov %esp, %ebp
    0x53,                               // push %ebx
    0x56,                               // push %esi
    0x57,                               // push %edi
    0x8b, 0x5d, 0x08,                   // mov 0x8(%ebp), %ebx
    0x89, 0x23,                         // mov %esp, (%ebx)
    0x8b, 0x65, 0x0c,                   // mov 0xc(%ebp), %esp
    0x5f,                               // pop %edi
    0x5e,                               // pop %esi
    0x5b,                               // pop %ebx
    0x5d,                               // pop %ebp
    0xc3                                // ret
};
const MachineInstruction cv_YieldImpl[] =
{
    0x55,                               // push %ebp
    0x8b, 0xec,                         // mov %esp, %ebp
    0x53,                               // push %ebx
    0x56,                               // push %esi
    0x57,                               // push %edi
    0x8b, 0x5d, 0x08,                   // mov 0x8(%ebp), %ebx
    0x89, 0x23,                         // mov %esp, (%ebx)
    0x8b, 0x65, 0x0c,                   // mov 0xc(%ebp), %esp
    0x5f,                               // pop %edi
    0x5e,                               // pop %esi
    0x5b,                               // pop %ebx
    0x5d,                               // pop %ebp
    0x31, 0xc0,                         // xor %eax, %eax
    0xc3                                // ret
};
const MachineInstruction cv_SuspendImpl[] =
{
    0x55,                               // push %ebp
    0x8b, 0xec,                         // mov %esp, %ebp
    0x53,                               // push %ebx
    0x8b, 0xdc,                         // mov %esp, %ebx
    0x8b, 0x65, 0x10,                   // mov 0x10(%ebp), %esp
    0x8b, 0x45, 0x08,                   // mov 0x8(%ebp), %eax
    0x83, 0xec, 0x04,                   // sub $0x4, %esp
    0x83, 0xe4, 0xf0,                   // and $0xfffffff0, %esp
    0x89, 0x04, 0x24,                   // mov %eax, (%esp)
    0xff, 0x55, 0x0c,                   // call *0xc(%ebp)
    0x8b, 0xe3,                         // mov %ebx, %esp
    0x5b,                               // pop %ebx
    0x5d,                               // pop %ebp
    0xc3                                // ret
};
const MachineInstruction cv_ReserveImpl[] =
{
    0x55,                               // push %ebp
    0x8b, 0xec,                         // mov %esp, %ebp
    0x53,                               // push %ebx
    0x8b, 0xdc,                         // mov %esp, %ebx
    0x8b, 0x65, 0x10,                   // mov 0x10(%ebp), %esp
    0x8b, 0x45, 0x08,                   // mov 0x8(%ebp), %eax
    0x83, 0xec, 0x04,                   // sub $0x4, %esp
    0x83, 0xe4, 0xf0,                   // and $0xfffffff0, %esp
    0x89, 0x04, 0x24,                   // mov %eax, (%esp)
    0xff, 0x55, 0x0c,                   // call *0xc(%ebp)
    0x8b, 0xe3,                         // mov %ebx, %esp
    0x5b,                               // pop %ebx
    0x5d,                               // pop %ebp
    0xc3                                // ret
};
const MachineInstruction cv_StackSpaceRemainingImpl[] =
{
    0x8b, 0xc4,                         // mov %esp, %eax
    0x2b, 0x44, 0x24, 0x04,             // sub 0x4(%esp), %eax
    0xc3                                // ret
};

#endif // ARCHITECTURE_AMD64_UNIX || ARCHITECTURE_AMD64_WINDOWS
#endif // ARCHITECTURE_IA32

#ifdef ARCHITECTURE_ARM

typedef unsigned MachineInstruction;

const MachineInstruction cv_StartImpl[] =
{
    0xe92d5ff0, // push {r4-r12,lr}
    0xe1a04002, // mov r4, r2
    0xe482d000, // str sp, [r2]
    0xe1a0d003, // mov sp, r3
    0xe12fff31, // blx r1
    0xe494d000, // ldr sp, [r4]
    0xe3a00001, // mov r0, #1
    0xe8bd9ff0  // pop {r4-r12,pc}
};
const MachineInstruction cv_ResumeImpl[] =
{
    0xe92d5ff0, // push {r4-r12,lr}
    0xe480d000, // str sp, [r0]
    0xe1a0d001, // mov sp, r1
    0xe8bd9ff0  // pop {r4-r12,pc}
};
const MachineInstruction cv_YieldImpl[] =
{
    0xe92d5ff0, // push {r4-r12,lr}
    0xe480d000, // str sp, [r0]
    0xe1a0d001, // mov sp, r1
    0xe3a00000, // mov r0, 0
    0xe8bd9ff0  // pop {r4-r12,pc}
};
const MachineInstruction cv_SuspendImpl[] =
{
    0xe92d5ff0, // push {r4-r12,lr}
    0xe1a0400d, // mov r4, sp
    0xe1a0d002, // mov sp, r2
    0xe12fff31, // blx r1
    0xe1a0d004, // mov sp, r4
    0xe8bd9ff0  // pop {r4-r12,pc}
};
const MachineInstruction cv_ReserveImpl[] =
{
    0xe92d5ff0, // push {r4-r12,lr}
    0xe1a0400d, // mov r4, sp
    0xe1a0d002, // mov sp, r2
    0xe12fff31, // blx r1
    0xe1a0d004, // mov sp, r4
    0xe8bd9ff0  // pop {r4-r12,pc}
};
const MachineInstruction cv_StackSpaceRemainingImpl[] =
{
    0xe04d0000, // sub r0, sp, r0
    0xe12fff1e  // bx lr
};

#endif // ARCHITECTURE_ARM

#ifdef ARCHITECTURE_THUMB

typedef unsigned short MachineInstruction;

const MachineInstruction cv_StartImpl[] =
{
    0xb5f0,
    0x4644,
    0x464d,
    0x4656,
    0x465f,
    0xb4f0,
    0x4664,
    0xb410,
    0x4614,
    0x466d,
    0x6025,
    0x469d,
    0x4788,
    0x6825,
    0x46ad,
    0xba10,
    0x46a4,
    0xbaf0,
    0x46bb,
    0x46b2,
    0x46a9,
    0x46a0,
    0x2001,
    0xbbf0
};
const MachineInstruction cv_ResumeImpl[] =
{
    0xb5f0,
    0x4644,
    0x464d,
    0x4656,
    0x465f,
    0xb4f0,
    0x4664,
    0xb410,
    0x466b,
    0x600b,
    0x4695,
    0xba10,
    0x46a4,
    0xbaf0,
    0x46bb,
    0x46b2,
    0x46a9,
    0x46a0,
    0xbbf0
};
const MachineInstruction cv_YieldImpl[] =
{
    0xb5f0,
    0x4644,
    0x464d,
    0x4656,
    0x465f,
    0xb4f0,
    0x4664,
    0xb410,
    0x466b,
    0x600b,
    0x4695,
    0xba10,
    0x46a4,
    0xbaf0,
    0x46bb,
    0x46b2,
    0x46a9,
    0x46a0,
    0x2000,
    0xbbf0
};
const MachineInstruction cv_SuspendImpl[] =
{
    0xb510,
    0x466c,
    0x4695,
    0x4788,
    0x46a5,
    0xbb10
};
const MachineInstruction cv_ReserveImpl[] =
{
    0xb510,
    0x466c,
    0x4695,
    0x4788,
    0x46a5,
    0xbb10
};
const MachineInstruction cv_StackSpaceRemainingImpl[] =
{
    0x4669,
    0x1a08,
    0x4770
};

#endif // ARCHITECTURE_THUMB

#ifdef ARCHITECTURE_SUPERH

typedef unsigned char MachineInstruction;

const MachineInstruction cv_StartImpl[] =
{
    0x22, 0x4f, // sts.l pr,@-r15
    0x86, 0x2f, // mov.l r8,@-r15
    0x96, 0x2f, // mov.l r9,@-r15
    0xa6, 0x2f, // mov.l r10,@-r15
    0xb6, 0x2f, // mov.l r11,@-r15
    0xc6, 0x2f, // mov.l r12,@-r15
    0xd6, 0x2f, // mov.l r13,@-r15
    0xe6, 0x2f, // mov.l r14,@-r15
    0x63, 0x68, // mov r6,r8
    0xf2, 0x26, // mov.l r15,@r6
    0x0b, 0x45, // jsr @r5
    0x73, 0x6f, // mov r7,r15
    0x82, 0x6f, // mov.l @r8,r15
    0xf6, 0x6e, // mov.l @r15+,r14
    0xf6, 0x6d, // mov.l @r15+,r13
    0xf6, 0x6c, // mov.l @r15+,r12
    0xf6, 0x6b, // mov.l @r15+,r11
    0xf6, 0x6a, // mov.l @r15+,r10
    0xf6, 0x69, // mov.l @r15+,r9
    0xf6, 0x68, // mov.l @r15+,r8
    0x26, 0x4f, // lds.l @r15+,pr
    0x0b, 0x00, // rts
    0x01, 0xe0  // mov #1,r0
};
const MachineInstruction cv_ResumeImpl[] =
{
    0x22, 0x4f, // sts.l pr,@-r15
    0x86, 0x2f, // mov.l r8,@-r15
    0x96, 0x2f, // mov.l r9,@-r15
    0xa6, 0x2f, // mov.l r10,@-r15
    0xb6, 0x2f, // mov.l r11,@-r15
    0xc6, 0x2f, // mov.l r12,@-r15
    0xd6, 0x2f, // mov.l r13,@-r15
    0xe6, 0x2f, // mov.l r14,@-r15
    0xf2, 0x24, // mov.l r15,@r4
    0x53, 0x6f, // mov r5,r15
    0xf6, 0x6e, // mov.l @r15+,r14
    0xf6, 0x6d, // mov.l @r15+,r13
    0xf6, 0x6c, // mov.l @r15+,r12
    0xf6, 0x6b, // mov.l @r15+,r11
    0xf6, 0x6a, // mov.l @r15+,r10
    0xf6, 0x69, // mov.l @r15+,r9
    0xf6, 0x68, // mov.l @r15+,r8
    0x26, 0x4f, // lds.l @r15+,pr
    0x0b, 0x00, // rts
    0x09, 0x00  // nop
};
const MachineInstruction cv_YieldImpl[] =
{
    0x22, 0x4f, // sts.l pr,@-r15
    0x86, 0x2f, // mov.l r8,@-r15
    0x96, 0x2f, // mov.l r9,@-r15
    0xa6, 0x2f, // mov.l r10,@-r15
    0xb6, 0x2f, // mov.l r11,@-r15
    0xc6, 0x2f, // mov.l r12,@-r15
    0xd6, 0x2f, // mov.l r13,@-r15
    0xe6, 0x2f, // mov.l r14,@-r15
    0xf2, 0x24, // mov.l r15,@r4
    0x53, 0x6f, // mov r5,r15
    0xf6, 0x6e, // mov.l @r15+,r14
    0xf6, 0x6d, // mov.l @r15+,r13
    0xf6, 0x6c, // mov.l @r15+,r12
    0xf6, 0x6b, // mov.l @r15+,r11
    0xf6, 0x6a, // mov.l @r15+,r10
    0xf6, 0x69, // mov.l @r15+,r9
    0xf6, 0x68, // mov.l @r15+,r8
    0x26, 0x4f, // lds.l @r15+,pr
    0x0b, 0x00, // rts
    0x00, 0xe0  // mov #0,r0
};
const MachineInstruction cv_SuspendImpl[] =
{
    0x86, 0x2f, // mov.l r8,@-r15
    0x22, 0x4f, // sts.l pr,@-r15
    0xf3, 0x68, // mov r15,r8
    0x0b, 0x45, // jsr @r5
    0x63, 0x6f, // mov r6,r15
    0x83, 0x6f, // mov r8,r15
    0x26, 0x4f, // lds.l @r15+,pr
    0x0b, 0x00, // rts
    0xf6, 0x68  // mov.l @r15+,r8
};
const MachineInstruction cv_ReserveImpl[] =
{
    0x86, 0x2f, // mov.l r8,@-r15
    0x22, 0x4f, // sts.l pr,@-r15
    0xf3, 0x68, // mov r15,r8
    0x0b, 0x45, // jsr @r5
    0x63, 0x6f, // mov r6,r15
    0x83, 0x6f, // mov r8,r15
    0x26, 0x4f, // lds.l @r15+,pr
    0x0b, 0x00, // rts
    0xf6, 0x68  // mov.l @r15+,r8
};
const MachineInstruction cv_StackSpaceRemainingImpl[] =
{
    0xf3, 0x60, // mov r15,r0
    0x0b, 0x00, // rts
    0x48, 0x30  // sub r4,r0
};

#endif // ARCHITECTURE_SUPERH

#ifdef ARCHITECTURE_MIPS

typedef unsigned MachineInstruction;

const MachineInstruction cv_StartImpl[] =
{
    0x27bdffc0, // addiu sp,sp,-64
    0xafbf003c, // sw ra,60(sp)
    0xafbc0038, // sw gp,56(sp)
    0xafbe0034, // sw fp,52(sp)
    0xafb00030, // sw s0,48(sp)
    0xafb1002c, // sw s1,44(sp)
    0xafb20028, // sw s2,40(sp)
    0xafb30024, // sw s3,36(sp)
    0xafb40020, // sw s4,32(sp)
    0xafb5001c, // sw s5,28(sp)
    0xafb60018, // sw s6,24(sp)
    0xafb70014, // sw s7,20(sp)
    0x00c08021, // move s0,a2
    0xacdd0000, // sw sp,0(a2)
    0x00a0c821, // move t9,a1
    0x24e7fff0, // addiu a3,a3,-16
    0x0320f809, // jalr t9
    0x00e0e821, // move sp,a3
    0x8e1d0000, // lw sp,0(s0)
    0x8fbf003c, // lw ra,60(sp)
    0x8fbc0038, // lw gp,56(sp)
    0x8fbe0034, // lw fp,52(sp)
    0x8fb00030, // lw s0,48(sp)
    0x8fb1002c, // lw s1,44(sp)
    0x8fb20028, // lw s2,40(sp)
    0x8fb30024, // lw s3,36(sp)
    0x8fb40020, // lw s4,32(sp)
    0x8fb5001c, // lw s5,28(sp)
    0x8fb60018, // lw s6,24(sp)
    0x8fb70014, // lw s7,20(sp)
    0x27bd0040, // addiu sp,sp,64
    0x03e00008, // jr ra
    0x24020001  // li v0,1
};
const MachineInstruction cv_ResumeImpl[] =
{
    0x27bdffc0, // addiu sp,sp,-64
    0xafbf003c, // sw ra,60(sp)
    0xafbc0038, // sw gp,56(sp)
    0xafbe0034, // sw fp,52(sp)
    0xafb00030, // sw s0,48(sp)
    0xafb1002c, // sw s1,44(sp)
    0xafb20028, // sw s2,40(sp)
    0xafb30024, // sw s3,36(sp)
    0xafb40020, // sw s4,32(sp)
    0xafb5001c, // sw s5,28(sp)
    0xafb60018, // sw s6,24(sp)
    0xafb70014, // sw s7,20(sp)
    0xac9d0000, // sw sp,0(a0)
    0x00a0e821, // move sp,a1
    0x8fbf003c, // lw ra,60(sp)
    0x8fbc0038, // lw gp,56(sp)
    0x8fbe0034, // lw fp,52(sp)
    0x8fb00030, // lw s0,48(sp)
    0x8fb1002c, // lw s1,44(sp)
    0x8fb20028, // lw s2,40(sp)
    0x8fb30024, // lw s3,36(sp)
    0x8fb40020, // lw s4,32(sp)
    0x8fb5001c, // lw s5,28(sp)
    0x8fb60018, // lw s6,24(sp)
    0x8fb70014, // lw s7,20(sp)
    0x03e00008, // jr ra
    0x27bd0040  // addiu sp,sp,64
};
const MachineInstruction cv_YieldImpl[] =
{
    0x27bdffc0, // addiu sp,sp,-64
    0xafbf003c, // sw ra,60(sp)
    0xafbc0038, // sw gp,56(sp)
    0xafbe0034, // sw fp,52(sp)
    0xafb00030, // sw s0,48(sp)
    0xafb1002c, // sw s1,44(sp)
    0xafb20028, // sw s2,40(sp)
    0xafb30024, // sw s3,36(sp)
    0xafb40020, // sw s4,32(sp)
    0xafb5001c, // sw s5,28(sp)
    0xafb60018, // sw s6,24(sp)
    0xafb70014, // sw s7,20(sp)
    0xac9d0000, // sw sp,0(a0)
    0x00a0e821, // move sp,a1
    0x8fbf003c, // lw ra,60(sp)
    0x8fbc0038, // lw gp,56(sp)
    0x8fbe0034, // lw fp,52(sp)
    0x8fb00030, // lw s0,48(sp)
    0x8fb1002c, // lw s1,44(sp)
    0x8fb20028, // lw s2,40(sp)
    0x8fb30024, // lw s3,36(sp)
    0x8fb40020, // lw s4,32(sp)
    0x8fb5001c, // lw s5,28(sp)
    0x8fb60018, // lw s6,24(sp)
    0x8fb70014, // lw s7,20(sp)
    0x27bd0040, // addiu sp,sp,64
    0x03e00008, // jr ra
    0x00001021  // move v0,zero
};
const MachineInstruction cv_SuspendImpl[] =
{
    0x27bdffe0, // addiu sp,sp,-32
    0xafbf0018, // sw ra,24(sp)
    0xafb00010, // sw s0,16(sp)
    0x03a08021, // move s0,sp
    0x00a0c821, // move t9,a1
    0x0320f809, // jalr t9
    0x24ddfff0, // addiu sp,a2,-16
    0x0200e821, // move sp,s0
    0x8fbf0018, // lw ra,24(sp)
    0x8fb00010, // lw s0,16(sp)
    0x03e00008, // jr ra
    0x27bd0020  // addiu sp,sp,32
};
const MachineInstruction cv_ReserveImpl[] =
{
    0x27bdffe0, // addiu sp,sp,-32
    0xafbf0018, // sw ra,24(sp)
    0xafb00010, // sw s0,16(sp)
    0x03a08021, // move s0,sp
    0x00a0c821, // move t9,a1
    0x0320f809, // jalr t9
    0x24ddfff0, // addiu sp,a2,-16
    0x0200e821, // move sp,s0
    0x8fbf0018, // lw ra,24(sp)
    0x8fb00010, // lw s0,16(sp)
    0x03e00008, // jr ra
    0x27bd0020  // addiu sp,sp,32
};
const MachineInstruction cv_StackSpaceRemainingImpl[] =
{
    0x03e00008, // jr ra
    0x03a41023  // sub v0,sp,a0
};

#endif // ARCHITECTURE_MIPS

#ifdef ARCHITECTURE_POWERPC

typedef unsigned MachineInstruction;

const unsigned cv_StartImpl[] =
{
    0x7c0802a6, // mflr    r0
    0x9421ffa8, // stwu    r1,-88(r1)
    0x90010060, // stw     r0,96(r1)
    0x7ce00026, // mfcr    r7
    0x90e1005c, // stw     r7,92(r1)
    0xbdc10008, // stmw    r14,8(r1)
    0x7cbf2b78, // mr      r31,r5
    0x7c270b78, // mr      r7,r1
    0x38e7ffc0, // addi    r7,r7,-64
    0x90ff0000, // stw     r7,0(r31)
    0x38c6ffc8, // addi    r6,r6,-56
    0x9426fff8, // stwu    r1,-8(r6)
    0x7cc13378, // mr      r1,r6
    0x7c8903a6, // mtctr   r4
    0x4e800421, // bctrl
    0x80ff0000, // lwz     r7,0(r31)
    0x38e70040, // addi    r7,r7,64
    0x7ce13b78, // mr      r1,r7
    0xb9c10008, // lmw     r14,8(r1)
    0x80e1005c, // lwz     r7,92(r1)
    0x7ceff120, // mtcr    r7
    0x80010060, // lwz     r0,96(r1)
    0x7c0803a6, // mtlr    r0
    0x38210058, // addi    r1,r1,88
    0x38600001, // li      r3,1
    0x4e800020  // blr
};
const unsigned cv_ResumeImpl[] =
{
    0x7c0802a6, // mflr    r0
    0x9421ffa8, // stwu    r1,-88(r1)
    0x90010060, // stw     r0,96(r1)
    0x7ce00026, // mfcr    r7
    0x90e1005c, // stw     r7,92(r1)
    0xbdc10008, // stmw    r14,8(r1)
    0x7c270b78, // mr      r7,r1
    0x38e7ffc0, // addi    r7,r7,-64
    0x90e30000, // stw     r7,0(r3)
    0x7c812378, // mr      r1,r4
    0xb9c10008, // lmw     r14,8(r1)
    0x80e1005c, // lwz     r7,92(r1)
    0x7ceff120, // mtcr    r7
    0x80010060, // lwz     r0,96(r1)
    0x7c0803a6, // mtlr    r0
    0x38210058, // addi    r1,r1,88
    0x4e800020  // blr
};
const unsigned cv_YieldImpl[] =
{
    0x7c0802a6, // mflr    r0
    0x9421ffa8, // stwu    r1,-88(r1)
    0x90010060, // stw     r0,96(r1)
    0x7ce00026, // mfcr    r7
    0x90e1005c, // stw     r7,92(r1)
    0xbdc10008, // stmw    r14,8(r1)
    0x90230000, // stw     r1,0(r3)
    0x7c812378, // mr      r1,r4
    0x38210040, // addi    r1,r1,64
    0xb9c10008, // lmw     r14,8(r1)
    0x80e1005c, // lwz     r7,92(r1)
    0x7ceff120, // mtcr    r7
    0x80010060, // lwz     r0,96(r1)
    0x7c0803a6, // mtlr    r0
    0x38210058, // addi    r1,r1,88
    0x38600000, // li      r3,0
    0x4e800020  // blr
};
const unsigned cv_SuspendImpl[] =
{
    0x7c0802a6, // mflr    r0
    0x9421ffe8, // stwu    r1,-24(r1)
    0x90010020, // stw     r0,32(r1)
    0x7ce00026, // mfcr    r7
    0x90e1001c, // stw     r7,28(r1)
    0xbfe10008, // stmw    r31,8(r1)
    0x7c3f0b78, // mr      r31,r1
    0x7ca12b78, // mr      r1,r5
    0x7c8903a6, // mtctr   r4
    0x4e800421, // bctrl
    0x7fe1fb78, // mr      r1,r31
    0xbbe10008, // lmw     r31,8(r1)
    0x80e1001c, // lwz     r7,28(r1)
    0x7ceff120, // mtcr    r7
    0x80010020, // lwz     r0,32(r1)
    0x7c0803a6, // mtlr    r0
    0x38210018, // addi    r1,r1,24
    0x4e800020  // blr
};
const unsigned cv_ReserveImpl[] =
{
    0x7c0802a6, // mflr    r0
    0x9421ffe8, // stwu    r1,-24(r1)
    0x90010020, // stw     r0,32(r1)
    0x7ce00026, // mfcr    r7
    0x90e1001c, // stw     r7,28(r1)
    0xbfe10008, // stmw    r31,8(r1)
    0x7c3f0b78, // mr      r31,r1
    0x7ca12b78, // mr      r1,r5
    0x3821ffc0, // addi    r1,r1,-64
    0x7c8903a6, // mtctr   r4
    0x4e800421, // bctrl
    0x7fe1fb78, // mr      r1,r31
    0xbbe10008, // lmw     r31,8(r1)
    0x80e1001c, // lwz     r7,28(r1)
    0x7ceff120, // mtcr    r7
    0x80010020, // lwz     r0,32(r1)
    0x7c0803a6, // mtlr    r0
    0x38210018, // addi    r1,r1,24
    0x4e800020  // blr
};
const unsigned cv_StackSpaceRemainingImpl[] =
{
    0x7c630850, // subf r3,r3,r1
    0x4e800020  // blr
};

#endif // ARCHITECTURE_POWERPC

#ifdef CONSTANT_DATA_IS_EXECUTABLE
# define GET_CODE_VECTOR(target, source) target = cv_##source##Impl
#else // CONSTANT_DATA_IS_EXECUTABLE
# define GET_CODE_VECTOR(target, source) target = static_cast<MachineInstruction *>(g_opera->ecmascript_module.opt_meta_methods[ES_OPT_##source])

void
OpPseudoThread::InitCodeVectors()
{
    OP_ASSERT(ES_OPT_COUNT == ARRAY_SIZE(g_opera->ecmascript_module.opt_meta_methods));

#define initcv(M) \
    const OpExecMemory* blk_##M = g_executableMemory->AllocateL(sizeof(cv_##M##Impl)); \
    op_memcpy(blk_##M->address, cv_##M##Impl, sizeof(cv_##M##Impl));                         \
    OpExecMemoryManager::FinalizeL(blk_##M);                                                 \
    g_opera->ecmascript_module.opt_meta_methods[ES_OPT_##M] = blk_##M->address;              \
    g_opera->ecmascript_module.runtime->opt_meta_method_block[ES_OPT_##M] = blk_##M;

    initcv(Start);
    initcv(Resume);
    initcv(Yield);
    initcv(Suspend);
    initcv(Reserve);
    initcv(StackSpaceRemaining);

#undef initcv
}

#endif // CONSTANT_DATA_IS_EXECUTABLE

BOOL
OpPseudoThread_StartImpl(OpPseudoThread *thread, OpPseudoThread::Callback callback, unsigned char **original_stack, unsigned char *thread_stack)
{
    union
    {
        const MachineInstruction *cv;
        BOOL (*fn)(OpPseudoThread *, OpPseudoThread::Callback, unsigned char **, unsigned char *);
    } u;
    GET_CODE_VECTOR(u.cv, Start);
    return u.fn(thread, callback, original_stack, thread_stack);
}

BOOL
OpPseudoThread_ResumeImpl(OpPseudoThread *thread, unsigned char **original_stack, unsigned char *thread_stack)
{
    union
    {
        const MachineInstruction *cv;
        BOOL (*fn)(unsigned char **, unsigned char *);
    } u;
    GET_CODE_VECTOR(u.cv, Resume);
    return u.fn(original_stack, thread_stack);
}

void
OpPseudoThread_YieldImpl(OpPseudoThread *thread, unsigned char **thread_stack, unsigned char *original_stack)
{
    union
    {
        const MachineInstruction *cv;
        void (*fn)(unsigned char **, unsigned char *);
    } u;
    GET_CODE_VECTOR(u.cv, Yield);
    u.fn(thread_stack, original_stack);
}

void
OpPseudoThread_SuspendImpl(OpPseudoThread *thread, OpPseudoThread::Callback callback, unsigned char *original_stack)
{
    union
    {
        const MachineInstruction *cv;
        void (*fn)(OpPseudoThread *, OpPseudoThread::Callback, unsigned char *);
    } u;
    GET_CODE_VECTOR(u.cv, Suspend);
    u.fn(thread, callback, original_stack);
}

void
OpPseudoThread_ReserveImpl(OpPseudoThread *thread, OpPseudoThread::Callback callback, unsigned char *new_stack_ptr)
{
    union
    {
        const MachineInstruction *cv;
        void (*fn)(OpPseudoThread *, OpPseudoThread::Callback, unsigned char *);
    } u;
    GET_CODE_VECTOR(u.cv, Reserve);
    u.fn(thread, callback, new_stack_ptr);
}

unsigned
OpPseudoThread_StackSpaceRemainingImpl(void *bottom)
{
    union
    {
        const MachineInstruction *cv;
        unsigned (*fn)(void *);
    } u;
    GET_CODE_VECTOR(u.cv, StackSpaceRemaining);
    return u.fn(bottom);
}

#endif // OPPSEUDOTHREAD_STACK_SWAPPING

#if defined(ES_RECORD_PEAK_STACK)

#define PAT_CH 0xFC

static unsigned char *
FindPeakStackPointer(unsigned char *block, unsigned block_size)
{
    unsigned size = block_size / sizeof(UINTPTR);

    UINTPTR pattern = 0;
    op_memset(&pattern, PAT_CH, sizeof(UINTPTR));

    UINTPTR *start = reinterpret_cast<UINTPTR *>(block), *end = start + size;
    while (size > 1)
    {
        size = size / 2;
        if (start[size - 1] == pattern)
        {
            if (start[size] != pattern)
            {
                OP_ASSERT(reinterpret_cast<unsigned char *>(start + size - 1) <= (block + block_size));
                OP_ASSERT(reinterpret_cast<unsigned char *>(start + size - 1) >= block);

                start += size - 1;
                return reinterpret_cast<unsigned char *>(start);
            }
            else
                start += size;
        }
        else
            end -= size;
    }

    return block + block_size;
}

static unsigned char *
SetPatternImpl(unsigned char *start, unsigned length)
{
    return static_cast<unsigned char *>(op_memset(start, PAT_CH, length - sizeof(UINTPTR)));
}
#endif // defined(ES_RECORD_PEAK_STACK)

OpPseudoThread::OpPseudoThread()
    : is_using_standard_stack(TRUE),
      original_stack(NULL),
#if defined(ES_RECORD_PEAK_STACK)
      max_peak(0),
      max_stack_size(0),
#endif
      first(NULL),
      current(NULL),
      reserve_previous(NULL)
{
}

OpPseudoThread::~OpPseudoThread()
{
    MemoryBlock *block = first;

    while (block)
    {
        MemoryBlock *next = block->next;
        if (block->segment)
            OpMemory::DestroySegment(block->segment);
        OP_DELETE(block);
        block = next;
    }
}

OP_STATUS
OpPseudoThread::Initialize(unsigned stacksize)
{
    MemoryBlock *block = first = current = OP_NEW(MemoryBlock, ());

    if (!block)
        return OpStatus::ERR_NO_MEMORY;

    block->segment = OpMemory::CreateStackSegment(stacksize);

    /* 'Block' is anchored via 'this->first' and is destroyed by the
       destructor. */
    if (!block->segment)
        return OpStatus::ERR_NO_MEMORY;

    block->memory = static_cast<unsigned char *>(block->segment->address);
    block->actual_size = block->usable_size = stacksize;

#ifdef OPPSEUDOTHREAD_CRASHLOG_SUPPORT
    block->usable_size -= 64;
    op_memset(block->memory + block->usable_size, 0, 64);
#endif // OPPSEUDOTHREAD_CRASHLOG_SUPPORT

#if defined(ES_RECORD_PEAK_STACK)
    block->stack_space_unused = stacksize;
    block->previoius_stack_pointer = block->memory + stacksize;
    block->previous_peak = 0;
    previous_reserved_stacksize = stacksize;
#endif

    thread_stack_ptr = block->memory + block->usable_size;
    return OpStatus::OK;
}

BOOL
OpPseudoThread::Start(Callback callback)
{
    OP_ASSERT(is_using_standard_stack);

    is_using_standard_stack = FALSE;

#ifdef OPPSEUDOTHREAD_CRASHLOG_SUPPORT
    current_thread = this;
    crashlog_trampoline_stack_ptr = NULL;
    crashlog_trampoline_callback = callback;
    callback = &CrashlogTrampoline;
#endif // OPPSEUDOTHREAD_CRASHLOG_SUPPORT

    OpMemory::SignalStackSwitch(OpMemory::STACK_SWITCH_SYSTEM_TO_CUSTOM, NULL, current->segment);

    BOOL res = OpPseudoThread_StartImpl(this, callback, &original_stack, current->memory + current->usable_size);

    OpMemory::SignalStackSwitch(OpMemory::STACK_SWITCH_CUSTOM_TO_SYSTEM, current->segment, NULL);

    is_using_standard_stack = TRUE;
#ifdef OPPSEUDOTHREAD_CRASHLOG_SUPPORT
    current_thread = NULL;
#endif // OPPSEUDOTHREAD_CRASHLOG_SUPPORT

    return res;
}

BOOL
OpPseudoThread::Resume()
{
    OP_ASSERT(is_using_standard_stack);

    is_using_standard_stack = FALSE;
#ifdef OPPSEUDOTHREAD_CRASHLOG_SUPPORT
    current_thread = this;
#endif // OPPSEUDOTHREAD_CRASHLOG_SUPPORT

    OpMemory::SignalStackSwitch(OpMemory::STACK_SWITCH_SYSTEM_TO_CUSTOM, NULL, current->segment);

    BOOL res = OpPseudoThread_ResumeImpl(this, &original_stack, thread_stack_ptr);

    OpMemory::SignalStackSwitch(OpMemory::STACK_SWITCH_CUSTOM_TO_SYSTEM, current->segment, NULL);

    is_using_standard_stack = TRUE;
#ifdef OPPSEUDOTHREAD_CRASHLOG_SUPPORT
    current_thread = NULL;
#endif // OPPSEUDOTHREAD_CRASHLOG_SUPPORT

    return res;
}

void
OpPseudoThread::Yield()
{
    OP_ASSERT(!is_using_standard_stack);

    OpPseudoThread_YieldImpl(this, &thread_stack_ptr, original_stack);

    current->InitializeTail(current->previous ? current->previous->stack_ptr : NULL, original_stack);
}

void
OpPseudoThread::Suspend(Callback callback)
{
#if defined(ES_RECORD_PEAK_STACK)
    current->stack_space_unused = StackSpaceRemaining();
#endif // _DEBUG

    if (!is_using_standard_stack)
    {
        unsigned char *original_stack_ptr = original_stack;

#ifdef OPPSEUDOTHREAD_CRASHLOG_SUPPORT
        current->stack_ptr = current->memory + StackSpaceRemaining();

        original_stack_ptr -= 64;

        unsigned char *ptr = reinterpret_cast<unsigned char *>(reinterpret_cast<UINTPTR>(original_stack - (16 + sizeof(void *))) & ~static_cast<UINTPTR>(15));

        op_memcpy(ptr, "CarakanPrevStack", 16);
        reinterpret_cast<void **>(ptr + 16)[0] = current->stack_ptr;
#endif // OPPSEUDOTHREAD_CRASHLOG_SUPPORT

        suspend_callback = callback;

        is_using_standard_stack = TRUE;
        OpPseudoThread_SuspendImpl(this, SuspendTrampoline, original_stack_ptr);
        is_using_standard_stack = FALSE;
    }
    else
        callback(this);
}

/* static */ void
OpPseudoThread::SuspendTrampoline(OpPseudoThread *thread)
{
    OpMemory::SignalStackSwitch(OpMemory::STACK_SWITCH_CUSTOM_TO_SYSTEM, thread->current->segment, NULL);

    thread->suspend_callback(thread);

    OpMemory::SignalStackSwitch(OpMemory::STACK_SWITCH_SYSTEM_TO_CUSTOM, NULL, thread->current->segment);
}

#ifdef OPPSEUDOTHREAD_CRASHLOG_SUPPORT
/* static */ OpPseudoThread *OpPseudoThread::current_thread = NULL;
#endif // OPPSEUDOTHREAD_CRASHLOG_SUPPORT

#if defined(ES_RECORD_PEAK_STACK)
/* static */ void
OpPseudoThread::ComputeStackUsage(OpPseudoThread *thread)
{
    unsigned char* peak = FindPeakStackPointer(thread->current->memory, thread->current->stack_space_unused);
    OP_ASSERT(peak <= (thread->current->memory + thread->current->stack_space_unused));
    OP_ASSERT(INTPTR(thread->current->previoius_stack_pointer - peak) >= 0);

    thread->current->previous_peak = thread->current->previoius_stack_pointer - peak;
    thread->max_peak = es_maxu(thread->max_peak, thread->current->previous_peak);
    thread->current->previoius_stack_pointer = thread->current->memory + thread->current->stack_space_unused;
    unsigned stack_size = 0;
    MemoryBlock *b = thread->current;
    while (b != NULL)
    {
        stack_size += (thread->current->usable_size - thread->current->stack_space_unused);
        b = b->previous;
    }

    thread->max_stack_size = es_maxu(thread->max_stack_size, stack_size);
}
/* static */ void
OpPseudoThread::SetPattern(OpPseudoThread *thread)
{
    OP_ASSERT(thread->current->stack_space_unused < thread->current->usable_size);
    OP_ASSERT(thread->current->stack_space_unused >= sizeof(UINTPTR));

    unsigned char* peak = FindPeakStackPointer(thread->current->memory, thread->current->stack_space_unused);
    if (peak < thread->current->memory + thread->current->stack_space_unused)
        SetPatternImpl(peak - 1, (thread->current->memory + thread->current->stack_space_unused) - peak + 1);
}
#endif

OP_STATUS
OpPseudoThread::Reserve(Callback callback, unsigned stacksize)
{
#if defined(ES_RECORD_PEAK_STACK)
    current->stack_space_unused = StackSpaceRemaining();
    if (!is_using_standard_stack)
    {
        Suspend(ComputeStackUsage);
        OP_ASSERT(current->previous_peak < previous_reserved_stacksize);
    }
    previous_reserved_stacksize = stacksize;
#endif
    if (!is_using_standard_stack && StackSpaceRemaining() < stacksize)
    {
#if defined(ES_RECORD_PEAK_STACK)
        OP_STATUS res = DoReserve(callback, stacksize);
        Suspend(SetPattern);
        return res;
#else
        return DoReserve(callback, stacksize);
#endif
    }
    else
    {
        callback(this);
#if defined(ES_RECORD_PEAK_STACK)
        if (!is_using_standard_stack)
            Suspend(SetPattern);
#endif
        return OpStatus::OK;
    }
}

unsigned
OpPseudoThread::StackSpaceRemaining()
{
    return OpPseudoThread_StackSpaceRemainingImpl(current->memory);
}

unsigned char *
OpPseudoThread::StackLimit()
{
#ifdef OPPSEUDOTHREAD_STACK_SWAPPING
    return current->memory;
#else // OPPSEUDOTHREAD_STACK_SWAPPING
    return 0;
#endif // OPPSEUDOTHREAD_STACK_SWAPPING
}

unsigned char *
OpPseudoThread::StackTop()
{
#ifdef OPPSEUDOTHREAD_STACK_SWAPPING
    return current->memory + current->usable_size;
#else // OPPSEUDOTHREAD_STACK_SWAPPING
    return 0;
#endif // OPPSEUDOTHREAD_STACK_SWAPPING
}

#ifdef _DEBUG

void
OpPseudoThread::DetectPointersIntoRanges(MemoryRange *ranges, unsigned ranges_count, void (*callback)(void *, void *), void *callback_data)
{
    DetectPointersIntoRangesInternal(is_using_standard_stack ? current->stack_space_unused : StackSpaceRemaining(), ranges, ranges_count, callback, callback_data);
}

void
OpPseudoThread::DetectPointersIntoRangesInternal(unsigned stack_space_remaining, MemoryRange *ranges, unsigned ranges_count, void (*callback)(void *, void *), void *callback_data)
{
    UINTPTR bottom = reinterpret_cast<UINTPTR>(ranges[0].start), top = bottom + ranges[0].length;

    for (unsigned index = 1; index < ranges_count; ++index)
        if (reinterpret_cast<UINTPTR>(ranges[index].start) < bottom)
            bottom = reinterpret_cast<UINTPTR>(ranges[index].start);
        else if (reinterpret_cast<UINTPTR>(ranges[index].start) + ranges[index].length > top)
            top = reinterpret_cast<UINTPTR>(ranges[index].start) + ranges[index].length;

    MemoryBlock *block = first;

    while (block)
    {
        unsigned char *memory = block->memory;
        unsigned size = block->usable_size;

        if (block == current)
        {
            memory += stack_space_remaining;
            size -= stack_space_remaining;
        }

        OP_ASSERT((reinterpret_cast<UINTPTR>(memory) & (sizeof(UINTPTR) - 1)) == 0);

        UINTPTR *ptr = reinterpret_cast<UINTPTR *>(memory), *stop = ptr + (size / sizeof(UINTPTR));

        while (ptr != stop)
        {
            if (*ptr >= bottom && *ptr <= top)
                for (unsigned index = 0; index < ranges_count; ++index)
                    if (*ptr >= reinterpret_cast<UINTPTR>(ranges[index].start) && *ptr < reinterpret_cast<UINTPTR>(ranges[index].start) + ranges[index].length)
                    {
                        callback(callback_data, reinterpret_cast<void *>(*ptr));
                        break;
                    }

            ++ptr;
        }

        if (block == current)
            break;
        else
            block = block->next;
    }
}

#endif // _DEBUG

OP_STATUS
OpPseudoThread::DoReserve(Callback callback, unsigned stacksize)
{
    MemoryBlock *previous = reserve_previous = current;

    previous->stack_ptr = current->memory + StackSpaceRemaining();

    reserved_stacksize = stacksize;
    Suspend(AllocateNewBlock);

    if (previous == current)
        return OpStatus::ERR_NO_MEMORY;

#ifdef OPPSEUDOTHREAD_CRASHLOG_SUPPORT
    crashlog_trampoline_stack_ptr = previous->stack_ptr;
    crashlog_trampoline_callback = callback;
    callback = &CrashlogTrampoline;
#endif // OPPSEUDOTHREAD_CRASHLOG_SUPPORT

    reserve_previous = previous;
    reserve_callback = callback;

    OpMemory::SignalStackSwitch(OpMemory::STACK_SWITCH_BEFORE_CUSTOM_TO_CUSTOM, previous->segment, current->segment);

    OpPseudoThread_ReserveImpl(this, ReserveTrampoline, current->memory + current->usable_size);

    OpMemory::SignalStackSwitch(OpMemory::STACK_SWITCH_AFTER_CUSTOM_TO_CUSTOM, current->segment, previous->segment);

    current = previous;
    return OpStatus::OK;
}

/* static */ void
OpPseudoThread::ReserveTrampoline(OpPseudoThread *thread)
{
    /* Store in local variable to preserve value in case of nested Reserve()
       calls (which are very likely to happen given that Reserve() is primarily
       used in the case of heavy recursion.) */
    MemoryBlock *previous = thread->reserve_previous;

    OpMemory::SignalStackSwitch(OpMemory::STACK_SWITCH_AFTER_CUSTOM_TO_CUSTOM, previous->segment, thread->current->segment);

    thread->reserve_callback(thread);

    OpMemory::SignalStackSwitch(OpMemory::STACK_SWITCH_BEFORE_CUSTOM_TO_CUSTOM, thread->current->segment, previous->segment);
}

/* static */ void
OpPseudoThread::AllocateNewBlock(OpPseudoThread *thread)
{
    MemoryBlock *block, *next = thread->current->next;

    if (next && next->usable_size >= thread->reserved_stacksize)
    {
        thread->current = next;
#if defined(ES_RECORD_PEAK_STACK)
        thread->current->stack_space_unused = thread->current->size;
        thread->current->previoius_stack_pointer = thread->current->memory + thread->current->size;
        thread->current->previous_peak = 0;
#endif
    }
    else
    {
        while (next)
        {
            MemoryBlock *next_next = next->next;
            OP_DELETE(next);
            next = next_next;
        }

        block = OP_NEW(MemoryBlock, (thread->current));

        if (!block)
            return;

        unsigned blocksize = block->previous->actual_size * 2;

        while (blocksize < thread->reserved_stacksize)
            blocksize += blocksize;

        block->segment = OpMemory::CreateStackSegment(blocksize);

        if (!block->segment)
        {
            OP_DELETE(block);
            return;
        }

        block->memory = static_cast<unsigned char *>(block->segment->address);
        block->actual_size = block->usable_size = blocksize;

#ifdef OPPSEUDOTHREAD_CRASHLOG_SUPPORT
        block->usable_size -= 64;
        op_memset(block->memory + block->usable_size, 0, 64);
#endif // OPPSEUDOTHREAD_CRASHLOG_SUPPORT

#if defined(ES_RECORD_PEAK_STACK)
        block->stack_space_unused = blocksize;
        block->previoius_stack_pointer = block->memory + blocksize;
        block->previous_peak = 0;
#endif

        thread->current = thread->current->next = block;
    }
}

void
OpPseudoThread::MemoryBlock::InitializeTail(void *previous_stack_ptr, void *original_stack_ptr)
{
#ifdef OPPSEUDOTHREAD_CRASHLOG_SUPPORT
    if (actual_size - usable_size == 64)
    {
        unsigned char *ptr = reinterpret_cast<unsigned char *>(reinterpret_cast<UINTPTR>(memory + actual_size - (16 + sizeof(void *))) & ~static_cast<UINTPTR>(15));

        op_memcpy(ptr, "CarakanPrevStack", 16);
        reinterpret_cast<void **>(ptr + 16)[0] = previous_stack_ptr ? previous_stack_ptr : original_stack_ptr;
    }
#endif // OPPSEUDOTHREAD_CRASHLOG_SUPPORT
}


#ifdef OPPSEUDOTHREAD_CRASHLOG_SUPPORT

/* static */ void
OpPseudoThread::CrashlogTrampoline(OpPseudoThread *thread)
{
    thread->current->InitializeTail(thread->crashlog_trampoline_stack_ptr, thread->original_stack);
    thread->crashlog_trampoline_callback(thread);
}

#endif // OPPSEUDOTHREAD_CRASHLOG_SUPPORT

#endif // OPPSEUDOTHREAD_THREADED
#endif // ES_OPPSEUDOTHREAD
