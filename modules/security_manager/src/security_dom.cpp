/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

/*
   Cross-module security manager: DOM
   Lars T Hansen

   See document-security.txt in the documentation directory for details.  */

#include "core/pch.h"

#include "modules/url/url2.h"
#include "modules/security_manager/include/security_manager.h"
#include "modules/dom/domutils.h"
#include "modules/doc/frm_doc.h"
#include "modules/dochand/win.h"
#include "modules/prefs/prefsmanager/collections/pc_doc.h"
#if defined(DOCUMENT_EDIT_SUPPORT) && defined(USE_OP_CLIPBOARD)
#include "modules/prefs/prefsmanager/collections/pc_js.h"
#endif // DOCUMENT_EDIT_SUPPORT && USE_OP_CLIPBOARD
#include "modules/console/opconsoleengine.h"
#include "modules/formats/uri_escape.h"

#if defined WEB_HANDLERS_SUPPORT && defined PREFS_HAS_PREFSFILE
#include "modules/prefs/prefsmanager/collections/pc_files.h"
#include "modules/prefsfile/prefsfile.h"
#include "modules/util/opfile/opfile.h"
#include "modules/util/glob.h"
#endif // WEB_HANDLERS_SUPPORT && PREFS_HAS_PREFSFILE

#ifdef XHR_SECURITY_OVERRIDE
#include "modules/windowcommander/src/WindowCommander.h"
#endif // XHR_SECURITY_OVERRIDE

#ifdef WEB_HANDLERS_SUPPORT
// Helper function that can LEAVE allowing us to use less TRAP's
static void LoadPrefsFileL(PrefsFile& prefs_file, const OpFile* global, const OpFile* local)
{
	if (!global && !local)
		LEAVE(OpStatus::ERR);

	prefs_file.ConstructL();

#ifdef PREFSFILE_CASCADE
	if (global && local)
	{
		prefs_file.SetFileL(local);
		prefs_file.SetGlobalFileL(global);
	}
	else
		prefs_file.SetFileL( local ? local : global);
#else
	prefs_file.SetFileL(global);
#endif
	prefs_file.LoadAllL();
}
#endif // WEB_HANDLERS_SUPPORT

OpSecurityManager_DOM::OpSecurityManager_DOM()
#ifdef SECMAN_ALLOW_DISABLE_XHR_ORIGIN_CHECK
	: allow_set_skip_origin_check(FALSE)
#endif // SECMAN_ALLOW_DISABLE_XHR_ORIGIN_CHECK
{
}

#if defined(GADGET_SUPPORT) && defined(SECMAN_CROSS_ORIGIN_XHR_GADGET_SUPPORT)
/* To support a nested security check for DOM_LOADSAVE for gadgets --
   the gadgets security check takes precedence, but a failed check
   will then defer to CORS -- we wrap the original callback inside
   another security callback. The wrapper delegates to the CORS
   security check if the gadget check disallows.

   (See the SECMAN_CROSS_ORIGIN_XHR_GADGET_SUPPORT documentation
   for further details.) */
class OpSecurityManager_DOM_LoadSaveCallback
	: public OpSecurityCheckCallback
{
public:
	OpSecurityManager_DOM_LoadSaveCallback(OpCrossOrigin_Request *request, const URL &target_url, OpSecurityCheckCallback *callback, OpSecurityCheckCancel **cancel)
		: request(request)
		, target_url(target_url)
		, callback(callback)
		, cancel(cancel)
	{
		request->SetIsAnonymous(TRUE);
		request->ClearWithCredentials();
	}

	virtual void OnSecurityCheckSuccess(BOOL allowed, ChoicePersistenceType type = PERSISTENCE_NONE)
	{
		if (!allowed)
		{
			OpSecurityContext source(request->GetURL(), request);
			OpSecurityContext target(target_url);
			OpStatus::Ignore(OpSecurityManager::CheckCrossOriginSecurity(source, target, callback, cancel));
		}
		else
			callback->OnSecurityCheckSuccess(allowed);
		OP_DELETE(this);
	}

	virtual void OnSecurityCheckError(OP_STATUS error)
	{
		callback->OnSecurityCheckError(error);
		OP_DELETE(this);
	}

private:
	OpCrossOrigin_Request *request;
	URL target_url;
	OpSecurityCheckCallback *callback;
	OpSecurityCheckCancel **cancel;
};
#endif // GADGET_SUPPORT && SECMAN_CROSS_ORIGIN_XHR_GADGET_SUPPORT

