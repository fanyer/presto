/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef GADGET_SUPPORT

#include "modules/doc/frm_doc.h"
#include "modules/dochand/win.h"
#include "modules/security_manager/include/security_manager.h"
#include "modules/security_manager/src/security_gadget_representation.h"
#include "modules/xmlutils/xmlfragment.h"
#include "modules/xmlutils/xmlnames.h"
#include "modules/gadgets/OpGadgetClass.h"
#include "modules/gadgets/OpGadget.h"
#include "modules/gadgets/gadget_utils.h"
#include "modules/url/url_man.h"

#ifdef SIGNED_GADGET_SUPPORT
# include "modules/libcrypto/include/CryptoXmlSignature.h"
#endif // SIGNED_GADGET_SUPPORT

/***********************************************************************************
**
**	ParseGadgetPolicy
**
**	Parse any security policy element and construct a security data structure in *pp.
**	The bitmap in 'allow' determines whether certain elements are to be recognized;
**	the bits are drawn from the enum in OpSecurityManager_Gadget.
**
**	ParseGadgetPolicy() leaves the XMLFragment state pointing either after the security
**	element, or to whatever element it was pointing at if there's no security element.
**
***********************************************************************************/

/* static */ OP_STATUS
OpSecurityManager_Gadget::ParseGadgetPolicy(XMLFragment* policy, int allow, GadgetSecurityPolicy** pp, OpString& error_reason)
{
	RETURN_IF_ERROR(GadgetSecurityPolicy::Make(pp, GadgetSecurityPolicy::GADGET));
	OpStackAutoPtr<GadgetSecurityPolicy>pp_ptr(*pp);
	RETURN_IF_ERROR(ParseGadgetPolicy(policy, allow, pp, error_reason));
	pp_ptr.release();

	return OpStatus::OK;
}

