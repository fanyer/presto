/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Peter Krefting
*/

#if !defined MODULES_ABOUT_OPERAABOUT_H && !defined NO_URL_OPERA
#define MODULES_ABOUT_OPERAABOUT_H

#include "modules/about/opgenerateddocument.h"
#include "modules/util/opfile/opfolder.h"

class OpFile;

/**
 * Generator for the opera:about (browser information) document.
 */
class OperaAbout : public OpGeneratedDocument
{
public:
#ifdef ABOUT_DESKTOP_ABOUT
	/**
	 * Factory method. Create the platform implementation of the OperaAbout
	 * interface. When you enable the desktop version of the opera:about
	 * screen, there is some information that your platform needs to fill
	 * in to complete the screen, these are defined as abstract in this
	 * implementation.
	 *
	 * @param url The URL to generate the document to.
	 */
	static OperaAbout *Create(URL &url);
#else
	static inline OperaAbout *Create(URL &url) { return OP_NEW(OperaAbout, (url)); }
#endif

	/**
	 * Generate the opera:about document to the specified internal URL.
	 * If you are building without using the desktop about, you must
	 * implement this method somewhere in your own code.
	 *
	 * @return OK on success, or any error reported by URL or string code.
	 */
	virtual OP_STATUS GenerateData();

protected:
	/**
	 * Constructor for the browser information page generator.
	 *
	 * @param url URL to write to.
	 */
	OperaAbout(URL &url)
		: OpGeneratedDocument(url, OpGeneratedDocument::HTML5)
		{}


#if defined ABOUT_DESKTOP_ABOUT || defined ABOUT_MOBILE_ABOUT
	// Services for OperaAbout and its subclasses ----
	/** Open list element */
	OP_STATUS OpenList();
	/** Add a list entry to the list
	 * @param header Heading to use for this entry.
	 * @param data The value for this header. Separate multiple tokens by
	 *             newlines.
	 */
	OP_STATUS ListEntry(Str::LocaleString header, const OpStringC &data);
	/** @overload */
	OP_STATUS ListEntry(const OpStringC &header, const OpStringC &data);
# ifdef ABOUT_DESKTOP_ABOUT
	/** @overload */
	OP_STATUS ListEntry(Str::LocaleString header, OpFileFolder data);
	/** @overload */
	OP_STATUS ListEntry(Str::LocaleString header, const OpFile *data);
# endif

# if defined _PLUGIN_SUPPORT_ || defined USER_JAVASCRIPT
	/** @overload */
	OP_STATUS ListEntry(Str::LocaleString header, const OpStringC &data, uni_char itemsep, uni_char tokensep);
# endif
	/** Close list element */
	OP_STATUS CloseList();
#endif

#ifdef ABOUT_DESKTOP_ABOUT
	// Interfaces subclasses must implement ----
	virtual OP_STATUS GetVersion(OpString *dest) = 0;	///< Version number string
public:
	virtual OP_STATUS GetBuild(OpString *dest) = 0;		///< Build number string
protected:
	/** Platform identifier and version.
	  * @param platform Return platform string, like "Linux" or "Win32".
	  * @param version  Return version string, like "2.6.20.1" or "Windows Vista". */
	virtual OP_STATUS GetPlatform(OpString *platform, OpString *version) = 0;
	/** Write toolkit details, if any. An implementation should use the
	  * ListEntry() API to add a line describing the toolkit in use. */
	virtual OP_STATUS WriteToolkit() { return OpStatus::OK; };

#ifdef PREFS_READ
	virtual OP_STATUS GetPreferencesFile(OpString *dest) = 0;		///< Name of preferences file (or equivalent)
#endif

	// Removed interfaces:
	// virtual OP_STATUS GetJavaVersion(OpString *dest) = 0; removed 2010-10-15 (CORE-27891)

	// Write sections of opera:about ----
	// Subclasses could override these if they wanted to.
	virtual OP_STATUS WriteVersion();	///< Write version info block
	virtual OP_STATUS WritePaths();		///< Write paths info block
//	virtual OP_STATUS WriteSound();		///< Write sound server info block
#endif // ABOUT_DESKTOP_ABOUT

#if defined ABOUT_DESKTOP_ABOUT || defined ABOUT_MOBILE_ABOUT
	virtual OP_STATUS WriteUA();		///< Write User-Agent info block
	virtual OP_STATUS WriteCredits();	///< Write third-party info block
#endif
};

#endif