#ifdef XHR_SECURITY_OVERRIDE
static OP_BOOLEAN
CheckSecurityOveride(const OpSecurityContext& source, const OpSecurityContext& target, BOOL *allowed, OpSecurityCheckCallback* security_callback)
{
	OP_ASSERT((allowed == NULL) != (security_callback == NULL));
	if (DOM_Utils::GetDocument(source.GetRuntime()))
	{
		WindowCommander *wc = DOM_Utils::GetDocument(source.GetRuntime())->GetWindow()->GetWindowCommander();
		URL origin_url= DOM_Utils::GetOriginURL(source.GetRuntime());
		// Can't keep both src and tgt in URL's internal buffer, so one needs to be duplicated.
		uni_char *src = uni_strdup(origin_url.GetAttribute(URL::KUniName_Username_Password_Hidden, TRUE).CStr());
		RETURN_OOM_IF_NULL(src);
		const uni_char *tgt = target.GetURL().GetAttribute(URL::KUniName_Username_Password_Hidden, TRUE).CStr();
		OpDocumentListener::XHRPermission res = wc->GetDocumentListener()->OnXHR(wc, src, tgt);
		op_free(src);
		switch(res)
		{
		case OpDocumentListener::ALLOW:
			if (security_callback)
				security_callback->OnSecurityCheckSuccess(TRUE);
			else
				*allowed = TRUE;
			return OpBoolean::IS_TRUE;
		case OpDocumentListener::DENY:
			if (security_callback)
				security_callback->OnSecurityCheckSuccess(FALSE);
			else
				*allowed = FALSE;
			return OpBoolean::IS_TRUE;
		}
	}
	return OpBoolean::IS_FALSE;
}
#endif // XHR_SECURITY_OVERRIDE

