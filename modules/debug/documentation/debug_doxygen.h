/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2007-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

/** \file
 *
 * \brief Doxygen documentation file
 *
 * This file is only used for documenting the debug module through
 * use of 'doxygen'.  It should not contain any code to be compiled,
 * and should therefore never be included by any other file.
 *
 * \author Morten Rolland, mortenro@opera.com
 * \author Markus Johansson, markus@opera.com
 */

/** \mainpage Opera debug module
 *
 * This module provides various macros and functionality useful to help
 * debugging and supervise the code.  Many of the macroes expands
 * to nothing if the relevant function is not enabled (i.e. for
 * release builds).  In debug builds, these macros will provide
 * extra information at run-time to help debug and monitor your code.
 *
 * \section howtouse How to use
 *
 * You will typically make a debugging build by specifying
 * <code>"wingogi - Win32 Debug Desktop"</code> as active project in
 * Visual C++, or by setting <code>DEBUGCODE=YES</code> in \c UNIX
 * makefiles (the exact procedure may vary).
 *
 * Once a debug build has been made, you will need a \c debug.txt file
 * with \ref dbg-debugtxtopt "debugging options" somewhere \c Opera looks for
 * it (this is currently a bit bogus, it looks in current working
 * directory on Linux).
 *
 * \section dbg-macros Macros
 *
 * Macros used to instrument the code:
 *
 * \li \c OP_ASSERT(expression) \n
 * Call the platform-dependent function \c Debug_OpAssert() if
 * \c expression is false. \c Debug_OpAssert() has to
 * be \ref dbg-porting "implemented by the platform".
 * See \ref dbg-opassert for details.
 * Evaluates to nothing in a release compilation.
 *
 * \li \c OP_NEW_DBG(function, key) \n
 * Creates a Debug object on the stack. Will print to system debug or file
 * if tracing is enabled and the given keyword is enabled.
 * Evaluates to nothing in a release compilation.
 *
 * \li \c OP_DBG(args) \n
 * Calls <code>"Debug::Dbg args"</code> on the Debug object created
 * with OP_NEW_DBG(). Note: The arguments must have a double set of
 * parentheses.
 * Example: <code>OP_DBG(("Foobar: rc=%d", rc));</code>
 * Evaluates to nothing in a release compilation.
 *
 * \li \c OP_DBG_LEN(str, len) \n
 * Calls \c DbgLen() on the Debug object created with OP_NEW_DBG().
 * Example: <code>OP_DBG_LEN(UNI_L("Opera"), 2)</code> prints \c "Op".
 * Evaluates to nothing in a release compilation.
 *
 * \b Note: Since these macros are defined to nothing in release builds,
 * it is important not to call functions or evaluate expressions with
 * side effects inside their arguments!  This might cause different
 * behavior in release and debug builds, which is not a good idea.
 *
 * \section dbg-functions Functions
 *
 * The functions below are available whenever the relevant API is
 * imported.  The code using these functions should be protected
 * by the same condition as that used to import the API.
 *
 * \b Important: Don't import any of the APIs below unconditionally,
 * as this would make all builds (including release builds) include
 * debugging code.
 *
 * \li \c dbg_printf() can be used to do simple one-off debugging
 * efforts, or it can be embedded into the code to warn of possible
 * problems.  The output may be sent to several output devices like
 * the system console and the Opera console (currently only the
 * system console is supported).  The \c dbg_printf() function must
 * be imported through the \c API_DBG_PRINTF api.
 *
 * \li \c dbg_systemoutput() can be used to output a single zero
 * terminated uni_char string to the device system console.  You
 * probably want to use \c dbg_printf() with the '%S' format
 * specifier instead.  The \c dbg_systemoutput() function must be
 * imported through the \c API_DBG_SYSTEM_OUTPUT api.
 */

/** \page dbg-debugtxt Format of debug.txt file
 *
 * \section dbg-debugtxtopt Options in debug.txt
 *
 * The \c debug.txt file may contain the following options listed below.
 * All options are default \c off.  Comments can be entered by using \c '#'
 * at the beginning of a line.
 *
 * \li \c output=debug.txt \n
 * Send the debugging output to a specific file.
 *
 * \li \c clearfile=on \n
 * Force opera to truncate the output file on startup.
 *
 * \li \c debugging=on \n
 * Enable debugging.
 *
 * \li \c console=on \n
 * Send debug info to console.
 *
 * \li \c tracing=on \n
 * Trace OP_NEW_DBG() macros.
 *
 * \li\c timing=on \n
 * Collect timing information.
 *
 * \li\c systemdebug=on \n
 * Send the output to the system console, if any is available.
 *
 * \section dbg-debugtxtkeys Using 'keys' to select logging
 *
 * By using 'keys' it is possible to have the debug module
 * only report parts of all the debugging info that is available.
 * This is useful when debugging a certain problem.
 * You specify a 'key' to the \c OP_NEW_DBG() macro, and this key can
 * be listed in the \c debug.txt file, to enable this key for debugging.
 *
 * It is possible to specify '*' (an asterisk) on a line by itself to
 * enable all keys for debugging.
 */


