/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) Opera Software ASA, 2001-2011
 *
 * Automatic proxy configuration through Javascript.
 * Lars T Hansen
 */
#include "core/pch.h"

#ifdef SUPPORT_AUTO_PROXY_CONFIGURATION

#include "modules/hardcore/mem/mem_man.h"
#include "modules/pi/OpSystemInfo.h"
#include "modules/url/url2.h"
#include "modules/url/url_sn.h"
#include "modules/url/protocols/scomm.h"
#include "modules/url/protocols/comm.h"
#include "modules/url/loadhandler/url_lh.h"
#include "modules/ecmascript/ecmascript.h"
#include "modules/autoproxy/autoproxy.h"

#ifdef OPERA_CONSOLE
# include "modules/console/opconsoleengine.h"
#endif // OPERA_CONSOLE
#include "modules/util/glob.h"

enum {
	EXECUTION_ASYNC_CUTOFF = 100,
	EXECUTION_SYNC_CUTOFF = 20000,
};

inline
JSProxyConfig::JSProxyConfig()
	: pending_comm_object(0)
	, is_dns_retry(FALSE)
	, runtime(0)
	, js_abort_time(0)
{
}

inline
JSProxyConfig::~JSProxyConfig()
{
}

#include "modules/autoproxy/autoproxy_lib.h"

/* static */ JSProxyConfig*
JSProxyConfig::Create( URL &source, OP_STATUS& stat )
{
	ES_ProgramText *OP_MEMORY_VAR program = OP_NEWA(ES_ProgramText, 5);
	OP_MEMORY_VAR int program_length = 5;
	OP_MEMORY_VAR int program_used = 0;
	URL_DataDescriptor *OP_MEMORY_VAR desc = 0;
	BOOL more = TRUE;
	JSProxyConfig *retval = 0;

	stat = OpStatus::ERR_NO_MEMORY;

	if (program == 0)
		goto cleanup;

	desc = source.GetDescriptor(NULL, TRUE);
	if(desc == 0)
		goto cleanup;

	while (more)
	{
		// "blen" and "ulen" must be unsigned long to receive values from
		// RetrieveDataL, but memcpy() takes size_t: check that they are
		// roughly the same.
		OP_ASSERT( sizeof( unsigned long ) == sizeof( size_t ) );

		OP_MEMORY_VAR unsigned long blen;
		TRAPD(err, blen = desc->RetrieveDataL(more));	// Unterminated, blen is number of bytes

		if (OpStatus::IsSuccess(err) && blen)
		{
			// We must assume RetrieveDataL always returns entire uni_chars, or
			// things will be mightily messed up from here on.
			OP_ASSERT( blen % sizeof(uni_char) == 0 );

			if (program_used == program_length)
			{
				ES_ProgramText *newprogram = OP_NEWA(ES_ProgramText, program_length*2);
				if (newprogram == 0)
					goto cleanup;
				op_memcpy( newprogram, program, sizeof(*program)*program_length );
				OP_DELETEA(program);
				program = newprogram;
				program_length *= 2;
			}

			const char *buf = desc->GetBuffer();
			unsigned long ulen = blen/sizeof(uni_char);
			uni_char *pbuf = OP_NEWA(uni_char, ulen);
			if( !pbuf )
				goto cleanup;
			op_memcpy( pbuf, buf, UNICODE_SIZE(ulen) );
			program[ program_used ].program_text = pbuf;
			program[ program_used ].program_text_length = ulen;
			++program_used;
			desc->ConsumeData(UNICODE_SIZE(ulen));
		}
	}

	retval = JSProxyConfig::Create( program, program_used, stat, &source );

cleanup:
	while (--program_used >= 0)
	{
		OP_DELETEA((uni_char*)program[program_used].program_text);
	}
	OP_DELETEA(program);
	OP_DELETE(desc);

	if (retval)
		stat = OpStatus::OK;

	return retval;
}

