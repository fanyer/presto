/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef MODULES_HARDCORE_BASE_DEPRECATE_H
#define MODULES_HARDCORE_BASE_DEPRECATE_H

#ifndef INSIDE_PI_IMPL
#  if defined(__GNUC__)
#    if __GNUC__ >= 3
#      if __GNUC__ > 3 || __GNUC_MINOR__ > 0 // 3.0 doesn't like deprecated
#        define COREAPI_DEPRECATED(X) X __attribute__((__deprecated__))
#        if defined(ENABLE_DEPRECATED_WARNINGS)
#          define DEPRECATED(X) X __attribute__((__deprecated__))
#        endif // ENABLE_DEPRECATED_WARNINGS
#      endif
#      define PURE_FUNCTION   __attribute__((__pure__))
#      if __GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 4) // Introduced in 3.4
#        define CHECK_RESULT(X) X __attribute__((warn_unused_result))
#      endif
#    endif
#  elif defined(_MSC_VER)
#    if _MSC_VER >= 1400
#      define COREAPI_DEPRECATED(X) __declspec(deprecated) X
#      if defined(ENABLE_DEPRECATED_WARNINGS)
#        define DEPRECATED(X) __declspec(deprecated) X
#      endif // ENABLE_DEPRECATED_WARNINGS
#    endif
#  endif
#endif // INSIDE_PI_IMPL

/** @def DEPRECATED(declaration)
 *
 * When deprecating a method or function, use the DEPRECATED macro on its
 * declaration.  Due to a bug in gcc < 3.4, it is necessary to separate the body
 * from the declaration; if the body was provided inline with the declaration,
 * you should move it (after the class body, if it's a method) to a separate
 * definition using the inline keyword.  The whole declaration of the method,
 * including type and any trailing const qualifier, should be included as the
 * parameter to DEPRECATED.
 *
 * Note that you can also use the doxygen @c \@deprecated directive in the
 * documentation.
 */
#ifndef DEPRECATED
# define DEPRECATED(X) X
#endif

/** @def COREAPI_DEPRECATED(declaration)
 *
 * Like the DEPRECATED macro, but for use in Core's public API, essentially
 * only in the windowcommander module, to signal deprecated parts of the Core
 * API.  Not for use anywhere else in Core.  Always enabled, unlike the regular
 * deprecated macro which must be enabled by importing API_HC_ENABLE_DEPRECATED.
 */
#ifndef COREAPI_DEPRECATED
# define COREAPI_DEPRECATED(X) X
#endif

/** @def PURE_FUNCTION
 *
 * If a function has no side-effects - i.e. it computes a value and returns it,
 * without modifying anything via a pointer or extern - and its return value
 * depends only on its parameters and/or global variables, then it is described
 * as "pure".  Some compilers may be able to implement extra optimisations for
 * these - notably, omitting extra calls to them when the return value is
 * already known, as in the classically inefficient: @code
 *
 *		for (int i = 0; i < strlen(p); i++)
 *			if (p[i] == sought) return sought;
 *
 * @endcode which asks for a call to strlen once per time round the loop.  (Of
 * course, such code should be fixed, rather than relying on optimizers; but it
 * isn't always as simple as in this case.)
 *
 * If your function is pure, you can make this known to compilers (currently
 * only gcc >= 3) by adding \c PURE_FUNCTION to the end of its declaration
 * (after any const).  A bug in gcc < 3.4 causes problems combining this with an
 * inline body, so you should use a separate inline definition (as for
 * DEPRECATED(), q.v.) if you want to inline the function.
 */
#ifndef PURE_FUNCTION
# define PURE_FUNCTION
#endif

/** @def CHECK_RESULT(declaration)
 *
 * If the result (the return value) of a function should always be checked,
 * use the CHECK_RESULT macro on its declaration. When declaring a function
 * CHECK_RESULT, the compiler may warn if the result of the function is not
 * used. It is commonly used for the return types OP_STATUS and OP_BOOLEAN,
 * to ensure error propagation.
 *
 * The macro should enclose the whole declaration of the function, including
 * type and any trailing const qualifier, but not ";", "{" or "= 0;".
 * Due to a bug in gcc, it is necessary to separate the body from the
 * declaration for stand-alone functions (not for class methods, though),
 * and use the CHECK_RESULT macro only on the declaration.
 */
// gcc needs the macro arg's end (the close-paren) to be at the position
// described above, but the start-point is presently just advisory until
// such time as we find some other compiler with equivalent functionality
// that constrains where the start (open-paren) should be.
#ifndef CHECK_RESULT
# define CHECK_RESULT(X) X
#endif

#endif // MODULES_HARDCORE_BASE_DEPRECATE_H
