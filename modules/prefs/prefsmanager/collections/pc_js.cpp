/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright 2002-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Peter Krefting
*/

#include "core/pch.h"

#include "modules/prefs/prefsmanager/collections/pc_js.h"
#include "modules/prefs/prefsmanager/hostoverride.h"
#include "modules/prefs/prefsmanager/prefstypes.h"
#include "modules/ecmascript_utils/esutils.h"
#include "modules/prefs/prefsmanager/collections/prefs_macros.h"

#include "modules/prefs/prefsmanager/collections/pc_js_c.inl"

PrefsCollectionJS *PrefsCollectionJS::CreateL(PrefsFile *reader)
{
	if (g_opera->prefs_module.m_pcjs)
		LEAVE(OpStatus::ERR);
	g_opera->prefs_module.m_pcjs = OP_NEW_L(PrefsCollectionJS, (reader));
	return g_opera->prefs_module.m_pcjs;
}

PrefsCollectionJS::~PrefsCollectionJS()
{
#ifdef PREFS_COVERAGE
	CoverageReport(
		m_stringprefdefault, PCJS_NUMBEROFSTRINGPREFS,
		m_integerprefdefault, PCJS_NUMBEROFINTEGERPREFS);
#endif

	g_opera->prefs_module.m_pcjs = NULL;
}

void PrefsCollectionJS::ReadAllPrefsL(PrefsModule::PrefsInitInfo *)
{
	// Read everything
	OpPrefsCollection::ReadAllPrefsL(
		m_stringprefdefault, PCJS_NUMBEROFSTRINGPREFS,
		m_integerprefdefault, PCJS_NUMBEROFINTEGERPREFS);
}

#ifdef PREFS_HOSTOVERRIDE
void PrefsCollectionJS::ReadOverridesL(const uni_char *host, PrefsSection *section, BOOL active, BOOL from_user)
{
	ReadOverridesInternalL(host, section, active, from_user,
	                       m_integerprefdefault, m_stringprefdefault);
}
#endif // PREFS_HOSTOVERRIDE

#ifdef PREFS_VALIDATE
void PrefsCollectionJS::CheckConditionsL(int which, int *value, const uni_char *)
{
	// Check any post-read/pre-write conditions and adjust value accordingly
	switch (static_cast<integerpref>(which))
	{
	case EcmaScriptEnabled:
	case IgnoreUnrequestedPopups:
	case TargetDestination:
#ifdef USER_JAVASCRIPT
	case UserJSEnabled:
	case UserJSEnabledHTTPS:
	case UserJSAlwaysLoad:
#endif
	case ScriptSpoof:
#ifdef ECMASCRIPT_DEBUGGER
# ifndef PREFS_SCRIPT_DEBUG
	case EcmaScriptDebugger:
# endif
#if defined ECMASCRIPT_REMOTE_DEBUGGER && !defined PREFS_TOOLS_PROXY
	case EcmaScriptRemoteDebugger:
#endif
#endif // ECMASCRIPT_DEBUGGER
#ifdef DELAYED_SCRIPT_EXECUTION
	case DelayedScriptExecution:
#endif
#ifdef PREFS_HAVE_BROWSERJS_TIMESTAMP
	case BrowserJSTimeStamp:
	case BrowserJSServerTimeStamp:
#endif
#ifdef DOM_FULLSCREEN_MODE
	case ChromelessDOMFullscreen:
#endif // DOM_FULLSCREEN_MODE
#ifdef GEOLOCATION_SUPPORT
	case AllowGeolocation:
#endif // GEOLOCATION_SUPPORT
#ifdef DOM_STREAM_API_SUPPORT
	case AllowCamera:
#endif // DOM_STREAM_API_SUPPORT
#if defined DOM_WEBWORKERS_SUPPORT
	case MaxWorkersPerWindow:
	case MaxWorkersPerSession:
#endif // DOM_WEBWORKERS_SUPPORT
		break; // Nothing to do.
#ifdef ECMASCRIPT_NATIVE_SUPPORT
	case JIT:
#endif // ECMASCRIPT_NATIVE_SUPPORT
#ifdef ES_HARDCORE_GC_MODE
	case HardcoreGCMode:
#endif // ES_HARDCORE_GC_MODE
#ifdef CANVAS3D_SUPPORT
	case ShaderValidation:
#endif // CANVAS3D_SUPPORT
#ifdef DOM_DEVICE_ORIENTATION_EVENT_SUPPORT
	case EnableOrientation:
#endif
#ifdef SPECULATIVE_PARSER
	case SpeculativeParser:
#endif // SPECULATIVE_PARSER
		break; // Nothing to do.

#if (defined ECMASCRIPT_REMOTE_DEBUGGER || defined SUPPORT_DEBUGGING_SHELL) && !defined PREFS_TOOLS_PROXY
# ifdef ECMASCRIPT_REMOTE_DEBUGGER
	case EcmaScriptRemoteDebuggerPort:
# endif
# ifdef SUPPORT_DEBUGGING_SHELL
	case OpShellProxyPort:
# endif
		if (*value < 0 || *value > 65535)
			*value = GetDefaultIntegerPref(static_cast<integerpref>(which));
		break;
#endif

#ifdef DOM_BROWSERJS_SUPPORT
	case BrowserJSSetting:
		// Valid values: 0=never load/download, 1=download/not load,
		// 2=load/download
		if (*value < 0 || *value > 2)
			*value = 1;
		break;
#endif
#if defined DOCUMENT_EDIT_SUPPORT && defined USE_OP_CLIPBOARD
	case LetSiteAccessClipboard:
		break;
#endif // DOCUMENT_EDIT_SUPPORT && USE_OP_CLIPBOARD

	default:
		// Unhandled preference! For clarity, all preferenes not needing to
		// be checked should be put in an empty case something: break; clause
		// above.
		OP_ASSERT(!"Unhandled preference");
	}
}

BOOL PrefsCollectionJS::CheckConditionsL(int which, const OpStringC &invalue,
                                         OpString **outvalue, const uni_char *)
{
	// Check any post-read/pre-write conditions and adjust value accordingly
	switch (static_cast<stringpref>(which))
	{
	case ES_NSAppName:
	case ES_IEAppName:
	case ES_OperaAppName:
	case ES_AppCodeName:
#if defined ECMASCRIPT_REMOTE_DEBUGGER && !defined PREFS_TOOLS_PROXY
	case EcmaScriptRemoteDebuggerIP:
#endif
#ifdef USER_JAVASCRIPT
	case UserJSFiles:
#endif
#if defined SUPPORT_DEBUGGING_SHELL && !defined PREFS_TOOLS_PROXY
	case OpShellProxyIP:
#endif
#if defined ES_OVERRIDE_FPMODE
	case FPMode:
#endif // ES_OVERRIDE_FPMODE
#ifdef WEB_HANDLERS_SUPPORT
	case DisallowedWebHandlers:
#endif
#ifdef VEGA_3DDEVICE
	case JS_HWAAccess:
#endif
		break; // Nothing to do.

	default:
		// Unhandled preference! For clarity, all preferenes not needing to
		// be checked should be put in an empty case something: break; clause
		// above.
		OP_ASSERT(!"Unhandled preference");
	}

	// When FALSE is returned, no OpString is created for outvalue
	return FALSE;
}
#endif // PREFS_VALIDATE