/* static */ JSProxyConfig*
JSProxyConfig::Create(const ES_ProgramText *source, int elts, OP_STATUS& stat, URL* srcurl)
{
	int library_size = 0;
	const uni_char * const *library_strings = NULL;
	JSProxyConfig *OP_MEMORY_VAR self = 0;
	ES_ProgramText *text = 0;
	ES_Context *context = 0;
	ES_Program *program = 0;
	int i;
	OP_STATUS r;
	ES_Runtime::CompileProgramOptions options;

	stat = OpStatus::ERR_NO_MEMORY;

	GetAPCLibrarySourceCode(&library_size, &library_strings);

	text = OP_NEWA(ES_ProgramText,  library_size+elts+1 );
	if (!text)
		goto failure;

	self = OP_NEW(JSProxyConfig, ());
	if (!self)
		goto failure;

	self->runtime = OP_NEW(ES_Runtime, ());
	if (!self->runtime)
		goto failure;
	if(self->runtime->Construct() != OpStatus::OK)
	{
		OP_DELETE(self->runtime);		// Special case: use delete, not Detach(), because construction failed
		self->runtime = NULL;
		goto failure;
	}

	TRAP(r,self->InitializeHostObjectsL());
	if (OpStatus::IsError(r))
		goto failure;

	/* First add the library source code, if any */
	for ( i=0 ; i < library_size ; i++ )
	{
		text[i].program_text = library_strings[i];
		text[i].program_text_length = uni_strlen(library_strings[i]);
	}

	/* We want the initialization call to be the first executable
	   statement, in case the initialization of the script references
	   the library.  */
	text[library_size].program_text = UNI_L("\n\nOPERA$init();");
	text[library_size].program_text_length = uni_strlen(text[library_size].program_text);

	/* User script */
	for ( i=0 ; i < elts ; i++ )
	{
		text[i+library_size+1].program_text = source[i].program_text;
		text[i+library_size+1].program_text_length = source[i].program_text_length;
	}

	options.global_scope = FALSE;
	options.prevent_debugging = TRUE;
	options.script_url = srcurl;
	options.when = UNI_L("while initializing FindProxyForURL");

	if (OpStatus::IsError(r = self->runtime->CompileProgram(text, library_size+elts+1, &program, options)))
	{
		stat = r;
		goto failure;
	}
	if (!program)
		goto failure;

	context = self->runtime->CreateContext(NULL);

	if (!context)
		goto failure;

	g_proxycfg->current_implementation = self;

	if (OpStatus::IsError(self->runtime->PushProgram( context, program )))
		goto failure;
	self->runtime->DeleteProgram( program );
	program = NULL;

	InitAPCLibrary(self->runtime);

	switch(self->ExecuteProgram( context, 0, NO_SUSPEND ))
	{
	case APC_OK:
		stat = OpStatus::OK;
		break;
	case APC_Error:
		stat = OpStatus::ERR;
		goto failure;
	case APC_Suspend:
		stat = OpStatus::ERR;
		goto failure;
	case APC_OOM:
		goto failure;
	}

	OP_DELETEA(text);
	self->runtime->DeleteContext( context );
	context = NULL;

	return self;

failure:
	if (text)
	{
		OP_DELETEA(text);
	}
	if (context)
	{
		self->runtime->DeleteContext( context );
		context = NULL;
	}
	if (program)
	{
		self->runtime->DeleteProgram( program );
		context = NULL;
	}
	Destroy(self);
	return NULL;
}

void JSProxyConfig::Destroy(JSProxyConfig* apc)
{
	if (apc)
	{
		if (apc->runtime)
		{
			apc->runtime->Detach();
			apc->runtime = NULL;
		}
		OP_DELETE(apc);
	}
}

void JSProxyConfig::InitializeHostObjectsL()
{
	EcmaScript_Object* f;

	f = OP_NEW_L(ProxyConfigDnsResolve, ());
	f->SetFunctionRuntimeL(runtime, UNI_L("OPERA$dnsResolve"), NULL, "s");
	LEAVE_IF_ERROR(runtime->PutInGlobalObject(f, UNI_L("OPERA$dnsResolve")));

	f = OP_NEW_L(ProxyConfigMyHostname, ());
	f->SetFunctionRuntimeL(runtime, UNI_L("OPERA$myHostname"), NULL, NULL);
	LEAVE_IF_ERROR(runtime->PutInGlobalObject(f, UNI_L("OPERA$myHostname")));

	f = OP_NEW_L(ProxyConfigShExpMatch, ());
	f->SetFunctionRuntimeL( runtime, UNI_L("shExpMatch"), NULL, "ss");
	LEAVE_IF_ERROR(runtime->PutInGlobalObject(f, UNI_L("shExpMatch")));

#if defined SELFTEST
	f = OP_NEW_L(DebugAPC, ());
	f->SetFunctionRuntimeL( runtime, UNI_L("debugAPC"), NULL, "s");
    LEAVE_IF_ERROR(runtime->PutInGlobalObject(f, UNI_L("debugAPC")));
#endif
}

