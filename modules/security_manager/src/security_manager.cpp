/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

/** @file security_manager.cpp Cross-module security manager. */

#include "core/pch.h"
#include "modules/doc/frm_doc.h"
#include "modules/dochand/win.h"
#include "modules/dom/domutils.h"
#include "modules/security_manager/include/security_manager.h"
#include "modules/url/url_man.h"
#include "modules/url/protocols/scomm.h"
#include "modules/url/protocols/comm.h"
#include "modules/util/opstring.h"
#ifdef GADGET_SUPPORT
#  include "modules/gadgets/OpGadgetClass.h"
#  include "modules/gadgets/OpGadget.h"
#endif
#include "modules/encodings/utility/charsetnames.h"

#ifdef DOM_ADDON_SUPPORT
#  include "modules/security_manager/src/dom_addon_manager.h"
#endif // DOM_ADDON_SUPPORT

/* Security manager global variables */

SecurityManagerModule::SecurityManagerModule()
	: secman_instance(NULL)
{
}

/* virtual */ void
SecurityManagerModule::InitL(const OperaInitInfo& info)
{
	secman_instance = OP_NEW_L(OpSecurityManager, ());
	secman_instance->ConstructL();
}

/* virtual */ void
SecurityManagerModule::Destroy()
{
	OP_DELETE(secman_instance);
	secman_instance = NULL;
}

/* Security contexts */

OpSecurityContext::OpSecurityContext(const URL& url)
{
	Init();
	this->url = url;
}

OpSecurityContext::OpSecurityContext(const URL& url, const URL& ref_url)
{
	Init();
	this->url = url;
	this->ref_url = ref_url;
}

#ifdef GADGET_SUPPORT
OpSecurityContext::OpSecurityContext(OpGadget *gadget)
{
	Init();
	OP_ASSERT(gadget);
	this->gadget = gadget;
	this->window = gadget->GetWindow();
}

OpSecurityContext::OpSecurityContext(OpGadget *gadget, Window *window)
{
	Init();
	OP_ASSERT(gadget);
	this->gadget = gadget;
	this->window = window;
}
#endif // GADGET_SUPPORT

#if defined(DOCUMENT_EDIT_SUPPORT) && defined(USE_OP_CLIPBOARD)
OpSecurityContext::OpSecurityContext(const URL &url, Window *window)
{
	Init();
	this->url = url;
	this->window = window;
}
#endif // DOCUMENT_EDIT_SUPPORT && USE_OP_CLIPBOARD

OpSecurityContext::OpSecurityContext(DOM_Runtime* origining_runtime)
{
	Init();
	this->origining_runtime = origining_runtime;

	FramesDocument* fd = DOM_Utils::GetDocument(origining_runtime);

	if (fd != NULL)
	{
		this->url = fd->GetSecurityContext();
		this->window = fd->GetWindow();
#ifdef GADGET_SUPPORT
		this->gadget = window->GetGadget();
#endif // GADGET_SUPPORT
	}
	else
	{
		/* Runtimes for web workers and extensions do not have a
		   FramesDocument, use the runtime's origin URL instead.
		   Store and reference that URL via a local field. */
		this->runtime_origin_url = DOM_Utils::GetOriginURL(origining_runtime);
		this->url = this->runtime_origin_url;
#if defined(DOM_WEBWORKERS_SUPPORT) && defined(GADGET_SUPPORT)
		FramesDocument* owner_fd = DOM_Utils::GetWorkerOwnerDocument(origining_runtime);
		if (owner_fd)
		{
			Window* window = owner_fd->GetWindow();
			OP_ASSERT(window);
			this->gadget = window->GetGadget();
			return;
		}
#endif // defined(DOM_WEBWORKERS_SUPPORT) && defined(GADGET_SUPPORT)
#ifdef EXTENSION_SUPPORT
		FramesDocument* shared_env_fd = DOM_Utils::GetExtensionUserJSOwnerDocument(origining_runtime);
		if (shared_env_fd)
		{
			OP_ASSERT(shared_env_fd->GetWindow());
			this->gadget = DOM_Utils::GetRuntimeGadget(origining_runtime);
		}
#endif // EXTENSION_SUPPORT
	}
}

