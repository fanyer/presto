/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright 2002-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Peter Krefting
*/

#include "core/pch.h"

#include "modules/prefs/prefsmanager/collections/pc_fontcolor.h"
#include "modules/prefs/prefsmanager/hostoverride.h"
#include "modules/prefs/prefsmanager/prefsmanager.h"
#include "modules/prefsfile/prefssection.h"
#include "modules/prefsfile/prefsentry.h"
#include "modules/display/FontAtt.h"
#include "modules/display/styl_man.h"
#include "modules/util/handy.h" // ARRAY_SIZE
#include "modules/util/gen_str.h" // GetStrTokens
#ifdef PREFS_CSS_TOOLTIP_COLOR
# include "modules/style/src/css_values.h"
#endif
#include "modules/prefs/prefsmanager/collections/prefs_macros.h"
#include "modules/prefs/prefsmanager/prefsinternal.h"
#include "modules/url/url2.h"

#include "modules/prefs/prefsmanager/collections/pc_fontcolor_c.inl"

// Available font settings
INITFONTS(PrefsCollectionFontsAndColors, PCFONTCOLORS_NUMBEROFFONTSETTINGS)
{
	INITSTART
	S(OP_SYSTEM_FONT_DOCUMENT_NORMAL,	"Normal"		),
	S(OP_SYSTEM_FONT_DOCUMENT_HEADER1,	"H1"			),
	S(OP_SYSTEM_FONT_DOCUMENT_HEADER2,	"H2"			),
	S(OP_SYSTEM_FONT_DOCUMENT_HEADER3,	"H3"			),
	S(OP_SYSTEM_FONT_DOCUMENT_HEADER4,	"H4"			),
	S(OP_SYSTEM_FONT_DOCUMENT_HEADER5,	"H5"			),
	S(OP_SYSTEM_FONT_DOCUMENT_HEADER6,	"H6"			),
	S(OP_SYSTEM_FONT_DOCUMENT_PRE,		"PRE"			),
	S(OP_SYSTEM_FONT_FORM_BUTTON,		"Form.Button"	),
	S(OP_SYSTEM_FONT_FORM_TEXT,			"Form.Text"		),
	S(OP_SYSTEM_FONT_FORM_TEXT_INPUT,	"Form.Input"	)
#ifdef _QUICK_UI_FONT_SUPPORT_
	,
	S(OP_SYSTEM_FONT_EMAIL_DISPLAY,		"Email"			),
	S(OP_SYSTEM_FONT_EMAIL_COMPOSE,		"EmailCompose"	),
	S(OP_SYSTEM_FONT_HTML_COMPOSE,		"HTMLCompose"	),
	S(OP_SYSTEM_FONT_UI_MENU,			"Menu"			),
	S(OP_SYSTEM_FONT_UI_TOOLBAR,		"Toolbar"		),
	S(OP_SYSTEM_FONT_UI_DIALOG,			"Dialog"		),
	S(OP_SYSTEM_FONT_UI_PANEL,			"Panel"			),
	S(OP_SYSTEM_FONT_UI_TOOLTIP,		"Tooltip"		),
	S(OP_SYSTEM_FONT_UI_HEADER,			"Titles"		)
#endif
	INITEND
};

// Available color settings
INITCOLORS(PrefsCollectionFontsAndColors, PCFONTCOLORS_NUMBEROFCOLORSETTINGS)
{
	INITSTART
	C(OP_SYSTEM_COLOR_DOCUMENT_NORMAL,		"Color.Normal"				),
	C(OP_SYSTEM_COLOR_DOCUMENT_HEADER1,		"Color.H1"					),
	C(OP_SYSTEM_COLOR_DOCUMENT_HEADER2,		"Color.H2"					),
	C(OP_SYSTEM_COLOR_DOCUMENT_HEADER3,		"Color.H3"					),
	C(OP_SYSTEM_COLOR_DOCUMENT_HEADER4,		"Color.H4"					),
	C(OP_SYSTEM_COLOR_DOCUMENT_HEADER5,		"Color.H5"					),
	C(OP_SYSTEM_COLOR_DOCUMENT_HEADER6,		"Color.H6"					),
	C(OP_SYSTEM_COLOR_DOCUMENT_PRE,			"Color.PRE"					),
	C(OP_SYSTEM_COLOR_BUTTON,				"Color.Form.Button"			),
	C(OP_SYSTEM_COLOR_TEXT,					"Color.Form.Text"			),
	C(OP_SYSTEM_COLOR_TEXT_INPUT,			"Color.Form.Input"			),
	C(OP_SYSTEM_COLOR_LINK,					"Link"						),
	C(OP_SYSTEM_COLOR_VISITED_LINK,			"Visited link"				),
	C(OP_SYSTEM_COLOR_DOCUMENT_BACKGROUND,	"Background"				),
	C(OP_SYSTEM_COLOR_BACKGROUND,       	"Edit Background"			),
	C(OP_SYSTEM_COLOR_TEXT_SELECTED,		"Selected Text"				),
	C(OP_SYSTEM_COLOR_BACKGROUND_SELECTED,	"Selected Background"		),
	C(OP_SYSTEM_COLOR_WORKSPACE,            "Workspace"                 )
#ifdef SEARCH_MATCHES_ALL
	, /* Extra selection colors for non-current selected text */
	C(OP_SYSTEM_COLOR_TEXT_SELECTED_NOFOCUS,		"Selected Text Unfocused"		),
	C(OP_SYSTEM_COLOR_BACKGROUND_SELECTED_NOFOCUS,	"Selected Background Unfocused"	)
	, /* Extra selection colors for search results */
	C(OP_SYSTEM_COLOR_BACKGROUND_HIGHLIGHTED,			"Highlighted Background"			),
	C(OP_SYSTEM_COLOR_TEXT_HIGHLIGHTED,					"Highlighted Text"					),
	C(OP_SYSTEM_COLOR_BACKGROUND_HIGHLIGHTED_NOFOCUS,	"Highlighted Background Unfocused"	),
	C(OP_SYSTEM_COLOR_TEXT_HIGHLIGHTED_NOFOCUS,			"Highlighted Text Unfocused"		)
#endif
#ifdef SKIN_SUPPORT
	, /* Color scheme */
	C(OP_SYSTEM_COLOR_SKIN,					"Skin"						)
#endif
#ifdef PREFS_CSS_TOOLTIP_COLOR
	, /* Need these entries because there is no default on Unix */
	C(OP_SYSTEM_COLOR_TOOLTIP_BACKGROUND,	"Tooltip Background"	),
	C(OP_SYSTEM_COLOR_TOOLTIP_TEXT,			"Tooltip Text"			)
#endif
	INITEND
};

// OverrideHostForPrefsCollectionFontsAndColors implementation ------------

#ifdef PREFS_HOSTOVERRIDE
/** This inherited class is needed to handle overridden font and colors.
  */
