/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef GADGET_SUPPORT

#include "modules/doc/frm_doc.h"
#include "modules/dochand/win.h"
#include "modules/security_manager/include/security_manager.h"
#include "modules/security_manager/src/security_gadget_representation.h"
#include "modules/security_manager/src/security_utilities.h"
#include "modules/xmlutils/xmlfragment.h"
#include "modules/xmlutils/xmlnames.h"
#include "modules/gadgets/OpGadgetManager.h"
#include "modules/gadgets/OpGadgetClass.h"
#include "modules/gadgets/OpGadget.h"
#include "modules/url/url_man.h"
#include "modules/url/protocols/comm.h"
#include "modules/util/glob.h"

#ifdef SIGNED_GADGET_SUPPORT
# include "modules/libcrypto/include/CryptoCertificate.h"
# include "modules/libcrypto/include/CryptoCertificateStorage.h"
#endif // SIGNED_GADGET_SUPPORT

#ifdef DOM_JIL_API_SUPPORT
# include "modules/device_api/jil/JILFSMgr.h"
#endif // DOM_JIL_API_SUPPORT

/*************************************************************************
 **
 ** OpSecurityManager_Gadget
 **
 *************************************************************************/

OpSecurityManager_Gadget::OpSecurityManager_Gadget()
	: unsigned_policy(NULL)
#ifdef SIGNED_GADGET_SUPPORT
	, signed_policy(NULL)
# ifdef SECMAN_PRIVILEGED_GADGET_CERTIFICATE
	, privileged_certificate(NULL)
# endif // SECMAN_PRIVILEGED_GADGET_CERTIFICATE
#endif // SIGNED_GADGET_SUPPORT
{
}

OpSecurityManager_Gadget::~OpSecurityManager_Gadget()
{
	gadget_security_policies.Clear();
	OP_DELETE(unsigned_policy);
#ifdef SIGNED_GADGET_SUPPORT
	OP_DELETE(signed_policy);
# ifdef SECMAN_PRIVILEGED_GADGET_CERTIFICATE
    OP_DELETE(privileged_certificate);
# endif // SECMAN_PRIVILEGED_GADGET_CERTIFICATE
#endif // SIGNED_GADGET_SUPPORT
}

/* virtual */
void OpSecurityManager_Gadget::ConstructL()
{
#ifndef SECMAN_SIGNED_GADGETS_ONLY
	LEAVE_IF_ERROR(SetDefaultGadgetPolicy(&unsigned_policy));
# ifdef SIGNED_GADGET_SUPPORT
	LEAVE_IF_ERROR(SetDefaultSignedGadgetPolicy(&signed_policy));
# endif // SIGNED_GADGET_SUPPORT
#else // SECMAN_SIGNED_GADGETS_ONLY
	LEAVE_IF_ERROR(SetRestrictedGadgetPolicy());
#endif // SECMAN_SIGNED_GADGETS_ONLY

#ifdef SIGNED_GADGET_SUPPORT
	OpStatus::Ignore(ReadAndParseGadgetSecurityMap());
#endif // SIGNED_GADGET_SUPPORT
}

/* static */ OP_STATUS
OpSecurityManager_Gadget::SetGlobalGadgetPolicy(XMLFragment* policy)
{
	GadgetSecurityPolicy* sec_pol = NULL;
	RETURN_IF_ERROR(GadgetSecurityPolicy::Make(&sec_pol, GadgetSecurityPolicy::GLOBAL));
	OpStackAutoPtr<GadgetSecurityPolicy> policy_ptr(sec_pol);
	OpString error_reason;
	RETURN_IF_ERROR(ParseGadgetPolicy(policy, ALLOW_BLACKLIST|ALLOW_ACCESS|ALLOW_CONTENT|ALLOW_PRIVATE_NETWORK, sec_pol, error_reason));
	OP_DELETE(g_secman_instance->unsigned_policy);
	g_secman_instance->unsigned_policy = sec_pol;

	policy_ptr.release();

	return OpStatus::OK;
}

#ifdef GADGETS_MUTABLE_POLICY_SUPPORT
/* static */ OP_STATUS
OpSecurityManager_Gadget::SetGlobalGadgetPolicy(OpGadget* gadget, XMLFragment* policy)
{
	GadgetSecurityPolicy* sec_pol = NULL;
	RETURN_IF_ERROR(GadgetSecurityPolicy::Make(&sec_pol, GadgetSecurityPolicy::GLOBAL));
	OpStackAutoPtr<GadgetSecurityPolicy> policy_ptr(sec_pol);
	OpString unused;
	RETURN_IF_ERROR(ParseGadgetPolicy(policy, ALLOW_BLACKLIST|ALLOW_ACCESS|ALLOW_CONTENT|ALLOW_PRIVATE_NETWORK, sec_pol, unused));

	// This will fail if we're not allowed to do this.
	RETURN_IF_ERROR(gadget->SetGlobalSecurityPolicy(sec_pol));

	policy_ptr.release();

	return OpStatus::OK;
}

/* static */ OP_STATUS
OpSecurityManager_Gadget::SetGlobalGadgetPolicy(OpGadget* gadget, const uni_char* xml)
{
	XMLFragment f;
	RETURN_IF_ERROR(f.Parse(xml));
	f.EnterElement(UNI_L("widgets"));
	f.EnterElement(UNI_L("security"));

	return OpSecurityManager_Gadget::SetGlobalGadgetPolicy(gadget, &f);
}

/* static */ void
OpSecurityManager_Gadget::ClearGlobalGadgetPolicy(OpGadget* gadget)
{
	OpStatus::Ignore(gadget->SetGlobalSecurityPolicy(NULL));
}
#endif // GADGETS_MUTABLE_POLICY_SUPPORT

/* static */ OP_STATUS
OpSecurityManager_Gadget::AddGadgetPolicy(OpGadgetClass* gadget, OpGadgetAccess* policy, OpString& error_reason)
{
	if (!gadget->SupportsNamespace(GADGETNS_W3C_1_0))
	{
		OP_ASSERT(!"Do not call this function for non w3c widgets");
		return OpStatus::ERR_NOT_SUPPORTED;
	}

	GadgetSecurityPolicy* sec_pol = gadget->GetSecurity();
	if (!sec_pol)
	{
		RETURN_IF_ERROR(GadgetSecurityPolicy::Make(&sec_pol, GadgetSecurityPolicy::GADGET));
		gadget->SetSecurity(sec_pol);
	}

	// At this point we're already inside the <access> element.
	return ParseGadgetPolicyElement(policy, ALLOW_W3C_ACCESS, sec_pol, error_reason);
}

