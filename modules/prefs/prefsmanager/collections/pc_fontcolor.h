/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright 2002-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Peter Krefting
*/

#ifndef PC_FONTCOLOR_H
#define PC_FONTCOLOR_H

#include "modules/prefs/prefsmanager/opprefscollection_override.h"
#include "modules/display/fontdb.h"
#include "modules/pi/OpSystemInfo.h"
#include "modules/pi/ui/OpUiInfo.h"
#include "modules/url/url_sn.h"

class OverrideHostForPrefsCollectionFontsAndColors;

/** Global PrefsCollectionFontsAndColors object (singleton). */
#define g_pcfontscolors (g_opera->prefs_module.PrefsCollectionFontsAndColors())

/**
  *
  * Preferences for fonts and colors.
  *
  * @author Peter Karlsson
  */
class PrefsCollectionFontsAndColors : public OpPrefsCollectionWithHostOverride
{
public:
	/**
	  * Create method.
	  *
	  * This method preforms all actions required to construct the object.
	  * PrefsManager calls this method to initialize the object, and will
	  * not call the normal constructor directly.
	  *
	  * @param reader Pointer to the PrefsFile object.
	  * @return Pointer to the created object.
	  */
	static PrefsCollectionFontsAndColors *CreateL(PrefsFile *reader);

	virtual ~PrefsCollectionFontsAndColors();

#include "modules/prefs/prefsmanager/collections/pc_fontcolor_h.inl"

/* Define the number of configurable fonts */
#ifdef _QUICK_UI_FONT_SUPPORT_
# define PCFONTCOLORS_UIFONTS 9
#else
# define PCFONTCOLORS_UIFONTS 0
#endif
#define PCFONTCOLORS_NUMBEROFFONTSETTINGS	(11 + PCFONTCOLORS_UIFONTS)

/* Define the number of configurable colors */
#ifdef PREFS_CSS_TOOLTIP_COLOR
# define PCFONTCOLORS_CSSTOOLTIPCOLORS 2
#else
# define PCFONTCOLORS_CSSTOOLTIPCOLORS 0
#endif
#ifdef SKIN_SUPPORT
# define PCFONTCOLORS_SKINCOLOR 1
#else
# define PCFONTCOLORS_SKINCOLOR 0
#endif
#ifdef SEARCH_MATCHES_ALL
# define PCFONTCOLORS_NOFOCUSCOLORS 2
#else
# define PCFONTCOLORS_NOFOCUSCOLORS 0
#endif
#if defined SEARCH_MATCHES_ALL
# define PCFONTCOLORS_HILITECOLORS 4
#else
# define PCFONTCOLORS_HILITECOLORS 0
#endif
#define PCFONTCOLORS_NUMBEROFCOLORSETTINGS	(18 + PCFONTCOLORS_CSSTOOLTIPCOLORS + PCFONTCOLORS_SKINCOLOR + PCFONTCOLORS_NOFOCUSCOLORS + PCFONTCOLORS_HILITECOLORS)

	// Read ------
	/**
	 * Read a color preference. This is used both for display code colors and
	 * for some UI colors, just to keep all the code in one place.
	 *
	 * @param which The color to retrieve.
	 * @param host Host context to retrieve the color for. NULL will retrieve
	 * the default context.
	 * @return Selected color.
	 */
	inline COLORREF GetColor(OP_SYSTEM_COLOR which, const uni_char *host = NULL) const
	{
		return GetColorInternal(which, host, NULL);
	}

	/** @overload */
	inline COLORREF GetColor(OP_SYSTEM_COLOR which, const ServerName *host) const
	{
		return GetColor(which, host ? host->UniName() : NULL);
	}

	/**
	 * Read a custom color preference.
	 *
	 * @param which The color to retrieve.
	 * @param host Host context to retrieve the color for. NULL will retrieve
	 * the default context.
	 * @return Selected color.
	 */
	inline COLORREF GetColor(customcolorpref which, const uni_char *host = NULL) const
	{
		return GetColorInternal(which, host, NULL);
	}