class OverrideHostForPrefsCollectionFontsAndColors : public OverrideHost
{
public:
	OverrideHostForPrefsCollectionFontsAndColors()
		: m_fontoverrides(NULL) {};
	virtual void ConstructL(const uni_char *host, BOOL from_user);
	virtual ~OverrideHostForPrefsCollectionFontsAndColors();

#ifdef PREFS_WRITE
	OP_STATUS WriteOverrideColorL(
		PrefsFile *reader,
		enum OP_SYSTEM_COLOR type,
		const char *colorsetting,
		COLORREF value,
		BOOL from_user);

	OP_STATUS WriteOverrideColorL(
		PrefsFile *reader,
		enum PrefsCollectionFontsAndColors::customcolorpref type,
		OpPrefsCollection::IniSection colorsection,
		const char *colorsetting,
		COLORREF value,
		BOOL from_user);

	void WriteOverrideFontL(
		PrefsFile *reader,
		int prefidx,
		const char *fontsection,
		const FontAtt &font,
		BOOL from_user);
#endif

	BOOL GetOverriddenColor(enum OP_SYSTEM_COLOR type, COLORREF &value);
	BOOL GetOverriddenColor(enum PrefsCollectionFontsAndColors::customcolorpref type, COLORREF &value);
	BOOL GetOverriddenFont(int prefidx, FontAtt &value);

private:
	/** Store fonts as a pointer to an array of fonts, so we only waste the
	  * array for hosts which actually have font overrides. */
	FontAtt **m_fontoverrides;

	void SetFontEntryL(int, const FontAtt &);
	friend class PrefsCollectionFontsAndColors;
};

void OverrideHostForPrefsCollectionFontsAndColors::ConstructL(const uni_char *host, BOOL from_user)
{
	OverrideHost::ConstructL(host, from_user);
}

OverrideHostForPrefsCollectionFontsAndColors::~OverrideHostForPrefsCollectionFontsAndColors()
{
	if (m_fontoverrides != NULL)
	{
		for (int i = 0; i < PCFONTCOLORS_NUMBEROFFONTSETTINGS; i ++)
		{
			OP_DELETE(m_fontoverrides[i]);
		}

		OP_DELETEA(m_fontoverrides);
	}
}

void OverrideHostForPrefsCollectionFontsAndColors::SetFontEntryL(int i, const FontAtt &font)
{
	if (NULL == m_fontoverrides)
	{
		// Allocate the array when we find insert first font, to not waste
		// the space if we have no overridden fonts.
		m_fontoverrides = OP_NEWA_L(FontAtt*, PCFONTCOLORS_NUMBEROFFONTSETTINGS);
		int j;
		for (j = 0; j < PCFONTCOLORS_NUMBEROFFONTSETTINGS; j++)
		{
			m_fontoverrides[j] = NULL;
		}
	}

	// Create font override entry
	if (NULL == m_fontoverrides[i])
	{
		// Allocate a font override entry if it wasn't already
		m_fontoverrides[i] = OP_NEW_L(FontAtt, ());
	}

	*m_fontoverrides[i] = font;
}

#ifdef PREFS_WRITE
OP_STATUS OverrideHostForPrefsCollectionFontsAndColors::WriteOverrideColorL(
	PrefsFile *reader,
	enum OP_SYSTEM_COLOR type,
	const char *colorsetting,
	COLORREF color,
	BOOL from_user)
{
	// Convert the COLORREF into a string
	uni_char strcolor[8]; /* ARRAY OK 2012-01-19 peter */
	PrefsCollectionFontsAndColors::SerializeColor(color, strcolor);

	// Fake a stringprefdefault so that we can use standard functions
	OpPrefsCollection::stringprefdefault tmpstrpref =
	{
		OpPrefsCollection::SColors, colorsetting, NULL
	};

	return WriteOverrideL(reader, &tmpstrpref, int(type), OpStringC(strcolor), from_user);
}

OP_STATUS OverrideHostForPrefsCollectionFontsAndColors::WriteOverrideColorL(
	PrefsFile *reader,
	enum PrefsCollectionFontsAndColors::customcolorpref type,
	OpPrefsCollection::IniSection colorsection,
	const char *colorsetting,
	COLORREF color,
	BOOL from_user)
{
	// Convert the COLORREF into a string
	uni_char strcolor[8]; /* ARRAY OK 2012-01-19 peter */
	PrefsCollectionFontsAndColors::SerializeColor(color, strcolor);

	// Fake a stringprefdefault so that we can use standard functions
	OpPrefsCollection::stringprefdefault tmpstrpref =
	{
		colorsection, colorsetting, NULL
	};

	// Fake an index above the static colors, so we can use the same
	// list as for them, compare ReadOverridesL() below
	return WriteOverrideL(reader, &tmpstrpref, int(type) + PCFONTCOLORS_NUMBEROFCOLORSETTINGS, OpStringC(strcolor), from_user);
}

void OverrideHostForPrefsCollectionFontsAndColors::WriteOverrideFontL(
	PrefsFile *reader,
	int prefidx,
	const char *fontsection,
	const FontAtt &font,
	BOOL from_user)
{
	if (reader->AllowedToChangeL(OpPrefsCollection::SectionNumberToString(OpPrefsCollection::SFonts), fontsection))
	{
		OpString key; ANCHOR(OpString, key);
		key.SetL("Fonts|");
		key.AppendL(fontsection);

		// Convert the font into a string
		OpString serializedfont; ANCHOR(OpString, serializedfont);
		LEAVE_IF_ERROR(font.Serialize(&serializedfont));
		OP_STATUS rc;
		BOOL overwrite = from_user;
#ifdef PREFSFILE_WRITE_GLOBAL
		if (from_user)
#else
		overwrite = TRUE;
#endif
		{
			rc = reader->WriteStringL(m_host, key, serializedfont);
		}
#ifdef PREFSFILE_WRITE_GLOBAL
		else
		{
			rc = reader->WriteStringGlobalL(m_host, key, serializedfont);

			// Check if we should overwrite the current value (i.e it is
			// not overwritten internally).
			overwrite = serializedfont.Compare(reader->ReadStringL(m_host, key)) == 0;
		}
#endif

		// Create a new override
		if (overwrite && OpStatus::IsSuccess(rc))
		{
			SetFontEntryL(prefidx, font);
		}
	}
}
#endif // PREFS_WRITE

BOOL OverrideHostForPrefsCollectionFontsAndColors::GetOverriddenColor(
	enum OP_SYSTEM_COLOR type, COLORREF &color)
{
	const OpStringC *strcolor;
	if (GetOverridden(int(type), &strcolor))
	{
		// Value existed, make sure it is a valid color specification
		int r = 0, g = 0, b = 0;
		if (OpStatus::IsSuccess(PrefsCollectionFontsAndColors::UnSerializeColor(strcolor, &r, &g, &b)))
		{
			color = OP_RGB(r, g, b);
			return TRUE;
		}
	}

	return FALSE;
}

