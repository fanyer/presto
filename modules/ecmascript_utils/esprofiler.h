/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2002 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef ES_UTILS_ESPROFILER_H
#define ES_UTILS_ESPROFILER_H

#ifdef ESUTILS_PROFILER_SUPPORT

#include "modules/util/adt/opvector.h"
#include "modules/util/opstring.h"
#include "modules/util/simset.h"

class ES_ProfilerImpl;
class ES_Runtime;
class ES_Object;

/* Simple ECMAScript profiler.  Overhead will be fairly high, so timing
   information will not be 100 % accurate. */
class ES_Profiler
{
public:
	static OP_STATUS Make(ES_Profiler *&profiler, ES_Runtime *target);
	/**< Creates a profiler and attaches it to the target runtime.  Any thread
	     started in the runtime after this will be profiled.  Only one profiler
	     can be active at any one time.  Creating another profiler will
	     effectively disable the previous one.  Also, creating a profiler while
	     debugging will disable (in other words, break) the debugger.

	     The created object is owned by the caller and is disabled by deleting
	     it again. */

	~ES_Profiler();
	/**< Destructor.  Disables the debugger. */

	/* Statistics about one thread's execution of code from one script. */
	class ScriptStatistics
	{
	public:
		~ScriptStatistics()
		{
			hits.DeleteAll();
			milliseconds_self.DeleteAll();
			milliseconds_total.DeleteAll();
		}

		OpString description;
		/**< Informal description of the script. */

		OpVector<OpString> lines;
		/**< Source lines.  The string objects are not owned by this object.
		     They are valid only as long as the ES_Profiler object is valid. */
		OpVector<unsigned> hits;
		/**< Number of times each statement of the script was "executed" during
		     the execution of the thread.  Statements spanning multimple lines
		     will be reported as the first line of the statement. */
		OpVector<double> milliseconds_self;
		/**< Number of milliseconds (approximately) spent executing each
		     statement of the script.  Statements spanning multimple lines will
		     be reported as first line of the statement. */
		OpVector<double> milliseconds_total;
		/**< Number of milliseconds (approximately) spent executing each
		     statement of the script.  Statements spanning multimple lines will
		     be reported as first line of the statement. */
	};

	/* Statistics about one thread. */
	class ThreadStatistics
	{
	public:
		OpString description;
		/**< Informal description of the thread. */

		unsigned total_hits;
		/**< Total number of statements executed during the execution of the
		     thread. */
		double total_milliseconds;
		/**< Total number of milliseconds spent executing the thread. */

		OpVector<ScriptStatistics> scripts;
		/**< Statistics about each executed script. */
	};

	/* Profiler listener.  Called with the profiling statistics of each thread
	   executed in the target runtime when as the thread finishes execution. */
	class Listener
	{
	public:
		virtual void OnThreadExecuted(ThreadStatistics *statistics) = 0;
		/**< Called when a thread has been executed in the target runtime.  The
		     statistics object is owned by the listener. */

		virtual void OnTargetDestroyed() {}
		/**< Called when the target runtime is destroyed.  The profiler will
		     automatically stop listening for any more events, but still needs
		     to be destroyed by its owner.

		     It is safe to delete the profiler from this call. */

	protected:
		virtual ~Listener() {}
		/**< Not called by the ES_Profiler object.  The owner of the ES_Profile
		     object is typically also the owner of the listener. */
	};

	void SetListener(Listener *listener);
	/**< Register a listener.  The listener is not owned by the ES_Profiler
	     object. */

private:
	ES_Profiler(ES_ProfilerImpl *impl);
	ES_ProfilerImpl *impl;
};

#endif // ESUTILS_PROFILER_SUPPORT
#endif // ES_UTILS_ESPROFILER_H