/* static */ OP_STATUS
OpSecurityManager_Gadget::SetGadgetPolicy(OpGadgetClass* gadget, XMLFragment* policy, OpString& error_reason)
{
	GadgetSecurityPolicy* sec_pol = NULL;
	RETURN_IF_ERROR(GadgetSecurityPolicy::Make(&sec_pol, GadgetSecurityPolicy::GADGET));
	OpStackAutoPtr<GadgetSecurityPolicy> policy_ptr(sec_pol);

	RETURN_IF_ERROR(ParseGadgetPolicy(policy, ALLOW_ACCESS|ALLOW_CONTENT, sec_pol, error_reason));
	gadget->SetSecurity(sec_pol);

	policy_ptr.release();

	return OpStatus::OK;
}

/* static */ OP_STATUS
OpSecurityManager_Gadget::SetDefaultGadgetPolicy(OpGadgetClass* gadget)
{
	GadgetSecurityPolicy* default_policy = NULL;
	RETURN_IF_ERROR(GadgetSecurityPolicy::Make(&default_policy, GadgetSecurityPolicy::GADGET));
	OpStackAutoPtr<GadgetSecurityPolicy> policy_ptr(default_policy);

	// Access
	RETURN_IF_ERROR(HelpConstructGadgetPolicy(&default_policy->access->entries, GadgetActionUrl::FIELD_PROTOCOL, GadgetActionUrl::DATA_NONE, UNI_L("http")));
	RETURN_IF_ERROR(HelpConstructGadgetPolicy(&default_policy->access->entries, GadgetActionUrl::FIELD_PROTOCOL, GadgetActionUrl::DATA_NONE, UNI_L("https")));
	RETURN_IF_ERROR(HelpConstructGadgetPolicy(&default_policy->access->entries, GadgetActionUrl::FIELD_PROTOCOL, GadgetActionUrl::DATA_NONE, UNI_L("mailto")));
	RETURN_IF_ERROR(HelpConstructGadgetPolicy(&default_policy->access->entries, GadgetActionUrl::FIELD_PROTOCOL, GadgetActionUrl::DATA_NONE, UNI_L("unite")));

	// Content
	default_policy->file_content = NO;
	default_policy->webserver_content = NO;
	default_policy->plugin_content = NO;

	// Private-network
	default_policy->private_network->allow = GadgetPrivateNetwork::NONE;

	gadget->SetSecurity(default_policy);
	policy_ptr.release();

	return OpStatus::OK;
}

/* static */ OP_STATUS
OpSecurityManager_Gadget::PostProcess(OpGadgetClass *gadget, OpString& error_reason)
{
	GadgetSecurityPolicy *sec_pol = gadget->GetSecurity();
	if (!sec_pol)
	{
		if (gadget->SupportsNamespace(GADGETNS_W3C_1_0))
		{
			// Create an EMPTY security policy for W3C widgets
			RETURN_IF_ERROR(GadgetSecurityPolicy::Make(&sec_pol, GadgetSecurityPolicy::GADGET));
			gadget->SetSecurity(sec_pol);
		}
		else
		{
			RETURN_IF_ERROR(OpSecurityManager::SetDefaultGadgetPolicy(gadget));
			sec_pol = gadget->GetSecurity();
		}
	}

	int allow = (gadget->GetAttribute(WIDGET_ACCESS_ATTR_NETWORK) & GADGET_NETWORK_PRIVATE ? GadgetPrivateNetwork::INTRANET : 0) |
				(gadget->GetAttribute(WIDGET_ACCESS_ATTR_NETWORK) & GADGET_NETWORK_PUBLIC ? GadgetPrivateNetwork::INTERNET : 0);
	if (allow)
		sec_pol->private_network->allow = allow;
	sec_pol->file_content = gadget->GetAttribute(WIDGET_ACCESS_ATTR_FILES) ? YES : NO;
	sec_pol->webserver_content = gadget->GetAttribute(WIDGET_ACCESS_ATTR_WEBSERVER) ? YES : NO;

	if (gadget->SupportsNamespace(GADGETNS_W3C_1_0) && !sec_pol->access->IsAnyAccessAllowed()  && !gadget->GetAttribute(WIDGET_JIL_ATTR_ACCESS_NETWORK))
		sec_pol->restricted = TRUE; // In the W3C widget-access spec; if you have no access elements, you will not get any network access

	return OpStatus::OK;
}

/* static */ OP_STATUS
OpSecurityManager_Gadget::HelpConstructGadgetPolicy(Head *list, GadgetActionUrl::FieldType ft, GadgetActionUrl::DataType dt, const uni_char* value)
{
	GadgetSecurityDirective *directive;
	GadgetActionUrl *action;
	Head *list2 = NULL;

	RETURN_IF_ERROR(GadgetSecurityDirective::Make(&directive));

	switch (ft)
	{
	case GadgetActionUrl::FIELD_HOST:
		list2 = &directive->host;
		break;
	case GadgetActionUrl::FIELD_PROTOCOL:
		list2 = &directive->protocol;
		break;
	case GadgetActionUrl::FIELD_PATH:
		list2 = &directive->path;
		break;
	case GadgetActionUrl::FIELD_PORT:
		list2 = &directive->port;
		break;
	default:
		OP_DELETE(directive);
		return OpStatus::ERR;
	};

	if (OpStatus::IsError(GadgetActionUrl::Make(&action, value, ft, dt)))
	{
		OP_DELETE(directive);
		return OpStatus::ERR_NO_MEMORY;
	}
	else
	{
		action->Into(list2);
		directive->Into(list);
	}

	return OpStatus::OK;
}

/* static */ OP_STATUS
OpSecurityManager_Gadget::SetDefaultGadgetPolicy()
{
	OP_DELETE(g_secman_instance->unsigned_policy); g_secman_instance->unsigned_policy = NULL;
	return SetDefaultGadgetPolicy(&g_secman_instance->unsigned_policy);
}

