/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Peter Krefting
*/

#include "core/pch.h"

#if defined PREFS_HAS_INI || defined PREFS_HAS_INI_ESCAPED || defined PREFS_HAS_LNG || defined PREFS_HAS_LNGLIGHT

#include "modules/hardcore/opera/setupinfo.h"
#include "modules/prefsfile/accessors/ini.h"
#include "modules/prefsfile/impl/backend/prefsmap.h"
#include "modules/prefsfile/prefsentry.h"
#include "modules/util/opfile/opfile.h"
#include "modules/util/opfile/opsafefile.h"
#include "modules/util/opfile/unistream.h"
#include "modules/util/tempbuf.h"

void IniAccessor::ParseSectionL(uni_char *key_p, PrefsMap *map)
{
	OpString key_copy; ANCHOR(OpString, key_copy);

	// initialize help pointer p
	uni_char* p = key_copy.ReserveL(uni_strlen(key_p));

	// skip section start
	key_p ++;

	// skip leading whitespace in section name
	while (*key_p == ' ') key_p ++;

	// copy and find the section end
	while (*key_p && *key_p != ']')
	{
		if (*key_p == '\\')
			key_p ++;

		*p ++ = *key_p ++;
	}

	// and back to last non-space
	while (*(p - 1) == ' ') p --;

	// terminate the section string, starting at key_copy, ending at p
	*p = 0;

	// create a new section
	m_current_section = map->GetSectionL(key_copy.CStr());
}

void IniAccessor::ParsePairL(uni_char *key_p)
{
	// Help pointers
	uni_char *value_p, *key_end_p, *value_trav_p, *value_end_p = NULL;

	// skip leading whitespace in key name
	while (uni_isspace(*key_p)) key_p++;

	// empty line?
	if (*key_p == '\n') return;

	// initialize value_p
	value_p = key_p;

	if ('"' == *key_p)
	{
		// quoted key; terminated at next quote
		do value_p ++;
		while (*value_p && (*value_p != '"'));

		// reset if matching failed
		if (*value_p != '"') value_p = key_p;
	}

	// find the equal sign
	while (*value_p && (*value_p != '=')) value_p++;

	key_end_p = value_p;

	// back key_end_p to the last non-space
	if ((key_end_p > key_p) && uni_isspace(*(key_end_p-1)))
	{
		key_end_p--;
		while ((key_end_p > key_p) && uni_isspace(*(key_end_p-1)))
			key_end_p--;
	}

	if (*value_p == '=')
	{

		// let value_p point to the character after the equal sign
		value_p ++;

		value_trav_p = value_p;
		value_end_p = value_p;

		// find the start of the value string
		if (*value_p == '\n' || 0 == *value_p)
		{
			// Value is empty
			*value_p = 0;
			value_end_p = value_p;
		}
		else
		{
			while (uni_isspace(*value_p)) value_p ++;

			// find the end of the value string
			while (*value_trav_p)
			{
				// remember last non-space
				if (!uni_isspace(*value_trav_p) && '\n' != *value_trav_p)
					value_end_p = value_trav_p;

				value_trav_p ++;
			}

			// terminate the value string, starting at value_p, ending at value_end_p
			*(value_end_p + 1) = 0;
		}
	}
	else
	{
		// This setting has no value
		value_p = NULL;
	}

	// terminate the keyname, starting at value_p, ending at key_end_p
	*(key_end_p --) = 0;

	// Remove quotation marks around value (allows leading or trailing spaces)
	if (value_p && value_end_p > value_p && *value_p == '"' && *value_end_p == '"')
	{
		*value_end_p = 0;
		*value_p = 0;
		value_p ++;
	}

	// Remove quotation marks around key (allows leading "[" and "=" in key)
	if (key_p && key_end_p > key_p && *key_p == '"' && *key_end_p == '"')
	{
		*key_end_p = 0;
		*key_p = 0;
		key_p ++;
	}

#if defined PREFS_HAS_INI_ESCAPED || defined PREFS_HAS_LNG
	// Expand tabs, CRs and LFs if so is requested
	if (value_p && m_expandescapes)
	{
		char hex[3] = { 0, 0, 0 };
		uni_char c, *esc_p, *continue_p;
		int skip;
		esc_p = uni_strchr(value_p, '\\');
		while (esc_p)
		{
			skip = 1;

			switch (*(esc_p + 1))
			{
			case 't':
				c = '\t';
				break;

			case 'r':
				c = '\r';
				break;

			case 'n':
				c = '\n';
				break;

			case 'x':
			{
				hex[0] = static_cast<char>(esc_p[2]);
				hex[1] = static_cast<char>(esc_p[3]);
				char *end_p;
				long int retval = op_strtol(hex, &end_p, 16);
				if (!*end_p)
					c = static_cast<char>(retval);
				else
					c = '?';
				skip = 3;
				break;
			}

			case '\\':
			default:
				c = *(esc_p + 1);
				break;
			}
			*esc_p = c;

			continue_p = ++ esc_p;
			while (*(esc_p + skip))
			{
				*esc_p = *(esc_p + skip);
				esc_p ++;
			}
			*esc_p = 0;
			esc_p = uni_strchr(continue_p, '\\');
		}
	}
#endif

	m_current_section->SetL(key_p, value_p);
}

