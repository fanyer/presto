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

#include <ctype.h>

#include "adjunct/desktop_util/opfile/desktop_opfile.h"
#include "adjunct/m2/src/engine/engine.h"
#include "adjunct/m2/src/import/M1_Prefs.h"


#define PREFS_MAX_LINE_LEN 255


M1_Entry::M1_Entry()
: m_type(VT_NONE),
  m_valueInteger(-1)
{
}


M1_Entry::~M1_Entry()
{
}


BOOL M1_Entry::GetValue(OpString8& string)
{
	if (VT_STRING == m_type)
	{
		string.Set(m_valueString);
	}
	else
	{
		return FALSE;
	}
	return TRUE;
}


BOOL M1_Entry::GetValue(int& integer)
{
	if (VT_INTEGER == m_type)
	{
		integer = m_valueInteger;
	}
	else
	{
		return FALSE;
	}
	return TRUE;
}


BOOL is_number(const char* in, int size)
{
	for (int i = 0; i < size; i++)
	{
		if (isalpha(in[i]))
			return FALSE;
	}
	return TRUE;
}

BOOL M1_Entry::SetValue(const OpStringC8& string)
{
	if (VT_INTEGER == m_type)
		return FALSE;

	if (is_number(string.CStr(), string.Length()))
	{
		m_type = VT_INTEGER;
		m_valueInteger = atol(string.CStr());
	}
	else
	{
		m_type = VT_STRING;
		m_valueString.Set(string);
		m_valueString.Strip(TRUE,TRUE);
	}
	return TRUE;
}


BOOL M1_Entry::SetValue(int integer)
{
	if (VT_STRING == m_type)
		return FALSE;

	m_type = VT_INTEGER;
	m_valueInteger = integer;

	return TRUE;
}


BOOL M1_Entry::IsSameAs(const OpStringC8& other)
{
	if (m_name.CompareI(other) == 0)
		return TRUE;

	return FALSE;
}


M1_Section::M1_Section()
{
}


M1_Section::~M1_Section()
{
	M1_Entry *entry = (M1_Entry*) m_entries.First();
	while (entry)
	{
		M1_Entry *e = (M1_Entry*) entry->Suc();

		entry->Out();
		OP_DELETE(entry);

		entry = e;
	}
}


M1_Preferences::M1_Preferences()
{
}


M1_Preferences::~M1_Preferences()
{
	Empty();
}


BOOL M1_Preferences::SetFile(const OpStringC& path)
{
	BOOL exists;
	if (DesktopOpFileUtils::Exists(path, exists) != OpStatus::OK ||
		!exists)
	{
		return FALSE;	// file not found
	}

	m_path.Set(path);
	Parse();

	return TRUE;
}


BOOL M1_Preferences::HasEntry(const OpStringC8& section, const OpStringC8& entry)
{
	return (_GetEntry(section, entry) != NULL);
}


BOOL M1_Preferences::HasSection(const OpStringC8& section)
{
	return (_GetSection(section) != NULL);
}


BOOL M1_Preferences::GetStringValue(const OpStringC8& section, const OpStringC8& key, OpString8& value)
{
	M1_Entry *entry = _GetEntry(section, key);

	if (entry)
	{
		OpString8 val;
		if (entry->GetValue(val))
		{
			value.Set(val);
			return TRUE;
		}
	}
	return FALSE;
}


BOOL M1_Preferences::GetIntegerValue(const OpStringC8& section, const OpStringC8& key, int& value)
{
	M1_Entry *entry = _GetEntry(section, key);

	if (entry)
	{
		int val = 0;
		if (entry->GetValue(val))
		{
			value = val;
			return TRUE;
		}
	}
	return FALSE;
}