/* static */ OP_STATUS
OpSecurityManager_Gadget::ParseGadgetPolicy(XMLFragment* policy, int allow, GadgetSecurityPolicy* pp, OpString& error_reason)
{
	OP_ASSERT(pp);

	OP_STATUS ret = OpStatus::OK;
	BOOL has_private_network = FALSE;

	while (OpStatus::IsSuccess(ret) && policy->HasMoreElements())
	{
		if (policy->EnterElement(UNI_L("blacklist")))
		{
			if (allow & ALLOW_BLACKLIST)
			{
				while (policy->HasMoreElements())
				{
					if (policy->EnterElement(UNI_L("exclude")))
					{
						GadgetSecurityDirective *exclude = NULL;
						if (OpStatus::IsSuccess(ret = GadgetSecurityDirective::Make(&exclude)))
							exclude->Into(&pp->blacklist->excludes);

						while (OpStatus::IsSuccess(ret) && policy->HasMoreElements())
						{
							GadgetActionUrl* act = NULL;
							ret = ParseActionUrlElement(policy, &act, exclude);
						}

						policy->LeaveElement(); // exclude
					}
					else if (policy->EnterElement(UNI_L("include")))
					{
						GadgetSecurityDirective *include = NULL;
						if (OpStatus::IsSuccess(ret = GadgetSecurityDirective::Make(&include)))
							include->Into(&pp->blacklist->includes);

						while (OpStatus::IsSuccess(ret) && policy->HasMoreElements())
						{
							GadgetActionUrl* act = NULL;
							ret = ParseActionUrlElement(policy, &act, include);
						}

						policy->LeaveElement();	// include
					}
					else
					{
						policy->EnterAnyElement();
						policy->LeaveElement();
					}
				}
			}
			else
				ret = OpStatus::ERR;

			policy->LeaveElement();  // blacklist
		}
		else if (policy->EnterElement(UNI_L("access")))
		{
			if (allow & ALLOW_ACCESS)
			{
				GadgetSecurityDirective *access = NULL;
				if (OpStatus::IsSuccess(ret = GadgetSecurityDirective::Make(&access)))
					access->Into(&pp->access->entries);

				while (OpStatus::IsSuccess(ret) && policy->HasMoreElements())
				{
					GadgetActionUrl *act = NULL;
					ret = ParseActionUrlElement(policy, &act, access);
				}
			}
			policy->LeaveElement();	// access
		}
		else if (policy->EnterElement(UNI_L("private-network")))
		{
			if ((allow & ALLOW_PRIVATE_NETWORK) && has_private_network == FALSE) // Only one private-network directive is allowed
			{
				has_private_network = TRUE;

				if (OpStatus::IsSuccess(ret))
				{
					const uni_char* allow_private = policy->GetAttribute(UNI_L("allow"));
					if (allow_private != NULL)
					{
						if (!uni_stricmp(allow_private, UNI_L("unrestricted")))
							pp->private_network->allow = GadgetPrivateNetwork::UNRESTRICTED;
						else if (!uni_stricmp(allow_private, UNI_L("restricted")))
							pp->private_network->allow = GadgetPrivateNetwork::RESTRICTED;
						else if (!uni_stricmp(allow_private, UNI_L("none")))
							pp->private_network->allow = GadgetPrivateNetwork::NONE;
						else
						{
							OpStatus::Ignore(error_reason.AppendFormat(UNI_L("Invalid <private-network> element - unrecognized 'allow' attr value :'%s'"), allow_private));
							ret = OpGadgetStatus::ERR_WRONG_WIDGET_FORMAT;
						}
					}

					while (OpStatus::IsSuccess(ret) && policy->HasMoreElements())
					{
						// Create a new directive for each host. Special case for private-network
						GadgetSecurityDirective *item = NULL;
						if (OpStatus::IsSuccess(ret = GadgetSecurityDirective::Make(&item)))
							item->Into(&pp->private_network->entries);

						if (OpStatus::IsSuccess(ret))
						{
							GadgetActionUrl *act = NULL;
							ret = ParseActionUrlElement(policy, &act, item);
						}
					}
				}
			}
			else
			{
				OpStatus::Ignore(error_reason.Set(UNI_L("Only one <private-network> element supported.")));
				ret = OpGadgetStatus::ERR_WRONG_WIDGET_FORMAT;
			}

			policy->LeaveElement();	// private-network
		}
		else if (policy->EnterElement(UNI_L("content")))
		{
			if (allow & ALLOW_CONTENT)
			{
				const uni_char* plugin = policy->GetAttribute(UNI_L("plugins"));
				const uni_char* webserver = policy->GetAttribute(UNI_L("webserver"));
				const uni_char* file = policy->GetAttribute(UNI_L("file"));
				const uni_char* jsplugins = policy->GetAttribute(UNI_L("jsplugins"));

				if (plugin != NULL)
				{
					if (!uni_stricmp(plugin, UNI_L("yes")))
						pp->plugin_content = YES;
					else if (!uni_stricmp(plugin, UNI_L("no")))
						pp->plugin_content = NO;
					else
					{
						OpStatus::Ignore(error_reason.AppendFormat(UNI_L("Invalid <content> element - unrecognized 'plugins' attr value :'%s'"), plugin));
						ret = OpGadgetStatus::ERR_WRONG_WIDGET_FORMAT;
					}
				}
				else
					pp->plugin_content = NO;

				if (webserver != NULL)
				{
					if (!uni_stricmp(webserver, UNI_L("yes")))
						pp->webserver_content = YES;
					else if (!uni_stricmp(webserver, UNI_L("no")))
						pp->webserver_content = NO;
					else
					{
						OpStatus::Ignore(error_reason.AppendFormat(UNI_L("Invalid <content> element - unrecognized 'webserver' attr value :'%s'"), webserver));
						ret = OpGadgetStatus::ERR_WRONG_WIDGET_FORMAT;
					}
				}
				else
					pp->webserver_content = NO;

				if (file != NULL)
				{
					if (!uni_stricmp(file, UNI_L("unrestricted")))
						pp->file_content = YES;
					else if (!uni_stricmp(file, UNI_L("restricted")))
						pp->file_content = MAYBE;
					else if (!uni_stricmp(file, UNI_L("none")))
						pp->file_content = NO;
					else
					{
						OpStatus::Ignore(error_reason.AppendFormat(UNI_L("Invalid <content> element - unrecognized 'file' attr value :'%s'"), file));
						ret = OpGadgetStatus::ERR_WRONG_WIDGET_FORMAT;
					}
				}
				else
					pp->file_content = NO;

				if (jsplugins != NULL)
				{
					if (!uni_stricmp(jsplugins, UNI_L("yes")))
						pp->jsplugin_content = YES;
					else if (!uni_stricmp(jsplugins, UNI_L("no")))
						pp->jsplugin_content = NO;
					else
					{
						OpStatus::Ignore(error_reason.AppendFormat(UNI_L("Invalid <content> element - unrecognized 'jsplugins' attr value :'%s'"), jsplugins));
						ret = OpGadgetStatus::ERR_WRONG_WIDGET_FORMAT;
					}
				}
				else
					pp->jsplugin_content = NO;
			}
			else
				ret = OpStatus::ERR;

			policy->LeaveElement();	// content
		}
		else
		{
			policy->EnterAnyElement();
			policy->LeaveElement();
		}
	}

	return ret;
}

