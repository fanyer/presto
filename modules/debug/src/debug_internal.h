/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

/** \file
 *
 * \brief Declare (most) functionality of the Debug class.
 *
 * Some of the methods declared here must be implemented
 * by the platform somewhere else.
 */

#ifndef DEBUG_INTERNAL_H
#define DEBUG_INTERNAL_H

#ifdef DEBUG_EXTERNAL_MACROS_HEADER
#include DEBUG_EXTERNAL_MACROS_HEADER
#endif

#ifdef DEBUG_ENABLE_PRINTF
/**
 * \brief Formatted output function for debugging purposes
 *
 * This function will perform a very simplistic 'printf' kind
 * of formatting and output the result to the relevant system console
 * and the opera log-file.
 *
 * The following items can be formatted and printed:
 *
 *  \%[0(n)]d - signed decimal (int) (with optional zero-padding)<br>
 *  \%u - unsigned decimal (unsigned int)<br>
 *  \%x - unsigned hexadecimal <br>
 *  \%z - size_t decimal <br>
 *  \%p - pointer <br>
 *  \%c - a single character (16-bit unicode values supported) <br>
 *  \%s - a zero terminated C-string in ISO 8859-1 encoding <br>
 *  \%S - a zero terminated 16-bit unicode string (uni_char*) <br>
 *  \%.*s - Like \%s but give max size as argument befor string pointer <br>
 *  \%\% - print a single '\%' <br>
 *
 * \param format Format string
 */
extern "C" void dbg_printf(const char* format, ...);

/**
 * \brief A varargs version of dbg_printf
 *
 * \see dbg_printf for details on the format string.
 *
 * \param format Format string
 * \param args The varargs list iterator
 */
extern "C" void dbg_vprintf(const char* format, va_list args);

/**
 * \brief Formating output function for debug logging
 *
 * This function performs the exact same operation as \c dbg_printf()
 * but only outputs to the opera log-file.  This function is typically
 * used for larger volumes of data, while dbg_printf() are used for
 * alerts and statistics.
 *
 * \param format Format string with same features as for dbg_printf()
 */
extern "C" void log_printf(const char* format, ...);

/**
 * \brief Output a timestamp
 *
 */

extern "C" void dbg_print_timestamp(void);

#endif // DEBUG_ENABLE_PRINTF


#ifdef DEBUG_ENABLE_SYSTEM_OUTPUT
/**
 * \brief Output a unicode string to the device system console
 *
 * This function needs to be implemented by the platform.
 * It should output to the screen, a debugging console, file
 * or other suitable place.
 *
 * If the platform does not have a suitable output mechanism, the
 * function should still be implemented but does nothing.
 *
 * \param txt The 16-bit unicode string (zero terminated) to print
 */
extern "C" void dbg_systemoutput(const uni_char* txt);

/**
 * \brief Output a utf8 string to the opera log-file
 *
 * This function must be implemented on the platform, possibly as a
 * stub.  The expected behaviour is that the platform will open the
 * logfile at some point, and the debug module will call this function
 * to record data in the file.  The contents written to the file
 * should be a byte-wise copy of the data provided by the debug
 * module.  The debug module will do the utf8 encoding, and this
 * should not be changed, to make all log-files compatible.  This same
 * goes for the newline character '\\n', which should preferably not be
 * rewritten into '\\r\\n'.
 *
 * The platform may decide to implement a write buffer for this
 * function, to avoid writing many small pieces to the logfile, or to
 * buffer data until some necessary information is available, like
 * the name of the logfile.
 *
 * The debug module will call this function once with a zero length
 * parameter during its initialization; the platform may opt to do
 * lazy initialization at this point, if everything else is ready.
 */
extern "C" void dbg_logfileoutput(const char* txt, int len);

#endif // DEBUG_ENABLE_SYSTEM_OUTPUT

#ifndef DEBUG_ENABLE_OPASSERT
# undef OP_ASSERT
# define OP_ASSERT(expr) ((void)0)

# undef OP_ASSERT_MSG
# define OP_ASSERT_MSG(expr, args) ((void)0)

# undef OP_ASSERT_STATUS
# define OP_ASSERT_STATUS(expr) ((void)0)

# undef OP_ASSERT_STATUS_FN
# define OP_ASSERT_STATUS_FN(expr, fn) ((void)0)
#endif

#ifdef _DEBUG

#ifndef OP_NEW_DBG
#  define OP_NEW_DBG(function, key)   Debug op_dbg_object(function, key)
#endif