#ifdef _DEBUG

static BOOL isFeatureEnabled(const int index)
{
#ifdef SETUP_INFO
	OpSetupInfo::Feature feature = OpSetupInfo::GetFeature(index);
	return feature.enabled;
#else
	return FALSE;
#endif
}


static int getFeatureIndex(const OpStringC& name)
{
# ifdef SETUP_INFO
	// Binary search on features
	int from = 0, to = OpSetupInfo::GetFeatureCount();
	OpSetupInfo::Feature feature;
	int index = -1;

	while (from < to)
	{
		index = (from + to) / 2;
		feature = OpSetupInfo::GetFeature(index);
		int comp = name.Compare(feature.name);
		if (comp < 0)
		{
			to = index;
		}
		else if (comp > 0)
		{
			from = index + 1;
		}
		else
		{
			break;
		}
	}

	if (from < to)
	{
		return index;
	}
	else
	{
		// The feature doesn't exist
		return -1;
	}

# else
	return -1;
# endif
}
#endif // _DEBUG

BOOL IniAccessor::ParseLineL(uni_char *key_p, PrefsMap *map)
{
	if (key_p==NULL)
		return TRUE;

	// skip leading whitespace in line
	while (uni_isspace(*key_p)) key_p++;

#ifdef _DEBUG
	// Due to the new resource prefixes for special builds in debug we need to remove any FEATURE_x
	// entries from the start of the line
	uni_char *condition = key_p;
	BOOL pass_value = TRUE;
	if (*condition == '!')
	{
		pass_value = FALSE;
		condition++;
	}
	if (uni_strncmp(condition, UNI_L("FEATURE_"), 8) == 0)
	{
		// Skip until there is a space
		while (*key_p && !uni_isspace(*key_p))
			key_p++;

		// Decide if the line should be kept or discarded
		OpString feature;
		if (OpStatus::IsError(feature.Set(condition, key_p - condition)))
			return FALSE;
		int feature_index = getFeatureIndex(feature);

		// first check if the feature exists
		if (feature_index != -1 && isFeatureEnabled(feature_index) != pass_value)
			return FALSE;

		// Skip the spaces which should find the real start of the line
		while (*key_p && uni_isspace(*key_p))
			key_p++;
	}
#endif // _DEBUG

	if (*key_p == '[')
	{
		// a section
		ParseSectionL(key_p, map);
	}
	else if (*key_p && *key_p != ';' && m_current_section)
	{
		// a pair (skip comments and empty lines)
		ParsePairL(key_p);
	}
	return FALSE;
}