/* static */ OP_STATUS
OpSecurityManager_Gadget::ParseGadgetPolicyElement(OpGadgetAccess* policy, int element_type, GadgetSecurityPolicy* pp, OpString& error_reason)
{
	OP_ASSERT(pp);

	if (element_type == ALLOW_W3C_ACCESS)
	{
		const uni_char* origin = policy->Name();
		BOOL sub = policy->Subdomains();

		if (!origin || !*origin)
			return OpStatus::OK; // No origin => ignore it.

		if (origin[0] == '*' && origin[1] == '\0')
		{
			pp->access->all = TRUE;
			return OpStatus::OK;
		}

		URL url = g_url_api->GetURL(origin);
		if (!url.IsValid() ||
			url.GetAttribute(URL::KPath).Compare("/") != 0 || // Empty path is translated  to "/". We assume also that "http://domain.org" == "http://domain.org/".
			url.GetAttribute(URL::KQuery).HasContent() || url.GetAttribute(URL::KFragment_Name).HasContent() ||
			url.GetAttribute(URL::KUserName).HasContent() || url.GetAttribute(URL::KPassWord).HasContent()
			)
		{
			OpStatus::Ignore(error_reason.AppendFormat(UNI_L("Invalid <access> element - unrecognized url: '%s'"), origin));
			return OpStatus::ERR_PARSING_FAILED;
		}
		// extract protocol, port and host
		OpString protocol;
		OpString hostname;
		OpString port;
		UINT16 port_int = url.GetAttribute(URL::KResolvedPort);

		if (sub)
			RETURN_IF_ERROR(hostname.Set(UNI_L("*.")));

		RETURN_IF_ERROR(protocol.Set(url.GetAttribute(URL::KProtocolName)));
		RETURN_IF_ERROR(hostname.Append(url.GetAttribute(URL::KHostName)));

		GadgetSecurityDirective *access = NULL;
		RETURN_IF_ERROR(GadgetSecurityDirective::Make(&access));
		access->Into(&pp->access->entries);

		GadgetActionUrl *action;
		RETURN_IF_ERROR(GadgetActionUrl::Make(&action, protocol.CStr(), GadgetActionUrl::FIELD_PROTOCOL, GadgetActionUrl::DATA_NONE));
		action->Into(&access->protocol);
		RETURN_IF_ERROR(GadgetActionUrl::Make(&action, hostname.CStr(), GadgetActionUrl::FIELD_HOST, GadgetActionUrl::DATA_HOST_STRING));
		action->Into(&access->host);

		if (port_int)
		{
			RETURN_IF_ERROR(port.AppendFormat(UNI_L("%d"), (int) port_int));
			RETURN_IF_ERROR(GadgetActionUrl::Make(&action, port.CStr(), GadgetActionUrl::FIELD_PORT, GadgetActionUrl::DATA_NONE));
			action->Into(&access->port);
		}

	}

	return OpStatus::OK;
}

/***********************************************************************************
**
**	ParseActionUrlElement
**
***********************************************************************************/