#ifndef OP_DBG
/**
 * The macro OP_DBG() can be used to add debug messages to a debug
 * file and/or to the console. The argument is a format string (as
 * uni_char* or as char*) with parameter values for the format
 * enclosed by parentheses and optionally followed by left-shift
 * operators to add further content to the debug-line.
 * \code
 *   OP_DBG((UNI_L("one int: %d; another int: "), 1) << 2);
 *   OP_DBG(("") << "one int: " << 1 << " another int: " << 2);
 * \endcode
 * Both examples create the same output.
 */
#  define OP_DBG(args)   op_dbg_object.Dbg args << "\n"
#endif

#ifndef OP_DBG_LEN
#  define OP_DBG_LEN(str, len)   op_dbg_object.DbgLen(str, len)
#endif

#else // _DEBUG

#define OP_NEW_DBG(function, key)		((void)0)
#define OP_DBG(args)					((void)0)
#define OP_DBG_LEN(str, len)			((void)0)

#endif // _DEBUG


#if defined(_DEBUG)

#include "modules/hardcore/base/op_types.h"
#include "modules/hardcore/base/opstatus.h"
#include "modules/hardcore/unicode/unicode.h"

// This define is needed both with and without a global opera object
#define DEBUG_DEBUGBUFFERSIZE 65536

class OpFile;

class DebugHead;

class DebugLink
{
public:
	DebugLink(void) : suc(0), pred(0), parent(0) {}
	virtual ~DebugLink(void) {}

	DebugLink*			Suc() const { return suc; }
    DebugLink*			Pred() const { return pred; }
    void				Out();
    void				Into(DebugHead* list);

private:
    DebugLink*			suc;
    DebugLink*			pred;
	DebugHead*			parent;
};

class DebugHead
{
public:
	friend class DebugLink;

	DebugHead() : first(0), last(0) {}
	~DebugHead(void);

    DebugLink*			First() const { return first; }

private:

    DebugLink*			first;
    DebugLink*			last;
};

class StringLink : public DebugLink
{
public:
	StringLink(const uni_char* str, DebugHead* list);			
	virtual ~StringLink();

	uni_char* data;

private:
	StringLink(const StringLink&);
	StringLink& operator =(const StringLink&);
};

struct DebugTime
{
	UINT32 sec;
	UINT16 msec;
};

class TimeStringLink : public StringLink
{
public:
	TimeStringLink(DebugTime t, const uni_char* s, DebugHead* list) : StringLink(s, list), time(t) {};

	DebugTime time;

private:
	TimeStringLink(const TimeStringLink&);
	TimeStringLink& operator =(const TimeStringLink&);
};

class OpPoint;
class OpRect;

class Debug
{
public:

	/**
	 * Creates a new Debug-object for the specified function.
	 * Timing of this function will be disabled.
	 * @param function Function name where the debug object is created.
	 * @param key Keyword used to control debugging and tracing.
	 */
	Debug(const char* function, const char* key);

    /**
	 * Destroys the Debug object.
	 * This is normally used when leaving a function and means that tracing is
	 * printed.
	 */
    ~Debug();

	/**
	 * This operator adds the specified uni_char string to the current
	 * debug line. The operator can be used inside an OP_DBG() macro:
	 * \code
	 *   OP_DBG((UNI_L("")) << UNI_L("some unicode text"));
	 * \endcode
	 * @param text is the uni_char string to add to the current debug
	 *  line.
	 * @return the current debug instance, which can be used by the
	 *  next operator.
	 */
	Debug& operator<<(const uni_char* text);

	/**
	 * This operator adds the specified char string to the current
	 * debug line. The operator can be used inside an OP_DBG() macro:
	 * \code
	 *   OP_DBG((UNI_L("")) << "some latin-1 text");
	 * \endcode
	 * @param text is the char string to add to the current debug line.
	 * @return the current debug instance, which can be used by the
	 *  next operator.
	 */
	inline Debug& operator<<(const char* text) { return AddDbg("%s", text); }

	/**
	 * This operator adds the specified number as a decimal string to
	 * the current debug line. The operator can be used inside an
	 * OP_DBG() macro:
	 * \code
	 *   OP_DBG((UNI_L("int: ")) << -18);
	 *   // prints "int: -18"
	 * \endcode
	 * @param number is the number to add to the current debug line.
	 * @return the current debug instance, which can be used by the
	 *  next operator.
	 */
	Debug& operator<<(int number);