#ifdef CORS_SUPPORT
OpSecurityContext::OpSecurityContext(DOM_Runtime* origining_runtime, OpCrossOrigin_Request *cross_origin_request)
{
	Init();
	this->origining_runtime = origining_runtime;
	this->cross_origin_request = cross_origin_request;

	FramesDocument* fd = DOM_Utils::GetDocument(origining_runtime);
	if (fd != NULL)
	{
		this->url = fd->GetSecurityContext();
		this->window = fd->GetWindow();
#ifdef GADGET_SUPPORT
		this->gadget = window->GetGadget();
#endif // GADGET_SUPPORT
	}
	else
	{
		/* Runtimes for web workers and extensions do not have a
		   FramesDocument, use the runtime's origin URL instead.
		   Store and reference that URL via a local field. */
		this->runtime_origin_url = DOM_Utils::GetOriginURL(origining_runtime);
		this->url = this->runtime_origin_url;
#if defined(DOM_WEBWORKERS_SUPPORT) && defined(GADGET_SUPPORT)
		FramesDocument* owner_fd = DOM_Utils::GetWorkerOwnerDocument(origining_runtime);
		if (owner_fd)
		{
			Window* window = owner_fd->GetWindow();
			OP_ASSERT(window);
			this->gadget = window->GetGadget();
		}
#endif // defined(DOM_WEBWORKERS_SUPPORT) && defined(GADGET_SUPPORT)
	}
}

OpSecurityContext::OpSecurityContext(const URL& url, OpCrossOrigin_Request *cross_origin_request)
{
	Init();
	this->url = url;
	this->cross_origin_request = cross_origin_request;
}
#endif // CORS_SUPPORT

OpSecurityContext::OpSecurityContext(FramesDocument* frames_doc)
{
	OP_ASSERT(frames_doc);

	Init();
	if (ES_Runtime *runtime = frames_doc->GetESRuntime())
		this->origining_runtime = DOM_Utils::GetDOM_Runtime(runtime);

	this->url = frames_doc->GetSecurityContext();
	this->doc = frames_doc;
	this->window = frames_doc->GetWindow();
}

OpSecurityContext::OpSecurityContext(const URL &url, InlineResourceType inline_type, BOOL from_user_css)
{
	Init();
	this->url = url;
	this->inline_type = inline_type;
	this->from_user_css = from_user_css;
}

#ifdef SECMAN_DOMOBJECT_ACCESS_RULES
OpSecurityContext::OpSecurityContext(const char* dom_access_rule, const uni_char* argument1, const uni_char* argument2)
: OpSecurityContext_DOMObject(dom_access_rule, argument1, argument2)
{
	Init();
}
#endif // SECMAN_DOMOBJECT_ACCESS_RULES

OpSecurityContext::OpSecurityContext(const URL &url, DocumentManager* docman)
{
	OP_ASSERT(docman);

	Init();
	this->document_manager = docman;
	this->url = url;

#ifdef GADGET_SUPPORT
	if (Window *w = docman->GetWindow())
	{
		this->window = w;
		this->gadget = w->GetGadget();
	}
#endif // GADGET_SUPPORT
}

#ifdef EXTENSION_SUPPORT
OpSecurityContext::OpSecurityContext(const URL &url, OpGadget *gadget)
{
	Init();
	this->gadget = gadget;
	this->url = url;
}
#endif // EXTENSION_SUPPORT

OpSecurityContext::OpSecurityContext(const URL &url, const char *charset, BOOL inline_element)
{
	OP_ASSERT(charset != NULL);

	Init();
	this->url = url;
	m_charset = charset;
	this->inline_element = inline_element;
}

OpSecurityContext::~OpSecurityContext()
{
	/* Nothing yet */
}

void
OpSecurityContext::Init()
{
	text_info = NULL;
	origining_runtime = NULL;
	m_charset = NULL;
	window = NULL;
#ifdef CORS_SUPPORT
	OpSecurityContext_CrossOrigin::Init();
#endif // CORS_SUPPORT
}

void OpSecurityContext::AddText(const uni_char* info)
{
	text_info = info;
}

const URL&
OpSecurityContext::GetURL() const
{
	return url;
}

const URL&
OpSecurityContext::GetRefURL() const
{
	return ref_url;
}

DOM_Runtime*
OpSecurityContext::GetRuntime() const
{
	return origining_runtime;
}

/* Security state */

OpSecurityState::OpSecurityState()
	: suspended(FALSE)
	, host_resolving_comm(NULL)
{
}

OpSecurityState::~OpSecurityState()
{
	if (host_resolving_comm != NULL)
	{
		SComm::SafeDestruction( host_resolving_comm );
		host_resolving_comm = NULL;
	}
}