/* static */ OP_STATUS
OpSecurityManager_Gadget::ParseActionUrlElement(XMLFragment* policy, GadgetActionUrl** action, GadgetSecurityDirective* directive)
{
	// WARNING:
	// If some some reason we should encounter an error or oom condition, we need to make sure that the
	// policy XMLFragment is left in a consistent state. That is, make sure that we have left any entered
	// elements and such.

	OP_ASSERT(policy);
	OP_ASSERT(action);
	OP_ASSERT(directive);

	OP_STATUS err = OpStatus::OK;
	GadgetActionUrl::FieldType ft;
	GadgetActionUrl::DataType dt;
	Head *p = NULL;

	if (policy->EnterElement(UNI_L("protocol")))	{ ft = GadgetActionUrl::FIELD_PROTOCOL; p = &(directive->protocol); }
	else if (policy->EnterElement(UNI_L("host")))	{ ft = GadgetActionUrl::FIELD_HOST; p = &(directive->host); }
	else if (policy->EnterElement(UNI_L("port")))	{ ft = GadgetActionUrl::FIELD_PORT; p = &(directive->port); }
	else if (policy->EnterElement(UNI_L("path")))	{ ft = GadgetActionUrl::FIELD_PATH; p = &(directive->path); }
	else ft = GadgetActionUrl::FIELD_NONE;

	TempBuffer buf;
	RETURN_IF_ERROR(policy->GetAllText(buf)); // TODO: bad - no LeaveElement
	const uni_char *str = buf.GetStorage();
	if (!str)
		str = UNI_L("");

	switch(ft)
	{
	case GadgetActionUrl::FIELD_PROTOCOL:
	case GadgetActionUrl::FIELD_PORT:
		{
			while (str != NULL)
			{
				if (OpStatus::IsSuccess(err = GadgetActionUrl::Make(action, str, ft, GadgetActionUrl::DATA_NONE)))
					(*action)->Into(p);
				else
				{
					OP_DELETE(*action);
					*action = NULL;
					policy->LeaveElement();
					return err;
				}

				str = uni_strchr(str, ',');

				if (str)
					++str;
			}

			policy->LeaveElement();
			return OpStatus::OK;
		}
	case GadgetActionUrl::FIELD_PATH:
		dt = GadgetActionUrl::DATA_NONE;
		break;

	case GadgetActionUrl::FIELD_HOST:
		{
			// Check type of host: localhost|string|range.
			const uni_char *type = policy->GetAttribute(UNI_L("type"));

			if (type && uni_strcmp(type, UNI_L("localhost")) == 0)
				dt = GadgetActionUrl::DATA_HOST_LOCALHOST;
			else if (str && OpSecurityUtilities::IsIPAddressOrRange(str))
				dt = GadgetActionUrl::DATA_HOST_IP;
			else // assume hostname
			{
				dt = GadgetActionUrl::DATA_HOST_STRING;

				if (str)
				{
					// If port is included in hostname; fail
					uni_char *ptr = uni_strstr(str, UNI_L(":"));
					if (ptr)
					{
						policy->LeaveElement();
						GADGETS_REPORT_ERROR_TO_CONSOLE((NULL, UNI_L("Error while parsing widgets.xml!\n\nIgnoring \"%s\"\n"), str));
						return OpStatus::ERR;
					}
				}
			}
			break;
		}
	default:
		policy->LeaveElement();
	case GadgetActionUrl::FIELD_NONE:
		return OpStatus::ERR;
	}

	if (OpStatus::IsSuccess(err = GadgetActionUrl::Make(action, str, ft, dt)))
		(*action)->Into(p);
	else
	{
		OP_DELETE(*action);
		*action = NULL;
	}

	policy->LeaveElement();
	return err;
}

#ifdef SIGNED_GADGET_SUPPORT
OP_STATUS
OpSecurityManager_Gadget::ReloadGadgetSecurityMap()
{
	gadget_security_policies.Clear();
	OP_DELETE(unsigned_policy); unsigned_policy = NULL;
	OP_DELETE(signed_policy); signed_policy = NULL;

	OP_STATUS err = ReadAndParseGadgetSecurityMap();

	if (!unsigned_policy)
	{
#ifndef SECMAN_SIGNED_GADGETS_ONLY
		SetDefaultGadgetPolicy(&unsigned_policy);
#else
		SetRestrictedGadgetPolicy();
#endif // SECMAN_SIGNED_GADGETS_ONLY
	}

	if (!signed_policy)
	{
#ifndef SECMAN_SIGNED_GADGETS_ONLY
		SetDefaultGadgetPolicy(&signed_policy);
#endif // SECMAN_SIGNED_GADGETS_ONLY
	}

	return err;
}