BOOL OverrideHostForPrefsCollectionFontsAndColors::GetOverriddenColor(
	enum PrefsCollectionFontsAndColors::customcolorpref type, COLORREF &color)
{
	const OpStringC *strcolor;
	if (GetOverridden(int(type) + PCFONTCOLORS_NUMBEROFCOLORSETTINGS, &strcolor))
	{
		// Value existed, make sure it is a valid color specification
		int r = 0, g = 0, b = 0;
		if (OpStatus::IsSuccess(PrefsCollectionFontsAndColors::UnSerializeColor(strcolor, &r, &g, &b)))
		{
			color = OP_RGB(r, g, b);
			return TRUE;
		}
	}

	return FALSE;
}

BOOL OverrideHostForPrefsCollectionFontsAndColors::GetOverriddenFont(
	int prefidx, FontAtt &value)
{
	if (m_fontoverrides != NULL && m_fontoverrides[prefidx] != NULL)
	{
		// An overridden font is defined
		value = *m_fontoverrides[prefidx];
		return TRUE;
	}

	return FALSE;
}
#endif // PREFS_HOSTOVERRIDE

// PrefsCollectionFontsAndColors implementation ---------------------------

PrefsCollectionFontsAndColors *PrefsCollectionFontsAndColors::CreateL(PrefsFile *reader)
{
	if (g_opera->prefs_module.m_pcfontscolors)
		LEAVE(OpStatus::ERR);
	g_opera->prefs_module.m_pcfontscolors = OP_NEW_L(PrefsCollectionFontsAndColors, (reader));
	return g_opera->prefs_module.m_pcfontscolors;
}

PrefsCollectionFontsAndColors::~PrefsCollectionFontsAndColors()
{
#ifdef PREFS_COVERAGE
	CoverageReport(NULL, 0, NULL, 0);
#endif

	OP_DELETEA(m_colors); m_colors = NULL;
	OP_DELETEA(m_customcolors); m_customcolors = NULL;
	OP_DELETEA(m_fonts);  m_fonts = NULL;
	g_opera->prefs_module.m_pcfontscolors = NULL;
}

void PrefsCollectionFontsAndColors::ReadAllPrefsL(PrefsModule::PrefsInitInfo *)
{
	// Read colors
	ReadColorsL();

	// Read fonts
	ReadFontsL();

#if defined FONTSWITCHING && defined PREFS_READ
	// Read international fonts
	ReadPreferredFontsL();
#endif

#ifdef PREFS_COVERAGE
	op_memset(m_fontprefused, 0, sizeof (int) * PCFONTCOLORS_NUMBEROFFONTSETTINGS);
	op_memset(m_colorprefused, 0, sizeof (int) * PCFONTCOLORS_NUMBEROFCOLORSETTINGS);
	op_memset(m_customcolorprefused, 0, sizeof (int) * PCFONTCOLORS_NUMBEROFCUSTOMCOLORPREFS);
#endif
}

#ifdef PREFS_HOSTOVERRIDE
void PrefsCollectionFontsAndColors::ReadOverridesL(const uni_char *host, PrefsSection *section, BOOL active, BOOL from_user)
{
	OverrideHostForPrefsCollectionFontsAndColors *override = NULL;

	// The entries for a host are stored in a section which has the
	// same name as the host itself.
	const PrefsEntry *overrideentry = section ? section->Entries() : NULL;
	while (overrideentry)
	{
		const OpStringC key(overrideentry->Key());
		const OpStringC value(overrideentry->Value());

		// The override is stored as Section|Key=Value, so we need to split
		// it up to lookup in the default values. We assume that no section
		// or key can be longer than 64 characters.
#define MAXLEN 64
		int sectionlen = key.FindFirstOf('|');
		int keylen = key.Length() - sectionlen - 1;
		if (sectionlen != KNotFound && sectionlen <= MAXLEN - 1 && keylen <= MAXLEN - 1)
		{
			char realsection[MAXLEN], realkey[MAXLEN]; /* ARRAY OK 2010-12-13 peter */
			uni_cstrlcpy(realsection, key.CStr(), sectionlen + 1);
			uni_cstrlcpy(realkey, key.CStr() + sectionlen + 1, MAXLEN);

			// Locate the preference and insert its value if we were allowed
			// to change it and it is ours.
			BOOL found = FALSE;
#ifdef PREFSMAP_USE_CASE_SENSITIVE
			if (0 == op_strcmp(realsection, "Colors"))
#else
			if (strni_eq(realsection, "COLORS", 7))
#endif
			{
				// Find the color entry
				for (int i = 0; i < PCFONTCOLORS_NUMBEROFCOLORSETTINGS; i ++)
				{
					if (
#ifdef PREFSMAP_USE_CASE_SENSITIVE
					    op_strcmp(realkey, m_colorsettings[i].setting) == 0
#else
					    op_stricmp(realkey, m_colorsettings[i].setting) == 0
#endif
					    && m_reader->AllowedToChangeL("Colors", m_colorsettings[i].setting))
					{
						if (!override)
						{
							override =
								static_cast<OverrideHostForPrefsCollectionFontsAndColors *>
								(FindOrCreateOverrideHostL(host, from_user));
						}
						override->InsertOverrideL(int(m_colorsettings[i].type), value);
						found = TRUE;
						break;
					}
				}
			}
#ifdef PREFSMAP_USE_CASE_SENSITIVE
			else if (0 == op_strcmp(realsection, "Fonts"))
#else
			else if (strni_eq(realsection, "FONTS", 6))
#endif
			{
				FontAtt font;

				// Find the font entry
				for (int j = 0; j < PCFONTCOLORS_NUMBEROFFONTSETTINGS; j ++)
				{
					if (
#ifdef PREFSMAP_USE_CASE_SENSITIVE
					    op_strcmp(realkey, m_fontsettings[j].setting) == 0
#else
					    op_stricmp(realkey, m_fontsettings[j].setting) == 0
#endif
					    && m_reader->AllowedToChangeL("Fonts", m_fontsettings[j].setting)
						&& font.Unserialize(value))
					{
						if (!override)
						{
							override =
								static_cast<OverrideHostForPrefsCollectionFontsAndColors *>
								(FindOrCreateOverrideHostL(host, from_user));
						}
						override->SetFontEntryL(j, font);
						found = TRUE;
						break;
					}
				}
			}

			// Locate custom color entry
			if (!found && m_reader->AllowedToChangeL(realsection, realkey))
			{
				OpPrefsCollection::IniSection section_id =
					OpPrefsCollection::SectionStringToNumber(realsection);

				// Locate the preference and insert its value if we were allowed
				// to change it and it is ours.
				const struct customcolorprefdefault *prefs = m_customcolorprefdefault;
				while (prefs->section != OpPrefsCollection::SNone)
				{
					if (prefs->section == section_id && !OVERRIDE_STRCMP(prefs->key, realkey))
					{
						if (!override)
						{
							override =
								static_cast<OverrideHostForPrefsCollectionFontsAndColors *>
								(FindOrCreateOverrideHostL(host, from_user));
						}
						// For overrides, the index is offset the number of
						// static colors; compare WriteOverrideColorL() above.
						override->InsertOverrideL((prefs - m_customcolorprefdefault) + PCFONTCOLORS_NUMBEROFCOLORSETTINGS, value);
						break;
					}
					prefs ++;
				}
			}
		}

		overrideentry = overrideentry->Suc();
	}

	if (override)
	{
		override->SetActive(active);
	}
}
#endif // PREFS_HOSTOVERRIDE