OP_STATUS IniAccessor::LoadStreamL(UnicodeFileInputStream *stream, PrefsMap *map, BOOL check_header)
{
	// Temporary storage for lines across buffers
	OpString temp_line;
	ANCHOR(OpString, temp_line);

	// Read converted data from the stream
	BOOL quit = FALSE;
	while (!quit && stream->has_more_data() && stream->no_errors())
	{
		int bufsize;
		uni_char *buf = stream->get_block(bufsize);
		bufsize /= sizeof (uni_char);
		if (buf)
		{
			// Work on one line at a time
			uni_char *current = buf;
			BOOL moreinbuffer = TRUE;
			do
			{
				uni_char *end = current;

				while (end < (buf + bufsize) && *end != '\n' && *end != '\r')
				{
					end ++;
				}

				if (end == buf + bufsize)
				{
					// Line continues into next buffer
					temp_line.AppendL(current, end - current);
					moreinbuffer = FALSE;
				}
				else
				{
					// Parse this line
					if (end > current || !temp_line.IsEmpty())
					{
						*end = 0;
						if (!temp_line.IsEmpty())
						{
							// Continue old line
							temp_line.AppendL(current);
							quit = ParseLineL(temp_line.CStr(), map);
							temp_line.Empty();
						}
						else
						{
#ifdef UPGRADE_SUPPORT
							if (check_header)
							{
								check_header = FALSE;
								switch (AllowReadingFile(current))
								{
								case HeaderNotRecognized:
								// This is a file without an indicator, i.e
								// an INI file from a non-Unicode version. We
								// no longer support such files, but read it
								// as if it was a current INI file. We need to
								// parse this line as usual.
									goto doread;

								case HeaderTooNew:
								// Incompatible change, cannot read.
									return OpStatus::ERR_NO_ACCESS;
								}
							}
							else
#endif
							{
#ifdef UPGRADE_SUPPORT
doread:
#endif
								quit = ParseLineL(current, map);
							}
						}
					}
				}

				// Point to next line
				current = end + 1;

				if (current >= buf + bufsize)
				{
					moreinbuffer = FALSE;
				}

			} while (moreinbuffer && !quit);
		}
	}

	if (!temp_line.IsEmpty())
	{
		ParseLineL(temp_line.CStr(), map);
	}

	return OpStatus::OK;
}

#ifdef UPGRADE_SUPPORT
IniAccessor::CompatibilityStatus IniAccessor::AllowReadingFile(const uni_char *header)
{
	return IsRecognizedHeader(header);
}
#endif

OP_STATUS IniAccessor::LoadL(OpFileDescriptor *file, PrefsMap *map)
{
	// Clear section cache
	m_current_section = NULL;

	OpStackAutoPtr<UnicodeFileInputStream> inifile(OP_NEW_L(UnicodeFileInputStream, ()));

	OP_STATUS rc =
		inifile->Construct(file, UnicodeFileInputStream::UTF8_FILE, TRUE);
	if (OpStatus::IsError(rc))
	{
		if (OpStatus::ERR_NO_MEMORY == rc) LEAVE(rc);
		return rc;
	}

	LoadStreamL(inifile.get(), map, TRUE);
	return OpStatus::OK;
}

#ifdef UPGRADE_SUPPORT
/*static*/ IniAccessor::CompatibilityStatus IniAccessor::IsRecognizedHeader(const uni_char *header)
{
	if (!header)
	{
		return HeaderNull;
	}

	if (0xFEFF == *header)
	{
		// Point past the BOM (which has been converted from its
		// UTF-8 representation here)
		++ header;
	}

	// Check for a valid version 2 preferences file, assume version 2 for
	// files without descriptor (version 1 is no longer supported).
	if (uni_strni_eq(header, "OPERA PREFERENCES VERSION ", 26))
	{
		// This is a file with an indicator. Check preferences file version
		double version = uni_atof(header + 26);

		// Any incompatible change will be in version 3
		if (version >= 3.0) return HeaderTooNew;
	}
	else
	{
		// This is a file without an indicator, i.e an INI file from a
		// non-Unicode version. We no longer support such files, but read
		// it as if it was a current INI file.
		return HeaderNotRecognized;
	}

	return HeaderCurrent;
}

/*static*/ IniAccessor::CompatibilityStatus IniAccessor::IsRecognizedHeader(const char *header, size_t len)
{
	OpString s;
	if (OpStatus::IsSuccess(s.SetFromUTF8(header, len)))
	{
		return IsRecognizedHeader(s.CStr());
	}

	return HeaderNull;
}
#endif // UPGRADE_SUPPORT