/* static */ OP_STATUS
OpSecurityManager_Gadget::SetDefaultGadgetPolicy(GadgetSecurityPolicy **policy)
{
	GadgetSecurityPolicy *default_policy;
	RETURN_IF_ERROR(GadgetSecurityPolicy::Make(&default_policy, GadgetSecurityPolicy::GLOBAL));
	OpStackAutoPtr<GadgetSecurityPolicy> policy_ptr(default_policy);

	// Access
	RETURN_IF_ERROR(HelpConstructGadgetPolicy(&default_policy->access->entries, GadgetActionUrl::FIELD_PROTOCOL, GadgetActionUrl::DATA_NONE, UNI_L("http")));
	RETURN_IF_ERROR(HelpConstructGadgetPolicy(&default_policy->access->entries, GadgetActionUrl::FIELD_PROTOCOL, GadgetActionUrl::DATA_NONE, UNI_L("https")));
	RETURN_IF_ERROR(HelpConstructGadgetPolicy(&default_policy->access->entries, GadgetActionUrl::FIELD_PROTOCOL, GadgetActionUrl::DATA_NONE, UNI_L("mailto")));
	RETURN_IF_ERROR(HelpConstructGadgetPolicy(&default_policy->access->entries, GadgetActionUrl::FIELD_PROTOCOL, GadgetActionUrl::DATA_NONE, UNI_L("unite")));

	// Content
	default_policy->file_content = MAYBE;
	default_policy->webserver_content = YES;
	default_policy->plugin_content = YES;

	// Private-network
	default_policy->private_network->allow = GadgetPrivateNetwork::UNRESTRICTED;

	RETURN_IF_ERROR(HelpConstructGadgetPolicy(&default_policy->private_network->entries, GadgetActionUrl::FIELD_HOST, GadgetActionUrl::DATA_HOST_LOCALHOST, NULL));
	RETURN_IF_ERROR(HelpConstructGadgetPolicy(&default_policy->private_network->entries, GadgetActionUrl::FIELD_HOST, GadgetActionUrl::DATA_HOST_IP, UNI_L("10.0.0.0-10.255.255.255")));
	RETURN_IF_ERROR(HelpConstructGadgetPolicy(&default_policy->private_network->entries, GadgetActionUrl::FIELD_HOST, GadgetActionUrl::DATA_HOST_IP, UNI_L("172.16.0.0-172.31.255.255")));
	RETURN_IF_ERROR(HelpConstructGadgetPolicy(&default_policy->private_network->entries, GadgetActionUrl::FIELD_HOST, GadgetActionUrl::DATA_HOST_IP, UNI_L("192.168.0.0-192.168.255.255")));
	RETURN_IF_ERROR(HelpConstructGadgetPolicy(&default_policy->private_network->entries, GadgetActionUrl::FIELD_HOST, GadgetActionUrl::DATA_HOST_IP, UNI_L("169.254.0.0-169.254.255.255")));

	*policy = policy_ptr.release();

	return OpStatus::OK;
}

#ifdef SIGNED_GADGET_SUPPORT
/* static */ OP_STATUS
OpSecurityManager_Gadget::SetDefaultSignedGadgetPolicy(GadgetSecurityPolicy **policy)
{
	RETURN_IF_ERROR(SetDefaultGadgetPolicy(policy));

	// TODO: Extend with additional relaxations or other stuff for signed widgets here.

	return OpStatus::OK;
}
#endif // SIGNED_GADGET_SUPPORT

/* static */ OP_STATUS
OpSecurityManager_Gadget::SetRestrictedGadgetPolicy()
{
	GadgetSecurityPolicy *default_policy;
	RETURN_IF_ERROR(GadgetSecurityPolicy::Make(&default_policy, GadgetSecurityPolicy::GLOBAL));
	default_policy->restricted = TRUE;

	OP_DELETE(g_secman_instance->unsigned_policy);
	g_secman_instance->unsigned_policy = default_policy;

	return OpStatus::OK;
}

class SuspendedSecurityCheck : private MessageObject
{
public:
	static OP_STATUS Make(SuspendedSecurityCheck*& suspended_security_check, const OpSecurityContext& source, const OpSecurityContext& target, OpSecurityCheckCallback* callback, OpSecurityState& sec_state)
	{
		OP_ASSERT(source.GetWindow() && source.GetGadget());
		OP_ASSERT(source.GetGadget() == source.GetWindow()->GetGadget());
		RETURN_OOM_IF_NULL(suspended_security_check = OP_NEW(SuspendedSecurityCheck, (source.GetWindow(), target.GetURL(), callback)));

		OP_ASSERT(sec_state.suspended && sec_state.host_resolving_comm);

		suspended_security_check->m_sec_state.suspended = TRUE;
		suspended_security_check->m_sec_state.host_resolving_comm = sec_state.host_resolving_comm;
		sec_state.host_resolving_comm = NULL;

		const OpMessage msgs[] = { MSG_COMM_NAMERESOLVED, MSG_COMM_LOADING_FAILED };

		OP_STATUS status = g_main_message_handler->SetCallBackList(suspended_security_check, suspended_security_check->m_sec_state.host_resolving_comm->Id(), msgs, sizeof(msgs) / sizeof(msgs[0]));
		if (OpStatus::IsError(status))
		{
			OP_DELETE(suspended_security_check);
			suspended_security_check = NULL;
		}

		return status;
	}

	virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
	{
		if (msg == MSG_COMM_NAMERESOLVED || msg == MSG_COMM_LOADING_FAILED)
		{
			OP_ASSERT(m_sec_state.suspended && m_sec_state.host_resolving_comm &&
					  par1 == static_cast<MH_PARAM_1>(m_sec_state.host_resolving_comm->Id()));

			BOOL unset_callbacks = TRUE;

			// The gadget object may be missing if the window is closing.
			if (m_window->GetGadget())
			{
				if (msg == MSG_COMM_NAMERESOLVED)
				{
					BOOL allowed;
					OP_STATUS status = g_secman_instance->CheckGadgetSecurity(m_window->GetGadget(), m_target_url, allowed, m_sec_state);
					if (OpStatus::IsError(status))
						m_callback->OnSecurityCheckError(status);
					else if (!m_sec_state.suspended)
						m_callback->OnSecurityCheckSuccess(allowed);
					else
						unset_callbacks = FALSE; // Wait for more messages, more stuff needs resolving.
				}
				else
					m_callback->OnSecurityCheckSuccess(FALSE);
			}
			else
				m_callback->OnSecurityCheckSuccess(FALSE);

			if (unset_callbacks)
			{
				g_main_message_handler->UnsetCallBacks(this);
				OP_DELETE(this); // Not needed anymore.
			}
		}
	}

private:
	SuspendedSecurityCheck(Window *window, URL target_url, OpSecurityCheckCallback* callback)
		: m_window(window),
		  m_target_url(target_url),
		  m_callback(callback) {}