const char *PrefsCollectionFontsAndColors::GetFontSectionName(
	enum OP_SYSTEM_FONT type, int *idxp)
{
	int i;
	for (i = 0; i < PCFONTCOLORS_NUMBEROFFONTSETTINGS; i ++)
		if (m_fontsettings[i].type == type)
		{
			if (idxp) *idxp = i;
#ifdef PREFS_READ
			return m_fontsettings[i].setting;
#else
			return NULL;
#endif
		}

	if (idxp) *idxp = -1;
	return NULL;
}

const char *PrefsCollectionFontsAndColors::GetColorSettingName(
	enum OP_SYSTEM_COLOR type, int *idxp)
{
	int i;
	for (i = 0; i < PCFONTCOLORS_NUMBEROFCOLORSETTINGS; i ++)
	{
		if (m_colorsettings[i].type == type)
		{
			*idxp = i;
#ifdef PREFS_READ
			return m_colorsettings[i].setting;
#else
			return NULL;
#endif
		}
	}

	*idxp = -1;
	return NULL;
}

COLORREF PrefsCollectionFontsAndColors::GetColorInternal(
	OP_SYSTEM_COLOR which, const uni_char *host, BOOL *overridden) const
{
	int idx = -1;
	for (int i = 0; i < PCFONTCOLORS_NUMBEROFCOLORSETTINGS; i ++)
	{
		if (m_colorsettings[i].type == which)
		{
			idx = i;
			break;
		}
	}

	if (idx != -1)
	{
#ifdef PREFS_COVERAGE
		++ m_colorprefused[idx];
#endif

#ifdef PREFS_HOSTOVERRIDE
		if (host)
		{
			OverrideHostForPrefsCollectionFontsAndColors *override =
				static_cast<OverrideHostForPrefsCollectionFontsAndColors *>
				(FindOverrideHost(host));

			if (override)
			{
				// Found a matching override
				COLORREF retval;
				if (override->GetOverriddenColor(which, retval))
				{
					// Value is overridden
					if (overridden)
						*overridden = TRUE;

					return retval;
				}
			}
		}
#endif // PREFS_HOSTOVERRIDE

		// No override set, or no host specified; return standard value
		if (overridden)
			*overridden = FALSE;

		return m_colors[idx];
	}
	else
	{
		OP_ASSERT(!"Invalid color selection");
#if defined DEBUG || defined _DEBUG
		return 0xFF69B4; // hotpink
#else
		return 0; // black
#endif
	}
}

COLORREF PrefsCollectionFontsAndColors::GetColorInternal(
	customcolorpref which, const uni_char *host, BOOL *overridden) const
{
#ifdef PREFS_COVERAGE
	++ m_customcolorprefused[idx];
#endif

#ifdef PREFS_HOSTOVERRIDE
	if (host)
	{
		OverrideHostForPrefsCollectionFontsAndColors *override =
			static_cast<OverrideHostForPrefsCollectionFontsAndColors *>
			(FindOverrideHost(host));

		if (override)
		{
			// Found a matching override
			COLORREF retval;
			if (override->GetOverriddenColor(which, retval))
			{
				// Value is overridden
				if (overridden)
					*overridden = TRUE;

				return retval;
			}
		}
	}
#endif // PREFS_HOSTOVERRIDE

	// No override set, or no host specified; return standard value
	if (overridden)
		*overridden = FALSE;

	return m_customcolors[which];
}

#ifdef PREFS_HOSTOVERRIDE
BOOL PrefsCollectionFontsAndColors::IsPreferenceOverridden(OP_SYSTEM_COLOR which, const uni_char *host) const
{
	BOOL is_overridden;
	GetColorInternal(which, host, &is_overridden);
	return is_overridden;
}

BOOL PrefsCollectionFontsAndColors::IsPreferenceOverridden(OP_SYSTEM_COLOR which, URL &host) const
{
	return IsPreferenceOverridden(which, host.GetAttribute(URL::KUniHostName).CStr());
}
#endif

#ifdef PREFS_HOSTOVERRIDE
BOOL PrefsCollectionFontsAndColors::IsPreferenceOverridden(customcolorpref which, const uni_char *host) const
{
	BOOL is_overridden;
	GetColorInternal(which, host, &is_overridden);
	return is_overridden;
}

BOOL PrefsCollectionFontsAndColors::IsPreferenceOverridden(customcolorpref which, URL &host) const
{
	return IsPreferenceOverridden(which, host.GetAttribute(URL::KUniHostName).CStr());
}
#endif

void PrefsCollectionFontsAndColors::ReadColorsL()
{
	OP_DELETEA(m_colors);
	m_colors = OP_NEWA_L(COLORREF, PCFONTCOLORS_NUMBEROFCOLORSETTINGS);

	int i;
	for (i = 0; i < PCFONTCOLORS_NUMBEROFCOLORSETTINGS; i ++)
	{
#ifdef PREFS_READ
		OP_ASSERT(m_colorsettings[i].setting != NULL); // Check table above
		m_colors[i] =
			ReadColorL(SColors, m_colorsettings[i].setting,
			           GetDefaultColor(m_colorsettings[i].type));
#else
		m_colors[i] = GetDefaultColor(m_colorsettings[i].type);
#endif
	}

	OP_DELETEA(m_customcolors);
	m_customcolors = OP_NEWA_L(COLORREF, PCFONTCOLORS_NUMBEROFCUSTOMCOLORPREFS);

	for (i = 0; i < PCFONTCOLORS_NUMBEROFCUSTOMCOLORPREFS; i ++)
	{
#ifdef PREFS_READ
		OP_ASSERT(m_customcolorprefdefault[i].key != NULL); // Check table above
		m_customcolors[i] =
			ReadColorL(m_customcolorprefdefault[i].section,
			           m_customcolorprefdefault[i].key,
			           m_customcolorprefdefault[i].defval);
#else
		m_colors[i] = m_customcolorprefdefault[i].defval;
#endif
	}
}

#ifdef PREFS_READ
COLORREF PrefsCollectionFontsAndColors::ReadColorL(IniSection section,
                                                   const OpStringC8 &colorkey,
                                                   COLORREF dfval)
{
	int r = 0, g = 0, b = 0;

	// Prefs3 unified color section
	OpStringC color(m_reader->ReadStringL(m_sections[section], colorkey));
	if (!OpStatus::IsSuccess(UnSerializeColor(&color, &r, &g, &b)))
	{
#ifdef UPGRADE_SUPPORT
		if (section == SColors)
		{
			// Color section from earlier version
			r = m_reader->ReadIntL(colorkey, "Red", GetRValue(dfval));
			g = m_reader->ReadIntL(colorkey, "Green", GetGValue(dfval));
			b = m_reader->ReadIntL(colorkey, "Blue", GetBValue(dfval));
		}
#endif

		// Use default value
		return dfval;
	}

	return OP_RGB(r, g, b);
}
#endif

