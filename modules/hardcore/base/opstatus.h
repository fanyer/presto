/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef MODULES_HARDCORE_BASE_OPSTATUS_H
#define MODULES_HARDCORE_BASE_OPSTATUS_H

#if defined(HAVE_LTH_MALLOC)
struct site_t;
#endif

#ifndef OVERRIDE_OP_STATUS_VALUES
#define OP_STATUS_ERR -1
#define OP_STATUS_ERR_NO_MEMORY -2
#define OP_STATUS_ERR_NULL_POINTER -3
#define OP_STATUS_ERR_OUT_OF_RANGE -4
#define OP_STATUS_ERR_NO_ACCESS -5
#define OP_STATUS_ERR_NOT_DELAYED -6
#define OP_STATUS_ERR_FILE_NOT_FOUND -7
#define OP_STATUS_ERR_NO_DISK -8
#define OP_STATUS_ERR_NOT_SUPPORTED -9
#define OP_STATUS_ERR_PARSING_FAILED -10
#define OP_STATUS_ERR_NO_SUCH_RESOURCE -11
#define OP_STATUS_ERR_BAD_FILE_NUMBER -12
#define OP_STATUS_ERR_YIELD -13
#endif // !OVERRIDE_OP_STATUS_VALUES

/* OpStatus::OK is always zero, and in the absence of user-defined success
 * codes it is always correct to test for success by comparing a return
 * value to zero: if it differs, it is an error.
 *
 * Other codes are non-zero but may be positive or negative, depending on
 * the platform.
 *
 * Additional success and error codes can be defined at non-negative offsets from
 * OpStatus::USER_SUCCESS and OpStatus::USER_ERROR, respectively.  The predicates
 * OpStatus::IsSuccess() and OpStatus::IsError() can be used to recognize
 * error and success codes defined in this manner, as well as predefined codes.
 *
 * The definition of OpStatus guarantees that there will be space for at least
 * 2048 additional error and success codes.
 */

#ifdef USE_DEBUGGING_OP_STATUS
class OP_STATUS;
#define OP_STATUS_ARG OP_STATUS&
#else
typedef int OP_STATUS;
#define OP_STATUS_ARG OP_STATUS
#endif // USE_DEBUGGING_OP_STATUS

#ifndef HAVE_FORCE_INLINE
#define op_force_inline inline
#endif

class OpStatus
{

public:

	static op_force_inline BOOL IsSuccess(const OP_STATUS_ARG status);

	static op_force_inline BOOL IsError(const OP_STATUS_ARG status);

	/**
	 * Check whether this is a raisable error code.
	 * Error codes returning TRUE must corespond to
     * error codes handled in MemoryManager::RaiseCondition
	 *
	 * @param status Status value to check
	 * @return TRUE if status may cause a raised condition.
	 */
	static op_force_inline BOOL IsRaisable(const OP_STATUS_ARG status);

    /**
	 * Check whether this is a fatal error. The definition of
	 * fatal is that it is one something that should cause a
	 * LEAVE to be executed. Currently, only ERR_NO_MEMORY will
	 * return TRUE.
	 *
	 * @param status Status value to check
	 * @return TRUE if status should cause a LEAVE.
	 */
	static op_force_inline BOOL IsFatal(const OP_STATUS_ARG status);

	/**
	 * Check whether this is a memory error. Use this when
	 * a situation occurs that sould raise a condition in the
	 * memory manager. Currently, only ERR_NO_MEMORY will
	 * return TRUE.
	 *
	 * @param status Status value to check
	 * @return TRUE if status should cause a LEAVE.
	 */
	static op_force_inline BOOL IsMemoryError(const OP_STATUS_ARG status);

    /**
     * Same as above, but for
     * - ERR_NO_DISK
     * - ERR_NO_ACCESS
     * - ERR_FILE_NOT_FOUND;
     */
	static op_force_inline BOOL IsDiskError(const OP_STATUS_ARG status);

#if defined DEBUG_ENABLE_OPASSERT || defined _DEBUG
	/**
	 * Currently used by OP_ASSERT_STATUS().
	 *
	 * @return a string with OpStatus::ErrorEnum literal value that
	 *         represents the parameter. NULL if value is not recognized.
	 */
	static const char* Stringify(const OP_STATUS_ARG value);
#endif // defined DEBUG_ENABLE_OPASSERT || defined _DEBUG

	static op_force_inline void Ignore(const OP_STATUS_ARG status);

	static op_force_inline int GetIntValue(const OP_STATUS_ARG status);

	enum ErrorEnum
	{
		OK                   = 0,								// Generic success
		ERR                  = OP_STATUS_ERR,					// Generic failure
		ERR_NO_MEMORY        = OP_STATUS_ERR_NO_MEMORY,			// No memory to complete operation
		ERR_NULL_POINTER     = OP_STATUS_ERR_NULL_POINTER,		// Null pointer detected where none allowed
		ERR_OUT_OF_RANGE     = OP_STATUS_ERR_OUT_OF_RANGE,		// Parameter out of permitted range
		ERR_NO_ACCESS        = OP_STATUS_ERR_NO_ACCESS,			// Attempting to write a read-only entity
		ERR_NOT_DELAYED      = OP_STATUS_ERR_NOT_DELAYED,		// Posted message was not delayed
		ERR_FILE_NOT_FOUND   = OP_STATUS_ERR_FILE_NOT_FOUND,	// File not found or could not be opened
		ERR_NO_DISK          = OP_STATUS_ERR_NO_DISK,			// Disk is full
		ERR_NOT_SUPPORTED    = OP_STATUS_ERR_NOT_SUPPORTED,		// The call is not supported for this module/object/platform
		ERR_PARSING_FAILED   = OP_STATUS_ERR_PARSING_FAILED,	// A parsing operation failed
		ERR_NO_SUCH_RESOURCE = OP_STATUS_ERR_NO_SUCH_RESOURCE,	// Resource not available (like not assigned listener)
		ERR_BAD_FILE_NUMBER  = OP_STATUS_ERR_BAD_FILE_NUMBER,	// Bad file number, operations on resource illegal (wrong socket id/file id)
		ERR_YIELD			 = OP_STATUS_ERR_YIELD,				// Yield this operation