/* Security manager */

OpSecurityManager::OpSecurityManager()
{
}

OpSecurityManager::~OpSecurityManager()
{
}

void
OpSecurityManager::ConstructL()
{
#ifdef GADGET_SUPPORT
	OpSecurityManager_Gadget::ConstructL();
#endif // GADGET_SUPPORT
#ifdef SECMAN_DOMOBJECT_ACCESS_RULES
	OpSecurityManager_DOMObject::ConstructL();
#endif // SECMAN_DOMOBJECT_ACCESS_RULES
}

/* static */ OP_STATUS
OpSecurityManager::CheckSecurity(OpSecurityManager::Operation op, const OpSecurityContext& source, const OpSecurityContext& target, BOOL& allowed)
{
	OpSecurityState dummy;
	return CheckSecurity(op, source, target, allowed, dummy);
}

/* static */ OP_STATUS
OpSecurityManager::CheckSecurity(OpSecurityManager::Operation op, const OpSecurityContext& source, const OpSecurityContext& target, BOOL& allowed, OpSecurityState& state)
{
	OP_STATUS status = OpStatus::ERR;
	allowed = FALSE;
	if (g_secman_instance != NULL)
	{
#ifdef SELFTEST
		OpSecurityManager::PrivilegedBlock* priv = static_cast<OpSecurityManager::PrivilegedBlock*>(g_secman_instance->m_privileged_blocks.Last());
		if (priv)
		{
			allowed = priv->AllowOperation(op);
			if (allowed)
				return OpStatus::OK;
		}
#endif // SELFTEST

		switch (op)
		{
		case DOM_STANDARD:
#ifdef CORS_SUPPORT
			if (OpCrossOrigin_Request *request = source.GetCrossOriginRequest())
				if (!request->HasCompleted(allowed))
					return g_secman_instance->CheckCrossOriginSecurity(source, target, allowed);
#endif // CORS_SUPPORT
			if (!allowed)
				allowed = OriginCheck(source, target);
			status = OpStatus::OK;
			break;

		case DOM_LOADSAVE:
			status = g_secman_instance->CheckLoadSaveSecurity(source, target, allowed, state);
			break;

		case DOM_ALLOWED_TO_NAVIGATE:
			allowed = g_secman_instance->AllowedToNavigate(source, target);
			status = OpStatus::OK;
			break;

		case DOM_OPERA_CONNECT:
			allowed = g_secman_instance->CheckOperaConnectSecurity(source);
			status = OpStatus::OK;
			break;


#ifdef JS_SCOPE_CLIENT
		case DOM_SCOPE:
			allowed = g_secman_instance->CheckScopeSecurity(source);
			return OpStatus::OK;
#endif // JS_SCOPE_CLIENT

		case PLUGIN_RUNSCRIPT:
#ifdef _PLUGIN_SUPPORT_
			status = g_secman_instance->CheckPluginSecurityRunscript(source, target, allowed);
#endif
			break;

		case DOC_SET_PREFERRED_CHARSET:
			status = g_secman_instance->CheckPreferredCharsetSecurity(source, target, allowed);
			break;

		case DOC_INLINE_LOAD:
			status = g_secman_instance->CheckInlineSecurity(source, target, allowed, state);
			break;

#ifdef XSLT_SUPPORT
		case XSLT_IMPORT_OR_INCLUDE:
		case XSLT_DOCUMENT:
			status = g_secman_instance->CheckXSLTSecurity(source, target, allowed);
#endif // XSLT_SUPPORT
			break;

		case GADGET_EXTERNAL_URL:
#ifdef GADGET_SUPPORT
			status = g_secman_instance->CheckGadgetExternalUrlSecurity(source, target, allowed);
#else
			allowed = TRUE;
			status = OpStatus::OK;
#endif // GADGET_SUPPORT
			break;

		case GADGET_ALLOWED_TO_NAVIGATE:
#ifdef GADGET_SUPPORT
			status = g_secman_instance->GadgetAllowedToNavigate(source, target, allowed);
#else
			allowed = TRUE;
			status = OpStatus::OK;
#endif // GADGET_SUPPORT
			break;

		case GADGET_JSPLUGINS:
#ifdef GADGET_SUPPORT
			status = g_secman_instance->CheckGadgetJSPluginSecurity(source, target, allowed);
#else
			allowed = TRUE;
			status = OpStatus::OK;
#endif // GADGET_SUPPORT
			break;

		case GADGET_DOM:
#ifdef GADGET_SUPPORT
			status = g_secman_instance->CheckGadgetDOMSecurity(source, target, allowed);
#else
			allowed = FALSE;
			status = OpStatus::OK;
#endif // GADGET_SUPPORT
			break;

#ifdef GADGETS_MUTABLE_POLICY_SUPPORT
		case GADGET_DOM_MUTATE_POLICY:
			status = g_secman_instance->CheckGadgetMutatePolicySecurity(source, target, allowed);
#endif // GADGETS_MUTABLE_POLICY_SUPPORT
			break;

		case UNITE_STANDARD:
#ifdef WEBSERVER_SUPPORT
			status = g_secman_instance->CheckUniteSecurity(source, target, allowed);
#else
			allowed = TRUE;
			status = OpStatus::OK;
#endif // WEBSERVER_SUPPORT
			break;

#ifdef GEOLOCATION_SUPPORT
		case GEOLOCATION:
			OP_ASSERT(!"You should be using some of the other variants of CheckSecurity");
			break;
#endif // GEOLOCATION_SUPPORT

#ifdef EXTENSION_SUPPORT
		case DOM_EXTENSION_ALLOW:
			status = g_secman_instance->CheckExtensionSecurity(source, target, allowed);
			break;
#endif // EXTENSION_SUPPORT

#if defined(DOCUMENT_EDIT_SUPPORT) && defined(USE_OP_CLIPBOARD)
		case DOM_CLIPBOARD_ACCESS:
			allowed = g_secman_instance->CheckClipboardAccess(source);
			status = OpStatus::OK;
			break;
#endif // DOCUMENT_EDIT_SUPPORT && USE_OP_CLIPBOARD

		case GADGET_MANAGER_ACCESS:
#ifdef GADGET_SUPPORT
			status = g_secman_instance->CheckHasGadgetManagerAccess(source, allowed);
#else
			allowed = TRUE;
			status = OpStatus::OK;
#endif // GADGET_SUPPORT
			break;

#ifdef WEB_HANDLERS_SUPPORT
		case WEB_HANDLER_REGISTRATION:
			status = g_secman_instance->CheckWebHandlerSecurity(source, target, allowed);
			break;
#endif // WEB_HANDLERS_SUPPORT

#ifdef CORS_SUPPORT
		case CORS_PRE_ACCESS_CHECK:
			status = g_secman_instance->CheckCrossOriginAccessible(source, target, allowed);
			break;

		case CORS_RESOURCE_ACCESS:
			status = g_secman_instance->CheckCrossOriginAccess(source, target, allowed);
			break;
#endif // CORS_SUPPORT

#ifdef DOM_WEBWORKERS_SUPPORT
		case WEB_WORKER_SCRIPT_IMPORT:
			status = g_secman_instance->CheckWorkerScriptImport(source, target, allowed);
			break;
#endif // DOM_WEBWORKERS_SUPPORT

		default:
			OP_ASSERT(!"Unknown security operation");
			break;
		}
	}
	else
		status = OpStatus::OK;

	return status;
}