OP_STATUS
OpSecurityManager_Gadget::ReadAndParseGadgetSecurityMap()
{
	OpString basepath;
	OpString path;
	OpFile file;
	BOOL exists = FALSE;
	XMLFragment f;
	BOOL has_unsigned = FALSE;
	BOOL has_signed = FALSE;

	RETURN_IF_ERROR(g_folder_manager->GetFolderPath(OPFILE_USERPREFS_FOLDER, basepath));
	RETURN_IF_ERROR(basepath.Append("security-doc.zip"));
	RETURN_IF_ERROR(path.Set(basepath));
	RETURN_IF_ERROR(path.Append(PATHSEP "widgets-sec-map.xml"));
	RETURN_IF_ERROR(file.Construct(path.CStr()));
	RETURN_IF_ERROR(file.Exists(exists));

	if (!exists)
#ifdef SECMAN_SIGNED_GADGETS_ONLY
		return OpStatus::ERR;
#else
		return OpStatus::OK;
#endif // SECMAN_SIGNED_GADGETS_ONLY

	CryptoXmlSignature::VerifyError error;
	OpAutoPtr<CryptoCertificate> cert;
	OP_STATUS result = CryptoXmlSignature::VerifyWidgetSignature(cert, error, basepath, g_libcrypto_cert_storage);

	if (OpStatus::IsError(result))
#ifdef SECMAN_SIGNED_GADGETS_ONLY
		return OpStatus::ERR;
#else
		return OpStatus::OK;
#endif // SECMAN_SIGNED_GADGETS_ONLY

	RETURN_IF_ERROR(file.Open(OPFILE_READ));
	RETURN_IF_ERROR(f.Parse(static_cast<OpFileDescriptor*>(&file)));

	if (f.EnterElement(UNI_L("security-map")))
	{
		OP_STATUS err = OpStatus::OK;
		while (OpStatus::IsSuccess(err) && f.HasMoreElements())
		{
			if (f.EnterElement(UNI_L("file")))
			{
				const uni_char *src = f.GetAttribute(UNI_L("src"));
				if (src && *src)
				{
					while (OpStatus::IsSuccess(err) && f.HasMoreElements())
					{
						if (f.EnterElement(UNI_L("name")))
						{
							const uni_char *name = f.GetText();
							if (name && *name)
							{
								// create mapping of name == src
								GadgetSecurityPolicy *policy = NULL;
								if (OpStatus::IsSuccess(ReadAndParseGadgetSecurity(basepath, src, &policy)))
								{
									policy->Into(&gadget_security_policies);
									RETURN_IF_ERROR(policy->SetName(name));
								}
							}
							f.LeaveElement(); // name
						}
						else
						{
							// Skip unsupported elements, non-fatal
							f.EnterAnyElement();
							f.LeaveElement();
						}
					}
				}

				f.LeaveElement(); // file
			}
			else if (f.EnterElement(UNI_L("unsigned")))
			{
				if (has_unsigned)
				{
					OP_ASSERT(!"Only 1 policy for unsigned widgets is allowed!");
					err = OpStatus::ERR;
				}
				else
				{
					const uni_char *src = f.GetAttribute(UNI_L("src"));
					if (src && *src)
					{
						OP_DELETE(unsigned_policy); unsigned_policy = NULL;
						// Setup default mapping for unsigned widgets
						if (OpStatus::IsSuccess(ReadAndParseGadgetSecurity(basepath, src, &unsigned_policy)))
							has_unsigned = TRUE;
					}
				}
				f.LeaveElement(); // unsigned
			}
			else if (f.EnterElement(UNI_L("signed")))
			{
				if (has_signed)
				{
					OP_ASSERT(!"Only 1 policy for signed widgets is allowed!");
					err = OpStatus::ERR;
				}
				else
				{
					const uni_char *src = f.GetAttribute(UNI_L("src"));
					if (src && *src)
					{
						OP_DELETE(signed_policy); signed_policy = NULL;
						// Setup default mapping for unsigned widgets
						if (OpStatus::IsSuccess(ReadAndParseGadgetSecurity(basepath, src, &signed_policy)))
							has_signed = TRUE;
					}
				}
				f.LeaveElement(); // unsigned
			}
		}
	}
	else
	{
		// Skip unsupported elements, non-fatal
		f.EnterAnyElement();
		f.LeaveElement();
	}

	OpStatus::Ignore(file.Close());

	return OpStatus::OK;
}
#endif // SIGNED_GADGET_SUPPORT