		// "Soft" error codes should not normally be used to report errors inside
		// the code but are appropriate when interfacing to the world.  For example,
		// OpStatus::ERR_SOFT_NO_MEMORY can be passed to Window::RaiseCondition() and
		// MemoryManager::RaiseCondition() to signal a soft (non-aborting) OOM
		// condition.
		ERR_SOFT_NO_MEMORY = -4097	// Note, IsMemoryError() must be FALSE for this
	};

protected:

	// Use USER_SUCCESS and USER_ERROR in classes
	// inherited from OpStatus.

	enum
	{
		USER_SUCCESS = 1,			// New success codes at non-negative offsets from this
		USER_ERROR = -4096			// New error codes at non-negative offsets from this
	};

private:

	// Definitions so that we cannot instantiate OpStatus.

	OpStatus();

	OpStatus(const OpStatus&);

};

class OpBoolean : public OpStatus
{
public:

	enum ExtraBoolEnum
	{
		IS_FALSE = USER_SUCCESS+1,		// FALSE is a macro
		IS_TRUE = USER_SUCCESS+2		// TRUE is a macro
	};

#if defined DEBUG_ENABLE_OPASSERT || defined _DEBUG
	/**
	 * Currently used by OP_ASSERT_STATUS().
	 *
	 * @return a string with OpBoolean::ExtraBoolEnum literal value that
	 *         represents the parameter. NULL if value is not recognized.
	 */
	static const char* Stringify(const OP_STATUS_ARG value);
#endif // defined DEBUG_ENABLE_OPASSERT || defined _DEBUG

protected:

	OpBoolean();
	OpBoolean(const OpBoolean&);
    OpBoolean& operator =(const OpBoolean&);

};

#ifdef USE_DEBUGGING_OP_STATUS

/**
 * Inheriting from this class using 'private virtual', makes it
 * impossible to subclass any further.  The subclass becomes a final
 * class.
 */
class PriVirFinal
{
protected:
	PriVirFinal() {};
};

class OP_STATUS : private virtual PriVirFinal
{
public:
	friend class OpStatus;

	inline OP_STATUS();
	inline OP_STATUS(OpStatus::ErrorEnum value);
	template<class T> inline OP_STATUS(const T& status);
	inline OP_STATUS(const OP_STATUS& status);
#if defined(HAVE_LTH_MALLOC)
	~OP_STATUS();
#else
	inline ~OP_STATUS();
#endif

	inline const OP_STATUS& operator=(const OP_STATUS& status);

	inline BOOL operator<(const OP_STATUS& value) const;
	inline BOOL operator<=(const OP_STATUS& value) const;
	inline BOOL operator>(const OP_STATUS& value) const;
	inline BOOL operator>=(const OP_STATUS& value) const;

	inline BOOL operator==(const OP_STATUS& value) const;
	inline BOOL operator!=(const OP_STATUS& value) const;

	static int GetIgnored();
	static int GetUndefined();

	OpStatus::ErrorEnum GetValue() const { return m_value; };

private:
	inline OP_STATUS(const int status);
	inline OP_STATUS(const unsigned int status);
	inline OP_STATUS(const unsigned char status);
	inline OP_STATUS(const char status);
	inline OP_STATUS(const long status);
	inline OP_STATUS(const unsigned long status);
	inline OP_STATUS(const unsigned short status);
	inline OP_STATUS(const short status);
	inline OP_STATUS(const bool status);
	template<class T> inline OP_STATUS(const T* status);

	OpStatus::ErrorEnum m_value;
	static int m_ignored;
	static int m_undefined;
	enum {
		UNDEFINED, // Not yet given a value
		UNCHECKED, // Unchecked value
		CHECKED    // Value checked at least once
	} m_status;

#ifdef STACK_TRACE_SUPPORT
	static const int STACK_LEVELS = 10;
	void* m_construct_addrs[STACK_LEVELS];
#endif
#ifdef PARTIAL_LTH_MALLOC
	BOOL created_in_non_oom_code;
#endif

	inline void SetChecked() const;
#if defined(HAVE_LTH_MALLOC)
	void Error(const char *msg, site_t* site) const;
#else
	void Error(const char *msg) const;
#endif
	void IgnoredOpStatus() const;
	void UndefinedOpStatus() const;
};
#endif // USE_DEBUGGING_OP_STATUS

/* OpBoolean is an extension of OpStatus, defined below;
   using OP_BOOLEAN has more information for the reader.
   */
typedef OP_STATUS OP_BOOLEAN;


#include "modules/hardcore/base/opstatus.icc"

#endif // !MODULES_HARDCORE_BASE_OPSTATUS_H
