/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2006 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

/** \file
 * \brief This file provides a default value for \c OP_ASSERT().
 *
 * If you need to use a different value for the \c OP_ASSERT() macro,
 * or one of the other macros listed below, apply the tweak \c
 * TWEAK_DEBUG_EXTERNAL_MACROS and define \c
 * DEBUG_EXTERNAL_MACROS_HEADER to point to a header file of your own
 * choosing.
 *
 * In this file, or the one you choose to provide yourself, the following
 * macros must/may be declared:
 *
 *    OP_ASSERT(expr)              Must be defined \n
 *    OP_ASSERT_MSG(expr, args)    Must be defined \n
 *    OP_ASSERT_ALIGNED(address)   Optional \n
 *    OP_STATIC_ASSERT(expr)       Optional \n
 *    OP_ASSERT_STATUS_FN(expr, fn) Optional \n
 *    OP_ASSERT_STATUS(expr)       Optional \n
 *    OP_CHECK_STATUS(expr)        Optional \n
 *    OP_NEW_DBG(function, key)    Optional \n
 *    OP_DBG(args)                 Optional \n
 *    OP_DBG_LEN(str, len)         Optional \n
 *
 * The "Optional" macros above may be defined if you need to, but this is
 * not recomended, and probably never needed.  If they are not defined,
 * default values from the debug module will be used.
 *
 * This file is intended to support WINGOGI, LINGOGI and Desktop with
 * suitable values, and provide a default value which should not
 * need tweaking on most platforms.
 */

#ifndef DEBUG_MACROS_H
#define DEBUG_MACROS_H

/**
 * \brief Default function to handle OP_ASSERT failures
 *
 * Unless the \c OP_ASSERT() macro is tweaked, this function is
 * called when an assert fails.  This function should be implemented
 * by the platform to catch and display the assertion failure.
 *
 * Note: This function should have C linkage.
 *
 * \param expr The expression that evaluated to false
 * \param file The file where the assertion was located
 * \param line The line number of the assertion
 */
extern "C" void Debug_OpAssert(const char* expr, const char* file, int line);


/**
 * \brief The official Opera assert macro.
 *
 * This macro may have different implementations depending on the
 * platform it is used on. See \ref dbg-portingmacros for details
 *
 * The default is to simply call the Debug::OpAssert method which should
 * be implemented by the platform.
 *
 * There is another definition used for \c WINGOGI and \c Windows
 * \c Desktop, which is slightly more complicated.
 * See \c modules/debug/src/debug_macros.h for details.
 */
#define OP_ASSERT(expr) do { \
	if (!(expr)) Debug_OpAssert(#expr, __FILE__, __LINE__); } while (0)

/**
 * \brief Test if a condition holds during runtime, with a runtime generated message.
 *
 * Always implemented here.
 *
 * Sibling of OP_ASSERT. While OP_ASSERT outputs the expression that failed,
 * this macros allows a runtime generated message to be used instead, in order
 * to be easier to inspect why the assert failed.
 *
 * Example:
 *   OP_ASSERT_MSG(my_var == 3, ("my_var == %d", my_var));
 *
 * @param expr The expression to check. If it evaluates to false, then the assert triggers.
 * @param args List of arguments to pass to OpString8::AppendFormat, wrapped by parenthesis.
 */
# define OP_ASSERT_MSG(expr, args) do { if (!(expr)) {\
	OP_ASSERT_FORMAT_ARGS(expr, args, message) \
	Debug_OpAssert(message, __FILE__, __LINE__); \
} } while(0)

# define OP_ASSERT_FORMAT_ARGS(expr, args, var) \
	const char *var; \
	OpString8 op_assert_assert_format_args_o; \
	if (OpStatus::IsSuccess(op_assert_assert_format_args_o.AppendFormat args)) \
		var = op_assert_assert_format_args_o.CStr(); \
	else \
		var = "Out-of-memory while formatting message for OP_ASSERT_FORMAT(" #expr ", " #args ")"; \

/** \cond */

#if defined(WINGOGI) || defined(MSWIN)

extern BOOL _DEBUG_IsRunByDebugger(void);

#ifdef WINGOGI
// For easy source access in visual studio:
# define OP_ASSERT_PRINT_WIN(expr) dbg_printf("%s (%d) : ASSERT FAILED. OP_ASSERT(%s)\n", \
									 __FILE__, __LINE__, #expr)
#else
# define OP_ASSERT_PRINT_WIN(expr) dbg_printf("ASSERT FAILED. OP_ASSERT(%s) %s:%d\n", \
									 #expr, __FILE__, __LINE__)
#endif // WINGOGI

#ifdef SIXTY_FOUR_BIT
# define OP_ASSERT_BREAK_WIN __debugbreak();
#else
# define OP_ASSERT_BREAK_WIN _asm { int 3 }
#endif // SIXTY_FOUR_BIT

#undef OP_ASSERT
#undef OP_ASSERT_MSG

#define OP_ASSERT(expr) do { \
	if (!(expr)) { \
		if (_DEBUG_IsRunByDebugger()) { \
			OP_ASSERT_PRINT_WIN(expr); \
			OP_ASSERT_BREAK_WIN \
		} else { \
			Debug_OpAssert(#expr, __FILE__, __LINE__); \
		} \
	} \
} while (0)