OP_STATUS
OpSecurityManager_Gadget::ReadAndParseGadgetSecurity(const uni_char *basepath, const uni_char *src, GadgetSecurityPolicy **sec_policy)
{
	OpString path;
	OpFile file;
	BOOL exists = FALSE;
	XMLFragment policy;

	RETURN_IF_ERROR(path.Set(basepath));
	RETURN_IF_ERROR(path.Append(PATHSEP));
	RETURN_IF_ERROR(path.Append(src));
	RETURN_IF_ERROR(file.Construct(path.CStr()));
	RETURN_IF_ERROR(file.Exists(exists));

	if (!exists)
		return OpStatus::ERR;

	RETURN_IF_ERROR(file.Open(OPFILE_READ));
	RETURN_IF_ERROR(policy.Parse(static_cast<OpFileDescriptor*>(&file)));

	// According to the specification https://wiki/developerwiki/Widgets/Security the mandatory elements are <widgets> and <security>
	if (!policy.EnterElement(UNI_L("widgets")) || ! policy.EnterElement(UNI_L("security")))
		return OpStatus::ERR;

	GadgetSecurityPolicy *sec_pol = NULL;
	RETURN_IF_ERROR(GadgetSecurityPolicy::Make(&sec_pol, GadgetSecurityPolicy::GLOBAL));
	OpStackAutoPtr<GadgetSecurityPolicy> policy_ptr(sec_pol);
	OpString error_reason;
	RETURN_IF_ERROR(ParseGadgetPolicy(&policy, ALLOW_BLACKLIST|ALLOW_ACCESS|ALLOW_CONTENT|ALLOW_PRIVATE_NETWORK, sec_pol, error_reason));

	if (sec_policy)
		*sec_policy = sec_pol;

	OpStatus::Ignore(file.Close());

	policy_ptr.release();

	return OpStatus::OK;
}

//////////////////////////////////////////////////////////////////////////

/* static */ OP_STATUS
GadgetSecurityPolicy::Make(GadgetSecurityPolicy** new_obj, PolicyContext context)
{
	*new_obj = OP_NEW(GadgetSecurityPolicy, (context));
	if (!*new_obj)
		return OpStatus::ERR_NO_MEMORY;

	OP_STATUS err;
	if (OpStatus::IsError(err = (*new_obj)->Construct()))
	{
		OP_DELETE(*new_obj); *new_obj = NULL;
		return err;
	}

	return OpStatus::OK;
}

GadgetSecurityPolicy::GadgetSecurityPolicy(PolicyContext context)
	: plugin_content(NO)
	, webserver_content(NO)
	, file_content(NO)
	, access(NULL)
	, private_network(NULL)
	, blacklist(NULL)
	, restricted(FALSE)
	, context(context)
{
}

GadgetSecurityPolicy::~GadgetSecurityPolicy()
{
	OP_DELETE(access);
	OP_DELETE(private_network);
	OP_DELETE(blacklist);
}

OP_STATUS
GadgetSecurityPolicy::Construct()
{
	// The top-level nodes _have_ to exist.
	RETURN_IF_ERROR(GadgetBlacklist::Make(&blacklist));
	RETURN_IF_ERROR(GadgetAccess::Make(&access));
	RETURN_IF_ERROR(GadgetPrivateNetwork::Make(&private_network));

	return OpStatus::OK;
}

GadgetSecurityDirective::GadgetSecurityDirective()
{
}

GadgetSecurityDirective::~GadgetSecurityDirective()
{
	Out();

	protocol.Clear();
	host.Clear();
	port.Clear();
	path.Clear();
}

/* static */ OP_STATUS
GadgetSecurityDirective::Make(GadgetSecurityDirective **new_obj)
{
	*new_obj = OP_NEW(GadgetSecurityDirective, ());
	if (!*new_obj)
		return OpStatus::ERR_NO_MEMORY;
	else
		return OpStatus::OK;
}

//////////////////////////////////////////////////////////////////////////