OP_STATUS
OpSecurityManager_DOM::CheckLoadSaveSecurity(const OpSecurityContext& source, const OpSecurityContext& target, OpSecurityCheckCallback* security_callback, OpSecurityCheckCancel **security_cancel)
{
#ifdef XHR_SECURITY_OVERRIDE
	OP_BOOLEAN result = CheckSecurityOveride(source, target, NULL, security_callback);
	RETURN_IF_ERROR(result);
	if (result == OpBoolean::IS_TRUE)
		return OpStatus::OK;
#endif // XHR_SECURITY_OVERRIDE

	BOOL allowed = FALSE;
#if defined(GADGET_SUPPORT) && !defined(SECMAN_CROSS_ORIGIN_XHR_GADGET_SUPPORT)
	// For gadgets, always defer to the gadgets model, and never go through
	// the normal model.
	if (source.IsGadget())
		return g_secman_instance->CheckGadgetSecurity(source, target, security_callback);
#elif defined(GADGET_SUPPORT) && defined(SECMAN_CROSS_ORIGIN_XHR_GADGET_SUPPORT)
	// ... But, if disallowed by the gadgets model, then consider it for CORS.
	if (source.IsGadget())
	{
		OpSecurityManager_DOM_LoadSaveCallback *nested_callback = NULL;
		if (OpCrossOrigin_Request *request = source.GetCrossOriginRequest())
			if (!request->HasCompleted(allowed))
			{
				RETURN_OOM_IF_NULL(nested_callback = OP_NEW(OpSecurityManager_DOM_LoadSaveCallback, (request, target.GetURL(), security_callback, security_cancel)));
				security_callback = nested_callback;
			}
		OP_STATUS status = g_secman_instance->CheckGadgetSecurity(source, target, security_callback);
		if (OpStatus::IsError(status))
			OP_DELETE(nested_callback);
		return status;
	}
#endif // GADGET_SUPPORT && !SECMAN_CROSS_ORIGIN_XHR_GADGET_SUPPORT

#ifdef CORS_SUPPORT
	// If this security check is over CORS, it takes precedence and
	// the same-origin checks below will not be considered.
	if (OpCrossOrigin_Request *request = source.GetCrossOriginRequest())
		if (!request->HasCompleted(allowed))
			return g_secman_instance->CheckCrossOriginSecurity(source, target, security_callback, security_cancel);
#endif // CORS_SUPPORT

	DOM_Runtime* origining_runtime = source.GetRuntime();
	const URL& url = target.GetURL();
	const URL& ref_url = target.GetRefURL();

	if (url.IsEmpty() || origining_runtime == NULL)
	{
		security_callback->OnSecurityCheckSuccess(FALSE);
		return OpStatus::OK;
	}

	URL src_url = DOM_Utils::GetOriginURL(origining_runtime);

	// Note: XMLHttpRequests use the real site address as source, not any custom document.domain.
	// See CORE-15635 for the background and analysis. That is why the origining_runtime isn't
	// used as is in the OriginCheck below.
	allowed = allowed ||
#ifdef SECMAN_ALLOW_DISABLE_XHR_ORIGIN_CHECK
	     DOM_Utils::HasRelaxedLoadSaveSecurity(origining_runtime) ||
#endif // SECMAN_ALLOW_DISABLE_XHR_ORIGIN_CHECK
	     OpSecurityManager::OriginCheck(src_url, target) ||
	     url.Type() == URL_DATA ||
	     OpSecurityManager::OperaUrlCheck(source.GetURL()) ||
	     url.SameServer(ref_url, URL::KCheckResolvedPort);

#ifdef OPERA_CONSOLE
	BOOL can_be_fixed_with_prefs = FALSE;
#endif // OPERA_CONSOLE

	if (allowed)
	{
		// Special restrictions on files. Read CORE-18118 if you want to.
		if (url.Type() == URL_FILE)
		{
			allowed = FALSE;

			// But help web developers by letting them turn on a pref to access them anyway.

			if (src_url.Type() == URL_FILE)
			{
				allowed = g_pcdoc->GetIntegerPref(PrefsCollectionDoc::AllowFileXMLHttpRequest);
#ifdef OPERA_CONSOLE
				if (!allowed)
					can_be_fixed_with_prefs = TRUE;
#endif // OPERA_CONSOLE
			}
		}
	}

#ifdef OPERA_CONSOLE
	if (!allowed && can_be_fixed_with_prefs && g_console)
	{
		OpConsoleEngine::Message msg(OpConsoleEngine::EcmaScript, OpConsoleEngine::Information);
		if (OpStatus::IsSuccess(src_url.GetAttribute(URL::KUniName_Username_Password_Hidden_Escaped, msg.url)) &&
		    OpStatus::IsSuccess(msg.message.Set(UNI_L("XMLHttpRequest to files is disabled for security reasons. Set \"Allow File XMLHttpRequest\" with opera:config#UserPrefs|AllowFileXMLHttpRequest to disable this security check."))))
		{
			g_console->PostMessageL(&msg);
		}
	}
#endif // OPERA_CONSOLE

	security_callback->OnSecurityCheckSuccess(allowed);
	return OpStatus::OK;
}