	/** @overload */
	inline COLORREF GetColor(customcolorpref which, const ServerName *host) const
	{
		return GetColor(which, host ? host->UniName() : NULL);
	}

	/**
	 * Read a font preference. This is used both for display code fonts and
	 * for some UI fonts, just to keep all the code in one place.
	 *
	 * @param type The font to retrieve.
	 * @param font The font object to store into.
	 * @param host Host contect to retrieve the font for. NULL will retrieve
	 * the default context.
	 */
	inline void GetFont(OP_SYSTEM_FONT type, FontAtt &font,
	                   const uni_char *host = NULL) const
	{
		GetFontInternal(type, font, host);
	}

	/** @overload */
	inline void GetFont(OP_SYSTEM_FONT type, FontAtt &font,
	             const ServerName *host) const
	{
		GetFont(type, font, host ? host->UniName() : NULL);
	}

#ifdef PREFS_HOSTOVERRIDE
	/**
	 * Check if color preference is overridden for this host.
	 *
	 * @param which Selects the preference to check.
	 * @param host Host context to check for override.
	 * @return TRUE if preference is overridden.
	 */
	BOOL IsPreferenceOverridden(OP_SYSTEM_COLOR which, const uni_char *host) const;
	BOOL IsPreferenceOverridden(OP_SYSTEM_COLOR which, URL &host) const;

	/**
	 * Check if custom color preference is overridden for this host.
	 *
	 * @param which Selects the preference to check.
	 * @param host Host context to check for override.
	 * @return TRUE if preference is overridden.
	 */
	BOOL IsPreferenceOverridden(customcolorpref which, const uni_char *host) const;
	BOOL IsPreferenceOverridden(customcolorpref which, URL &host) const;

	/**
	 * Check if font preference is overridden for this host.
	 *
	 * @param which Selects the preference to check.
	 * @param host Host context to check for override.
	 * @return TRUE if preference is overridden.
	 */
	BOOL IsPreferenceOverridden(OP_SYSTEM_FONT which, const uni_char *host) const;
	BOOL IsPreferenceOverridden(OP_SYSTEM_FONT which, URL &host) const;
#endif // PREFS_HOSTOVERRIDE

#ifdef PREFS_HAVE_STRING_API
	virtual BOOL GetPreferenceL(IniSection section, const char *key, OpString &target,
		BOOL defval, const uni_char *host);
#endif

	// Defaults ------
	/**
	 * Get default value for a font preference.
	 *
	 * @param type The font to retrieve.
	 * @param font The font object to store into.
	 */
	static inline void GetDefaultFont(OP_SYSTEM_FONT type, FontAtt &font)
	{
		g_op_ui_info->GetSystemFont(type, font);
	}
	
	/**
	 * Get default value for a color preference.
	 *
	 * @param which the color to retrieve.
	 * @return Selected color
	 */
	static inline COLORREF GetDefaultColor(OP_SYSTEM_COLOR which)
	{
		return g_op_ui_info->GetSystemColor(which);
	}