void M1_Preferences::Parse()
{
	Empty();

    OpFile old;
    RETURN_VOID_IF_ERROR(old.Construct(m_path.CStr()));
    RETURN_VOID_IF_ERROR(old.Open(OPFILE_READ));

	M1_Section *section = OP_NEW(M1_Section, ());
	if (section)
	{
        section->Into(&m_sections);
        OpString8 text;

        while (!old.Eof())
        {
            char *key = NULL;
            char *value = NULL;

            if (OpStatus::IsError(old.ReadLine(text)))
                break;
            char *line = text.CStr();

            if (*line == '[')
            {
                // Section
                line++;
                char *t = strrchr(line, ']');
                if (t)
                    *t = 0;
                else
                {
                    t = strchr(line, '\n');
                    if (t)
                        *t = 0;
                }

                section = OP_NEW(M1_Section, ());
                section->SetName(line);
                section->Into(&m_sections);
            }
            else if (*line == ';')
            {
                // Comment, ignore it
            }
            else if (*line != '\n')
            {
                // key or key=value
                char *v = strchr(line, '=');
                if (v && v != line)
                {
                    // key=value
                    *v = 0;
                    v++;
                    key = line;
                    value = v;
                    char *t = strchr(value, '\n');
                    if (t)
                        *t = 0;

                }
                else
                {
                    // key
                    value = NULL;
                    key = line;
                    char *t = strchr(key, '\n');
                    if (t)
                        *t = 0;
                }
            }

            M1_Entry *entry = NULL;
            if (key)
            {
                char* pos = NULL;
                if ((pos = strstr(key, "?B?")) != NULL)	// the data in the ini-file is NOT Base64, it's QP!!!
                {
                    *(++pos) = 'Q';
                }
                entry = OP_NEW(M1_Entry, ());
                entry->SetName(key);
            }

            if (value && entry)
            {
                char* pos = NULL;
                if ((pos = strstr(value, "?B?")) != NULL)	// the data in the ini-file is NOT Base64, it's QP!!!
                {
                    *(++pos) = 'Q';
                }
                entry->SetValue(value);
            }

            if (entry)
                entry->Into(&section->m_entries);
        }
	}

	old.Close();
}


M1_Section* M1_Preferences::_GetSection(const OpStringC8& nameA, BOOL create)
{
	M1_Section *section = (M1_Section*) m_sections.First();
	while (section)
	{
		if (section && !section->Name().IsEmpty() && section->Name().CompareI(nameA) == 0)
		{
			return section;
		}

		section = (M1_Section*) section->Suc();
	}

	if (create)
	{
		section = OP_NEW(M1_Section, ());
		section->SetName(nameA);
		section->Into(&m_sections);
	}

	return section;
}


M1_Entry* M1_Preferences::_GetEntry(const OpStringC8& sec, const OpStringC8& nameA, BOOL create)
{
	M1_Section *section = _GetSection(sec, create);
	if (!section)
		return NULL;

	M1_Entry* entry = (M1_Entry*) section->m_entries.First();

	while (entry)
	{
		if (entry->IsSameAs(nameA))
			return entry;

		entry = (M1_Entry*) entry->Suc();
	}

	if (entry)
		return entry;

	if (create)
	{
		entry = OP_NEW(M1_Entry, ());
		entry->SetName(nameA);
		entry->Into(&section->m_entries);
		return entry;
	}

	return NULL;
}

M1_Entry* M1_Preferences::_GetEntry(const OpStringC8& sec, int index, BOOL create)
{
	M1_Section *section = _GetSection(sec, create);
	if (!section)
		return NULL;

	M1_Entry* entry = (M1_Entry*) section->m_entries.First();

	index++;
	while (entry && index)
	{
		index--;

		if (index)
			entry = (M1_Entry*) entry->Suc();
	}

	if (entry)
		return entry;

	if (create)
	{
		entry = OP_NEW(M1_Entry, ());
		entry->Into(&section->m_entries);
	}

	return NULL;
}


BOOL M1_Preferences::GetEntry(const OpStringC8& section, OpString8& entry, int index)
{
	M1_Entry* ent = _GetEntry(section, index);
	if (ent)
	{
		entry.Set(ent->Name());
		return TRUE;
	}
	return FALSE;
}


void M1_Preferences::Empty()
{
	M1_Section *section = (M1_Section*) m_sections.First();
	while (section)
	{
		M1_Section *s = (M1_Section*) section->Suc();

		section->Out();
		OP_DELETE(section);

		section = s;
	}
}

#endif //M2_SUPPORT