OP_STATUS
OpSecurityManager_DOM::CheckLoadSaveSecurity(const OpSecurityContext& source, const OpSecurityContext& target, BOOL& allowed, OpSecurityState& state)
{
	allowed = FALSE;
#ifdef XHR_SECURITY_OVERRIDE
	OP_BOOLEAN result = CheckSecurityOveride(source, target, &allowed, NULL);
	RETURN_IF_ERROR(result);
	if (result == OpBoolean::IS_TRUE)
		return OpStatus::OK;
#endif // XHR_SECURITY_OVERRIDE

#ifdef GADGET_SUPPORT
	// For gadgets, always defer to the gadgets model, and never go through
	// the normal model.

	if (source.IsGadget())
		return g_secman_instance->CheckGadgetSecurity(source, target, allowed, state);
#endif

	DOM_Runtime* origining_runtime = source.GetRuntime();
	const URL& url = target.GetURL();
	const URL& ref_url = target.GetRefURL();

	if (url.IsEmpty() || origining_runtime == NULL)
		return OpStatus::OK;

#ifdef OPERA_CONSOLE
	BOOL can_be_fixed_with_prefs = FALSE;
#endif // OPERA_CONSOLE

	URL src_url = DOM_Utils::GetOriginURL(origining_runtime);

	// Note: XMLHttpRequests use the real site address as source, not any custom document.domain.
	// See CORE-15635 for the background and analysis. That is why the origining_runtime isn't
	// used as is in the OriginCheck below.
	allowed =
#ifdef SECMAN_ALLOW_DISABLE_XHR_ORIGIN_CHECK
		DOM_Utils::HasRelaxedLoadSaveSecurity(origining_runtime) ||
#endif // SECMAN_ALLOW_DISABLE_XHR_ORIGIN_CHECK
		OpSecurityManager::OriginCheck(src_url, target) ||
		url.Type() == URL_DATA ||
		OpSecurityManager::OperaUrlCheck(source.GetURL()) ||
		url.SameServer(ref_url, URL::KCheckResolvedPort);

	if (allowed)
	{
		// Special restrictions on files. Read CORE-18118 if you want to.
		if (url.Type() == URL_FILE)
		{
			allowed = FALSE;

			// But help web developers by letting them turn on a pref to access them anyway.

			if (src_url.Type() == URL_FILE)
			{
				allowed = g_pcdoc->GetIntegerPref(PrefsCollectionDoc::AllowFileXMLHttpRequest);
#ifdef OPERA_CONSOLE
				if (!allowed)
					can_be_fixed_with_prefs = TRUE;
#endif // OPERA_CONSOLE
			}
		}
	}

#ifdef OPERA_CONSOLE
	if (!allowed && can_be_fixed_with_prefs && g_console)
	{
		OpConsoleEngine::Message msg(OpConsoleEngine::EcmaScript, OpConsoleEngine::Information);
		if (OpStatus::IsSuccess(src_url.GetAttribute(URL::KUniName_Username_Password_Hidden_Escaped, msg.url)) &&
		    OpStatus::IsSuccess(msg.message.Set(UNI_L("XMLHttpRequest to files is disabled for security reasons. Set \"Allow File XMLHttpRequest\" with opera:config#UserPrefs|AllowFileXMLHttpRequest to disable this security check."))))
		{
			g_console->PostMessageL(&msg);
		}
	}
#endif // OPERA_CONSOLE

	return OpStatus::OK;
}

BOOL
OpSecurityManager_DOM::CheckOperaConnectSecurity(const OpSecurityContext &source)
{
	DOM_Runtime *rt = source.GetRuntime();
	RETURN_VALUE_IF_NULL(rt, FALSE);

	if (DOM_Utils::GetOriginURL(rt).GetAttribute(URL::KName).CompareI("opera:debug") == 0)
		return TRUE;

	FramesDocument *doc = DOM_Utils::GetDocument(rt);

	if (doc && doc->GetWindow() && doc->GetWindow()->GetType() == WIN_TYPE_DEVTOOLS)
		return TRUE;

	return FALSE;
}