OP_STATUS
JSProxyConfig::FindProxyForURL( const uni_char *url,
							    const uni_char *host,
							    uni_char **result,
							    JSProxyPendingContext** pcontext)
{
	JSProxyPendingContext *p;
	ES_Context* context = NULL;
	APC_Status apcstat = APC_OOM;

	// OPTIMIZEME: we can avoid compiling a program on every lookup by caching a
	// function that calls FindProxyForURL(), and just pushing and calling this
	// with two arguments rather than embedding those args in a literal program
	// here.  This will be a major speed boost on tiny devices.
	ES_Object* fn = NULL;
	const uni_char *format = UNI_L("return FindProxyForURL(\"%s\",\"%s\");");
	size_t program_text_length = uni_strlen(url) + uni_strlen(host) + uni_strlen(format) - 4;	// -4 for %s twice
	uni_char* program_text = OP_NEWA(uni_char,  program_text_length + 1 );

	ES_Runtime::CreateFunctionOptions options;
	options.generate_result = TRUE;
	options.prevent_debugging = TRUE;

	if (program_text == 0)
		goto cleanup;
	uni_snprintf( program_text, program_text_length + 1, format, url, host );

	if (OpStatus::IsError(runtime->CreateFunction(NULL, 0, program_text, program_text_length, &fn, 0, NULL, options)))
		goto cleanup;

	runtime->Protect(fn);
	OP_DELETEA(program_text);
	program_text = NULL;

	context = runtime->CreateContext(NULL);

	if (!context)
		goto cleanup;

	g_proxycfg->current_implementation = this;

	if (OpStatus::IsError(runtime->PushCall(context, fn, 0, NULL)))
		goto cleanup;
	runtime->Unprotect(fn);
	fn = NULL;
	is_dns_retry = FALSE;
	switch ((apcstat = ExecuteProgram( context, result, ALLOW_SUSPEND )))
	{
	case APC_OK:
		*pcontext = NULL;
		if (context)
		{
			runtime->DeleteContext( context );
			context = NULL;
		}
		return OpStatus::OK;
	case APC_OOM:
	case APC_Error:
		goto cleanup;
	case APC_Suspend:
		*result = NULL;
		p = OP_NEW(JSProxyPendingContext, ( runtime, context, g_proxycfg->current_implementation->pending_comm_object, FALSE ));
		g_proxycfg->current_implementation->pending_comm_object = NULL;
		if (!p)
			goto cleanup;
		*pcontext = p;
		return OpStatus::OK;
	}

cleanup:
	*result   = NULL;
	*pcontext = NULL;
	if (program_text)
	{
		OP_DELETEA(program_text);
	}
	if (fn)
	{
		runtime->Unprotect(fn);
		fn=NULL;
	}
	if (context)
	{
		runtime->DeleteContext( context );
		context = NULL;
	}
	return apcstat == APC_OOM ? OpStatus::ERR_NO_MEMORY : OpStatus::ERR;
}

OP_STATUS
JSProxyConfig::FindProxyForURL( JSProxyPendingContext* state, uni_char **result)
{
	g_proxycfg->current_implementation = this;

	is_dns_retry = state->comm != NULL;
	switch (ExecuteProgram( state->context, result, ALLOW_SUSPEND ))
	{
	case APC_OK :
		state->lookup_succeeded = TRUE;
		return OpStatus::OK;
	case APC_OOM:
		state->lookup_succeeded = FALSE;
		return OpStatus::ERR_NO_MEMORY;
	case APC_Error :
		state->lookup_succeeded = FALSE;
		return OpStatus::ERR;
	case APC_Suspend:
		SComm::SafeDestruction(state->comm);
		state->comm = g_proxycfg->current_implementation->pending_comm_object;
		g_proxycfg->current_implementation->pending_comm_object = NULL;
		state->lookup_succeeded = FALSE;
		return OpStatus::OK;
	}
	/*NOTREACHED*/
	return 0;
}

