/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 * 
 * George Refseth, rfz@opera.com
 */

#include "core/pch.h"

#ifdef M2_SUPPORT

# include "adjunct/m2/src/engine/engine.h"
# include "adjunct/m2/src/import/jsprefs.h"


# define NETSCAPE_USER_PREF "user_pref"
/* obsolete:
#define NETSCAPE_USER_PREF_TOKEN_MAIL_POP_NAME ("mail.pop_name")
#define NETSCAPE_USER_PREF_TOKEN_MAIL_SMTP_NAME ("mail.smtp_name")
#define NETSCAPE_USER_PREF_TOKEN_MAIL_DIR ("mail.directory")
#define NETSCAPE_USER_PREF_TOKEN_MAIL_POP_SERVER ("network.hosts.pop_server")
#define NETSCAPE_USER_PREF_TOKEN_MAIL_SMTP_SERVER ("network.hosts.smtp_server")
#define NETSCAPE_USER_PREF_TOKEN_MAIL_USER_ORGANIZATION ("mail.identity.organization")
#define NETSCAPE_USER_PREF_TOKEN_MAIL_USER_EMAIL ("mail.identity.useremail")
#define NETSCAPE_USER_PREF_TOKEN_MAIL_USER_NAME ("mail.identity.username")
#define NETSCAPE_USER_PREF_TOKEN_MAIL_LEAVE_ON_SERVER ("mail.leave_on_server")
#define NETSCAPE_USER_PREF_TOKEN_MAIL_USER_REPLY ("mail.identity.reply_to")
#define NETSCAPE_USER_PREF_TOKEN_MAIL_SIGNATURE ("mail.signature_file")
#define NETSCAPE_USER_PREF_TOKEN_MAIL_SIGNATURE_FLAG ("mail.use_signature_file")

#define NETSCAPE_TRUE "true"
#define NETSCAPE_FALSE "false"
*/

JsPrefsFile::JsPrefsFile()
{
	m_nHead = OP_NEW(JsPrefNode, ());
}

JsPrefNode::JsPrefNode(char* cToken, char* cVal)
{
	m_Token = NULL;
	m_Value = NULL;
	m_next = NULL;

	if(cToken && *cToken)
	{
		SetToken(cToken);
	}

	if(cVal && *cVal)
	{
		SetValue(cVal);
	}
}


BOOL JsPrefsFile::SetFile(const OpStringC& name)
{
	BOOL success = FALSE;

	OpFile* f = MessageEngine::GetInstance()->GetGlueFactory()->CreateOpFile(name);

	if (f != NULL && OpStatus::IsSuccess(f->Open(OPFILE_READ|OPFILE_SHAREDENYWRITE)))
	{
		do
		{
			OpString8 tmp8;
			f->ReadLine(tmp8);

			if (tmp8.HasContent())
			{
				//* do the parsing of token and value here*/
				char* pToken = tmp8.CStr();
				while (*pToken == ' ') pToken++;	//skip blanks
				if(strncmp(pToken, NETSCAPE_USER_PREF, ARRAY_SIZE(NETSCAPE_USER_PREF)-1) == 0)	//check if the line is a prefs entry
				{
					pToken += ARRAY_SIZE(NETSCAPE_USER_PREF)-1;
					char* pValue = strchr(pToken, ',');

					if (pValue)
					{
						*pValue='\0';
						pValue++;
					}
					else
						continue;

					//strip trailing and leading [ ("; chars ]

					pToken = FixStr(pToken);
					StripAllCharsInString(pToken, ";)(\",");
					pValue = FixStr(pValue);
					StripAllCharsInString(pValue, ";)(\"");

					if (*pToken && *pValue)
					{
						Add(pToken, pValue);
					}
				}
			}

		} while (!f->Eof());

		f->Close();

		success = TRUE;
	}

	if (f)
	{
		MessageEngine::GetInstance()->GetGlueFactory()->DeleteOpFile(f);
	}

	return success;
}


JsPrefsFile::~JsPrefsFile()
{
	JsPrefNode* node = m_nHead;
	while(node)
	{
		JsPrefNode* nextnode = node->m_next;
		OP_DELETE(node);
		node = nextnode;
	}

}

BOOL JsPrefsFile::Add(char* pszToken, char* pszValue)
{
	JsPrefNode* node = m_nHead;
	while (node->m_next)
	{
		node = node->m_next;
	}
	node->m_next = OP_NEW(JsPrefNode, (pszToken, pszValue));
	return node->m_next ? TRUE : FALSE;
}

char* JsPrefsFile::Find(const char* pType)
{
	JsPrefNode* node = m_nHead;					// no content in head
	while ((node = node->m_next) != NULL)
	{
		if(strcmp(node->GetToken(), pType) == 0)
			return node->GetValue();
	}
	return NULL;

}

char* JsPrefsFile::GetValue(char* pType)
{
	return Find(pType);
}

void JsPrefsFile::StripAllCharsInString(char* string, const char* szCharsToStrip)
{
	char *position = string;

	while (*position)
	{
		if (!op_strchr(szCharsToStrip, *position))
			*string++ = *position;

		position++;
	}
	*string = 0;
}

// Fix a string, remove newlines (replaced with spaces), and strip trailing and leading spaces
char* JsPrefsFile::FixStr(char *psz, char stripFront, char stripTail)
{
	if (psz)
	{
		char* pch;
		//	first loop
		pch = psz;
		while (*pch)
		{
			if (*pch == '\r' && *(pch+1) == '\n')
			{
				*pch = 2;
				*(++pch) = 2;
			}
			else if (*pch == '\r' || *pch == '\n' || *pch == 3)
				*pch = ' ';

			++pch;
		}

		//strip leading spaces
		while(*psz == ' ' || (stripFront && *psz == stripFront))
			++psz;

		//strip trailing spaces
		pch = psz + strlen(psz);
		while(*(pch-1) == ' ' || (stripTail && *(pch-1) == stripTail))
			pch--;
		*pch = 0;
	}
	return psz;
}



//////////////////////////////////////////////////////////////////////////////////////
JsPrefNode::~JsPrefNode()
{
	OP_DELETEA(m_Token);
	OP_DELETEA(m_Value);
}

char* JsPrefNode::GetValue()
{
	return m_Value;
}

char* JsPrefNode::SetValue(char* pValue)
{
	OP_DELETEA(m_Value);
	m_Value = new char[strlen(pValue)+1];
	if (m_Value)
		strcpy(m_Value, pValue);
	return m_Value;
}

char* JsPrefNode::SetToken(char* pToken)
{
	OP_DELETEA(m_Token);
	m_Token = new char[strlen(pToken)+1];
	if (m_Token)
		strcpy(m_Token, pToken);
	return m_Token;
}

#endif //M2_SUPPORT