	Window *m_window;
	URL m_target_url;
	OpSecurityCheckCallback* m_callback;
	OpSecurityState m_sec_state;
};



OP_STATUS
OpSecurityManager_Gadget::HelpCheckGadgetSecurity(const GadgetSecurityPolicy *local, const GadgetSecurityPolicy *global, const URL &url, CurrentState &cur_state, OpSecurityState &sec_state)
{
	OP_ASSERT(local);

	BOOL access_allowed = TRUE;
	sec_state.suspended = FALSE;

	if (local->restricted)
	{
		cur_state.allowed = FALSE;
		return OpStatus::OK;
	}
	else if (local->access->all)
	{
		cur_state.allowed = TRUE;
		return OpStatus::OK;
	}

	GadgetSecurityDirective *si = NULL;

	BOOL tmp_access = FALSE;
	// access
	for (si = (GadgetSecurityDirective *) local->access->entries.First(); si != NULL && !tmp_access && !sec_state.suspended; si = (GadgetSecurityDirective *) si->Suc())
	{
		int hits = 0;
		GadgetActionUrl *q = NULL;

		for (q = si->protocol.First(); q != NULL && !sec_state.suspended && !q->AllowAccessTo(url, sec_state); q = q->Suc()) {}
		hits += (q != NULL);

#ifdef EXTERNAL_PROTOCOL_SCHEME_SUPPORT
		// If this is an external protocol, we don't care about hostnames, ports etc etc
		if (access_allowed && urlManager->IsAnExternalProtocolHandler((URLType)url.GetAttribute(URL::KRealType)))
		{
			cur_state.allowed = TRUE;
			return OpStatus::OK;
		}
#endif // EXTERNAL_PROTOCOL_SCHEME_SUPPORT

		for (q = si->port.First(); q != NULL && !sec_state.suspended && !q->AllowAccessTo(url, sec_state); q = q->Suc()) {}
		hits += (q || si->port.First() == NULL);

		for (q = si->host.First(); q != NULL && !sec_state.suspended && !q->AllowAccessTo(url, sec_state); q = q->Suc()) {}
		hits += (q || si->host.First() == NULL);

		for (q = si->path.First(); q != NULL && !sec_state.suspended && !q->AllowAccessTo(url, sec_state); q = q->Suc()) {}
		hits += (q || si->path.First() == NULL);

		tmp_access |= hits == 4;
	}

	if (local->access->entries.First())
		access_allowed = tmp_access;

	// The following is only valid for the the global security local
	if (local->context == GadgetSecurityPolicy::GLOBAL)
	{
		// blacklist, excludes
		for (si = (GadgetSecurityDirective *) local->blacklist->excludes.First(); si != NULL && access_allowed && !sec_state.suspended; si = (GadgetSecurityDirective *) si->Suc())
		{
			int hits = 0;
			GadgetActionUrl *q = NULL;

			for (q = si->protocol.First(); q != NULL && !sec_state.suspended && !q->AllowAccessTo(url, sec_state); q = q->Suc()) {}
			hits += (q || si->protocol.First() == NULL);

			for (q = si->port.First(); q != NULL && !sec_state.suspended && !q->AllowAccessTo(url, sec_state); q = q->Suc()) {}
			hits += (q || si->port.First() == NULL);

			for (q = si->host.First(); q != NULL && !sec_state.suspended && !q->AllowAccessTo(url, sec_state); q = q->Suc()) {}
			hits += (q || si->host.First() == NULL);

			for (q = si->path.First(); q != NULL && !sec_state.suspended && !q->AllowAccessTo(url, sec_state); q = q->Suc()) {}
			hits += (q || si->path.First() == NULL);

			access_allowed = hits != 4;
		}

		// blacklist, includes
		for (si = (GadgetSecurityDirective *) local->blacklist->includes.First(); si != NULL && !access_allowed && !sec_state.suspended; si = (GadgetSecurityDirective *) si->Suc())
		{
			int hits = 0;
			GadgetActionUrl *q = NULL;

			for (q = si->protocol.First(); q != NULL && !sec_state.suspended && !q->AllowAccessTo(url, sec_state); q = q->Suc()) {}
			hits += (q || si->protocol.First() == NULL);

			for (q = si->port.First(); q != NULL && !sec_state.suspended && !q->AllowAccessTo(url, sec_state); q = q->Suc()) {}
			hits += (q || si->port.First() == NULL);

			for (q = si->host.First(); q != NULL && !sec_state.suspended && !q->AllowAccessTo(url, sec_state); q = q->Suc()) {}
			hits += (q || si->host.First() == NULL);

			for (q = si->path.First(); q != NULL && !sec_state.suspended && !q->AllowAccessTo(url, sec_state); q = q->Suc()) {}
			hits += (q || si->path.First() == NULL);

			access_allowed = hits == 4;
		}

		// private_network
		if (access_allowed && !sec_state.suspended)
		{
			cur_state.is_intranet = FALSE;

			for (si = (GadgetSecurityDirective *) local->private_network->entries.First(); si != NULL && access_allowed && !sec_state.suspended; si = (GadgetSecurityDirective *) si->Suc())
			{
				int hits = 0;
				GadgetActionUrl *q = NULL;

				for (q = si->protocol.First(); q != NULL && !sec_state.suspended && !q->AllowAccessTo(url, sec_state); q = q->Suc()) {}
				hits += (q || si->protocol.First() == NULL);

				for (q = si->port.First(); q != NULL && !sec_state.suspended && !q->AllowAccessTo(url, sec_state); q = q->Suc()) {}
				hits += (q || si->port.First() == NULL);

				for (q = si->host.First(); q != NULL && !sec_state.suspended && !q->AllowAccessTo(url, sec_state); q = q->Suc()) {}
				hits += (q || si->host.First() == NULL);

				for (q = si->path.First(); q != NULL && !sec_state.suspended && !q->AllowAccessTo(url, sec_state); q = q->Suc()) {}
				hits += (q || si->path.First() == NULL);

				cur_state.is_intranet |= hits == 4;
			}
		}
	} // END OF: if (local->context == GadgetSecurityPolicy::GLOBAL)

	if (access_allowed && !sec_state.suspended)
	{
		if (local->context == GadgetSecurityPolicy::GADGET)
		{
			UINT8 global_access = global->private_network->allow;
			if (global_access & GadgetPrivateNetwork::RESTRICTED)
			{
				if (local->private_network->allow & GadgetPrivateNetwork::INTRANET && cur_state.is_intranet)
					local->private_network->allow = GadgetPrivateNetwork::INTRANET;
				else if (local->private_network->allow & GadgetPrivateNetwork::INTERNET && !cur_state.is_intranet)
					local->private_network->allow = GadgetPrivateNetwork::INTERNET;
				else
					access_allowed = FALSE;
			}
			else if (global_access & GadgetPrivateNetwork::UNRESTRICTED)
			{
				BOOL has_intranet_access = local->private_network->allow & GadgetPrivateNetwork::INTRANET ? TRUE : FALSE;
				BOOL has_internet_access = local->private_network->allow & GadgetPrivateNetwork::INTERNET ? TRUE : FALSE;
				if (cur_state.is_intranet && !has_intranet_access)
					access_allowed = FALSE;
				else if (!cur_state.is_intranet && !has_internet_access)
					access_allowed = FALSE;
			}
			else if (global_access & GadgetPrivateNetwork::NONE && cur_state.is_intranet)
				access_allowed = FALSE;
		}
	}

	if (sec_state.suspended)
		access_allowed = FALSE;

	cur_state.allowed = access_allowed;

	return OpStatus::OK;
}