JSProxyConfig::APC_Status
JSProxyConfig::ExecuteProgram( ES_Context *context, uni_char **result, JSProxyConfig::SuspensionBehavior sb )
{
	ES_Eval_Status status = ES_NORMAL;
	BOOL looping = TRUE;

	// Calculate the time when JS execution will be aborted.
	// For infinite loops prevention.
	// If we can suspend (used in normal execution) we timeslice
	// on EXECUTION_ASYNC_CUTOFF, otherwise (initial initialization of the pac script)
	// we allow the script to run for much longer (EXECUTION_SYNC_CUTOFF) before
	// we give up.
	double now = g_op_time_info->GetRuntimeMS();
	if (sb == ALLOW_SUSPEND)
		js_abort_time = now + EXECUTION_ASYNC_CUTOFF;
	else
		js_abort_time = now + EXECUTION_SYNC_CUTOFF;
	do
	{
		void* abort_time = static_cast <void*> (&js_abort_time);

		status = runtime->ExecuteContext(
			context,             // context
			FALSE,               // want_string_result
			FALSE,               // want_exceptions
			FALSE,               // allow_cross_origin_errors
			&ESRuntimeOutOfTime, // external_out_of_time
			abort_time           // external_out_of_time_data
		);

		if (ESRuntimeOutOfTime(abort_time))
		{
			looping = FALSE;
			if (status == ES_SUSPENDED)
				sb = NO_SUSPEND; // Abort script that is this slow, probably an infinite loop.
		}

		switch (status)
		{
		case ES_SUSPENDED :
			if (sb == ALLOW_SUSPEND)
				return APC_Suspend;
			break;
		case ES_NORMAL :
			if (result)
			{
				*result = uni_strdup( UNI_L("") );
				if (*result == NULL)
					return APC_OOM;
			}
			return APC_OK;
		case ES_NORMAL_AFTER_VALUE :
			if (result)
			{
				ES_Value value;
				runtime->GetReturnValue( context, &value );
				if (value.type == VALUE_STRING)
				{
					*result = uni_strdup( value.value.string );
					if (*result == 0)
						return APC_OOM;
				}
				else if (value.type == VALUE_NULL)
					*result = uni_strdup( UNI_L("DIRECT") );
				else
					*result = 0;
			}
			return APC_OK;
		case ES_ERROR :
			looping = FALSE;
			break;
		case ES_ERROR_NO_MEMORY :
			return APC_OOM;
		default:
			break;
		}
	} while (looping);

	if (result)
		*result = 0;
	if (status == ES_SUSPENDED)
	{
#ifdef OPERA_CONSOLE
		if (g_console->IsLogging())
		{
			OpConsoleEngine::Message msg(OpConsoleEngine::EcmaScript, OpConsoleEngine::Error);
			msg.message.Set("Proxy config script ran too long and was terminated");
			TRAPD(rc, g_console->PostMessageL(&msg));
		}
#endif // OPERA_CONSOLE
	}
	return APC_Error;
}

BOOL JSProxyConfig::ESRuntimeOutOfTime(void* abort_time)
{
	if (!abort_time)
		return TRUE;

	const double* deadline_p = static_cast <const double*> (abort_time);
	OP_ASSERT(deadline_p);
	const double& deadline = *deadline_p;

	OP_ASSERT(g_op_system_info);
	double now = g_op_time_info->GetRuntimeMS();

	return (now > deadline);
}


JSProxyPendingContext::JSProxyPendingContext( ES_Runtime* runtime, ES_Context* context, Comm* comm, BOOL lookup_succeeded )
	: runtime(runtime)
	, context(context)
	, comm(comm)
	, lookup_succeeded(lookup_succeeded)
{
}

JSProxyPendingContext::~JSProxyPendingContext()
{
	SComm::SafeDestruction( comm );
	runtime->DeleteContext( context );
}