	/**
	 * This operator adds the specified unsigned number as a decimal
	 * string to the current debug line. The operator can be used
	 * inside an OP_DBG() macro:
	 * \code
	 *   OP_DBG((UNI_L("unsigned: ")) << 23);
	 *   // prints "unsigned: 23"
	 * \endcode
	 * @param number is the number to add to the current debug line.
	 * @return the current debug instance, which can be used by the
	 *  next operator.
	 */
	Debug& operator<<(unsigned int number);

	/**
	 * This operator adds the specified number as a decimal string to
	 * the current debug line. The operator can be used inside an
	 * OP_DBG() macro:
	 * \code
	 *   OP_DBG((UNI_L("long: ")) << (long)-18);
	 *   // prints "long: -18"
	 * \endcode
	 * @param number is the number to add to the current debug line.
	 * @return the current debug instance, which can be used by the
	 *  next operator.
	 */
	Debug& operator<<(long number);

	/**
	 * This operator adds the specified unsigned number as a decimal
	 * string to the current debug line. The operator can be used
	 * inside an OP_DBG() macro:
	 * \code
	 *   OP_DBG((UNI_L("unsigned long: ")) << (unsigned long)23);
	 *   // prints "unsigned long: 23"
	 * \endcode
	 * @param number is the number to add to the current debug line.
	 * @return the current debug instance, which can be used by the
	 *  next operator.
	 */
	Debug& operator<<(unsigned long number);

	/**
	 * This operator adds the specified floating point number as a
	 * decimal string in the style [-]ddd.dddddd with precision 6 to
	 * the current debug line. The operator can be used inside an
	 * OP_DBG() macro:
	 * \code
	 *   OP_DBG((UNI_L("double: ")) << 17.349);
	 *   // prints "double: 17.349000"
	 * \endcode
	 * @param number is the number to add to the current debug line.
	 * @return the current debug instance, which can be used by the
	 *  next operator.
	 */
	Debug& operator<<(double number);

	/**
	 * This operator adds the hex address of the specified pointer as
	 * a string to the current debug line. The operator can be used
	 * inside an OP_DBG() macro:
	 * \code
	 *   void* pointer = ...;
	 *   OP_DBG((UNI_L("pointer: ")) << pointer);
	 *   // prints e.g. "pointer: 0xc089be8" (or whatever the address is)
	 * \endcode
	 * @param pointer is the pointer for which the address is added to
	 *  the current debug line.
	 * @return the current debug instance, which can be used by the
	 *  next operator.
	 */
	Debug& operator<<(const void* pointer);

	/**
	 * This operator adds the specified OpPoint in the format "(X,Y)"
	 * to the current debug line. The operator can be used inside an
	 * OP_DBG() macro:
	 * @code
	 *   OpPoint point(10, 263);
	 *   OP_DBG(("point: ") << point);
	 *   // prints "point: (10,263)"
	 * @endcode
	 * @param p is the point to print to the current debug line.
	 * @return the current debug instance, which can be used by the
	 *  next operator.
	 */
	Debug& operator<<(const OpPoint& p);

	/**
	 * This operator adds the specified OpRect in the format "(X,Y)+WxH"
	 * to the current debug line. The operator can be used inside an
	 * OP_DBG() macro:
	 * @code
	 *   OpRect rect(8, 3, 400, 263);
	 *   OP_DBG(("rectangle: ") << rect);
	 *   // prints "rectangle: (8,3)+400x263"
	 * @endcode
	 * @param r is the rectangle to print to the current debug line.
	 * @return the current debug instance, which can be used by the
	 *  next operator.
	 */
	Debug& operator<<(const OpRect& r);

	/**
	 * Prints a string using printf()-style.
	 * Will be printed like this for a function named func,
	 * a key named core and the string to print is "printing":
	 *
	 * core:func: printing
	 *
	 * Debugging must be enabled for the keyword given in the
	 * Debug-constructor.
	 * @param format printf()-style-format.
	 * @return the current debug instance, which can be used by any of
	 *  the left shift operators.
	 */
	/** @{ */
	Debug& Dbg(const uni_char* format, ...);
	Debug& Dbg(const char* format, ...);
	/** @} */

	/**
	 * Prints a string of given length.
	 * Will be printed like this for a function named func,
	 * a key named core and the string to print is "printing":
	 *
	 * core:func: printing
	 *
	 * Debugging must be enabled for the keyword given in the
	 * Debug-constructor.
	 * @param s String to print.
	 * @param len Length of string to print.
	 */
	void DbgLen(const uni_char* s, int len);