OP_STATUS
OpSecurityManager_Gadget::CheckGadgetSecurity(const OpSecurityContext& source, const OpSecurityContext& target, BOOL& allowed, OpSecurityState& sec_state)
{
	const URL &url = target.GetURL();
	OpGadget *gadget = source.GetGadget();
	GadgetSecurityPolicy* local = gadget->GetClass()->GetSecurity();
	const GadgetSecurityPolicy *global = GetPolicy(gadget);

	// global == NULL could be that verification failed
	if (global == NULL || global->restricted)
	{
		allowed = FALSE;
		return OpStatus::OK;
	}

	const OpStringC widget_id = url.GetAttribute(URL::KUniHostName);
	URLType type = (URLType) url.GetAttribute(URL::KRealType);

	// If this is a widget trying to access itself, allow it.
	if ( (type == URL_WIDGET && widget_id.CompareI(gadget->GetIdentifier()) == 0)
		 || type == URL_MOUNTPOINT	// Mountpoint url are checked by DOM and OpGadgetManager
		 || type == URL_DATA)		// data uris dont access network and thus should be safe
	{
		allowed = TRUE;
		return OpStatus::OK;
	}

	CurrentState cur_state;

	// This might be confusing, but when checking the global policy it's passed as the local.
	RETURN_IF_ERROR(HelpCheckGadgetSecurity(global, NULL, url, cur_state, sec_state));
	if (sec_state.suspended)
	{
		allowed = FALSE;
		return OpStatus::OK;
	}

	if (local != NULL && cur_state.allowed)
	{
		RETURN_IF_ERROR(HelpCheckGadgetSecurity(local, global, url, cur_state, sec_state));
		if (sec_state.suspended)
		{
			allowed = FALSE;
			return OpStatus::OK;
		}
	}

	allowed = cur_state.allowed;

	return OpStatus::OK;
}

class PostSignatureVerificationGadgetSecurityCheck
	: public OpGadgetListener
{
public:
	PostSignatureVerificationGadgetSecurityCheck(OpGadget* gadget, const OpSecurityContext& source, const OpSecurityContext& target, OpSecurityCheckCallback* callback)
		: m_gadget(gadget), m_source_context(source), m_target_context(target), m_callback(callback)
	{
	}

	virtual void OnGadgetUpdateReady(const OpGadgetUpdateData& data) {}
	virtual void OnGadgetUpdateError(const OpGadgetErrorData& data) {}
	virtual void OnGadgetDownloadPermissionNeeded(const OpGadgetDownloadData& data, GadgetDownloadPermissionCallback *callback) {}
	virtual void OnGadgetDownloaded(const OpGadgetDownloadData& data) {}
	virtual void OnGadgetInstalled(const OpGadgetInstallRemoveData& data) {}
	virtual void OnGadgetRemoved(const OpGadgetInstallRemoveData& data) {}
	virtual void OnGadgetUpgraded(const OpGadgetStartStopUpgradeData& data) {}
	virtual void OnRequestRunGadget(const OpGadgetDownloadData& data) {}
	virtual void OnGadgetStarted(const OpGadgetStartStopUpgradeData& data) {}
	virtual void OnGadgetStartFailed(const OpGadgetStartFailedData& data) {}
	virtual void OnGadgetInstanceCreated(const OpGadgetInstanceData& data) {}
	virtual void OnGadgetInstanceRemoved(const OpGadgetInstanceData& data) {}
	virtual void OnGadgetDownloadError(const OpGadgetErrorData& data) {}

	virtual void OnGadgetStopped(const OpGadgetStartStopUpgradeData& data)
	{
		if (data.gadget == m_gadget)
		{
			g_gadget_manager->DetachListener(this);
			m_callback->OnSecurityCheckError(OpStatus::ERR); // A cancel notification would be better
			OP_DELETE(this);
		}
	}

	virtual void OnGadgetSignatureVerified(const OpGadgetSignatureVerifiedData& data)
	{
		if (data.gadget_class == m_gadget->GetClass())
		{
			// CheckGadgetSecurity reports errors by calling the callback, ignore return value.
			OpStatus::Ignore(g_secman_instance->CheckGadgetSecurity(m_source_context, m_target_context, m_callback));
			g_gadget_manager->DetachListener(this);
			OP_DELETE(this);
		}
	}

private:
	OpGadget* m_gadget;
	OpSecurityContext m_source_context;
	OpSecurityContext m_target_context;
	OpSecurityCheckCallback* m_callback;
};

