/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2002-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef CSSMANAGER_H
#define CSSMANAGER_H

#include "modules/prefs/prefsmanager/collections/pc_files.h"
#include "modules/prefs/prefsmanager/opprefslistener.h"
#include "modules/util/opfile/opfile.h"

class CSS;
class HTML_Element;
class OpFile;
class HLDocProfile;
class OpInputAction;

/**
 * Class used to handle local CSS files.
 */
class CSSManager : public PrefsCollectionFilesListener
{
public:
	/**
	 * Enumeration describing the CSSes that are stored by the CSS manager.
	 */
	enum css_id
	{
		BrowserCSS = 0,	///< Browser's local style sheet
#ifdef _WML_SUPPORT_
		WML_CSS,		///< WML style sheet
#endif
#ifdef CSS_MATHML_STYLESHEET
		MathML_CSS,		///< MathML stylesheet
#endif // CSS_MATHML_STYLESHEET
#ifdef SUPPORT_VISUAL_ADBLOCK
		ContentBlockCSS,	///< css file used in content block edit mode
#endif
		LocalCSS,		///< User's local style sheet
		FirstUserStyle  ///< Other user stylesheets
	};

	CSSManager();
	void ConstructL();
	virtual ~CSSManager();

	/**
	 * Retrieve the parsed style sheet.
	 *
	 * @param which The type of style sheet to retrieve.
	 * @return The parsed style sheet. NULL if none.
	 */
	CSS* GetCSS(unsigned int which) { return m_localcss[which].css; }

	/**
	 * Load all local CSSes. Will query the preferences subsystem for the
	 * file names, and load and parse the local CSS files for the default
	 * context.
	 */
	void LoadLocalCSSL();

#if defined LOCAL_CSS_FILES_SUPPORT
	/**
	 * Update the enabled/disabled status of the extra user stylesheets
	 * based on prefs. Call this when the prefs are changed.
	 */
	void UpdateActiveUserStyles();
#endif // LOCAL_CSS_FILES_SUPPORT

	/**
	 * Gets a signal that a file preference has changed, so that CSSManager
	 * can reload the appropriate files.
	 *
	 * @param which The file that has changed.
	 * @param what File object describing the new file.
	 */
	virtual void FileChangedL(PrefsCollectionFiles::filepref which, const OpFile* what);

#ifdef LOCAL_CSS_FILES_SUPPORT
	virtual void LocalCSSChanged();
#endif

	/** Create an HTML Link element as a placeholder for a local stylesheet.
		@return A new HTML_Element of Markup::HTE_LINK type. Leaves on OOM. */
	static HTML_Element* NewLinkElementL(const uni_char* url);

	/** Load css from a file and return the HTML_Element tree of dummy link
		elements for the file itself and its imports. */
	static HTML_Element* LoadCSSFileL(OpFile* file, BOOL user_defined);

	/** Load and parse a stylesheet based on the href. URL must be file: or data:. */
	static OP_STATUS LoadCSS_URL(HTML_Element* css_he, BOOL user_defined);

	/**
	 * Handle a specific action. Should be called from VisualDevice::OnInputAction.
	 * We handle the actions from style/module.actions here.
	 *
	 * @param action The action.
	 * @param handled Set to TRUE if the action was handled.
	 * @return OpStatus::OK or the LEAVE value from LoadLocalCSSL on error.
	 */
	OP_STATUS OnInputAction(OpInputAction* action, BOOL& handled);

private:

	struct LocalStylesheet
	{
		HTML_Element* elm;
		CSS* css;

		LocalStylesheet() : elm(NULL), css(NULL) {};
		~LocalStylesheet();
	};

	/** An array of LINK HTML_Element object pointers for the local stylesheets
		and their CSS objects. Host overrides are found in the CSSCollection objects
		per document. */
	LocalStylesheet* m_localcss;

#ifdef EXTENSION_SUPPORT
public:
	typedef UINTPTR ExtensionStylesheetOwnerID;
	struct ExtensionStylesheet
		: public Link, public LocalStylesheet
	{
		OpString path;
		ExtensionStylesheetOwnerID owner;
	};

private:
	/** A linked list of LINK HTML_Element object pointers for the local
	    stylesheets defined by extensions, and their CSS objects. */
	Head m_extensionstylesheets;

public:
	/**
	 * Get the user style sheets that were loaded by an extension.
	 *
	 * @return The first user style sheet from an extension.
	 */
	ExtensionStylesheet* GetExtensionUserCSS() { return reinterpret_cast<ExtensionStylesheet*>(m_extensionstylesheets.First()); }

	/**
	 * Add a user style sheet from a loaded extension.
	 *
	 * @param path The path to the extension user CSS file.
	 * @param owner The owner of this user CSS file.
	 * @return OpStatus::OK if everything went fine.
	 */
	OP_STATUS AddExtensionUserCSS(const uni_char* path, ExtensionStylesheetOwnerID owner);

	/**
	 * Remove user style sheets that was loaded by an extension.
	 *
	 * @param owner The owner to remove all user style sheets from.
	 */
	void RemoveExtensionUserCSSes(ExtensionStylesheetOwnerID owner);
#endif
};

#endif // CSSMANAGER_H