	/**
	 * Prints a string with Dbg() if the given expression
	 * evaluates to TRUE.
	 * @param expression specifies whether or not to print the
	 *  formatted debug message.
	 * @param format specifies the format of the debug message.
	 * @return the current debug instance, which can be used by any of
	 *  the left shift operators.
	 */
	/** @{ */
	Debug& DbgIf(BOOL expression, const uni_char* format, ...);
	Debug& DbgIf(BOOL expression, const char* format, ...);
	/** @} */

	/**
	 * Starts a timer for the given keyword. If a timer with
	 * that keyword is already started, the timer is restarted.
	 * @param key Keyword which determines if timing info should be printed.
	 * @param s Additional string to print.
	 */
	/** @{ */
    static void TimeStart(const uni_char* key, const uni_char* s);
	static void TimeStart(const char* key, const char* s);
	/** @} */

	/**
	 * Stops an active timer. Will do nothing if the timer is not started.
	 * @param key Keyword which determines if timing info should be printed.
	 * @param s Additional string to print.
	 */
	/** @{ */
    static void TimeEnd(const uni_char* key,  const uni_char* s);
	static void TimeEnd(const char* key, const char* s);
	/** @} */

	/**
	 * Flushes all output streams.
	 */
    static void Sync();

	/**
	 * Initializes debug settings, loading them from a file.
	 * The file should look something like this:
	 *
	 * output=debug.txt
	 * clearfile=on
	 * debugging=on
	 * core
	 * mail
	 * \#main
	 * tracing=on
	 * timing=off
	 * core
	 * systemdebug=off
	 *
	 * The example-settings above means that debugging output
	 * is enabled for the keywords core and mail. Tracing is
	 * enabled for all keywords and timing is disabled for all
	 * keywords. No debug output will be written to the system
	 * debug output. Output will be written to the file debug.txt.
	 * clearfile=on means that the output file will be cleared
	 * every time Opera is being debugged so that the file will
	 * contain debug info for the last session only when Opera
	 * is terminated.
	 * If no output is to be written simply remove or comment out
	 * the output-line. Comments are lines beginning with #.
	 * By default, the output is prefixed with key and function
	 * name. This can be disabled with 'prefix=off'
	 *
	 * @param filename File containing settings.
	 * @return NULL on success, or error message allocated with new [].
	 */
	static uni_char* InitSettings(const char* filename);

	static void Free();
//private:

	/**
	 * Internal Dbg-function which takes va_list instead of "..."
	 */
	/** @{ */
	void VDbg(const uni_char* format, va_list arglist);
	void VDbg(const char* format, va_list arglist);
	/** @} */

	/**
	 * Enables or disables debugging output
	 * This function applies to all Debug-classes.
	 * @param value <code>TRUE</code> enables debugging and
	 *              <code>FALSE</code> disables debugging
	 */
	static void EnableDebugging(BOOL value);

	/**
	 * Enables or disables function tracing
	 * This function applies to all Debug-classes.
	 * @param value <code>TRUE</code> enables tracing and
	 *              <code>FALSE</code> disables tracing
	 */
	static void EnableTracing(BOOL value);

	/**
	 * Enables or disables interval timing.
     * This function applies to all Debug-classes.
     * @param value <code>TRUE</code> enables timing and
     *              <code>FALSE</code> disables timing
     */
	static void EnableTiming(BOOL value);

	/**
     * Enables or disables system debug output.
     * Currently there is only support for such output
     * in Windows. When enabled the same list of
     * keywords as debugging uses will be used.
     * This function applies to all Debug-classes.
     * @param value <code>TRUE</code> enables timing and
     *              <code>FALSE</code> disables timing
     */
	static void EnableSystemDebugOutput(BOOL value);

	/**
	 * Adds a keyword to the list of keywords being accepted
	 * when printing debug output.
     * @param keyword A keyword to add.
     */
	static StringLink* AddKeyword(const uni_char* keyword);

	/**
	 * Checks if debugging is enabled for a given keyword.
	 * @param keyword keyword.
	 * @return <code>TRUE</code> if debugging is enabled.
	 */
    static BOOL DoDebugging(const uni_char* keyword);
	static BOOL DoDebugging(const char* keyword);

    /**
	 * Indents the output line, including timestamp if enabled.
	 */
    static void Indent();

	/**
	 * Emits a timestamp if timestamps are enabled, otherwise does nothing.
	 *
	 * To be run before each new line starts (included in Indent so if Indent is
	 * called this doesn't have to be called).
	 */
	static void Timestamp();

	/**
     * Gets us into a function with proper indents,
	 * printing the name of the function.
	 */
    void Enter();

