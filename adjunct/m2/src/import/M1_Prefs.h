/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 * 
 * George Refseth, rfz@opera.com
 */

#ifndef M1_PREFS_H_INCLUDED
#define M1_PREFS_H_INCLUDED

# include "modules/util/simset.h"

typedef int ValueType;
#define VT_NONE			0
#define VT_INTEGER		1
#define VT_STRING		2

class M1_Entry : public Link
{
public:
	M1_Entry();
	~M1_Entry();

	inline OpString8& Name() { return m_name; };
	void SetName(const OpStringC8& name) { if (OpStatus::IsSuccess(m_name.Set(name))) m_name.Strip(TRUE,TRUE); };

	BOOL GetValue(int& integer);
	BOOL GetValue(OpString8& string);

	BOOL SetValue(const OpStringC8& string);
	BOOL SetValue(int integer);

	BOOL IsSameAs(const OpStringC8& other);

private:
	M1_Entry(const M1_Entry&);
	M1_Entry& operator =(const M1_Entry&);

private:
	ValueType m_type;
	OpString8 m_valueString;
	int		  m_valueInteger;
	OpString8 m_name;
};


class M1_Section : public Link
{
public:
	M1_Section();
	~M1_Section();

	inline OpString8& Name() { return m_name; };
	void SetName(const OpStringC8& name) { m_name.Set(name); };

private:
	M1_Section(const M1_Section&);
	M1_Section& operator =(const M1_Section&);

private:
	friend class M1_Preferences;
	friend class M1_Entry;

	OpString8 m_name;
	Head	  m_entries;
};


class M1_Preferences
{
public:
	M1_Preferences();
	virtual ~M1_Preferences();

	BOOL HasEntry(const OpStringC8& section, const OpStringC8& key);
	BOOL HasSection(const OpStringC8& section);

	BOOL GetStringValue(const OpStringC8& section, const OpStringC8& key, OpString8& value);
	BOOL GetIntegerValue(const OpStringC8& section, const OpStringC8& key, int& value);
	BOOL GetEntry(const OpStringC8& section, OpString8& entry, int index);

	BOOL SetFile(const OpStringC& path);

private:
	M1_Preferences(const M1_Preferences&);
	M1_Preferences& operator =(const M1_Preferences&);

private:
	void Empty();
	void Parse();

	M1_Section*	_GetSection(const OpStringC8& name, BOOL create = FALSE);
	M1_Entry*	_GetEntry(const OpStringC8& section, const OpStringC8& key, BOOL create = FALSE);
	M1_Entry*	_GetEntry(const OpStringC8& section, int index, BOOL create = FALSE);

	OpString	m_path;
	Head		m_sections;
};

#endif	//M1_PREFS_H_INCLUDED