OP_STATUS
OpSecurityManager_Gadget::CheckGadgetSecurity(const OpSecurityContext& source, const OpSecurityContext& target, OpSecurityCheckCallback* callback)
{
	OpGadget *gadget = source.GetGadget();
	if (gadget->SignatureState() == GADGET_SIGNATURE_PENDING)
	{
		PostSignatureVerificationGadgetSecurityCheck *post_signature_check = OP_NEW(PostSignatureVerificationGadgetSecurityCheck, (gadget, source, target, callback));
		if (!post_signature_check)
		{
			callback->OnSecurityCheckError(OpStatus::ERR_NO_MEMORY);
			return OpStatus::ERR_NO_MEMORY;
		}

		g_gadget_manager->AttachListener(post_signature_check);

		return OpStatus::OK;
	}

	BOOL allowed;
	OpSecurityState sec_state;

	OP_STATUS status = CheckGadgetSecurity(source, target, allowed, sec_state);

	if (OpStatus::IsError(status))
	{
		callback->OnSecurityCheckError(status);
		return status;
	}

	if (sec_state.suspended)
	{
		SuspendedSecurityCheck *s;

		OP_STATUS status = SuspendedSecurityCheck::Make(s, source, target, callback, sec_state);

		if (OpStatus::IsError(status))
			callback->OnSecurityCheckError(status);

		return status;
	}

	callback->OnSecurityCheckSuccess(allowed);

	return OpStatus::OK;
}

OP_STATUS OpSecurityManager_Gadget::CheckGadgetJSPluginSecurity(const OpSecurityContext& source, const OpSecurityContext&, BOOL& allowed)
{
	OpGadget* gadget = source.GetGadget();
	GadgetSecurityPolicy* local = gadget->GetClass()->GetSecurity();
	const GadgetSecurityPolicy *global = GetPolicy(gadget);

	allowed = FALSE;

	if (local && global && local->jsplugin_content == YES && global->jsplugin_content == YES
#ifdef SIGNED_GADGET_SUPPORT // If we have support for signed widgets, only these have access to jsplugins
		&& gadget->SignatureState() == GADGET_SIGNATURE_SIGNED
#endif // SIGNED_GADGET_SUPPORT
		)
		allowed = TRUE;

	return OpStatus::OK;
}

OP_STATUS
OpSecurityManager_Gadget::CheckGadgetDOMSecurity(const OpSecurityContext& source, const OpSecurityContext& target, BOOL& allowed)
{
	const URL &url = source.GetURL();
	OpGadget *gadget = target.GetGadget();

	if(url.IsEmpty() || !gadget)
	{
		allowed = FALSE;
		return OpStatus::OK;
	}

	const OpStringC widget_id = url.GetAttribute(URL::KUniHostName);
	URLType type = url.Type();

	// Only URLs of these types are allowed to use the widget DOM API.
	if ( (type == URL_WIDGET && widget_id.CompareI(gadget->GetIdentifier()) == 0)
		 || type == URL_MOUNTPOINT)	// Mountpoint url are checked by DOM and OpGadgetManager
	{
		allowed = TRUE;
		return OpStatus::OK;
	}

	allowed = FALSE;

	return OpStatus::OK;
}

#ifdef GADGETS_MUTABLE_POLICY_SUPPORT
OP_STATUS
OpSecurityManager_Gadget::CheckGadgetMutatePolicySecurity(const OpSecurityContext& /* source */, const OpSecurityContext& target, BOOL& allowed)
{
	OpGadget *gadget = target.GetGadget();

	allowed = (gadget && gadget->GetAllowGlobalPolicy());

	return OpStatus::OK;
}
#endif // GADGETS_MUTABLE_POLICY_SUPPORT

BOOL
OpSecurityManager_Gadget::AllowGadgetFileUrlAccess(const URL& source_url, const URL& target_url)
{
	OP_ASSERT(target_url.Type() == URL_FILE);

#ifdef DOM_JIL_API_SUPPORT
	/* Special case for JIL widget which may load inlines using file:// not only from its archive
	 * but also from virtual filesystem it has acces to.
	 */
	if (g_DAPI_jil_fs_mgr->IsFileAccessAllowed(target_url))
		return TRUE;
#endif // DOM_JIL_API_SUPPORT

	OpString gadget_url_s, target_url_s;

	RETURN_VALUE_IF_ERROR(source_url.GetAttribute(URL::KUniName_Username_Password_Escaped_NOT_FOR_UI, gadget_url_s), FALSE);
	RETURN_VALUE_IF_ERROR(target_url.GetAttribute(URL::KUniName_Username_Password_Escaped_NOT_FOR_UI, target_url_s), FALSE);

	unsigned length = gadget_url_s.Length();
	const uni_char *string = gadget_url_s.CStr();
	while (length != 0)
		if (string[length - 1] == '/')
			break;
		else
			--length;

	if (length == 0)
		/* Really odd.  This would mean the widget is loaded from
		   the system's root.  Doubt it will happen, but if it
		   does, we probably don't want to allow it to access any
		   other files. */
		return FALSE;

	/* Check that 'gadgeturl', the gadgets "root" URL, is a prefix
	   of 'targeturl'. */
	if (length >= (unsigned int)target_url_s.Length())
		return FALSE;
	if (uni_strncmp(gadget_url_s.CStr(), target_url_s.CStr(), length) != 0)
		return FALSE;

	return TRUE;
}

OP_STATUS
OpSecurityManager_Gadget::CheckGadgetUrlLoadSecurity(const OpSecurityContext& source, const OpSecurityContext& target, BOOL& allowed, OpSecurityState& state)
{
	allowed = FALSE;
	state.host_resolving_comm = NULL;
	state.suspended = FALSE;

	URL target_url = target.GetURL();
	Window* window = source.GetWindow();

	OP_ASSERT(window);

	/* If the top document manager has no current document, this URL access
	 * must be for loading the top-level widget document itself,
	 * and so we allow it without checking. */
	if (window->DocManager()->GetCurrentDoc())
	{
		// only allow loading files that belong to the widget.
		if (target_url.Type() == URL_FILE)
		{
			allowed = AllowGadgetFileUrlAccess(window->DocManager()->GetCurrentDoc()->GetURL(), target_url);
			return OpStatus::OK;
		}
		else
			return CheckGadgetSecurity(source.GetGadget(), target_url, allowed, state);
	}

	allowed = TRUE;
	return OpStatus::OK;
}

