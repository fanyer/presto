/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2006-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Anders Oredsson
*/

#include "core/pch.h"

#ifdef SELFTEST

#include "modules/dom/domenvironment.h"
#include "modules/dom/domutils.h"
#include "modules/ecmascript/ecmascript.h"
#include "modules/ecmascript_utils/esasyncif.h"
#include "modules/selftest/optestsuite.h"
#include "modules/selftest/operaselftestdispatcher.h"
#include "modules/scope/scope_selftest_listener.h"


int
OperaSelftestDispatcher::_es_selftestRun(ES_Value *argv, int argc, ES_Value* /*return_value*/, DOM_Environment::CallbackSecurityInfo *security_info)
{
	
	//Register that the dispatcher is running!
	g_is_selftest_dispatcher_running = true;
	
	//Make sure call is done from opera:selftest
	if (security_info->type != URL_OPERA)
		return ES_EXCEPT_SECURITY;

	BOOL oom = FALSE;

	if (argc >= 3 && argv[0].type == VALUE_OBJECT && argv[1].type == VALUE_OBJECT)
	{
		int sargc = 1, sargc_copy;
		char *sargv[16], *sargv_copy[16];
		char zero_chr = 0;
		ES_Object* o;
		int index;

		sargv[0] = &zero_chr;

		for (index = 2; index < argc; ++index)
			if (argv[index].type == VALUE_STRING)
			{
				sargv[sargc] = uni_down_strdup(argv[index].value.string);
				if (!sargv[sargc])
				{
					oom = TRUE;
					break;
				}
				else if (++sargc == 16)
					break;
			}

		if (!oom)
		{
			if (!g_selftest_window.GetObject())
			{
				o = argv[0].value.object;

				DOM_Environment *dom_env = DOM_Utils::GetDOM_Environment(DOM_Utils::GetDOM_Object(o));
				if (!dom_env || OpStatus::IsError(g_selftest_window.Protect(dom_env->GetRuntime(), o)))
					oom = TRUE;
			}

			if (!oom && !g_selftest_callback.GetObject())
			{
				o = argv[1].value.object;

				DOM_Environment *dom_env = DOM_Utils::GetDOM_Environment(DOM_Utils::GetDOM_Object(g_selftest_window.GetObject()));
				if (!dom_env || OpStatus::IsError(g_selftest_callback.Protect(dom_env->GetRuntime(), o)))
					oom = TRUE;
			}

			if (!oom)
			{
				sargc_copy = sargc;
				op_memcpy(sargv_copy, sargv, sargc * sizeof sargv[0]);

				SELFTEST_MAIN(&sargc_copy, sargv_copy);
				SELFTEST_RUN(1);
			}
		}

		for (index = sargc - 1; index > 0; --index)
			op_free(sargv[index]);
	}

	return oom ? ES_NO_MEMORY : ES_FAILED;
};

void 
OperaSelftestDispatcher::_scope_selftestRun(const char* modules, BOOL spartan_readable)
{
	g_selftests_running_from_scope = TRUE;

	//no need to run on empty module list
	OP_ASSERT(modules != 0);
	OP_ASSERT(*modules != 0);

	//Register that the dispatcher is running!
	g_is_selftest_dispatcher_running = true;

	//create valid argument from module list: -test-module=util,url,
	const char* moduleArgPrefix = "-test-module=";

	char *moduleArg = OP_NEWA(char,op_strlen(moduleArgPrefix) + op_strlen(modules) + 1);
	if(moduleArg != 0)
	{
		
		op_memcpy(moduleArg, moduleArgPrefix, op_strlen(moduleArgPrefix));
		op_memcpy(&moduleArg[op_strlen(moduleArgPrefix)], modules, op_strlen(modules)+1);

		//setup arguments
		int sargc = spartan_readable ? 3 : 2;
		char *empty = const_cast<char*>("");
		char *test_spartan_readable = const_cast<char*>(spartan_readable ? "-test-spartan-readable" : NULL);
		char *sargv[16] = { empty, moduleArg, test_spartan_readable, NULL };
		//todo: copy "modules" parameter!

		//run selftests
		SELFTEST_MAIN(&sargc, sargv);
		SELFTEST_RUN(1);

		//clean up!
		OP_DELETEA(moduleArg);
	}
}

int
OperaSelftestDispatcher::_es_selftestReadOutput(ES_Value *argv, int argc, ES_Value* return_value, DOM_Environment::CallbackSecurityInfo *security_info)
{
	//Make sure call is done from opera:selftest
	if (security_info->type != URL_OPERA)
		return ES_EXCEPT_SECURITY;

	return_value->type = VALUE_STRING;

	if (g_selftest_output_buffer_used > 0)
	{
		g_selftest_output_buffer[g_selftest_output_buffer_used] = 0;
		g_selftest_output_buffer_used = 0;
		return_value->value.string = g_selftest_output_buffer;
	}
	else
		return_value->value.string = UNI_L("");

	g_selftest_output_cb_called = FALSE;
	return ES_VALUE;
}

