/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#include "core/pch.h"

#include "modules/encodings/decoders/utf8-decoder.h"
#include "modules/formats/uri_escape.h"
#include "modules/util/simset.h"

#include "modules/url/url2.h"
#include "modules/url/prot_sel.h"
#include "modules/url/url_name.h"
#include "modules/url/url_scheme.h"

URL_Scheme_Authority_List::~URL_Scheme_Authority_List()
{
	URL_Scheme_Authority_Components *item;

	while((item = (URL_Scheme_Authority_Components *) First()) != NULL)
		item->Out(); // Non-destructive, just in case. All objects are reference-counted with delete when all references are released
}


URL_Scheme_Authority_Components *URL_Scheme_Authority_List::FindSchemeAndAuthority(OP_STATUS &op_err, URL_Name_Components_st *components, BOOL create)
{
	URL_Scheme_Authority_Components *item;

	op_err = OpStatus::OK;

	if(!components || !components->scheme_spec)
		return NULL;

	item = (URL_Scheme_Authority_Components *) First();

	while(item)
	{
		if(item->Match(components))
			break;
		item = (URL_Scheme_Authority_Components *) item->Suc();
	}

	if(!item && create)
	{
		OpAutoPtr<URL_Scheme_Authority_Components> new_item(OP_NEW(URL_Scheme_Authority_Components, ()));

		if(new_item.get() == NULL)
		{
			op_err = OpStatus::ERR_NO_MEMORY;
			return NULL;
		}

		op_err = new_item->Construct(components);
		RETURN_VALUE_IF_ERROR(op_err, NULL);

		new_item->Into(this);
		item = new_item.release();
	}

	if(item)
	{
		// List is LRU sorted
		item->Out();
		item->IntoStart(this); 
	}

	components->basic_components = item;
	return item;
}

URL_Scheme_Authority_Components::URL_Scheme_Authority_Components()
{
	scheme_spec = NULL;
	port = 0;
}

URL_Scheme_Authority_Components::~URL_Scheme_Authority_Components()
{
	if(InList())
		Out();
}

OP_STATUS URL_Scheme_Authority_Components::Construct(URL_Name_Components_st *components)
{
	if(!components)
		return OpStatus::ERR_NULL_POINTER;
	scheme_spec = components->scheme_spec;
	RETURN_IF_ERROR(username.Set(components->username));
	RETURN_IF_ERROR(password.Set(components->password));
	if(scheme_spec && scheme_spec->have_servername)
		server_name = components->server_name;
	port = components->port;
	max_length = (scheme_spec && scheme_spec->protocolname ? op_strlen(scheme_spec->protocolname) : 0) + 
					username.Length()+ password.Length() + 
					(server_name != (ServerName *) NULL ? server_name->GetName().Length() : 0) + // ASCII version of servername is always the longest
					3 /* "://" */+ 2 /* ":" and "@" */ +5 /* Port */; // Just add maximum lengths to make this easier
	return OpStatus::OK;
}

BOOL URL_Scheme_Authority_Components::Match(URL_Name_Components_st *components)
{
	if(!components)
		return FALSE;
	
	if(scheme_spec != components->scheme_spec ||
		server_name != components->server_name ||
		(port ? port : scheme_spec->default_port) != (components->port ? components->port : scheme_spec->default_port))
		return FALSE;

	if(username.Compare(components->username) != 0)
	{
		if(username.IsEmpty() || components->username == NULL)
			return FALSE;

		if(UriUnescape::stricmp(username.CStr(), components->username, UriUnescape::All) != 0)
			return FALSE;
	}

	if(password.Compare(components->password) != 0)
	{
		if(password.IsEmpty() || components->password == NULL)
			return FALSE;

		if(UriUnescape::stricmp(password.CStr(), components->password, UriUnescape::All) != 0)
			return FALSE;
	}

	return TRUE;
}

unsigned URL_Scheme_Authority_Components::GetMaxOutputStringLength() const
{
	if (!scheme_spec)
		return 0;
	unsigned max_len = 0;
	max_len += op_strlen(scheme_spec->protocolname);
	max_len += username.Length();
	max_len += 2; // For : and @ in passwords displays.

	unsigned password_len = password.Length();
	password_len = MAX(password_len, 8); // Might be replaced by ******.
	max_len += password_len;

	max_len += 3; // For "://".

	if (server_name && server_name->Name())
		max_len += op_strlen(server_name->Name());

	max_len += 6; // For port number and colon.

	return max_len;
}