OP_STATUS
OpSecurityManager_Gadget::CheckGadgetUrlLoadSecurity(const OpSecurityContext& source, const OpSecurityContext& target, OpSecurityCheckCallback* callback)
{
	URL target_url = target.GetURL();
	Window* window = source.GetWindow();

	OP_ASSERT(window);

	/* If the top document manager has no current document, this URL access
	 * must be for loading the top-level widget document itself,
	 * and so we allow it without checking. */
	if (window->DocManager()->GetCurrentDoc())
	{
		// only allow loading files that belong to the widget.
		if (target_url.Type() == URL_FILE)
		{
			BOOL allowed = AllowGadgetFileUrlAccess(window->DocManager()->GetCurrentDoc()->GetURL(), target_url);
			callback->OnSecurityCheckSuccess(allowed);
			return OpStatus::OK;
		}
		else
			return CheckGadgetSecurity(source.GetGadget(), target_url, callback);
	}

	callback->OnSecurityCheckSuccess(TRUE);
	return OpStatus::OK;
}

#ifdef EXTENSION_SUPPORT
OP_STATUS
OpSecurityManager_Gadget::CheckGadgetExtensionSecurity(const OpSecurityContext& source, const OpSecurityContext& target, BOOL& allowed)
{
	OpGadget *owner = target.GetGadget();
	OP_ASSERT(owner);

	FramesDocument *accessing_doc = source.GetFramesDocument();
	Window *window = accessing_doc->GetWindow();

	// If privacy mode is in effect, allow only if there's an opt-in in effect.
	if (window->GetPrivacyMode() && !owner->GetExtensionFlag(OpGadget::AllowUserJSPrivate))
		allowed = FALSE;
	// If HTTPS, only considered allowed if extension permits it.
	else if (target.GetURL().Type() == URL_HTTPS && !owner->GetExtensionFlag(OpGadget::AllowUserJSHTTPS))
		allowed = FALSE;
	// Never give access to directory lister to an extension! (or at least till we get some flesystem API)
	else if (target.GetURL().Type() == URL_FILE && target.GetURL().GetAttribute(URL::KIsDirectoryListing, TRUE) == TRUE)	
		allowed = FALSE;
	else
		allowed = TRUE;

	return OpStatus::OK;
}
#endif // EXTENSION_SUPPORT

OP_STATUS
OpSecurityManager_Gadget::CheckGadgetExternalUrlSecurity(const OpSecurityContext& source, const OpSecurityContext& target, BOOL& allowed)
{
	const URL &url = target.GetURL();

	OP_ASSERT(url.IsValid());
	if (url.IsEmpty())
		return OpStatus::ERR;

	URLType type = (URLType) url.GetAttribute(URL::KRealType);
	BOOL have_server_name = !!url.GetAttribute(URL::KHaveServerName); // Forbid links with no server name (like data:). (CORE-22912)

	allowed = (have_server_name || type == URL_DATA) && (type != URL_WIDGET) && (type != URL_MOUNTPOINT) && (type != URL_FILE);

	return OpStatus::OK;
}

OP_STATUS
OpSecurityManager_Gadget::GadgetAllowedToNavigate(const OpSecurityContext& source, const OpSecurityContext& target, BOOL& allowed)
{
	OP_ASSERT(source.GetDoc() && target.GetURL().IsValid());

	allowed = FALSE;

	FramesDocument *doc = source.GetDoc();
	Window *window = doc->GetWindow();

	OP_ASSERT(window->GetGadget());


	// Don't allow if it's an extension background process or if trying to replace the toplevel document in a widget.
	if (window->GetType() == WIN_TYPE_GADGET && (window->GetGadget()->IsExtension() || !doc->GetDocManager()->GetFrame()))
		return OpStatus::OK;

	if (doc->GetURL().Type() != URL_WIDGET)
	{
		// Navigating away from a non-widget url is checked by DOM_ALLOWED_TO_NAVIGATE.
		allowed = TRUE;
		return OpStatus::OK;
	}

	allowed = doc->GetURL().SameServer(target.GetURL()); // Only allow to navigate within the gadget.

	return OpStatus::OK;
}

const GadgetSecurityPolicy *
OpSecurityManager_Gadget::GetPolicy(OpGadget *gadget)
{
	if (gadget == NULL)
		return NULL;

#ifdef GADGETS_MUTABLE_POLICY_SUPPORT
	// Check if the instance has overriden the global policy.
	if (gadget->GetGlobalSecurityPolicy())
		return gadget->GetGlobalSecurityPolicy();
#endif // GADGETS_MUTABLE_POLICY_SUPPORT

#ifdef SELFTEST
	if (g_gadget_manager->GetAllowBundledGlobalPolicy())
	{
		// return global widget.xml bundled inside widget; for testing purposes.
		GadgetSecurityPolicy *global = gadget->GetClass()->GetBundledGlobalPolicy();
		if (global)
			return global;
		else if (OpStatus::IsSuccess(ReadAndParseGadgetSecurity(gadget->GetGadgetPath(), UNI_L("widgets.xml"), &global)))
		{
			gadget->GetClass()->SetBundledGlobalPolicy(global);
			return global;
		}
	}
#endif // SELFTEST

#ifdef SIGNED_GADGET_SUPPORT
	switch (gadget->SignatureState())
	{
	case GADGET_SIGNATURE_UNSIGNED:
		return unsigned_policy;
	case GADGET_SIGNATURE_SIGNED:
		{
			const uni_char *name = gadget->SignatureId();

			for (GadgetSecurityPolicy *local = (GadgetSecurityPolicy *) gadget_security_policies.First(); local; local = (GadgetSecurityPolicy *) local->Suc())
			{
				if (local->signature_name.CompareI(name) == 0)
					return local;
			}

			return signed_policy;
		}
	default:
		OP_ASSERT(!"Gadget shouldn't be run unless we know whether it is signed or unsigned - see CORE-35168");
	case GADGET_SIGNATURE_VERIFICATION_FAILED:
		return NULL;
	}

#else // SIGNED_GADGET_SUPPORT
	return unsigned_policy;
#endif // !SIGNED_GADGET_SUPPORT
}