size_t
OperaSelftestDispatcher::_scope_selftestReadOutput(uni_char* output_buffer, size_t output_buffer_max_size)
{
	//output_buffer_max_size = number of uni_chars
	//returns number of uni_chars written
	if(g_selftests_running_from_scope && g_selftest_output_buffer_used > 0)
	{
		//remove X first bytes of g_selftest_output_buffer, rest of buffer should be left
		unsigned bytes_to_read =  g_selftest_output_buffer_used;
		if(bytes_to_read > output_buffer_max_size)
		{
			bytes_to_read = output_buffer_max_size;
		}

		//copy data to output buffer
		if(bytes_to_read > 0)
		{
			op_memcpy(output_buffer, g_selftest_output_buffer, sizeof(uni_char) * bytes_to_read );
		}

		//do we need to move data in the buffer?
		if(bytes_to_read < g_selftest_output_buffer_used)
		{
			size_t rest_in_byffer = g_selftest_output_buffer_used - bytes_to_read;
			op_memmove(g_selftest_output_buffer, &g_selftest_output_buffer[bytes_to_read], sizeof(uni_char) * rest_in_byffer );
		}

		//reduce 'used' size
		g_selftest_output_buffer_used -= bytes_to_read;
		return bytes_to_read;
	}
	else
		return 0;
}

BOOL
OperaSelftestDispatcher::_selftestWriteOutput(const char* str)
{
	if(!g_is_selftest_dispatcher_running)
		return FALSE;

	// double buffer size if needed
	unsigned str_length = op_strlen(str);
	if (g_selftest_output_buffer_allocated < g_selftest_output_buffer_used + str_length)
	{
		unsigned new_allocated = g_selftest_output_buffer_allocated ? g_selftest_output_buffer_allocated : 1024;

		while (new_allocated < g_selftest_output_buffer_used + str_length)
			new_allocated += new_allocated;

		uni_char *new_buffer = OP_NEWA(uni_char, new_allocated + 1);//(uni_char*)op_malloc(sizeof(uni_char) * (new_allocated + 1)); //new uni_char[new_allocated + 1];

		if (!new_buffer)
			return TRUE;
		op_memcpy(new_buffer, g_selftest_output_buffer, g_selftest_output_buffer_used * sizeof(uni_char));

		OP_DELETEA(g_selftest_output_buffer); //delete[] g_selftest_output_buffer;

		g_selftest_output_buffer = new_buffer;
		g_selftest_output_buffer_allocated = new_allocated;
	}

	// copy into buffer
	const char *ptr8 = str + str_length;
	uni_char *stop16 = g_selftest_output_buffer + g_selftest_output_buffer_used, *ptr16 = stop16 + str_length;

	while (ptr16 != stop16)
		*--ptr16 = *--ptr8;

	g_selftest_output_buffer_used += str_length;

	//do DOM callbacks if we are connected to a opera:selftest window
	if(_isSelftestConnectedToWindow())
	{
		if (!g_selftest_output_cb_called)
		{
			DOM_Environment* dom_env = g_selftest_window.GetObject() ? DOM_Utils::GetDOM_Environment(DOM_Utils::GetDOM_Object(g_selftest_window.GetObject())) : NULL;
			ES_AsyncInterface* ai = dom_env ? dom_env->GetAsyncInterface() : NULL;

			if (!ai)
			{
				return FALSE;
			}

			ES_Value argv[1];
			argv[0].type = VALUE_BOOLEAN;
			argv[0].value.boolean = FALSE;
			ai->CallFunction(g_selftest_callback.GetObject(), NULL, 1, argv);
			g_selftest_output_cb_called = TRUE;
		}
	}

	//tell scope that we received data from running selftests
	if(g_selftests_running_from_scope)
	{
		//pull some data
		uni_char scopeBuffer[1024]; // ARRAY OK 2010-08-10 anderso
		size_t bytesRead = _scope_selftestReadOutput(scopeBuffer, 1023);
		scopeBuffer[bytesRead] = 0; //0-terminate anything we want to send
		if(bytesRead > 0)
		{
			//call scope!
			if(OpStatus::IsError(OpScopeSelftestListener::OnSelftestOutput(scopeBuffer)))
			{
				return FALSE;
			}
		}
	}

	return TRUE;
};