#ifdef JS_SCOPE_CLIENT
BOOL
OpSecurityManager_DOM::CheckScopeSecurity(const OpSecurityContext &source)
{
	DOM_Runtime *rt = source.GetRuntime();
	RETURN_VALUE_IF_NULL(rt, FALSE);

	FramesDocument *doc = DOM_Utils::GetDocument(rt);

	return (doc && doc->GetWindow()->GetType() == WIN_TYPE_DEVTOOLS);
}
#endif // JS_SCOPE_CLIENT

#ifdef SECMAN_ALLOW_DISABLE_XHR_ORIGIN_CHECK
OP_STATUS OpSecurityManager_DOM::SetSkipXHROriginCheck(DOM_Runtime* runtime, BOOL& state)
{
	if (allow_set_skip_origin_check)
	{
		DOM_Utils::SetRelaxedLoadSaveSecurity(runtime, state);
		return OpStatus::OK;
	}
	else
		return OpStatus::ERR;
}

void OpSecurityManager_DOM::AllowSetSkipXHROriginCheck(BOOL& allow)
{
	allow_set_skip_origin_check = allow;
}
#endif // ALLOW_DISABLE_XHR_ORIGIN_CHECK

BOOL OpSecurityManager_DOM::AllowedToNavigate(const OpSecurityContext& source, const OpSecurityContext& target)
{
	/* A browsing context 'source' is allowed to navigate a second browsing
	   context 'target' if one of the following conditions is true:

	   1. The origin of the active document of 'source' is the same as the
	   origin of the active document of 'target'. */

	if (OpSecurityManager::OriginCheck(target, source))
		return TRUE;

	BOOL target_is_secure = FALSE;

	if (target.GetURL().IsValid())
		switch (target.GetURL().GetAttribute(URL::KSecurityStatus))
		{
		case SECURITY_STATE_NONE:
		case SECURITY_STATE_UNKNOWN:
			break;

		default:
			target_is_secure = TRUE;
		}

	// Skip the rest if we don't have target's runtime.
	if (!target.GetRuntime())
		return FALSE;

	// Perform this step only if we have the runtime of source.
	if (source.GetRuntime())
	{
		/* 2. The browsing context 'source' is a nested browsing context and its
		   top-level browsing context is 'target'.*/

		FramesDocument* pdoc = DOM_Utils::GetDocument(source.GetRuntime());

		while (pdoc)
		{
			DocumentManager* doc_man = pdoc->GetDocManager();

			if (doc_man->GetParentDoc())
				pdoc = doc_man->GetParentDoc();
			else
				break;
		}

		if (pdoc == DOM_Utils::GetDocument(target.GetRuntime()))
			return TRUE;
	}

	/* 3. The browsing context 'target' is an auxiliary browsing context and 'source'
	   is allowed to navigate 'target''s opener browsing context.

	   Opera-specific extension to algorithm: don't allow this if target is
	   secure.  (Note: if source and target have the same security context,
	   point 1 above will already have allowed it.)

	   For web compatibility reasons, also allow 'source' to navigate 'target' if 'target' is
	   a top-level browsing context and is source's opener. As the 'target' top-level
	   browsing context will have UI security indicators (and address) of its own,
	   this extension is exempt from the only-secure restriction (CORE-41534.) */

	if (FramesDocument *target_doc = DOM_Utils::GetDocument(target.GetRuntime()))
	{
		URL opener = target_doc->GetWindow()->GetOpenerSecurityContext();

		if (!target_doc->GetParentDoc())
			if (!target_is_secure && AllowedToNavigate(source, opener))
				return TRUE;
			else if (DOM_Utils::GetDocument(source.GetRuntime())->GetWindow()->GetOpener() == target_doc)
				return TRUE;
	}

	/* 4. The browsing context 'target' is not a top-level browsing context, but
	   there exists an ancestor browsing context of 'target' whose active
	   document has the same origin as the active document of 'source'
	   (possibly in fact being 'source' itself). */
	FramesDocument* pdoc = DOM_Utils::GetDocument(target.GetRuntime());

	while (pdoc)
	{
		DocumentManager* doc_man = pdoc->GetDocManager();

		pdoc = doc_man->GetParentDoc();
		if (pdoc && OpSecurityManager::OriginCheck(source, pdoc))
			return TRUE;

		/* Opera-specific extension to algorithm: don't allow this if target is
		   secure.  (Note: if source and target have the same security context,
		   point 1 above will already have allowed it.) */
		if (target_is_secure)
			break;
	}

	return FALSE;
}