/* static */ OP_STATUS
GadgetActionUrl::Make(GadgetActionUrl** new_obj, const uni_char* value, FieldType ft, DataType dt)
{
	*new_obj = OP_NEW(GadgetActionUrl, ());
	if (!*new_obj)
		return OpStatus::ERR_NO_MEMORY;

	OP_STATUS err;
	if (OpStatus::IsError(err = (*new_obj)->Construct(value, ft, dt)))
	{
		OP_DELETE(*new_obj);
		*new_obj = NULL;
		return err;
	}

	return OpStatus::OK;
}

GadgetActionUrl::GadgetActionUrl()
	: ft(FIELD_NONE)
	, dt(DATA_NONE)
{
	op_memset(&field, 0, sizeof(field));
}

GadgetActionUrl::~GadgetActionUrl()
{
	if (ft == FIELD_PATH ||
		dt == DATA_HOST_STRING)
		op_free(field.value);
}

OP_STATUS
GadgetActionUrl::Construct(const uni_char* value, GadgetActionUrl::FieldType ft, GadgetActionUrl::DataType data)
{
	if (ft == FIELD_NONE)
		return OpStatus::ERR;

	this->ft = ft;
	this->dt = data;

	switch (ft)
	{
		case FIELD_PROTOCOL:
		{
			if (!value)
				return OpStatus::ERR;
			field.protocol = urlManager->MapUrlType(value);
			if (field.protocol == URL_UNKNOWN)
			{
				// Do not fail for external protocols if we don't support them
				// just ignore them
#ifdef EXTERNAL_PROTOCOL_SCHEME_SUPPORT
				OpString8 temp;
				RETURN_IF_ERROR(temp.Set(value));
				field.protocol = urlManager->AddProtocolHandler(temp.CStr());
#else
				field.protocol = URL_NULL_TYPE;
#endif //EXTERNAL_PROTOCOL_SCHEME_SUPPORT
			}

			return OpStatus::OK;
		}
		case FIELD_HOST:
		{
			switch(this->dt)
			{
				case DATA_HOST_LOCALHOST:
					return OpStatus::OK;
				case DATA_HOST_STRING:
				{
					if (!value)
						return OpStatus::ERR;
					field.value = uni_strdup(value);
					if (field.value == NULL)
						return OpStatus::ERR_NO_MEMORY;

					// Convert string to lower case.
					field.value = uni_strlwr(field.value);
					return OpStatus::OK;
				}
				case DATA_HOST_IP:
				{
					if (!value)
						return OpStatus::ERR;
					RETURN_VALUE_IF_ERROR(field.host.Parse(value), OpStatus::ERR);
					return OpStatus::OK;
				}
				default:
					return OpStatus::ERR;
			}
		}
		case FIELD_PORT:
		{
			if (!value)
				return OpStatus::ERR;
			RETURN_VALUE_IF_ERROR(OpSecurityUtilities::ParsePortRange(value, field.port.low, field.port.high), OpStatus::ERR);
			return OpStatus::OK;
		}
		case FIELD_PATH:
		{
			if (!value)
				return OpStatus::ERR;
			field.value = uni_strdup(value);
			if (field.value == NULL)
				return OpStatus::ERR_NO_MEMORY;

			return OpStatus::OK;
		}
	}

	return OpStatus::OK;
}

//////////////////////////////////////////////////////////////////////////

/* static */ OP_STATUS
GadgetAccess::Make(GadgetAccess** new_obj)
{
	*new_obj = OP_NEW(GadgetAccess, ());
	if (!*new_obj)
		return OpStatus::ERR_NO_MEMORY;

	return OpStatus::OK;
}

//////////////////////////////////////////////////////////////////////////

/* static */ OP_STATUS
GadgetPrivateNetwork::Make(GadgetPrivateNetwork** new_obj)
{
	*new_obj = OP_NEW(GadgetPrivateNetwork, ());
	if (!*new_obj)
		return OpStatus::ERR_NO_MEMORY;

	return OpStatus::OK;
}

//////////////////////////////////////////////////////////////////////////

/* static */ OP_STATUS
GadgetBlacklist::Make(GadgetBlacklist** new_obj)
{
	*new_obj = OP_NEW(GadgetBlacklist, ());
	if (!*new_obj)
		return OpStatus::ERR_NO_MEMORY;

	return OpStatus::OK;
}

#endif // GADGET_SUPPORT