#if defined PREFS_WRITE && defined PREFS_HOSTOVERRIDE
OP_STATUS PrefsCollectionFontsAndColors::OverridePrefL(
	const uni_char *host, OP_SYSTEM_COLOR type, COLORREF value, BOOL from_user)
{
	int idx;
	const char *colorsetting = GetColorSettingName(type, &idx);

	if (colorsetting)
	{
# ifdef PREFS_COVERAGE
		++ m_colorprefused[idx];
# endif

		OverrideHostForPrefsCollectionFontsAndColors *override =
			static_cast<OverrideHostForPrefsCollectionFontsAndColors *>
			(FindOrCreateOverrideHostL(host, from_user));

		OP_STATUS rc =
			override->WriteOverrideColorL(m_overridesreader, type,
			                              colorsetting, value, from_user);

		if (OpStatus::IsSuccess(rc))
		{
# ifdef PREFSFILE_WRITE_GLOBAL
			if (from_user)
# endif
			{
				rc = m_overridesreader->WriteStringL(UNI_L("Overrides"), host, NULL);
			}
# ifdef PREFSFILE_WRITE_GLOBAL
			else
			{
				rc = m_overridesreader->WriteStringGlobalL(UNI_L("Overrides"), host, NULL);
			}
# endif
		}

		if (OpStatus::IsSuccess(rc))
		{
			BroadcastOverride(host);
		}
		return rc;
	}
	else
	{
		return OpStatus::ERR_OUT_OF_RANGE;
	}
}
#endif // PREFS_HOSTOVERRIDE

#if defined PREFS_WRITE && defined PREFS_HOSTOVERRIDE
OP_STATUS PrefsCollectionFontsAndColors::OverridePrefL(
	const uni_char *host, customcolorpref which, COLORREF value, BOOL from_user)
{
# ifdef PREFS_COVERAGE
	++ m_customcolorprefused[idx];
# endif

	OverrideHostForPrefsCollectionFontsAndColors *override =
		static_cast<OverrideHostForPrefsCollectionFontsAndColors *>
		(FindOrCreateOverrideHostL(host, from_user));

	OP_STATUS rc =
		override->WriteOverrideColorL(m_overridesreader, which,
		                              m_customcolorprefdefault[which].section,
		                              m_customcolorprefdefault[which].key,
		                              value, from_user);

	if (OpStatus::IsSuccess(rc))
	{
# ifdef PREFSFILE_WRITE_GLOBAL
		if (from_user)
# endif
		{
			rc = m_overridesreader->WriteStringL(UNI_L("Overrides"), host, NULL);
		}
# ifdef PREFSFILE_WRITE_GLOBAL
		else
		{
			rc = m_overridesreader->WriteStringGlobalL(UNI_L("Overrides"), host, NULL);
		}
# endif

		if (OpStatus::IsSuccess(rc))
		{
			BroadcastOverride(host);
		}
		return rc;
	}
	else
	{
		return OpStatus::ERR_OUT_OF_RANGE;
	}
}
#endif // PREFS_HOSTOVERRIDE

#if defined PREFS_WRITE && defined PREFS_HOSTOVERRIDE
OP_STATUS PrefsCollectionFontsAndColors::OverridePrefL(
	const uni_char *host, OP_SYSTEM_FONT type, const FontAtt &font, BOOL from_user)
{
	int idx;
	const char *fontsection = GetFontSectionName(type, &idx);
	if (idx != -1)
	{
# ifdef PREFS_COVERAGE
		++ m_fontprefused[idx];
# endif

		OverrideHostForPrefsCollectionFontsAndColors *override =
			static_cast<OverrideHostForPrefsCollectionFontsAndColors *>
			(FindOrCreateOverrideHostL(host, from_user));

		override->WriteOverrideFontL(m_overridesreader, idx, fontsection, font,
		                             from_user);
		OP_STATUS rc = OpStatus::OK;
		OpStatus::Ignore(rc);

		if (OpStatus::IsSuccess(rc))
		{
# ifdef PREFSFILE_WRITE_GLOBAL
			if (from_user)
# endif
			{
				rc = m_overridesreader->WriteStringL(UNI_L("Overrides"), host, NULL);
			}
# ifdef PREFSFILE_WRITE_GLOBAL
			else
			{
				rc = m_overridesreader->WriteStringGlobalL(UNI_L("Overrides"), host, NULL);
			}
# endif
		}

		if (OpStatus::IsSuccess(rc))
		{
			BroadcastOverride(host);
		}
		return OpStatus::OK;
	}
	else
	{
		return OpStatus::ERR_OUT_OF_RANGE;
	}
}
#endif // PREFS_HOSTOVERRIDE

#ifdef PREFS_WRITE
OP_STATUS PrefsCollectionFontsAndColors::WriteColorL(OP_SYSTEM_COLOR type,
                                                     COLORREF color)
{
	int idx = -1;
# ifdef PREFS_READ
	const char *colorkey =
# endif
		GetColorSettingName(type, &idx);
	if (idx != -1)
	{
# ifdef PREFS_COVERAGE
		++ m_colorprefused[idx];
# endif

# ifdef PREFS_READ
		uni_char strcolor[8]; /* ARRAY OK 2012-01-19 peter */
		SerializeColor(color, strcolor);
		OP_STATUS rc =
			m_reader->WriteStringL("Colors", colorkey, strcolor);
# else
		const OP_STATUS rc = OpStatus::OK;
# endif

		if (OpStatus::IsSuccess(rc) && m_colors[idx] != color)
		{
			m_colors[idx] = color;
			BroadcastChange(int(type), int(color));
		}
		return rc;
	}
	else
	{
		return OpStatus::ERR_OUT_OF_RANGE;
	}
}
#endif // PREFS_WRITE

#ifdef PREFS_WRITE
OP_STATUS PrefsCollectionFontsAndColors::WriteColorL(customcolorpref which,
                                                     COLORREF color)
{
# ifdef PREFS_COVERAGE
	++ m_customcolorprefused[idx];
# endif

# ifdef PREFS_READ
	uni_char strcolor[8]; /* ARRAY OK 2012-01-19 peter */
	SerializeColor(color, strcolor);
	OP_STATUS rc =
		m_reader->WriteStringL(m_sections[m_customcolorprefdefault[which].section],
		                       m_customcolorprefdefault[which].key,
							   strcolor);
# else
	const OP_STATUS rc = OpStatus::OK;
# endif

	if (OpStatus::IsSuccess(rc) && m_customcolors[which] != color)
	{
		m_customcolors[which] = color;
		// Use complement value in broadcast to avoid confusion with
		// OP_SYSTEM_COLOR values.
		BroadcastChange(~int(which), int(color));
	}
	return rc;
}
#endif // PREFS_WRITE