#ifdef PREFSFILE_WRITE
OP_STATUS IniAccessor::StoreL(OpFileDescriptor *file, const PrefsMap *prefsMap)
{
	if (file->IsWritable())
	{
		// If this is a disk file, we need to use a OpSafeFile to rewrite
		// it, to ensure we don't lose data if writing fails.
		OpStackAutoPtr<OpSafeFile> safefile(NULL);
		if (file->Type() == OPFILE)
		{
			safefile.reset(OP_NEW_L(OpSafeFile, ()));
			LEAVE_IF_ERROR(safefile->Construct(static_cast<OpFile *>(file)));
			file = safefile.get();
		}

		LEAVE_IF_ERROR(file->Open(OPFILE_WRITE | OPFILE_TEXT));
		LEAVE_IF_ERROR(file->WriteUTF8Line(UNI_L("\xFEFFOpera Preferences version 2.1")));
		LEAVE_IF_ERROR(file->WriteUTF8Line(UNI_L("; Do not edit this file while Opera is running")));
		LEAVE_IF_ERROR(file->WriteUTF8Line(UNI_L("; This file is stored in UTF-8 encoding")));

		const PrefsSection *section = prefsMap->Sections();
		while (section != NULL)
		{
			StoreSectionL(file, section);
			section = section->Suc();
		}

		if (safefile.get())
			LEAVE_IF_ERROR(safefile->SafeClose());
		else
			LEAVE_IF_ERROR(file->Close());

		return OpStatus::OK;
	}

	// File isn't writable
	return OpStatus::ERR_NO_ACCESS;
}
#endif // PREFSFILE_WRITE

void IniAccessor::StoreSectionL(OpFileDescriptor *f, const PrefsSection *section)
{
	OpString s; ANCHOR(OpString, s);

	// copy section name, escape characters if necessary
	s.ReserveL((uni_strlen(section->Name()) * 2) + 4);
	s.SetL(UNI_L("\n["));
	const uni_char* s_p = section->Name();
	uni_char* d_p = s.CStr() + 2;
	if (s_p)
	{
		while (*s_p)
		{
			if (*s_p == ']' || *s_p == '\\')
				*d_p++ = '\\';
			*d_p++ = *s_p++;
		}
	}
	*d_p++ = ']';
	*d_p = 0;

	LEAVE_IF_ERROR(f->WriteUTF8Line(s));
	const PrefsEntry *entry = section->Entries();
	while (entry != NULL)
	{
		StoreEntryL(f, entry);
		entry = entry->Suc();
	}
}

void IniAccessor::StoreEntryL(OpFileDescriptor *f, const PrefsEntry *entry)
{
	const uni_char *value = entry->Value();
	const uni_char *key   = entry->Key();
	TempBuffer line; ANCHOR(TempBuffer, line);
	line.SetCachedLengthPolicy(TempBuffer::TRUSTED);
	// Allocate at least the minimum amount needed for string + some slack
	line.ExpandL(uni_strlen(key) + (value ? uni_strlen(value) : 0) + 6 + /*slack*/ 8);

	if ('[' == *key || '"' == *key || ';' == *key || uni_strchr(key, '='))
	{
		// Quote key
		line.AppendL("\"");
		line.AppendL(key);
		line.AppendL("\"");
	}
	else
	{
		line.AppendL(key);
	}

	if (value != NULL)
	{
		line.AppendL("=");

		const uni_char *ptr = value;
		if (*ptr)
		{
			// Quote value if it would risk being mangled on re-read
			// note that the calls to stripdup() in PrefsManager defeats this
			// for spaces, but it is needed for quotes
			BOOL needquote = FALSE;
			while (*(++ ptr)) ; // Seek to last position
			ptr --;

			if (uni_isspace(*value) || '\"' == *value ||
			    uni_isspace(*ptr)   || '\"' == *ptr     )
			{
				needquote = TRUE;
				line.AppendL("\"");
			}

#ifdef PREFS_HAS_INI_ESCAPED
			if (m_expandescapes)
			{
				// Escape all dangerous characters.
				ptr = value;
				while (*ptr)
				{
					switch (*ptr)
					{
					case '\n': line.AppendL("\\n"); break;
					case '\r': line.AppendL("\\r"); break;
					case '\t': line.AppendL("\\t"); break;
					case '\\': line.AppendL("\\\\"); break;
					case '"':  line.AppendL("\\x22"); break;
					default:
						line.AppendL(*ptr);
					}

					ptr ++;
				}
			}
			else
#endif
			{
				line.AppendL(value);
			}

			if (needquote)
			{
				line.AppendL("\"");
			}
		}
	}

	line.AppendL(uni_char(0));
	LEAVE_IF_ERROR(f->WriteUTF8Line(line.GetStorage()));
}

#endif /* PREFS_HAS_INI || PREFS_HAS_INI_ESCAPED || PREFS_HAS_LNG || PREFS_HAS_LNGLIGHT */