#ifdef EXTENSION_SUPPORT
OP_STATUS
OpSecurityManager_DOM::CheckExtensionSecurity(const OpSecurityContext& source, const OpSecurityContext& target, BOOL& allowed)
{
    OpGadget *owner = target.GetGadget();

    if (owner)
    {
        RETURN_IF_ERROR(g_secman_instance->CheckGadgetExtensionSecurity(source, target, allowed));
        if (!allowed)
            return OpStatus::OK;
    }

    FramesDocument *frames_doc = source.GetFramesDocument();
    Window *window = frames_doc->GetWindow();

    // Do not run on gadgets, dialogs, or devtools.
    if (window->GetGadget() || window->GetType() == WIN_TYPE_DIALOG || window->GetType() == WIN_TYPE_DEVTOOLS)
        allowed = FALSE;
    // Extension JS not allowed run in javascript: or opera: content.
    else if (target.GetURL().Type() == URL_OPERA || target.GetURL().Type() == URL_JAVASCRIPT)
        allowed = FALSE;
    else
        allowed = TRUE;

    return OpStatus::OK;
}
#endif // EXTENSION_SUPPORT

#if defined(DOCUMENT_EDIT_SUPPORT) && defined(USE_OP_CLIPBOARD)
BOOL
OpSecurityManager_DOM::CheckClipboardAccess(const OpSecurityContext& source)
{
	return g_pcjs->GetIntegerPref(PrefsCollectionJS::LetSiteAccessClipboard, source.GetURL()) ||
	       source.GetWindow() && source.GetWindow()->GetType() == WIN_TYPE_DEVTOOLS;
}
#endif // DOCUMENT_EDIT_SUPPORT && USE_OP_CLIPBOARD

#ifdef WEB_HANDLERS_SUPPORT
/* static */
double
OpSecurityManager_DOM::GetProtocolMatchRank(const OpString& protocol, const OpString& pattern)
{
	const double MIN_MATCH_RANK = 0.001;
	double match_rank = .0;
	int protocol_idx = 0;
	BOOL last_was_wildcard = FALSE;
	unsigned asterisk_cnt = 0;
	for (int idx = 0; idx < pattern.Length(); )
	{
		BOOL local_last_was_wildcard = last_was_wildcard;
		last_was_wildcard = FALSE;
		if (protocol_idx < protocol.Length() && protocol.DataPtr()[protocol_idx] == pattern.DataPtr()[idx])
		{
			++protocol_idx;
			++match_rank;
		}
		else if (pattern.DataPtr()[idx] == '*') // Count * wildcard match as 0.25 because it's very generic.
		{
			if (!local_last_was_wildcard)
				match_rank += 0.25;
			++asterisk_cnt;
			last_was_wildcard = TRUE;
		}
		else if (pattern.DataPtr()[idx] == '?') // Count ? wildcard match as 0.5 because it's less generic.
		{
			match_rank += 0.5;
			last_was_wildcard = TRUE;
		}
		else if (local_last_was_wildcard)
		{
			++protocol_idx;
			continue;
		}
		else
			++protocol_idx;

		++idx;
	}

	match_rank /= protocol.Length();
	double generic_coef = MAX(1, op_abs(protocol.Length() - pattern.Length())) * asterisk_cnt * 0.01;
	match_rank -= generic_coef;
	/* Use the minimal match rank in case the match rank goes below 0 (might happen when the protocol name is long
	   and the pattern is very short and have asterisks (i.e. is very generic) */
	match_rank = MAX(match_rank, MIN_MATCH_RANK);
	return match_rank;
}

