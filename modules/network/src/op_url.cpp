/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"
#include "modules/network/op_url.h"
#include "modules/network/src/op_server_name_impl.h"
#include "modules/url/url_man.h"

OpURL::OpURL(const URL &a_url)
:m_url(a_url)
{
}

OpURL::OpURL(URL_Rep* url_rep, const char* rel)
:m_url(url_rep, rel)
{
}

OpURL::OpURL(const OpURL& url)
:m_url(url.GetURL())
{
}

OpURL::OpURL(const OpURL& url, const uni_char* rel)
:m_url(url.GetURL(), rel)
{
}

OP_STATUS OpURL::GetNameUsername(OpString8 &val, BOOL escaped /*= FALSE*/, Password_Display password /*= NoPassword*/) const
{
	if (password == NoPassword)
		return m_url.GetAttribute(escaped ? URL::KName_Username_Escaped : URL::KName_Username, val);
	else if (password == PasswordVisible_NOT_FOR_UI)
		return m_url.GetAttribute(escaped ? URL::KName_Username_Password_Escaped_NOT_FOR_UI : URL::KName_Username_Password_NOT_FOR_UI, val);
	else
		return m_url.GetAttribute(escaped ? URL::KName_Username_Password_Hidden_Escaped : URL::KName_Username_Password_Hidden, val);
}

OP_STATUS OpURL::GetNameUsername(OpString &val, BOOL escaped /*= FALSE*/, Password_Display password /*= NoPassword*/, unsigned short charsetID /*= 0*/) const
{
	if (password == NoPassword)
		return m_url.GetAttribute(escaped ? URL::KUniName_Username_Escaped : URL::KUniName_Username, charsetID, val);
	else if (password == PasswordVisible_NOT_FOR_UI)
		return m_url.GetAttribute(escaped ? URL::KUniName_Username_Password_Escaped_NOT_FOR_UI : URL::KUniName_Username_Password_NOT_FOR_UI, charsetID, val);
	else
		return m_url.GetAttribute(escaped ? URL::KUniName_Username_Password_Hidden_Escaped : URL::KUniName_Username_Password_Hidden, charsetID, val);
}

OP_STATUS OpURL::GetNameWithFragmentUsername(OpString8 &val, BOOL escaped /*= FALSE*/, Password_Display password /*= NoPassword*/) const
{
	if (password == NoPassword)
		return m_url.GetAttribute(escaped ? URL::KName_With_Fragment_Username_Escaped : URL::KName_With_Fragment_Username, val);
	else if (password == PasswordVisible_NOT_FOR_UI)
		return m_url.GetAttribute(escaped ? URL::KName_With_Fragment_Username_Password_Escaped_NOT_FOR_UI : URL::KName_With_Fragment_Username_Password_NOT_FOR_UI, val);
	else
		return m_url.GetAttribute(escaped ? URL::KName_With_Fragment_Username_Password_Hidden_Escaped : URL::KName_With_Fragment_Username_Password_Hidden, val);

}

OP_STATUS OpURL::GetNameWithFragmentUsername(OpString &val, BOOL escaped /*= FALSE*/, Password_Display password /*= NoPassword*/, unsigned short charsetID /*= 0*/) const
{
	if (password == NoPassword)
		return m_url.GetAttribute(escaped ? URL::KUniName_With_Fragment_Username_Escaped : URL::KUniName_With_Fragment_Username, charsetID, val);
	else if (password == PasswordVisible_NOT_FOR_UI)
		return m_url.GetAttribute(escaped ? URL::KUniName_With_Fragment_Username_Password_Escaped_NOT_FOR_UI : URL::KUniName_With_Fragment_Username_Password_NOT_FOR_UI, charsetID, val);
	else
		return m_url.GetAttribute(escaped ? URL::KUniName_With_Fragment_Username_Password_Hidden_Escaped : URL::KUniName_With_Fragment_Username_Password_Hidden, charsetID, val);
}

void OpURL::GetServerName(class ServerNameCallback *callback) const
{
	OP_ASSERT(callback);
	OP_STATUS status = OpStatus::OK;
	OpServerNameImpl *result = OP_NEW(OpServerNameImpl, ((ServerName *) m_url.GetAttribute(URL::KServerName, NULL)));
	if (!result)
		status = OpStatus::ERR_NO_MEMORY;
	callback->ServerNameResult(result, status);
}

OpURL OpURL::Make(const uni_char* url)
{
	return OpURL(urlManager->GetURL(url, 0));
}

OpURL OpURL::Make(OpURL prnt_url, const char* url, const char* rel)
{
	return OpURL(urlManager->GetURL(prnt_url.GetURL(), OpStringC8(url), OpStringC8(rel), FALSE));
}

OpURL OpURL::Make(const char* url, const char* rel)
{
	return OpURL(urlManager->GetURL(url, rel, FALSE, 0));
}

OpURL OpURL::Make(const uni_char* url, const uni_char* rel)
{
	return OpURL(urlManager->GetURL(url, rel, FALSE, 0));
}

OpURL OpURL::Make(OpURL prnt_url, const uni_char* url)
{
	return OpURL(urlManager->GetURL(prnt_url.GetURL(), url));
}

OpURL OpURL::Make(OpURL prnt_url, const uni_char* url, const uni_char* rel)
{
	return OpURL(urlManager->GetURL(prnt_url.GetURL(), url, rel));
}

OpURL OpURL::Make(const char* url)
{
	return OpURL(urlManager->GetURL(url, 0));
}

OpURL OpURL::Make(OpURL prnt_url, const char* url)
{
	return OpURL(urlManager->GetURL(prnt_url.GetURL(), url, FALSE));
}