/* virtual */ int
ProxyConfigDnsResolve::Call(ES_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime* origining_runtime)
{
	OpString ip_addr;
	Comm *comm = 0;
	ServerName *server = 0;
	const uni_char *uni_host = 0;
	CommState s;
	OpSocketAddress * sa;
	OP_STATUS result = OpStatus::OK;

	if (argc < 1)
	{
		// FIXME: want to throw an object here, not a string.
		return_value->type = VALUE_STRING;
		return_value->value.string = UNI_L("Autoproxy: too few args to DNSResolve");
		return ES_EXCEPTION;
	}

	return_value->type = VALUE_BOOLEAN;
	return_value->value.boolean = FALSE;

	uni_host = argv[0].value.string;
	server = g_url_api->GetServerName(uni_host, TRUE); // Returns NULL on OOM
	if(!server)
		return ES_NO_MEMORY;

	// Second entry: suspended lookup succeeded or failed.  Detect success or
	// failure here and return a value not equal to UNDEFINED to the caller.

	if (server->IsHostResolved())
	{
		sa = server->SocketAddress();
		if(sa != NULL && sa->IsValid())
		{
			result = sa->ToString(&ip_addr);
			if(result != OpStatus::OK)
				return ES_NO_MEMORY;
		}
		goto dns_lookup_completed;
	}

	if (g_proxycfg->current_implementation->is_dns_retry)
		goto dns_lookup_completed;

	OP_ASSERT( g_proxycfg->current_implementation->pending_comm_object == NULL );

	comm = Comm::Create(g_main_message_handler, server, 80);
	if(!comm)
		return ES_NO_MEMORY;

	// First, start the lookup.
	s = comm->LookUpName( server );
	if (s == COMM_REQUEST_FINISHED)
	{
		// Got it
		sa = comm->HostName()->SocketAddress();

		SComm::SafeDestruction( comm );

		result = sa->ToString(&ip_addr);
		if(result != OpStatus::OK)
			return ES_NO_MEMORY;

		goto dns_lookup_completed;
	}
	else if (s != COMM_LOADING && s != COMM_WAITING_FOR_SYNC_DNS)
	{
		SComm::SafeDestruction( comm );

		// FIXME: want to throw an object here, not a string.
		return_value->type = VALUE_STRING;
		return_value->value.string = UNI_L("Autoproxy: DNS lookup failed unexpectedly");
		return ES_EXCEPTION;
	}

	// Then save the state to continue later.
	g_proxycfg->current_implementation->pending_comm_object = comm;

	// Then stop execution and return a value that tells the caller to retry.
	// g_proxycfg->current_implementation->context->SuspendExecution();
	return_value->type = VALUE_UNDEFINED;
	return ES_SUSPEND | ES_VALUE;

dns_lookup_completed:
	g_proxycfg->current_implementation->is_dns_retry = FALSE;
	if (ip_addr.Length() > 0)
	{
		uni_char *buf = (uni_char*)g_memory_manager->GetTempBuf();
		uni_strcpy(buf, ip_addr.CStr());

		return_value->type = VALUE_STRING;
		return_value->value.string = buf;
	}
	return ES_VALUE;
}

/* virtual */ int
ProxyConfigMyHostname::Call(ES_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime* origining_runtime)
{
#ifdef COMM_LOCALHOSTNAME_IS_SERVERNAME
	const char *myname = NULL;
	if (Comm::GetLocalHostName())
		myname = Comm::GetLocalHostName()->Name();
#else
	const char *myname = Comm::GetLocalHostName();
#endif
	uni_char *buf = (uni_char*)g_memory_manager->GetTempBuf();

	op_strncpy( (char*)buf, myname, g_memory_manager->GetTempBufLen()/2 );
	((char*)buf)[ g_memory_manager->GetTempBufLen()/2-1 ] = 0;
	make_doublebyte_in_place( buf, op_strlen((char*)buf)+1 );

	return_value->type = VALUE_STRING;
	return_value->value.string = buf;
	return ES_VALUE;
}

/* virtual */ int
ProxyConfigShExpMatch::Call(ES_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime* origining_runtime)
{
	if (argc < 2 || argv[0].type != VALUE_STRING || argv[1].type != VALUE_STRING)
	{
		// FIXME: want to throw an object here, not a string.
		return_value->type = VALUE_STRING;
		return_value->value.string = UNI_L("Autoproxy: too few args to ShExpMatch");
		return ES_EXCEPTION;
	}

	return_value->type = VALUE_BOOLEAN;
	return_value->value.boolean = !!ShExpMatch( argv[0].value.string, argv[1].value.string );
	return ES_VALUE;
}