OP_STATUS
OpSecurityManager_Gadget::CheckHasGadgetManagerAccess(const OpSecurityContext& source, BOOL& allowed)
{
	allowed = FALSE;
	URL src_url = source.GetURL();
	if (src_url.GetAttribute(URL::KType) == URL_OPERA)
	{
		OpString8 path;
		RETURN_IF_ERROR(src_url.GetAttribute(URL::KPath, path));
		if (op_stricmp("widgets", path)  == 0
#ifdef WEBSERVER_SUPPORT
			 || op_stricmp("unite", path)  == 0
#endif //WEBSERVER_SUPPORT
#ifdef EXTENSION_SUPPORT
			 || op_stricmp("extensions", path)  == 0
#endif //EXTENSION_SUPPORT
			 )
			 allowed = TRUE;
	}
	return OpStatus::OK;
}

#if defined(SIGNED_GADGET_SUPPORT) && defined(SECMAN_PRIVILEGED_GADGET_CERTIFICATE)
void
OpSecurityManager_Gadget::SetGadgetPrivilegedCA(CryptoCertificate * certificate)
{
	OP_DELETE(privileged_certificate);
	privileged_certificate = certificate;
}
#endif // SECMAN_PRIVILEGED_GADGET_CERTIFICATE && SIGNED_GADGET_SUPPORT

/*************************************************************************
 **
 ** GadgetActionUrl
 **
 *************************************************************************/

GadgetActionUrl::WILDCARD_MATCH_STATUS
GadgetActionUrl::WildcardCmp(const uni_char *expr, const uni_char *data) const
{
	OP_ASSERT(expr);
	OP_ASSERT(data);

	// do reverse comparison

	const uni_char* expr_start = expr;
	const uni_char* expr_end = expr_start + uni_strlen(expr_start) - 1;

	const uni_char* data_start = data;
	const uni_char* data_end = data_start + uni_strlen(data_start) - 1;

	while (expr_end >= expr_start && data_end >= data_start)
	{
		if (*expr_end != *data_end)
		{
			if (*expr_end == '*')
				return GadgetActionUrl::IS_MATCH;
			else
				return GadgetActionUrl::NO_MATCH;
		}

		expr_end--;
		data_end--;
	}

	if (expr_end >= expr_start && expr_end[0] == '.' && expr_end[-1] == '*')
		return GadgetActionUrl::IS_MATCH;
	else if (expr_end >= expr_start || data_end >= data_start)
		return GadgetActionUrl::NO_MATCH;
	else
		return GadgetActionUrl::IS_MATCH;
}

BOOL
GadgetActionUrl::AllowAccessTo(const URL& url, OpSecurityState& state)
{
	switch (ft)
	{
	case FIELD_PROTOCOL:
		{
			BOOL result =	(field.protocol != URL_UNKNOWN) &&
							(((URLType)url.GetAttribute(URL::KRealType) == field.protocol) ||
							 ((URLType)url.GetAttribute(URL::KType) == field.protocol));
			return result;
		}

	case FIELD_PORT:
		{
			unsigned int port = url.GetAttribute(URL::KResolvedPort);
			OpSecurityManager::ResolveDefaultPort((URLType) url.GetAttribute(URL::KRealType), port);
			return (port >= field.port.low && port <= field.port.high);
		}

	case FIELD_PATH:
		{
			const uni_char *path =  url.GetAttribute(URL::KUniPath).CStr();

			if (path)
				return uni_strnicmp(field.value, path, uni_strlen(field.value)) == 0;
			else
				return FALSE;
		}

	case FIELD_HOST:
		if (dt == DATA_HOST_LOCALHOST)
		{
			const OpStringC str = url.GetAttribute(URL::KUniHostName);
			if (str.CompareI("localhost") == 0 || str.Compare("127.0.0.1") == 0)
				return TRUE;
		}
		else if (dt == DATA_HOST_STRING)
		{
			const OpStringC str = url.GetAttribute(URL::KUniHostName);
			if (str.IsEmpty())
				return FALSE;

			GadgetActionUrl::WILDCARD_MATCH_STATUS match_status =
				WildcardCmp(this->field.value, str.CStr());

			if (match_status == GadgetActionUrl::IS_MATCH)
				return TRUE;
		}
		else if (dt == DATA_HOST_IP)
		{
			IPAddress resolved_ip;

			RETURN_VALUE_IF_ERROR(OpSecurityUtilities::ResolveURL(url, resolved_ip, state), FALSE);
			if (state.suspended)
				return FALSE;

			int res = 0;
			RETURN_VALUE_IF_ERROR(field.host.Compare(resolved_ip, res), FALSE);
			if (res == 0)
				return TRUE;
		}
		// follow thru
	default:
		return FALSE;
	}
}

OP_STATUS
GadgetActionUrl::GetProtocol(URLType &protocol) const
{
	if (ft != FIELD_PROTOCOL)
		return OpStatus::ERR;

	protocol = field.protocol;
	return OpStatus::OK;
}

OP_STATUS
GadgetActionUrl::GetProtocol(OpString &protocol) const
{
	if (ft != FIELD_PROTOCOL)
		return OpStatus::ERR;

	const char *protocol_name = urlManager->MapUrlType(field.protocol);

	// protocol_name is allowed to be NULL, it means that the protocol is not known.
	// protocol is set to an empty string in that case.
	return protocol.Set(protocol_name);
}

OP_STATUS
GadgetActionUrl::GetHost(OpString &host) const
{
	if (ft != FIELD_HOST)
		return OpStatus::ERR;

	switch (dt)
	{
	case DATA_HOST_LOCALHOST:
		return host.Set(UNI_L("localhost"));
	case DATA_HOST_STRING:
		return host.Set(field.value);
	case DATA_HOST_IP:
		return IPRange::Export(field.host, host);
	default:
		return OpStatus::ERR;
	}
}

OP_STATUS
GadgetActionUrl::GetPort(unsigned &low, unsigned &high) const
{
	if (ft != FIELD_PORT)
		return OpStatus::ERR;

	low = field.port.low;
	high = field.port.high;

	return OpStatus::OK;
}

OP_STATUS
GadgetActionUrl::GetPath(OpString &path) const
{
	if (ft != FIELD_PATH)
		return OpStatus::ERR;

	return path.Set(field.value);
}

#endif // GADGET_SUPPORT