OP_STATUS
OpSecurityManager_DOM::CheckWebHandlerSecurity(const OpSecurityContext& source, const OpSecurityContext& target, BOOL& allowed)
{
	allowed = FALSE;

	OpString protocol;
	RETURN_IF_ERROR(protocol.Set(target.GetText()));
	OP_ASSERT(protocol.HasContent());
	protocol.MakeLower();

#ifdef PREFS_HAS_PREFSFILE
	const OpFile* global_handlers_ignore_file = g_pcfiles->GetFile(PrefsCollectionFiles::HandlersIgnoreFile);
	BOOL global_exists = FALSE;
	if (global_handlers_ignore_file)
		RETURN_IF_ERROR(global_handlers_ignore_file->Exists(global_exists));

#ifdef PREFSFILE_CASCADE
	OpFileFolder ignore;
	const uni_char* filename;
	g_pcfiles->GetDefaultFilePref(PrefsCollectionFiles::HandlersIgnoreFile, &ignore, &filename);
	OpFile local_handlers_ignore_file;
	RETURN_IF_ERROR(local_handlers_ignore_file.Construct(filename, OPFILE_HOME_FOLDER));
	BOOL local_exists;
	RETURN_IF_ERROR(local_handlers_ignore_file.Exists(local_exists));
#endif // PREFSFILE_CASCADE

	if (global_exists
#ifdef PREFSFILE_CASCADE
		|| local_exists
#endif
		)
	{
#ifdef PREFSFILE_CASCADE
		PrefsFile handlers_ignore_prefs_file(PREFS_INI, local_exists && global_exists ? 1 : 0, 0);
		TRAPD(exception, LoadPrefsFileL(handlers_ignore_prefs_file, global_exists ? global_handlers_ignore_file : NULL, local_exists ? &local_handlers_ignore_file : NULL));
#else
		PrefsFile handlers_ignore_prefs_file(PREFS_INI);
		TRAPD(exception, LoadPrefsFileL(handlers_ignore_prefs_file, global_handlers_ignore_file, NULL));
#endif
		RETURN_IF_ERROR(exception);

		PrefsSection *section = NULL;
		TRAP(exception, section = handlers_ignore_prefs_file.ReadSectionL(UNI_L("Protocol")));
		RETURN_IF_ERROR(exception);
		RETURN_OOM_IF_NULL(section);
		OpAutoPtr<PrefsSection> ap_section(section);
		/* Algorithm: When no explicit match is found check all the entries of
		 * handlers-ignore.ini (there might be the entries with * or ? wildcards there).
		 * When some matching is found its rank is calculated and at the end the match
		 * with the highest rank is taken. The rank is calculated as follows: if a
		 * character at the same position in the pattern and the protocol name matches
		 * the rank is increased by 1. If the wildcard is encountered the rank is
		 * increased either by 0.5 (when '?' is encountered) or by 0.25 (when '*' is encountered).
		 * At the end the rank is divided by the length of the protocol name.
		 * It's also decreased by MAX(1, op_abs(protocol.Length() - pattern.Length())) * '*'-wildcard-count * 0.01
		 * because if a pattern is shorter and have more asterisks then it's more generic.
		 *
		 * Goals: This causes that the entires order in the file is not important
		 * because all entries are checked anyway. This is very significant in case of
		 * pref files cascades. The algorithm is not perfect but it's simple and it should
		 * do.
		 */
		const PrefsEntry *entry = section->FindEntry(protocol.CStr());
		if (entry)
		{
			allowed = uni_atoi(entry->Value()) != 0;
		}
		else
		{
			double highest_match_rank = .0;
			BOOL highest_match_rank_allowed = FALSE;
			entry = section->Entries();
			while (entry)
			{
				OP_ASSERT(entry->Key());
				OpString pattern;
				RETURN_IF_ERROR(pattern.Set(entry->Key()));
				pattern.MakeLower();
				if (OpGlob(protocol.CStr(), pattern.CStr(), FALSE)) // Some match has been found. Check how good it is (approximately).
				{
					double match_rank = GetProtocolMatchRank(protocol, pattern);
					int entry_val = uni_atoi(entry->Value());
					// Take higher rank's value or restrictive one in case of equal ranks.
					if (highest_match_rank < match_rank ||
					    (highest_match_rank == match_rank && entry_val == 0))
					{
						highest_match_rank = match_rank;
						highest_match_rank_allowed = entry_val != 0;
					}
				}
				entry = entry->Suc();
			};

			allowed = highest_match_rank_allowed;
		}

		handlers_ignore_prefs_file.Flush();
	}
#endif // PREFS_HAS_PREFSFILE

	return OpStatus::OK;
}
#endif // WEB_HANDLERS_SUPPORT

