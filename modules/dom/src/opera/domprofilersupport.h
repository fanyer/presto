/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-1999 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef DOM_DOMPROFILERSUPPORT_H
#define DOM_DOMPROFILERSUPPORT_H

#ifdef ESUTILS_PROFILER_SUPPORT

#include "modules/dom/src/domobj.h"
#include "modules/ecmascript_utils/esprofiler.h"

class DOM_ScriptStatistics;

class DOM_ScriptStatistics_lines
	: public DOM_Object
{
private:
	DOM_ScriptStatistics_lines(DOM_ScriptStatistics *parent) : parent(parent) {}

	DOM_ScriptStatistics *parent;

public:
	static OP_STATUS Make(DOM_ScriptStatistics_lines *&lines, DOM_ScriptStatistics *parent);

	virtual ES_GetState GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_GetState GetIndex(int property_index, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_PutState PutIndex(int property_index, ES_Value *value, ES_Runtime *origining_runtime);

	virtual void GCTrace();
};

class DOM_ScriptStatistics_hits
	: public DOM_Object
{
private:
	DOM_ScriptStatistics_hits(DOM_ScriptStatistics *parent) : parent(parent) {}

	DOM_ScriptStatistics *parent;

public:
	static OP_STATUS Make(DOM_ScriptStatistics_hits *&hits, DOM_ScriptStatistics *parent);

	virtual ES_GetState GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_GetState GetIndex(int property_index, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_PutState PutIndex(int property_index, ES_Value *value, ES_Runtime *origining_runtime);

	virtual void GCTrace();
};

class DOM_ScriptStatistics_milliseconds
	: public DOM_Object
{
private:
	DOM_ScriptStatistics_milliseconds(DOM_ScriptStatistics *parent, OpVector<double> *milliseconds) : parent(parent), milliseconds(milliseconds) {}

	DOM_ScriptStatistics *parent;
	OpVector<double> *milliseconds;

public:
	static OP_STATUS Make(DOM_ScriptStatistics_milliseconds *&milliseconds, DOM_ScriptStatistics *parent, OpVector<double> *vector);

	virtual ES_GetState GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_GetState GetIndex(int property_index, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_PutState PutIndex(int property_index, ES_Value *value, ES_Runtime *origining_runtime);

	virtual void GCTrace();
};

class DOM_ThreadStatistics;

class DOM_ScriptStatistics
	: public DOM_Object
{
private:
	DOM_ScriptStatistics(DOM_ThreadStatistics *parent, ES_Profiler::ScriptStatistics *statistics) : parent(parent), statistics(statistics) {}

	DOM_ThreadStatistics *parent;
	DOM_ScriptStatistics_lines *lines;
	DOM_ScriptStatistics_hits *hits;
	DOM_ScriptStatistics_milliseconds *milliseconds_self;
	DOM_ScriptStatistics_milliseconds *milliseconds_total;

	friend class DOM_ScriptStatistics_lines;
	friend class DOM_ScriptStatistics_hits;
	friend class DOM_ScriptStatistics_milliseconds;
	ES_Profiler::ScriptStatistics *statistics;

public:
	static OP_STATUS Make(DOM_ScriptStatistics *&script, DOM_ThreadStatistics *parent, ES_Profiler::ScriptStatistics *statistics);

	virtual ES_GetState GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);

	virtual void GCTrace();
};

class DOM_ThreadStatistics_scripts
	: public DOM_Object
{
private:
	DOM_ThreadStatistics_scripts(DOM_ThreadStatistics *parent) : parent(parent), items(NULL) {}
	virtual ~DOM_ThreadStatistics_scripts() { OP_DELETEA(items); }

	DOM_ThreadStatistics *parent;
	DOM_ScriptStatistics **items;

public:
	static OP_STATUS Make(DOM_ThreadStatistics_scripts *&scripts, DOM_ThreadStatistics *parent);

	virtual ES_GetState GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_GetState GetIndex(int property_index, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_PutState PutIndex(int property_index, ES_Value *value, ES_Runtime *origining_runtime);

	virtual void GCTrace();
};

class DOM_Profiler;

class DOM_ThreadStatistics
	: public DOM_Object
{
private:
	DOM_ThreadStatistics(ES_Profiler::ThreadStatistics *statistics) : statistics(statistics) {}

	friend class DOM_ThreadStatistics_scripts;
	ES_Profiler::ThreadStatistics *statistics;

	DOM_ThreadStatistics_scripts *scripts;

public:
	static OP_STATUS Make(DOM_ThreadStatistics *&thread, DOM_Profiler *parent, ES_Profiler::ThreadStatistics *statistics);

	virtual ES_GetState GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);

	virtual void GCTrace();
};

class DOM_Profiler
	: public DOM_Object,
	  public ES_Profiler::Listener
{
private:
	DOM_Profiler() : onthread(NULL), profiler(NULL) {}

	ES_Object *onthread;
	ES_Profiler *profiler;

public:
	static OP_STATUS Make(DOM_Profiler *&profiler, DOM_Object *reference);

	virtual void Initialise(ES_Runtime *runtime);

	virtual ES_GetState GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);

	virtual void GCTrace();

	virtual void OnThreadExecuted(ES_Profiler::ThreadStatistics *statistics);
	virtual void OnTargetDestroyed();

	DOM_DECLARE_FUNCTION(start);
	DOM_DECLARE_FUNCTION(stop);
};

#endif // ESUTILS_PROFILER_SUPPORT
#endif // DOM_DOMPROFILERSUPPORT_H