#ifdef PREFS_WRITE
BOOL PrefsCollectionFontsAndColors::ResetColorL(OP_SYSTEM_COLOR type)
{
	BOOL is_deleted = FALSE;
	int idx = -1;
# ifdef PREFS_READ
	const char *colorkey =
# endif
		GetColorSettingName(type, &idx);
	if (idx != -1)
	{
# ifdef PREFS_COVERAGE
		++ m_colorprefused[idx];
# endif

# ifdef PREFS_READ
		is_deleted = m_reader->DeleteKeyL(m_sections[SColors], colorkey);
# else
		is_deleted = TRUE;
# endif
		if (is_deleted && m_colors[idx] != GetDefaultColor(type))
		{
			m_colors[idx] = GetDefaultColor(type);
			BroadcastChange(int(type), int(m_colors[idx]));
		}
	}
	else
	{
		LEAVE(OpStatus::ERR_OUT_OF_RANGE);
	}

	return is_deleted;
}
#endif // PREFS_WRITE

#ifdef PREFS_WRITE
BOOL PrefsCollectionFontsAndColors::ResetColorL(customcolorpref which)
{
	BOOL is_deleted = FALSE;
# ifdef PREFS_COVERAGE
	++ m_customcolorprefused[which];
# endif

# ifdef PREFS_READ
	is_deleted = m_reader->DeleteKeyL(m_sections[m_customcolorprefdefault[which].section],
	                                  m_customcolorprefdefault[which].key);
# else
	is_deleted = TRUE;
# endif
	if (is_deleted && m_customcolors[which] != GetDefaultColor(which))
	{
		m_customcolors[which] = GetDefaultColor(which);
		// Use complement value in broadcast to avoid confusion with
		// OP_SYSTEM_COLOR values.
		BroadcastChange(~int(which), int(m_customcolors[which]));
	}

	return is_deleted;
}
#endif // PREFS_WRITE

BOOL PrefsCollectionFontsAndColors::GetFontInternal(
	OP_SYSTEM_FONT type, FontAtt &font, const uni_char *host) const
{
	int i;
	for (i = 0; i < PCFONTCOLORS_NUMBEROFFONTSETTINGS; i ++)
	{
		if (m_fontsettings[i].type == type)
		{
#ifdef PREFS_COVERAGE
			++ m_fontprefused[i];
#endif

#ifdef PREFS_HOSTOVERRIDE
			if (host)
			{
				OverrideHostForPrefsCollectionFontsAndColors *override =
					static_cast<OverrideHostForPrefsCollectionFontsAndColors *>
					(FindOverrideHost(host));

				if (override)
				{
					// Found a matching override
					if (override->GetOverriddenFont(i, font))
					{
						// Value is overridden
						return TRUE;
					}
				}
			}
#endif // PREFS_HOSTOVERRIDE

			// No override set, or no host specified; return standard value
			font = m_fonts[i];
			return FALSE;
		}
	}

	OP_ASSERT(!"Invalid font selection");
	return FALSE;
}

#ifdef PREFS_HOSTOVERRIDE
BOOL PrefsCollectionFontsAndColors::IsPreferenceOverridden(OP_SYSTEM_FONT which, const uni_char *host) const
{
	FontAtt unused;
	return GetFontInternal(which, unused, host);
}

BOOL PrefsCollectionFontsAndColors::IsPreferenceOverridden(OP_SYSTEM_FONT which, URL &host) const
{
	return IsPreferenceOverridden(which, host.GetAttribute(URL::KUniHostName).CStr());
}
#endif

#ifdef PREFS_HAVE_STRING_API
BOOL PrefsCollectionFontsAndColors::GetPreferenceL(
	IniSection section, const char *key, OpString &target,
	BOOL defval, const uni_char *host)
{
	if (SFonts == section)
	{
		for (int i = 0; i < PCFONTCOLORS_NUMBEROFFONTSETTINGS; ++ i)
		{
			if (0 == op_strcmp(m_fontsettings[i].setting, key))
			{
# ifdef PREFS_COVERAGE
				++ m_fontprefused[i];
# endif
				FontAtt font;
				if (defval)
				{
					GetDefaultFont(m_fontsettings[i].type, font);
				}
				else
				{
					GetFont(m_fontsettings[i].type, font, host);
				}
				LEAVE_IF_ERROR(font.Serialize(&target));
				return TRUE;
			}
		}
	}

	if (SColors == section)
	{
		for (int i = 0; i < PCFONTCOLORS_NUMBEROFCOLORSETTINGS; ++ i)
		{
			if (0 == op_strcmp(m_colorsettings[i].setting, key))
			{
# ifdef PREFS_COVERAGE
				++ m_colorprefused[i];
# endif

				COLORREF color =
					defval ? GetDefaultColor(m_colorsettings[i].type)
					       : GetColor(m_colorsettings[i].type, host);
				uni_char strcolor[8]; /* ARRAY OK 2012-01-19 peter */
				SerializeColor(color, strcolor);
				target.SetL(strcolor);
				return TRUE;
			}
		}
	}

	for (int j = 0; j < PCFONTCOLORS_NUMBEROFCUSTOMCOLORPREFS; ++ j)
	{
		if (m_customcolorprefdefault[j].section == section &&
		    0 == op_strcmp(m_customcolorprefdefault[j].key, key))
		{
# ifdef PREFS_COVERAGE
			++ m_customcolorprefused[j];
# endif
			COLORREF color =
				defval ? GetDefaultColor(static_cast<customcolorpref>(j))
				       : GetColor(static_cast<customcolorpref>(j), host);
			uni_char strcolor[8]; /* ARRAY OK 2012-01-19 peter */
			SerializeColor(color, strcolor);
			target.SetL(strcolor);
			return TRUE;
		}
	}

	return FALSE;
}
#endif

void PrefsCollectionFontsAndColors::ReadFontsL()
{
	if (!m_fonts)
	{
		m_fonts = OP_NEWA_L(FontAtt, PCFONTCOLORS_NUMBEROFFONTSETTINGS);
	}

	int i;
	OpString section_name16; ANCHOR(OpString, section_name16);
	for (i = 0; i < PCFONTCOLORS_NUMBEROFFONTSETTINGS; i ++)
	{
#ifdef PREFS_READ
		OP_ASSERT(m_fontsettings[i].setting != NULL); // Check table above this traps

		// Fetch the default font, then try to overwrite it with the
		// serialized form found in the preferences file.
		GetDefaultFont(m_fontsettings[i].type, m_fonts[i]);
		OpStringC serializedfont(m_reader->ReadStringL(m_sections[SFonts], m_fontsettings[i].setting));
		if (!serializedfont.IsEmpty())
		{
			m_fonts[i].Unserialize(serializedfont.CStr());
		}
#else
		GetDefaultFont(m_fontsettings[i].type, m_fonts[i]);
#endif
	}
}

