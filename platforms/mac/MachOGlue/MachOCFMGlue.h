#ifndef _MACH_TO_CFM_H_
#define _MACH_TO_CFM_H_

#ifdef __cplusplus
extern "C" {
#endif

// CFM function pointer
typedef struct TVector_struct
{
    ProcPtr fProcPtr;
    UInt32 fTOC;
} TVector_rec, *TVector_ptr;

void*	CFMFunctionPointerForMachOFunctionPointer( void* inMachProcPtr );
void	DisposeCFMFunctionPointer( void* inCfmProcPtr );

void*	MachOFunctionPointerForCFMFunctionPointer( void* inCfmProcPtr );
void	DisposeMachOFunctionPointer( void* inMachProcPtr );

#ifdef __cplusplus
}
#endif

#endif // _MACH_TO_CFM_H_