/** \page dbg-porting Porting debug to a new platform
 *
 * Some aspects of the debug module needs to be ported to work well on
 * new platforms.  This may be due to many factors, but typically to
 * output the results somewhere special, or integrate with existing
 * debugging facilities.
 *
 * As of \c anticimex_2_beta_1, the debug module does not contain
 * (much) platform specific code, and the platform itself is
 * responsible for implementing what is needed to make debug function.
 *
 * \section dbg-portingimpl Implementing missing functionality
 *
 * The following methods must be implemented (somewhere) in the
 * platform code:
 *
 * <pre>
 * extern "C"
 * void Debug_OpAssert(const char* expr, const char* file, int line)
 * {
 *   ...
 * }
 *
 * extern "C"
 * void dbg_systemoutput(const uni_char* string)
 * {
 *   ...
 * }
 *
 * void Debug::GetTime(DebugTime &time)
 * {
 *   ...
 * }
 * </pre>
 *
 * This is sufficient if the default declaration of the \c OP_ASSERT
 * macro works for you.
 *
 * \section dbg-portingmacros Tweaking the macros in debug
 *
 * On some platforms, it may not be good enough to use the default
 * declaration of OP_ASSERT and others. In these cases you can
 * create your own header file in the platform somewhere, and force
 * the debug module to use your header file instead of the default
 * declaration by applying the following tweak:
 *
 * \c TWEAK_DEBUG_EXTERNAL_MACROS (from modules/debug/module.tweaks).
 *
 * When applying this tweak, the \c DEBUG_EXTERNAL_MACROS_HEADER macro
 * should contain the pathname of the platform specific header file,
 * e.g. \c platforms/xyz-toaster/toaster_debug_macros.h.
 *
 * Have a look in \c modules/debug/src/debug_macros.h for an example.
 */

/** \page dbg-memorymngmnt Memory management in debug module
 *
 * The memory management in debug is limited.
 *
 * \section dbg-memorystatic Static memory allocations
 *
 * There are some static allocations made in debug. The global opera
 * object contains some arrays for temporary storage of text strings,
 * in addition to ordinary globals like flags and pointers. The
 * various platforms may also allocate static memory when implementing
 * support for the debug module.
 *
 * \section dbg-memorydyn Dynamic memory allocations
 *
 * Upon startup, objects are allocated on the heap to represent the
 * information in the \c debug.txt file. These objects are kept
 * unchanged for the lifetime of the debug module. They will be
 * freed on termination of debug/Opera.
 *
 * These allocations are not OOM safe, but this should not cause
 * problems as the allocations happen at the dawn of time, when memory
 * should be available.
 *
 * The \c OP_NEW_DBG(), \c OP_DBG() and \c OP_DBG_LEN() macros will
 * callocate memory for unicode conversion purposes.  If these allocations
 * fail, the reporting may be incomplete, e.g. missing or incomplete output.
 *
 * \section dbg-memorystk Stack allocations
 *
 * The debugging macro OP_NEW_DBG(function, key) will allocate a 20
 * byte object (approximately) on the stack.
 */

/** \page dbg-opassert Verifying assumptions with OP_ASSERT
 *
 * The OP_ASSERT macro takes one parameter of boolean type. In debug
 * builds, this parameter is evaluated: if true, the macro does
 * nothing else. If false, it calls the \c Debug_OpAssert function
 * (which you can break on in a debugger). This generally prints a
 * warning and (platform-dependently) may abort or offer you a choice
 * of what to do next.
 *
 * \b Important: Don't use expressions with side effects in the
 * OP_ASSERT macro.  This is especially true for function calls
 * (unless you are certain they have, and will allways have,
 * no side effects).  The reason for this is that the expression
 * will be evaluated in debug builds, but not in release builds,
 * causing possibly different behaviour and hard to find bugs (the
 * bug goes away in debugging builds).
 *
 * You should only include assertions in your code where the condition
 * being asserted can only be false if there is a bug in the
 * code. Flaws in data, whether from the user or a remote web-site,
 * should be handled gracefully; these and other problems outside
 * Opera's control may sensibly be reported via OP_DBG, but should not
 * be the subject of assertions.
 *
 * \b Note: According to coding standards, any assertion failure
 * is to be considered a bug and reported in BTS.
 *
 * Despite this fact, it is a good idea to include as many \c
 * OP_ASSERT as you see fit to cover your code with
 * pre/post-conditioning.  Any assert happening at some later point
 * will force a rethinking of the logic, and flaws can be found at an
 * earlier stage.
 *
 * \section dbg-assertgood Examples of good asserts
 *
 * <pre>
 * OP_ASSERT(x >= 0);
 * OP_ASSERT(!"Should never get here");
 * OP_ASSERT(g_foobar_enabled && foobar_init_ok);
 * OP_ASSERT(!strcmp("not found", result));
 * </pre>
 *
 * \section dbg-assertbad Examples of bad asserts
 *
 * <pre>
 * OP_ASSERT(x++ >= 0);
 * OP_ASSERT(OpStatus::IsSuccess(OpTrimMemory()));
 * OP_ASSERT(this != 0);
 *
 * char* foo = new char[num_chars];
 * OP_ASSERT(foo != 0);  // Fix OOM later (not a good idea)
 * </pre>
 */