#if defined PREFS_READ && defined UPGRADE_SUPPORT
void PrefsCollectionFontsAndColors::ReadFontsOldFormatL()
{
	for (int i = 0; i < PCFONTCOLORS_NUMBEROFFONTSETTINGS; ++ i)
	{
		enum OP_SYSTEM_FONT type = m_fontsettings[i].type;

		// Get default font
		FontAtt dlf;
		GetDefaultFont(type, dlf);

		// Font section name is Font.something
		char oldsection[64]; /* ARRAY OK 2010-12-09 peter */
		op_snprintf(oldsection, ARRAY_SIZE(oldsection), "Font.%s", m_fontsettings[i].setting);

		// Read actual font
		OpStringC facename(m_reader->ReadStringL(oldsection, "FaceName", dlf.GetFaceName()));
		short fontnumber = styleManager->GetFontNumber(facename.CStr());

		FontAtt font;
		// Only use the font if it is recognized
		font.SetFontNumber(fontnumber != -1 ? fontnumber : dlf.GetFontNumber());

		font.SetSize(op_abs(m_reader->ReadIntL(oldsection, "Height", dlf.GetSize())));
		font.SetWeight(     m_reader->ReadIntL(oldsection, "Weight", dlf.GetWeight() * 100) / 100);
		font.SetItalic(   m_reader->ReadBoolL(oldsection, "Italic",    dlf.GetItalic()));
		font.SetUnderline(m_reader->ReadBoolL(oldsection, "Underline", dlf.GetUnderline()));
		font.SetOverline( m_reader->ReadBoolL(oldsection, "Overline",  dlf.GetOverline()));
		font.SetStrikeOut(m_reader->ReadBoolL(oldsection, "StrikeOut", dlf.GetStrikeOut()));

		OpString serializedfont; ANCHOR(OpString, serializedfont);
		LEAVE_IF_ERROR(font.Serialize(&serializedfont));

		OpString serializeddlffont; ANCHOR(OpString, serializeddlffont);
		LEAVE_IF_ERROR(dlf.Serialize(&serializeddlffont));

		if (serializedfont.Compare(serializeddlffont) != 0)
		{
#ifdef PREFS_WRITE
			WriteFontL(type, font);
#else
			m_fonts[i] = font;
#endif
		}
	}
}
#endif

#ifdef PREFS_WRITE
OP_STATUS PrefsCollectionFontsAndColors::WriteFontL(OP_SYSTEM_FONT type, const FontAtt &font)
{
	// Common function for finding font section name
	int idx = -1;
# ifdef PREFS_READ
	const char *name =
# endif
		GetFontSectionName(type, &idx);
	if (idx != -1)
	{
# ifdef PREFS_COVERAGE
		++ m_fontprefused[idx];
# endif

		OpString serializedfont; ANCHOR(OpString, serializedfont);
		LEAVE_IF_ERROR(font.Serialize(&serializedfont));
# ifdef PREFS_READ
		OP_STATUS rc = m_reader->WriteStringL(m_sections[SFonts], name, serializedfont);
# else
		OP_STATUS rc = OpStatus::OK;
# endif

		if (OpStatus::IsSuccess(rc) && !(m_fonts[idx] == font))
		{
			m_fonts[idx] = font;
			BroadcastChange(int(type), serializedfont);
		}
		return rc;
	}
	else
	{
		return OpStatus::ERR_OUT_OF_RANGE;
	}
}
#endif // PREFS_WRITE

#ifdef PREFS_WRITE
BOOL PrefsCollectionFontsAndColors::ResetFontL(OP_SYSTEM_FONT type)
{
	BOOL is_deleted = FALSE;
	int idx = -1;
# ifdef PREFS_READ
	const char *name =
# endif
		GetFontSectionName(type, &idx);
	if (idx != -1)
	{
# ifdef PREFS_COVERAGE
		++ m_fontprefused[idx];
# endif

# ifdef PREFS_READ
		is_deleted = m_reader->DeleteKeyL(m_sections[SFonts], name);
# else
		is_deleted = TRUE;
# endif
		if (is_deleted)
		{
			FontAtt default_font;
			GetDefaultFont(m_fontsettings[idx].type, default_font);
			if (!(m_fonts[idx] == default_font))
			{
				GetDefaultFont(m_fontsettings[idx].type, m_fonts[idx]);
				OpString serializedfont; ANCHOR(OpString, serializedfont);
				LEAVE_IF_ERROR(m_fonts[idx].Serialize(&serializedfont));
				BroadcastChange(int(type), serializedfont);
			}
		}
	}
	else
	{
		LEAVE(OpStatus::ERR_OUT_OF_RANGE);
	}

	return is_deleted;
}
#endif // PREFS_WRITE

#ifdef PREFS_HOSTOVERRIDE
// Override the implementation in OpPrefsCollectionWithHostOverride so that we
// can use our extended version which supports color and font properties
OpOverrideHost *PrefsCollectionFontsAndColors::CreateOverrideHostObjectL(
	const uni_char *host, BOOL from_user)
{
	OpStackAutoPtr<OverrideHostForPrefsCollectionFontsAndColors>
		newobj(OP_NEW_L(OverrideHostForPrefsCollectionFontsAndColors, ()));
	newobj->ConstructL(host, from_user);
# ifdef PREFS_WRITE
	if (g_prefsManager->IsInitialized())
	{
		g_prefsManager->OverrideHostAddedL(host);
	}
# endif
	return newobj.release();
}
#endif // PREFS_HOSTOVERRIDE

#ifdef FONTSWITCHING
# ifdef PREFS_READ
void PrefsCollectionFontsAndColors::ReadPreferredFontsL()
{
	/*DOC
	 *section=Preferred fonts
	 *name=propsection
	 *key=unicode block number
	 *type=string
	 *value=font name
	 *description=International font override, proportional font
	 */
	/*DOC
	 *section=Preferred fonts monospace
	 *name=monosection
	 *key=unicode block number
	 *type=string
	 *value=font name
	 *description=International font override, monospace font
	 */

	for (int i = 0; i <= 1; ++ i)
	{
		BOOL monospace = i != 0;
		OpStringC section_name(monospace ? UNI_L("Preferred fonts monospace") : UNI_L("Preferred fonts"));
		OpStackAutoPtr<PrefsSection> section(m_reader->ReadSectionL(section_name));

		const PrefsEntry *entry = section.get() ? section->Entries() : NULL;
		while (entry)
		{
			const uni_char *key = entry->Key();
			const uni_char *value = entry->Value();
			if (key && *key && value && *value)
			{
				int block_nr = uni_atoi(key);
				LEAVE_IF_ERROR(styleManager->SetPreferredFontForScript(static_cast<UINT8>(block_nr), monospace, value));
			}
			entry = entry->Suc();
		}
	}
}
# endif

# ifdef PREFS_WRITE
void PrefsCollectionFontsAndColors::WritePreferredFontL(int block_nr, BOOL monospace, const uni_char* font_name, OpFontInfo::CodePage codepage)
{
#  ifdef PREFS_READ
	OpStringC section(monospace ? UNI_L("Preferred fonts monospace") : UNI_L("Preferred fonts"));

	// We code simplified Chinese as block 55 and traditional Chinese as block 59
	switch (codepage)
	{
	case OpFontInfo::OP_SIMPLIFIED_CHINESE_CODEPAGE:
		block_nr = 55; break;
	case OpFontInfo::OP_TRADITIONAL_CHINESE_CODEPAGE:
		block_nr = 59; break;

	default: break; // use supplied value of block_nr
	}

	uni_char block_str[3 + sizeof(int) * 53 / 22]; /* ARRAY OK 2010-12-09 peter */
	uni_itoa(block_nr, block_str, 10);

	if (font_name)
	{
		m_reader->WriteStringL(section, block_str, font_name);
	}
	else
	{
		m_reader->DeleteKeyL(section.CStr(), block_str);
	}
#  endif
}
# endif // PREFS_WRITE
#endif // FONTSWITCHING