void
OperaSelftestDispatcher::_selftestFinishOutput()
{
	if (g_selftest_window.GetObject())
	{
		DOM_Environment* dom_env = DOM_Utils::GetDOM_Environment(DOM_Utils::GetDOM_Object(g_selftest_window.GetObject()));
		ES_AsyncInterface* ai = dom_env ? dom_env->GetAsyncInterface() : NULL;
		if (ai)
		{
			ES_Value argv[1];
			argv[0].type = VALUE_BOOLEAN;
			argv[0].value.boolean = TRUE;
			ai->CallFunction(g_selftest_callback.GetObject(), NULL, 1, argv);
		}

		g_selftest_window.Unprotect();
		g_selftest_callback.Unprotect();
	}

	//tell scope that we finished running selftests
	if(g_selftests_running_from_scope)
	{
		//call scope!
		OpScopeSelftestListener::OnSelftestFinished();
		g_selftests_running_from_scope = FALSE;
	}
};

BOOL
OperaSelftestDispatcher::_isSelftestDispatcherRunning()
{
	return g_is_selftest_dispatcher_running;
}

BOOL
OperaSelftestDispatcher::_isSelftestConnectedToWindow()
{
	return g_selftest_window.GetObject() != NULL;
}

BOOL
OperaSelftestDispatcher::_isSelftestConnectedToScope()
{
	return g_selftests_running_from_scope;
}

/*static*/ int
OperaSelftestDispatcher::es_selftestRun(ES_Value *argv, int argc, ES_Value* return_value, DOM_Environment::CallbackSecurityInfo *security_info)
{
	return g_selftest.selftestDispatcher->_es_selftestRun(argv, argc, return_value, security_info);
};

/*static*/ int
OperaSelftestDispatcher::es_selftestReadOutput(ES_Value *argv, int argc, ES_Value* return_value, DOM_Environment::CallbackSecurityInfo *security_info)
{
	return g_selftest.selftestDispatcher->_es_selftestReadOutput(argv, argc, return_value, security_info);
};

OperaSelftestDispatcher::OperaSelftestDispatcher() :
	g_is_selftest_dispatcher_running(FALSE),
	g_selftest_output_buffer(NULL),
	g_selftest_output_buffer_used(0),
	g_selftest_output_buffer_allocated(0),
	g_selftest_output_cb_called(FALSE),
	g_selftests_running_from_scope(FALSE)
{
}

OperaSelftestDispatcher::~OperaSelftestDispatcher()
{
	if(g_selftest_output_buffer != 0)
	{
		OP_DELETEA(g_selftest_output_buffer); //delete [] g_selftest_output_buffer;
		g_selftest_output_buffer = 0;
	}
}

void OperaSelftestDispatcher::Reset()
{
	//Do not delete buffers, since there could be a
	//Dalayed callback comming to empty the rest of the output.
	//
	//This is actually happening, so they will be emptied by
	//that callback, or at exit!

	g_is_selftest_dispatcher_running = FALSE;
	g_selftests_running_from_scope = FALSE;
}

void
OperaSelftestDispatcher::registerSelftestCallbacks()
{
	DOM_Environment::AddCallback(es_selftestRun, DOM_Environment::OPERA_CALLBACK, "selftestrun", "--s");
	DOM_Environment::AddCallback(es_selftestReadOutput, DOM_Environment::OPERA_CALLBACK, "selftestoutput", NULL);
}

/*static*/ BOOL
OperaSelftestDispatcher::selftestWriteOutput(const char* str)
{
	return g_selftest.selftestDispatcher->_selftestWriteOutput(str);
}

/*static*/ void
OperaSelftestDispatcher::selftestFinishOutput()
{
	g_selftest.selftestDispatcher->_selftestFinishOutput();
}

/*static*/ BOOL
OperaSelftestDispatcher::isSelftestDispatcherRunning()
{
	return g_selftest.selftestDispatcher->_isSelftestDispatcherRunning();
}

/*static*/ void
OperaSelftestDispatcher::selftestRunFromScope(const char* modules, BOOL spartan_readable)
{
	g_selftest.selftestDispatcher->_scope_selftestRun(modules, spartan_readable);
}

/**
 * helper to check if we have a window (if not, we are command line)
 */
/*static*/ BOOL
OperaSelftestDispatcher::isSelftestConnectedToWindow()
{
	return g_selftest.selftestDispatcher->_isSelftestConnectedToWindow();
}

/*static*/ BOOL
OperaSelftestDispatcher::isSelftestConnectedToScope()
{
	//so that we are sure what we return!
	if(!g_selftest.selftestDispatcher->_isSelftestConnectedToScope())
	{
		return false;
	}
	return true;
}

#endif