#define OP_ASSERT_MSG(expr, args) do { \
	if (!(expr)) { \
		OP_ASSERT_FORMAT_ARGS(expr, args, message) \
		if (_DEBUG_IsRunByDebugger()) { \
			OP_ASSERT_PRINT_WIN(message); \
			OP_ASSERT_BREAK_WIN \
		} else { \
			Debug_OpAssert(message, __FILE__, __LINE__); \
		} \
	} \
} while (0)

#endif // WINGOGI || MSWIN

/** \endcond */

/**
 * \brief Convenience macro for asserting correct alignment
 *
 * This macro is a simple wrapper around OP_ASSERT for asserting
 * the correct alignment of a given memory address in the context
 * of a given type.
 *
 * Note that this macro is never needed by code using "regular" stack- or
 * heap-allocated objects. However, it may be useful in cases resembling the
 * following examples:
 * - Before casting a \c void* to a different type.
 * - When given a \c void* (or otherwise incorrectly typed) pointer into which
 *   an object (of a different type) should be stored (typically in the
 *   core/platform or the platform/customer interface).
 * - Other cases involving object placement at arbitrary addresses.
 *
 * This macro depends on the op_strideof() macro #defined in
 * modules/memory/memory_fundamentals.h
 */
#ifdef op_strideof
# define OP_ASSERT_ALIGNED(TypeName, addr) \
	OP_ASSERT(((UINTPTR)(addr)) % op_strideof(TypeName) == 0)
#else
# define OP_ASSERT_ALIGNED(TypeName, addr) ((void)0)
#endif // op_strideof

/**
 * \brief Test if a condition holds at compile time
 *
 * Always implemented here.
 *
 * This macro can be used to perform tests involving constant expressions at
 * compile time. Examples include tests on enum values and C++ constants, and
 * tests on types (e.g. from template or macro parameters), which can't be done
 * at compile time using just the preprocessor.
 * It works both in namespace, class and block scopes.
 *
 * The macro evaluates the condition both in debug and release builds.
 */
// GCC gives a nice error message: "size of array ‘op_static_assert_4_on_line_44’ is negative"
#define OP_STATIC_ASSERT_NAME_(a, b) op_static_assert_ ## a ## _on_line_ ## b
#define OP_STATIC_ASSERT_NAME(a, b) OP_STATIC_ASSERT_NAME_(a, b)
// __COUNTER__ supported by GCC 4.3+, Intel 7.0+, and Visual C++ 7.0+
// Still, the probability of OP_STATIC_ASSERT producing name clashes
// is quite scarce if they are kept in a limited scope.
#define OP_STATIC_ASSERT(expr) typedef char OP_STATIC_ASSERT_NAME(__COUNTER__, __LINE__)[ (expr) ? 1 : -1 ];

/**
 * \brief Test if an expression returns an OP_STATUS value which represents success.
 *
 * Always implemented here.
 *
 * Tests if an expression return an OP_STATUS value which is success, else
 * it prints the error code's literal form, if it's part of OpStatus::ErrorEnum
 * or recognized by the 'prettify_function'.
 *
 * Example:
 *   OP_ASSERT_STATUS(MethodCall());
 *   OP_ASSERT_STATUS_FN(MethodCall(), MyStatus::Stringify);
 *
 * @param expr The expression to check. If it evaluates to an error, then the assert triggers.
 * @param prettify_function A function that converts the OP_STATUS values into a
 *        string, in case you're using values outside OpStatus::ErrorEnum. Unrecognized values
 *        return NULL.
 */
# define OP_ASSERT_STATUS_FN(expr, prettify_function) do { \
	OP_STATUS op_assert_status_temp = (expr); \
	if (OpStatus::IsError(op_assert_status_temp)) {\
		const char *op_assert_status_pretty_value = (prettify_function)(op_assert_status_temp); \
		if (op_assert_status_pretty_value) \
			OP_ASSERT_MSG(0, ("'" #expr "' returned error %d (%s)", op_assert_status_temp, op_assert_status_pretty_value)); \
		else \
			OP_ASSERT_MSG(0, ("'" #expr "' returned error %d", op_assert_status_temp)); \
} } while(0)
# define OP_ASSERT_STATUS(expr) OP_ASSERT_STATUS_FN(expr, OpStatus::Stringify)

/**
 * \brief Test if an expression returns an OP_STATUS value which represents success.
 *
 * Always implemented here.
 *
 * Tests if an expression return an OP_STATUS value which is success, else
 * it prints the error code in text form, if it's part of OpStatus::ErrorEnum.
 *
 * Unlike the regular OP_ASSERT_STATUS(), this macro evaluates the expression in
 * builds with asserts disabled, but not the return value check.
 *
 * Example:
 *   OP_CHECK_STATUS(MethodCall());
 *
 * @param expr The expression to check. If it evaluates to an error, then the assert triggers.
 */
#ifdef DEBUG_ENABLE_OPASSERT
# define OP_CHECK_STATUS(expr) OP_ASSERT_STATUS(expr)
# define OP_CHECK_STATUS_FN(expr, fn) OP_ASSERT_STATUS_FN(expr, fn)
#else
# define OP_CHECK_STATUS(expr) OpStatus::Ignore(expr)
# define OP_CHECK_STATUS_FN(expr, fn) OpStatus::Ignore(expr)
#endif // DEBUG_ENABLE_OPASSERT

#endif // !DEBUG_MACROS_H