	/**
	 * Get default value for a custom color preference.
	 *
	 * @param which the color to retrieve.
	 * @return Selected color
	 */
	inline COLORREF GetDefaultColor(customcolorpref which)
	{
		return m_customcolorprefdefault[which].defval;
	}

#ifdef PREFS_WRITE
	// Write ------
# ifdef PREFS_HOSTOVERRIDE
	/** Write overridden color preferences.
	  *
	  * @param host Host to write an override for
	  * @param type The color to override.
	  * @param value Value for the override.
	  * @param from_user TRUE if the preference is entered by the user.
	  * @return ERR_NO_ACCESS if override is not allowed, ERR_OUT_OF_RANGE
	  * if type is invalid, OK otherwise.
	  */
	OP_STATUS OverridePrefL(const uni_char *host, OP_SYSTEM_COLOR type,
	                        COLORREF value, BOOL from_user);
# endif // PREFS_HOSTOVERRIDE

# ifdef PREFS_HOSTOVERRIDE
	/** Write overridden custom color preferences.
	  *
	  * @param host Host to write an override for
	  * @param which The color to override.
	  * @param value Value for the override.
	  * @param from_user TRUE if the preference is entered by the user.
	  * @return ERR_NO_ACCESS if override is not allowed, ERR_OUT_OF_RANGE
	  * if type is invalid, OK otherwise.
	  */
	OP_STATUS OverridePrefL(const uni_char *host, customcolorpref which,
	                        COLORREF value, BOOL from_user);
# endif // PREFS_HOSTOVERRIDE

# ifdef PREFS_HOSTOVERRIDE
	/** Write overridden font preferences.
	  *
	  * @param host Host to write an override for
	  * @param type The font to override.
	  * @param font Value for the override.
	  * @param from_user TRUE if the preference is entered by the user.
	  * @return ERR_OUT_OF_RANGE if type is invalid, OK otherwise.
	  */
	OP_STATUS OverridePrefL(const uni_char *host, OP_SYSTEM_FONT type,
	                        const FontAtt &font, BOOL from_user);
# endif // PREFS_HOSTOVERRIDE

	/** Write a color preference.
	  *
	  * The changed color preference will be announced as an integer
	  * change to the associated listeners, using the OP_SYSTEM_COLOR
	  * as an index.
	  *
	  * @param type Which color to write.
	  * @param color The color to write.
	  * @return ERR_NO_ACCESS if override is not allowed, ERR_OUT_OF_RANGE
	  * if type is invalid, OK otherwise.
	  */
	OP_STATUS WriteColorL(OP_SYSTEM_COLOR type, COLORREF color);

	/** Write a custom color preference.
	  *
	  * The changed color preference will be announced as an integer
	  * change to the associated listeners, using the complement value
	  * (~customcolorpref, always negative) as an index, to avoid
	  * confusion with the system colors which are announced as
	  * positive integers.
	  *
	  * @param which Which color to write.
	  * @param color The color to write.
	  * @return ERR_NO_ACCESS if override is not allowed, ERR_OUT_OF_RANGE
	  * if type is invalid, OK otherwise.
	  */
	OP_STATUS WriteColorL(customcolorpref which, COLORREF color);

	/** Write a font preference.
	  *
	  * The changed font preference will be announced as a string change
	  * to the associated listeners, using the OP_SYTEM_FONT as an index.
	  * The serialized (INI file) form of the font will be announced.
	  *
	  * @param type Which font to write.
	  * @param font The font to write.
	  * @return ERR_NO_ACCESS if override is not allowed, ERR_OUT_OF_RANGE
	  * if type is invalid, OK otherwise.
	  */
	OP_STATUS WriteFontL(OP_SYSTEM_FONT type, const FontAtt &font);

# ifdef FONTSWITCHING
	/** Write a preferred font preference.
	  *
	  * @param block_nr Unicode block number
	  * @param monospace Is this the monospace or proportional font?
	  * @param font_name Name of preferred font for the block
	  * @param codepage Additional codepage information for font
	  */
	void WritePreferredFontL(int block_nr, BOOL monospace,
	                         const uni_char *font_name,
							 OpFontInfo::CodePage codepage);
# endif // FONTSWITCHING

# ifdef PREFS_HAVE_STRING_API
	virtual BOOL WritePreferenceL(IniSection section, const char *key, const OpStringC &value);
#  ifdef PREFS_HOSTOVERRIDE
	virtual BOOL OverridePreferenceL(const uni_char *host, IniSection section, const char *key, const OpStringC &value, BOOL from_user);
	virtual BOOL RemoveOverrideL(const uni_char *host, IniSection section, const char *key, BOOL from_user);
#  endif
# endif
#endif // PREFS_WRITE

#ifdef PREFS_WRITE
	// Reset ------
	/** Reset a color preference. Resets the preference to default by
	  * removing the set value from the preference storage.
	  *
	  * @param type The preference to reset.
	  * @return TRUE if the delete succeeded.
	  */
	BOOL ResetColorL(OP_SYSTEM_COLOR type);