/* This is the GLOB(7) algorithm as commonly used on Unix systems.
   The exact behavior of the algorithm is specified by a standard
   (namely, POSIX 1003.2, 3.13).  The following procedure implements
   the subset of that standard that corresponds with the basic GLOB
   algorithm.  It does not have any POSIX extensions: locale
   sensitivity, character classes, collating symbols, or equivalence
   classes.  Here is a summary of the behavior:

	\c		matches the character c
	c		matches the character c
	*		matches zero or more characters except '/'
	?		matches any character except '/'
	[...]	matches any character in the set except '/'

			Inside the set, a leading '!' denotes complementation
			(the set matches any character not in the set).

			A ']' can occur in the set if it comes first (possibly
			following '!').  A range a-b in the	set denotes a character
			range; the set contains all	characters a through b inclusive.
			A '-' can occur	in the set if it comes first (possibly
			following '!') or if it comes last.  '\', '*', '[', and '?'
			have no special meaning inside a set, and '/' may not
			be in the set.

   Unlike the standard globber, this one does not treat a leading '.'
   specially.

   This code has been modified to match the behavior of Netscape and
   MSIE: it does not treat '/' specially.

   The code is written for compactness and low space usage rather
   than for speed; it precomputes nothing and remembers nothing.
*/
BOOL ProxyConfigShExpMatch::ShExpMatch( const uni_char *str, const uni_char *pat )
{
#ifdef OP_GLOB_SUPPORT
	return OpGlob( str, pat, /*slash_is_special=*/FALSE );
#else
	int complement=0;
	int match = 0;
	uni_char c;

	OP_ASSERT( *pat != 0);

	while (*pat != 0)
	{
		switch (c = *pat++)
		{
		case '*' :
			if (*pat == 0)
			{
				while (*str != 0 )
					str++;
				return *str == 0;
			}

			do
			{
				if (ShExpMatch( str, pat ))
					return TRUE;
				++str;
			} while (*str != 0);
			return FALSE;
		case '?':
			++str;
			break;
		case '[':
			complement = 0;
			if (*pat == 0)
				return FALSE;
			if (*pat == '!')
			{
				complement = 1;
				++pat;
				if (*pat == 0)
					return FALSE;
			}
			do
			{
				match = 0;
				uni_char c1, c2;

				OP_ASSERT( *pat != 0 );

				c1 = *pat;
				++pat;
				if (*pat == 0)
					return FALSE;
				if (*pat == '-')
				{
					++pat;
					c2 = *pat;
					if (c2 == 0)
						return FALSE;
					if (c2 == ']')
					{
						--pat;
						match = (c1 == *str);
					}
					else
					{
						++pat;
						match = (c1 <= *str && *str <= c2);
					}
				}
				else
					match = (c1 == *str);
				if (match != complement)
					while (*pat && *pat != ']')
						++pat;
			} while (*pat && *pat != ']');
			if(match == complement)
				return FALSE;
			++str;
			if (*pat != ']')
				return FALSE;
			++pat;
			break;
		case '\\':
			if (*pat == 0 || c != *str)
				return FALSE;
			++pat;
			++str;
			break;
		default:
			if (c != *str)
				return FALSE;
			str++;
			break;
		}
	}
	return *str == 0;
#endif // OP_GLOB_SUPPORT
}

#ifdef SELFTEST
#include <stdio.h>
/* virtual */ int
DebugAPC::Call(ES_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime* origining_runtime)
{
#ifdef YNP_WORK
	FILE *dafp = fopen("test_apc.txt","a");
	while(argc--)
	{
		if(argv->type == VALUE_STRING)
			fprintf(dafp,"%s\n",make_singlebyte_in_tempbuffer(argv->value.string,uni_strlen(argv->value.string)));
		argv++;
	}
	fclose(dafp);
#endif // YNP_WORK
	return_value->type = VALUE_UNDEFINED;
	return ES_VALUE;
}

/* static */ void
DebugAPC::LogNumberOfLoads()
{
#ifdef YNP_WORK
	FILE *dafp = fopen("test_apc.txt","a");
	if (dafp)
	{
		fprintf(dafp,
				"Number of loads of proxy script %s (should be 1, was %d)\n",
				g_proxycfg->number_of_loads_from_url==1 ? "OK" : "ERROR",
				g_proxycfg->number_of_loads_from_url );
		fclose(dafp);
	}
#endif // YNP_WORK
}
#endif // SELFTEST

#endif // SUPPORT_AUTO_PROXY_CONFIGURATION