enum URL_Name_Password_Display {PASSWORD_DISPLAY_NONE, PASSWORD_DISPLAY_HIDE,  PASSWORD_DISPLAY};

void URL_Scheme_Authority_Components::OutputString(URL::URL_NameStringAttribute password_hide, char *url_buffer, size_t buffer_len, int linkid) const
{
	if(!scheme_spec)
		return;

	BOOL display_username = FALSE;
	BOOL force_portnumber_display = (linkid != URL_NAME_LinkID_None);
	URL_Name_Password_Display display_password = PASSWORD_DISPLAY_NONE;
	size_t buffer_len1;

	switch(password_hide)
	{
	case URL::KName_Username_Password_NOT_FOR_UI:
	case URL::KName_Username_Password_Escaped_NOT_FOR_UI:
	case URL::KName_With_Fragment_Username_Password_NOT_FOR_UI:
	case URL::KName_With_Fragment_Username_Password_Escaped_NOT_FOR_UI:
		display_username = TRUE;
		display_password = PASSWORD_DISPLAY;
		break;

	case URL::KName_Username_Password_Hidden:
	case URL::KName_Username_Password_Hidden_Escaped:
	case URL::KName_With_Fragment_Username_Password_Hidden:
	case URL::KName_With_Fragment_Username_Password_Hidden_Escaped:
		display_username = TRUE;
		display_password = PASSWORD_DISPLAY_HIDE;
		break;

	case URL::KName_Username:
	case URL::KName_Username_Escaped:
	case URL::KName_With_Fragment_Username:
	case URL::KName_With_Fragment_Username_Escaped:
		display_username = TRUE;
		break;
	}

	buffer_len1 = buffer_len- op_strlen(url_buffer);
	op_strncat(url_buffer, scheme_spec->protocolname, buffer_len1);
	
	const ServerName *srvname = server_name;
	BOOL has_servername = (srvname && srvname->Name() && *srvname->Name() != '\0');
	
	buffer_len1 = buffer_len- op_strlen(url_buffer);
	op_strncat(url_buffer,(has_servername  || scheme_spec->have_servername? "://" : ":"), buffer_len1);
	
	if (has_servername)
	{
		// display_password != PASSWORD_DISPLAY_NONE => display_username
		if (display_username && (username.CStr() || password.CStr()))
		{
			buffer_len1 = buffer_len- op_strlen(url_buffer);
			StrCatSnprintf(url_buffer,buffer_len1,
				(display_password != PASSWORD_DISPLAY_NONE && password.HasContent() ? "%s:%s@" : "%s@"),
				(username.HasContent() ? username.CStr() : ""),
				(display_password == PASSWORD_DISPLAY? (password.HasContent() ? password.CStr() : "") : "*****"));
		}
		

		unsigned short portnum = port;

		if(force_portnumber_display && !port)
			portnum = scheme_spec->default_port;

		buffer_len1 = buffer_len- op_strlen(url_buffer);
		op_strncat(url_buffer, server_name->Name(), buffer_len1);
		if(portnum)
		{
			size_t bufferpos = op_strlen(url_buffer);

			if(buffer_len - bufferpos >7) // Max stringlength of port is 5 digits + ":"
			{
				url_buffer[bufferpos++] = ':';
				op_itoa(portnum, url_buffer+bufferpos, 10);
			}
		}
		//StrCatSnprintf(url_buffer,buffer_len1, (portnum ? "%s:%u" : "%s"), server_name->Name(), (unsigned) portnum);
	}
}