	/** Reset a custom color preference. Resets the preference to default by
	  * removing the set value from the preference storage.
	  *
	  * @param which The preference to reset.
	  * @return TRUE if the delete succeeded.
	  */
	BOOL ResetColorL(customcolorpref which);

	/** Reset a font preference. Resets the preference to default by
	  * removing the set value from the preference storage.
	  *
	  * @param type The preference to reset.
	  * @return TRUE if the delete succeeded.
	  */
	BOOL ResetFontL(OP_SYSTEM_FONT type);
#endif // PREFS_WRITE

	// Fetch preferences from file ------
	virtual void ReadAllPrefsL(PrefsModule::PrefsInitInfo *info);
#ifdef PREFS_HOSTOVERRIDE
	virtual void ReadOverridesL(const uni_char *host, PrefsSection *section, BOOL active, BOOL from_user);
#endif // PREFS_HOSTOVERRIDE

	// Upgrade ------
#if defined PREFS_READ && defined UPGRADE_SUPPORT
	void ReadFontsOldFormatL();
#endif

#ifdef PREFS_ENUMERATE
	// Enumeration helpers ------
	virtual unsigned int GetNumberOfPreferences() const
	{
		return PCFONTCOLORS_NUMBEROFCUSTOMCOLORPREFS +
# ifdef PREFS_WRITE
			PCFONTCOLORS_NUMBEROFFONTSETTINGS + 
# endif
			PCFONTCOLORS_NUMBEROFCOLORSETTINGS;
	}

	virtual unsigned int GetPreferencesL(struct prefssetting *settings) const;
#endif

	// Serialization and unserialization ------
	/**
	 * Convert a string containing a standard #RRGGBB color representation
	 * into red, green and blue values.
	 *
	 * @param color String representation of color.
	 * @param r     Output for red value.
	 * @param g     Output for green value.
	 * @param b     Output for blue value.
	 * @return OK on success, ERR on error.
	 */
	static inline OP_STATUS UnSerializeColor(const OpStringC *color, int *r, int *g, int *b)
	{
		if (7 == color->Length() &&
		    3 == uni_sscanf(color->CStr(), UNI_L("#%2x%2x%2x"), r, g, b))
			return OpStatus::OK;
		else
			return OpStatus::ERR;
	}

	/**
	 * Convert a COLORREF containing a color into a standard #RRGGBB color
	 * representation.
	 *
	 * @param color Color value.
	 * @param s     Output for string. Must be at least 8 uni_char long.
	 */
	static inline void SerializeColor(COLORREF color, uni_char *s)
	{
		uni_snprintf(s, 8, UNI_L("#%02x%02x%02x"),
		             GetRValue(color), GetGValue(color), GetBValue(color));
	}

	// Special APIs ------
#ifdef PREFS_HAVE_REREAD_FONTS
	/** Only to be called from PrefsNotifier. The ini file must have
	  * been read into memory before calling this, since we check if
	  * the keys are set. */
	inline void FontChangedL() { ReadFontsL(); }
#endif

#ifdef PREFS_HAVE_NOTIFIER_ON_COLOR_CHANGED
	inline void ColorChangedL() { ReadColorsL(); }
#endif // PREFS_HAVE_NOTIFIER_ON_COLOR_CHANGED

private:
	/** Single-phase constructor.
	  *
	  * @param reader Pointer to the PrefsFile object.
	  */
	PrefsCollectionFontsAndColors(PrefsFile *reader)
		: OpPrefsCollectionWithHostOverride(FontsAndColors, reader),
		  m_colors(NULL), m_customcolors(NULL), m_fonts(NULL)
		{
#ifndef HAS_COMPLEX_GLOBALS
			InitFonts();
			InitColors();
			InitCustomColors();
#endif
		}