/* static */ OP_STATUS
OpSecurityManager::CheckSecurity(OpSecurityManager::Operation op, const OpSecurityContext& source, const OpSecurityContext& target, OpSecurityCheckCallback* callback)
{
	return CheckSecurity(op, source, target, callback, NULL);
}

/* static */ OP_STATUS
OpSecurityManager::CheckSecurity(OpSecurityManager::Operation op, const OpSecurityContext& source, const OpSecurityContext& target, OpSecurityCheckCallback* callback, OpSecurityCheckCancel **cancel)
{
	if (cancel)
		*cancel = NULL;

	if (g_secman_instance != NULL)
	{
#ifdef SELFTEST
		OpSecurityManager::PrivilegedBlock* priv = static_cast<OpSecurityManager::PrivilegedBlock*>(g_secman_instance->m_privileged_blocks.Last());
		if (priv)
		{
			if (priv->AllowOperation(op))
			{
				callback->OnSecurityCheckSuccess(TRUE);
				return OpStatus::OK;
			}
		}
#endif // SELFTEST

		switch (op)
		{
#ifdef SECMAN_DOMOBJECT_ACCESS_RULES
		case DOM_ACCESS_PROPERTY:
			return g_secman_instance->CheckDOMObjectSecurity(source, target, callback, ACCESS_TYPE_PROPERTY_ACCESS);
			break;
		case DOM_INVOKE_FUNCTION:
			return g_secman_instance->CheckDOMObjectSecurity(source, target, callback, ACCESS_TYPE_FUNCTION_CALL);
			break;
#endif // SECMAN_DOMOBJECT_ACCESS_RULES

		case DOM_LOADSAVE:
			return g_secman_instance->CheckLoadSaveSecurity(source, target, callback, cancel);

		case DOM_STANDARD:
		{
			BOOL allowed = FALSE;
#ifdef CORS_SUPPORT
			if (OpCrossOrigin_Request *request = source.GetCrossOriginRequest())
				if (!request->HasCompleted(allowed))
					return g_secman_instance->CheckCrossOriginSecurity(source, target, callback, cancel);
#endif // CORS_SUPPORT
			if (!allowed)
				allowed = OriginCheck(source, target);
			callback->OnSecurityCheckSuccess(allowed);
			return OpStatus::OK;
		}

		case GADGET_STANDARD:
#ifdef GADGET_SUPPORT
			return g_secman_instance->CheckGadgetSecurity(source, target, callback);
#else
			callback->OnSecurityCheckSuccess(TRUE);
			return OpStatus::OK;
#endif // GADGET_SUPPORT

		case DOCMAN_URL_LOAD:
			return g_secman_instance->CheckDocManUrlLoadingSecurity(source, target, callback);

#ifdef GEOLOCATION_SUPPORT
		case GEOLOCATION:
			return g_secman_instance->CheckGeolocationSecurity(source, callback, cancel);
#endif // GEOLOCATION_SUPPORT

#ifdef DOM_STREAM_API_SUPPORT
		case GET_USER_MEDIA:
			return g_secman_instance->CheckCameraSecurity(source, callback, cancel);
#endif // DOM_STREAM_API_SUPPORT
#ifdef EXTENSION_SUPPORT
		case DOM_EXTENSION_SCREENSHOT:
			{
				// note: If we decide to actually ask user for permission on taking a screenshot then the code
				//       below should be moved to OpSecurityManager_UserConsent.
				if (!source.GetGadget()->GetAttribute(WIDGET_EXTENSION_SCREENSHOT_SUPPORT))
					callback->OnSecurityCheckSuccess(FALSE);
				else
				{
					BOOL allowed;
					OpSecurityContext gadget_context(target.GetURL(), source.GetGadget());
					g_secman_instance->CheckGadgetExtensionSecurity(target, gadget_context, allowed);
					callback->OnSecurityCheckSuccess(allowed);
				}
				return OpStatus::OK;
			}
#endif // EXTENSION_SUPPORT

		default:
			OP_ASSERT(!"Unknown security operation");
			return OpStatus::ERR_NOT_SUPPORTED;
		}
	}

	return OpStatus::OK;
}

