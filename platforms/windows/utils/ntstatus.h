// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
//
// Copyright (C) 2003-2010 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Øyvind Østlund
//
// This file contain copies of NTSTATUS error codes from ntstatus.h
// We can not include ntstatus.h since it will redefine a lot of the same
// error codes as already defined when we indirectly includes winnt.h.
//
// It also includes 4 helper macros found from winternl.h
//
// - NT_SUCCESS(status)
// - NT_INFORMATION(status)
// - NT_WARNING(status)
// - MT_ERROR(status)
// 

#ifndef NTSTATUS_H
#define NTSTATUS_H

//
// Generic test for success on any status value (non-negative numbers
// indicate success).
//
#ifndef NT_SUCCESS
#define NT_SUCCESS(Status) (((NTSTATUS)(Status)) >= 0)
#endif

//
// Generic test for information on any status value.
//
#ifndef NT_INFORMATION
#define NT_INFORMATION(Status) ((((ULONG)(Status)) >> 30) == 1)
#endif

//
// Generic test for warning on any status value.
//
#ifndef NT_WARNING
#define NT_WARNING(Status) ((((ULONG)(Status)) >> 30) == 2)
#endif

//
// Generic test for error on any status value.
//
#ifndef NT_ERROR
#define NT_ERROR(Status) ((((ULONG)(Status)) >> 30) == 3)
#endif


/////////////////////////////////////////////////////////////////////////
//
// Standard Success values (from ntstatus.h)
//
/////////////////////////////////////////////////////////////////////////
#ifndef NT_STATUS_SUCCESS
//
// The success status codes 0 - 63 are reserved for wait completion status.
// FacilityCodes 0x5 - 0xF have been allocated by various drivers.
//
#define NT_STATUS_SUCCESS						((NTSTATUS)0x00000000L) // ntsubauth
#endif // NT_STATUS_SUCCESS

#ifndef NT_STATUS_INFO_LENGTH_MISMATCH
//
// MessageId: NT_STATUS_INFO_LENGTH_MISMATCH
//
// MessageText:
//
// The specified information record length does not match the length required for the specified information class.
//
#define NT_STATUS_INFO_LENGTH_MISMATCH			((NTSTATUS)0xC0000004L)
#endif // NT_STATUS_INFO_LENGTH_MISMATCH
#ifndef NT_STATUS_NOT_IMPLEMENTED
//
// MessageId: NT_STATUS_NOT_IMPLEMENTED
//
// MessageText:
//
// {Not Implemented}
// The requested operation is not implemented.
//
#define NT_STATUS_NOT_IMPLEMENTED				((NTSTATUS)0xC0000002L)
#endif // NT_STATUS_NOT_IMPLEMENTED
#ifndef NT_STATUS_INVALID_INFO_CLASS
//
// MessageId: NT_STATUS_INVALID_INFO_CLASS
//
// MessageText:
//
// {Invalid Parameter}
// The specified information class is not a valid information class for the specified object.
//
#define NT_STATUS_INVALID_INFO_CLASS			((NTSTATUS)0xC0000003L)    // ntsubauth
#endif // NT_STATUS_INVALID_INFO_CLASS
#ifndef NT_STATUS_INVALID_DEVICE_REQUEST
//
// MessageId: NT_STATUS_INVALID_DEVICE_REQUEST
//
// MessageText:
//
// The specified request is not a valid operation for the target device.
//
#define NT_STATUS_INVALID_DEVICE_REQUEST		((NTSTATUS)0xC0000010L)
#endif // NT_STATUS_INVALID_DEVICE_REQUEST
#ifndef NT_STATUS_NO_MEMORY
//
// MessageId: NT_STATUS_NO_MEMORY
//
// MessageText:
//
// {Not Enough Quota}
// Not enough virtual memory or paging file quota is available to complete the specified operation.
//
#define NT_STATUS_NO_MEMORY					((NTSTATUS)0xC0000017L)    // winnt
#endif // NT_STATUS_NO_MEMORY
#ifndef NT_STATUS_ACCESS_DENIED
//
// MessageId: NT_STATUS_ACCESS_DENIED
//
// MessageText:
//
// {Access Denied}
// A process has requested access to an object, but has not been granted those access rights.
//
#define NT_STATUS_ACCESS_DENIED				((NTSTATUS)0xC0000022L)
#endif // NT_STATUS_ACCESS_DENIED
#ifndef NT_STATUS_OBJECT_TYPE_MISMATCH
//
// MessageId: NT_STATUS_OBJECT_TYPE_MISMATCH
//
// MessageText:
//
// {Wrong Type}
// There is a mismatch between the type of object required by the requested operation and the type of object that is specified in the request.
//
#define NT_STATUS_OBJECT_TYPE_MISMATCH			((NTSTATUS)0xC0000024L)
#endif // NT_STATUS_ACCESS_DENIED

#endif // NTSTATUS_H