	/**
	 * Gets us out of a function with proper indents.
	 */
    void Exit();

	/**
	 * Returns a string for printing. Contains time used
	 * in function- and interval timing.
	 */
    const uni_char* Time_stuff();

private:

	/**
	 * Makes sure Debug has no public default constructor.
	 */
	Debug();

	/**
	 * Makes sure Debug has no public copy constructor.
	 */
    Debug(const Debug &);
	Debug& operator =(const Debug&);

	/**
	 * \brief Utility function to indent some output.
	 *
	 * The buffer provided will be filled with two spaces for each
	 * indentation level, but indentation will stop early so that the
	 * terminating NUL character will always fit inside the buffer as
	 * specified by the buffer size.
	 *
	 * \param buf A buffer where the indentation goes
	 * \param level Level of indentation, 1=No indentation, 2=one level etc.
	 * \param bufsize Buffer size
	 * \returns The number of characters in the buffer
	 */
	static int mkindent(uni_char* buf, int level, int bufsize);

public:

	/**
	 * Sets the output file to use when printing debug output.
	 * @param filename File name for new output file.
	 */
	static void SetOutputFile(const uni_char* filename);

	/**
	 * Closes previously opened file and opens the new one.
	 */
    static void SetupOutput();

	/**
	 * Prints a string to the system debug output.
	 * In Visual Studio the string will be printed to
	 * the Debug-tab.
	 *
	 * \deprecated This method is deprecated in favor of
	 * \c dbg_systemoutput() which has C-linkage and does not
	 * depend on the full \c Debug class being available.
	 *
	 * @param str String to be printed.
	 */
	DEPRECATED(static void SystemDebugOutput(const uni_char* str));

#ifdef OPERA_CONSOLE
	/**
	 * Prints a string to the opera console (which may in
	 * turn be logged remotely by the scope infrastructure).
	 * @param str String to be printed.
	 */
	static void ConsoleOutput(const uni_char* str);
#endif

	/**
	 * Finds a string in a linked list of StringLinks.
	 * @param list The linked list.
	 * @param s String to find.
	 * @return The found item or NULL if none was found.
	 */
	static DebugLink* Find(DebugHead* list, const uni_char* s);
	static DebugLink* Find(DebugHead* list, const char* s);

	/**
	 * Prints a string to a file.
	 * @param str String to print.
	 */
	static void PrintToFile(const uni_char* str);

	/**
	 * Reads a line from the given input file.
     * In UNICODE the file is read in UTF-8 format.
	 * @param file File to read from.
	 * @param str The read string.
	 * @param len Length of buffer
	 * @return TRUE if a line has been read.
	 *         FALSE if the end of file has been reached.
	 */
	static BOOL ReadLine(OpFile* file, uni_char* str, int len);

public:
	/**
	 * Adds the specified format string with the specified argument
	 * list to the current debug line.
	 * @param format printf()-style-format.
	 * @return the debug object.
	 */
	/** @{ */
	Debug& AddDbg(const uni_char* format, ...);
	Debug& AddDbg(const char* format, ...);
	/** @} */

private:
	/**
	 * Prints the global debug buffer g_dbg_mybuff to the debug file
	 * and/or to the console depending on which debug output is
	 * enabled. This method should be called by all debug methods,
	 * that prepare a debug string in g_dbg_mybuff.
	 */
	static void FlushDbgBuffer();

	/**
	 * Gets the current system time.
	 * @param time Out parameter with the current time.
	 */
	static void GetTime(DebugTime &time);

    // Object data
	uni_char* function_name;
	uni_char* debugging_key;
    struct DebugTime Start;

	BOOL do_debugging;
};

// #define USE_TIMING

class DebugTimer
{
public:
	enum module_t
	{
		FORMAT,
		IMAGE,
		FONT,
		PARSING,
		OOM,
		N_MODULES	// NOTE: Must end with this
	};

#ifndef USE_TIMING

	DebugTimer(module_t) {}

#else

	DebugTimer(module_t mod);
	~DebugTimer();

	static const char* GetSummary();

private:

	typedef long millisecs_t;

	typedef unsigned int timers_t;

	static millisecs_t GetTime();

	struct timerdata_t
	{
		millisecs_t start;
		millisecs_t total;
		timers_t    timers;
	};

	module_t m_module;

	static timerdata_t g_timerdata[N_MODULES];

#endif // USE_TIMING

};

#endif // _DEBUG

#endif //  DEBUG_INTERNAL_H