	/** Color preference information and default values */
	PREFS_STATIC struct customcolorprefdefault
	{
#ifdef PREFS_READ
		enum IniSection section;	///< Section of INI file
		const char *key;			///< Key in INI file
#endif
		COLORREF defval;			///< Default value
	} m_customcolorprefdefault[PCFONTCOLORS_NUMBEROFCUSTOMCOLORPREFS + 1];

	// Font and color helpers
	PREFS_STATIC struct fontsections
	{
		enum OP_SYSTEM_FONT type;
#ifdef PREFS_READ
		const char *setting;
#endif
	} m_fontsettings[PCFONTCOLORS_NUMBEROFFONTSETTINGS];

	PREFS_STATIC struct colorsettings
	{
		enum OP_SYSTEM_COLOR type;
#ifdef PREFS_READ
		const char *setting;
#endif
	} m_colorsettings[PCFONTCOLORS_NUMBEROFCOLORSETTINGS];

#ifdef PREFS_COVERAGE
	mutable int m_fontprefused[PCFONTCOLORS_NUMBEROFFONTSETTINGS];
	int m_colorprefused[PCFONTCOLORS_NUMBEROFCOLORSETTINGS];
	int m_customcolorprefused[PCFONTCOLORS_NUMBEROFCUSTOMCOLORPREFS];
#endif

	const char *GetFontSectionName(enum OP_SYSTEM_FONT, int *);
	const char *GetColorSettingName(enum OP_SYSTEM_COLOR, int *);
#ifdef PREFS_READ
	COLORREF ReadColorL(IniSection section, const OpStringC8 &colorkey, COLORREF dfval);
#endif
	void ReadColorsL();
	void ReadFontsL();
#if defined PREFS_READ && defined FONTSWITCHING
	void ReadPreferredFontsL();
#endif

	COLORREF *m_colors;			///< All color preferences
	COLORREF *m_customcolors;	///< All custom color preferences
	FontAtt *m_fonts;			///< All font preferences

#ifdef PREFS_HOSTOVERRIDE
	virtual OpOverrideHost *CreateOverrideHostObjectL(const uni_char *, BOOL);
#endif // PREFS_HOSTOVERRIDE

	/** Internal helper for GetColor */
	COLORREF GetColorInternal(OP_SYSTEM_COLOR which, const uni_char *host,
							  BOOL *overridden) const;
	/** Internal helper for GetColor */
	COLORREF GetColorInternal(customcolorpref which, const uni_char *host,
							  BOOL *overridden) const;

	/** Internal helper for GetFont */
	BOOL GetFontInternal(OP_SYSTEM_FONT type, FontAtt &font, const uni_char *) const;

#ifdef PREFS_VALIDATE
	// These are not used.
	virtual void CheckConditionsL(int, int *, const uni_char *)
	{ OP_ASSERT(!"Invalid call"); LEAVE(OpStatus::ERR_NOT_SUPPORTED); }
	virtual BOOL CheckConditionsL(int, const OpStringC &, OpString **, const uni_char *)
	{ OP_ASSERT(!"Invalid call"); LEAVE(OpStatus::ERR_NOT_SUPPORTED); return FALSE; }
#endif

	// Debug code ------
#ifdef PREFS_COVERAGE
	virtual void CoverageReport(
		const struct stringprefdefault *, int,
		const struct integerprefdefault *, int);
#endif

	// Friends -----
	// Uses ReadFontL() and WriteFontL():
	friend class OverrideHostForPrefsCollectionFontsAndColors;

#ifndef HAS_COMPLEX_GLOBALS
	void InitFonts();
	void InitColors();
	void InitCustomColors();
#endif
};

#endif // PC_FONTCOLOR_H