#ifdef SELFTEST

OpSecurityManager::PrivilegedBlock::PrivilegedBlock()
{
	this->Into(&g_secman_instance->m_privileged_blocks);
}

OpSecurityManager::PrivilegedBlock::~PrivilegedBlock()
{
	this->Out();
}

/* virtual */ BOOL
OpSecurityManager::PrivilegedBlock::AllowOperation(Operation op)
{
	return TRUE;
}

#endif // SELFTEST

/* virtual */
OpSecurityCallbackHelper::~OpSecurityCallbackHelper()
{
}

/* virtual */ void
OpSecurityCallbackHelper::OnSecurityCheckSuccess(BOOL allowed, ChoicePersistenceType type)
{
	is_allowed = allowed;
	Finish();
}

/* virtual */ void
OpSecurityCallbackHelper::OnSecurityCheckError(OP_STATUS status)
{
	error = status;
	Finish();
}

void
OpSecurityCallbackHelper::Cancel()
{
	owner = NULL;
}

void
OpSecurityCallbackHelper::Reset()
{
	is_finished = FALSE;
	is_allowed = FALSE;
}

void
OpSecurityCallbackHelper::Finish()
{
	if (!is_finished)
	{
		is_finished = TRUE;
		if (owner)
			owner->SecurityCheckCompleted(is_allowed, error);
	}
	if (!owner)
		OP_DELETE(this);
}

void
OpSecurityCallbackHelper::Release()
{
	/* If the security callback hasn't completed, cancel it
	   so that it cleans up after itself upon being called
	   by the security manager. If it has completed, safe
	   to delete as owner has declared non-interest. */
	if (!IsFinished())
		Cancel();
	else
		OP_DELETE(this);
}