#ifdef DOM_WEBWORKERS_SUPPORT

/* Returns TRUE if a path separator character */
#define IS_LIKELY_PATH_SEP_CHARACTER(ch) ((ch) == '/' || (ch) == '\\' || (ch) == PATHSEPCHAR)

OP_STATUS
OpSecurityManager_DOM::CheckWorkerScriptImport(const OpSecurityContext& source, const OpSecurityContext& target, BOOL& allowed)
{
	allowed = FALSE;

	URLType target_type = target.GetURL().Type();
	switch (source.GetURL().Type())
	{
	case URL_HTTP:
	case URL_HTTPS:
	case URL_DATA:
		allowed = target_type == URL_HTTP || target_type == URL_HTTPS || target_type == URL_DATA;
		break;

	case URL_FILE:
		if (target_type == URL_FILE)
		{
			/* The target must be at or below the source/worker's directory. */

			OpString worker_file, target_file;

			{
				OpString worker_string, target_string;

				RETURN_IF_ERROR(source.GetURL().GetAttribute(URL::KUniName_Username_Password_Escaped_NOT_FOR_UI, worker_string));
				RETURN_IF_ERROR(target.GetURL().GetAttribute(URL::KUniName_Username_Password_Escaped_NOT_FOR_UI, target_string));

				RETURN_OOM_IF_NULL(worker_file.Reserve(worker_string.Length() + 1));
				UriUnescape::Unescape(worker_file.CStr(), worker_string.CStr(), UriUnescape::ExceptUnsafeCtrl);

				RETURN_OOM_IF_NULL(target_file.Reserve(target_string.Length() + 1));
				UriUnescape::Unescape(target_file.CStr(), target_string.CStr(), UriUnescape::ExceptUnsafeCtrl);
			}

			unsigned length = worker_file.Length();
			const uni_char *str = worker_file.CStr();
			while (length != 0)
				if (str[length - 1] == '/')
					break;
				else
					--length;

			allowed = length > 0 && length < static_cast<unsigned>(target_file.Length()) &&
			          uni_strncmp(worker_file.CStr(), target_file.CStr(), length) == 0;

			if (allowed)
			{
				const uni_char *dotdot = target_file.CStr() + length;
				while (dotdot && allowed)
				{
					dotdot = uni_strstr(dotdot, UNI_L(".."));
					if (dotdot && (IS_LIKELY_PATH_SEP_CHARACTER(dotdot[-1]) && IS_LIKELY_PATH_SEP_CHARACTER(dotdot[2])))
						allowed = FALSE;
					else
						dotdot = dotdot ? dotdot + 1 : NULL;
				}
			}
		}
		else
			allowed = target_type == URL_HTTP || target_type == URL_HTTPS || target_type == URL_DATA;
		break;
	default:
		break;
	}

	return OpStatus::OK;
}
#undef IS_LIKELY_PATH_SEP_CHARACTER
#endif // DOM_WEBWORKERS_SUPPORT