#if defined PREFS_WRITE && defined PREFS_HAVE_STRING_API
BOOL PrefsCollectionFontsAndColors::WritePreferenceL(IniSection section, const char *key, const OpStringC &value)
{
	if (SFonts == section)
	{
		for (int i = 0; i < PCFONTCOLORS_NUMBEROFFONTSETTINGS; ++ i)
		{
			if (0 == STRINGAPI_STRCMP(m_fontsettings[i].setting, key))
			{
# ifdef PREFS_COVERAGE
				++ m_fontprefused[i];
# endif
				FontAtt new_font;
				if (new_font.Unserialize(value.CStr()))
				{
					return OpStatus::IsSuccess(WriteFontL(m_fontsettings[i].type, new_font));
				}

				// Invalid font value.
				LEAVE(OpStatus::ERR_OUT_OF_RANGE);
			}
		}
	}

	if (SColors == section)
	{
		for (int i = 0; i < PCFONTCOLORS_NUMBEROFCOLORSETTINGS; ++ i)
		{
			if (0 == STRINGAPI_STRCMP(m_colorsettings[i].setting, key))
			{
#ifdef PREFS_COVERAGE
				++ m_colorprefused[i];
#endif

				int r = 0, g = 0, b = 0;
				if (OpStatus::IsSuccess(UnSerializeColor(&value, &r, &g, &b)))
				{
					return OpStatus::IsSuccess(WriteColorL(m_colorsettings[i].type, OP_RGB(r, g, b)));
				}

				// Invalid color value.
				LEAVE(OpStatus::ERR_OUT_OF_RANGE);
			}
		}
	}

	for (int j = 0; j < PCFONTCOLORS_NUMBEROFCUSTOMCOLORPREFS; ++ j)
	{
		if (m_customcolorprefdefault[j].section == section &&
		    0 == op_strcmp(m_customcolorprefdefault[j].key, key))
		{
# ifdef PREFS_COVERAGE
			++ m_customcolorprefused[j];
# endif

			int r = 0, g = 0, b = 0;
			if (OpStatus::IsSuccess(UnSerializeColor(&value, &r, &g, &b)))
			{
				return OpStatus::IsSuccess(WriteColorL(static_cast<customcolorpref>(j), OP_RGB(r, g, b)));
			}

			// Invalid color value.
			LEAVE(OpStatus::ERR_OUT_OF_RANGE);
		}
	}

	return FALSE;
}

# ifdef PREFS_HOSTOVERRIDE
BOOL PrefsCollectionFontsAndColors::OverridePreferenceL(const uni_char *, IniSection, const char *, const OpStringC &, BOOL)
{
	// Not implemented

	return FALSE;
}

BOOL PrefsCollectionFontsAndColors::RemoveOverrideL(const uni_char *, IniSection, const char *, BOOL)
{
	// Not implemented

	return FALSE;
}
# endif
#endif // PREFS_WRITE && PREFS_HAVE_STRING_API

#ifdef PREFS_COVERAGE
void PrefsCollectionFontsAndColors::CoverageReport(
	const struct stringprefdefault *, int,
	const struct integerprefdefault *, int)
{
	OpPrefsCollection::CoverageReport(NULL, 0, NULL, 0);

	BOOL all = TRUE;
	CoverageReportPrint("* Color preferences:\n");
	for (int i = 0; i < PCFONTCOLORS_NUMBEROFCOLORSETTINGS; ++ i)
	{
		if (!m_colorprefused[i])
		{
			CoverageReportPrint("  \"Colors\".\"");
			CoverageReportPrint(m_colorsettings[i].setting);
			CoverageReportPrint("\" is unreferenced\n");
			all = FALSE;
		}
	}
	if (all)
	{
		CoverageReportPrint("  All color preferences referenced\n");
	}

	all = TRUE;
	CoverageReportPrint("* Font preferences:\n");
	for (int j = 0; j < PCFONTCOLORS_NUMBEROFFONTSETTINGS; ++ j)
	{
		if (!m_fontprefused[j])
		{
			CoverageReportPrint("  \"Fonts\".\"");
			CoverageReportPrint(m_fontsettings[j].setting);
			CoverageReportPrint("\" is unreferenced\n");
			all = FALSE;
		}
	}
	if (all)
	{
		CoverageReportPrint("  All font preferences referenced\n");
	}
}
#endif

#ifdef PREFS_ENUMERATE
unsigned int PrefsCollectionFontsAndColors::GetPreferencesL(struct prefssetting *settings) const
{
# ifdef PREFS_WRITE
	// Copy font values
	const fontsections *fonts = m_fontsettings;
	FontAtt *curfont = m_fonts;
	for (int i = 0; i < PCFONTCOLORS_NUMBEROFFONTSETTINGS; ++ i)
	{
		settings->section = m_sections[SFonts];
		settings->key     = fonts->setting;
		LEAVE_IF_ERROR(curfont->Serialize(&settings->value));
		settings->type    = prefssetting::string;

		++ settings;
		++ fonts;
		++ curfont;
	}
# endif

	// Copy color values
	uni_char strcolor[8]; /* ARRAY OK 2012-01-19 peter */

	const colorsettings *colors = m_colorsettings;
	COLORREF *cur = m_colors;
	for (int j = 0; j < PCFONTCOLORS_NUMBEROFCOLORSETTINGS; ++ j)
	{
		settings->section = m_sections[SColors];
		settings->key     = colors->setting;

		SerializeColor(*cur, strcolor);

		settings->value.SetL(strcolor);
		settings->type    = prefssetting::color;

		++ settings;
		++ colors;
		++ cur;
	}

	// Copy custom color values
	const customcolorprefdefault *customcolors = m_customcolorprefdefault;
	COLORREF *curcustom = m_customcolors;
	for (int k = 0; k < PCFONTCOLORS_NUMBEROFCUSTOMCOLORPREFS; ++ k)
	{
		settings->section = m_sections[customcolors->section];
		settings->key     = customcolors->key;

		SerializeColor(*curcustom, strcolor);

		settings->value.SetL(strcolor);
		settings->type    = prefssetting::color;

		++ settings;
		++ customcolors;
		++ curcustom;
	}

	return
		PCFONTCOLORS_NUMBEROFCUSTOMCOLORPREFS +
# ifdef PREFS_WRITE
		PCFONTCOLORS_NUMBEROFFONTSETTINGS +
# endif
		PCFONTCOLORS_NUMBEROFCOLORSETTINGS;
}
#endif
