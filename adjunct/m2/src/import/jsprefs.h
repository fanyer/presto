/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 * 
 * George Refseth, rfz@opera.com
 */

#ifndef JSPREFS_H
#define JSPREFS_H

class JsPrefNode
{
	friend class JsPrefsFile;
protected:
	JsPrefNode* m_next;
	char* m_Token;
	char* m_Value;
	char* SetToken(char*);
	char* SetValue(char*);
public:
	/** Default creates a null object */
	JsPrefNode() { m_Token = NULL; m_Value = NULL; m_next = NULL; };
	JsPrefNode(char* cToken, char* cVal);
	~JsPrefNode();

	/** Return node value*/
	char* GetValue();

	/** Return node token*/
	char* GetToken() { return m_Token; };
};

class JsPrefsFile
{
private:
	JsPrefNode* m_nHead;
public:
	JsPrefsFile();
	~JsPrefsFile();

	BOOL SetFile(const OpStringC& name);

	BOOL Add(char*, char*);
	char* Find(const char*);
	char* GetValue(char* pType);

	static void StripAllCharsInString(char* string, const char* szCharsToStrip);
	static char* FixStr(char *psz, char stripFront = 0, char stripTail = 0);
};

#endif // JSPREFS_H