void URL_Scheme_Authority_Components::OutputString(URL::URL_UniNameStringAttribute password_hide, uni_char *url_buffer, size_t buffer_len) const
{
	if(!scheme_spec)
		return;

	BOOL display_username = FALSE;
	BOOL display_only_escaped = FALSE;
	URL_Name_Password_Display display_password = PASSWORD_DISPLAY_NONE;
	size_t buffer_len1;

	switch(password_hide)
	{
	case URL::KUniName_Username_Password_Escaped_NOT_FOR_UI:
	case URL::KUniName_With_Fragment_Username_Password_Escaped_NOT_FOR_UI:
		display_only_escaped = TRUE;
		// fall-through
	case URL::KUniName_Username_Password_NOT_FOR_UI:
	case URL::KUniName_With_Fragment_Username_Password_NOT_FOR_UI:
		display_username = TRUE;
		display_password = PASSWORD_DISPLAY;
		break;

	case URL::KUniName_Username_Password_Hidden_Escaped:
	case URL::KUniName_With_Fragment_Username_Password_Hidden_Escaped:
		display_only_escaped = TRUE;
		// fall-through
	case URL::KUniName_Username_Password_Hidden:
	case URL::KUniName_With_Fragment_Username_Password_Hidden:
		display_username = TRUE;
		display_password = PASSWORD_DISPLAY_HIDE;
		break;

	case URL::KUniName_Username_Escaped:
	case URL::KUniName_With_Fragment_Username_Escaped:
		display_only_escaped = TRUE;
		// fall-through
	case URL::KUniName_Username:
	case URL::KUniName_With_Fragment_Username:
		display_username = TRUE;
		break;
	}

	buffer_len1 = buffer_len- uni_strlen(url_buffer);
	uni_strncat(url_buffer, scheme_spec->uni_protocolname, buffer_len1);

	const ServerName *srvname = server_name;
	BOOL has_servername = (srvname && srvname->UniName() && *srvname->UniName()!='\0');

	buffer_len1 = buffer_len- uni_strlen(url_buffer);
	uni_strncat(url_buffer,(has_servername || scheme_spec->have_servername ? UNI_L("://") : UNI_L(":")), buffer_len1);

	if (has_servername)
	{
		int len;
		int len2 = uni_strlen(url_buffer);
		// display_password != PASSWORD_DISPLAY_NONE => display_username
		if (display_username && (username.CStr() || password.CStr())) do
		{
			UTF8toUTF16Converter converter;
			if(OpStatus::IsError(converter.Construct()))
				break;
			int read = 0;

			if(username.HasContent())
			{
				len = username.Length();
				len = converter.Convert((char *) username.CStr(), len, (char *) (url_buffer + len2), UNICODE_SIZE(buffer_len-len2-1), &read);
				if(len != -1)
					len2 += UNICODE_DOWNSIZE(len);
			}

			if(display_password != PASSWORD_DISPLAY_NONE && password.CStr())
			{
				url_buffer[len2++] = ':';

				if(display_password == PASSWORD_DISPLAY_HIDE)
				{
					uni_strcpy(url_buffer+len2, UNI_L("*****"));
					len2 += 5;
				}
				else
				{
					len = password.Length();
					len = converter.Convert((char *) password.CStr(), len, (char *) (url_buffer + len2), UNICODE_SIZE(buffer_len-len2-1), &read);
					if(len != -1)
						len2 += UNICODE_DOWNSIZE(len);
				}
			}

			url_buffer[len2++] = '@';
			url_buffer[len2] = 0;
		}while(0);

		unsigned short portnum = port;

		buffer_len1 = buffer_len- uni_strlen(url_buffer);
		if(display_only_escaped)
		{
			do{
				UTF8toUTF16Converter converter;
				if(OpStatus::IsError(converter.Construct()))
				{
					display_only_escaped = FALSE;
					break;
				}
				int read = 0;

				len = server_name->GetName().Length();
				len = converter.Convert((char *) server_name->Name(), len, (char *) (url_buffer + len2), UNICODE_SIZE(buffer_len-len2-1), &read);
				if(len != -1)
					len2 += UNICODE_DOWNSIZE(len);

				url_buffer[len2] = 0;
				if(portnum)
				{
					//buffer_len1 = buffer_len- len2;
					//size_t bufferpos = uni_strlen(url_buffer);
					
					if(buffer_len - len2 >7) // Max stringlength of port is 5 digits + ":"
					{
						url_buffer[len2++] = ':';
						uni_itoa(portnum, url_buffer+len2, 10);
					}
					//uni_snprintf(url_buffer+len2, buffer_len1, UNI_L(":%u"), (unsigned) portnum);
				}
			}while(0);
		}


		if(!display_only_escaped)
		{
			uni_strncat(url_buffer, server_name->UniName(), buffer_len1);
			if(portnum)
			{
				size_t bufferpos = uni_strlen(url_buffer);
				
				if(buffer_len - bufferpos >7) // Max stringlength of port is 5 digits + ":"
				{
					url_buffer[bufferpos++] = ':';
					uni_itoa(portnum, url_buffer+bufferpos, 10);
				}
			}
			//uni_snprintf(url_buffer+len2, buffer_len1, (portnum ? UNI_L("%s:%u") : UNI_L("%s")), server_name->UniName(), (unsigned) portnum);
		}
	}

}
