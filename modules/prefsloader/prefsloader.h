/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2004-2007 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Tord Akerbæk
*/

/**
 * @file prefsloader.h
 * Object administrating the downloading of preferences.
 *
 * The classes in this file are only for internal use in the prefsload submodule.
 * They should not be used outside of the module. The result would be unpredictable.
 * Interact with the submodule only through g_PrefsLoadManager.
 */

#if !defined PREFSLOADER_H && defined PREFS_DOWNLOAD
#define PREFSLOADER_H

#include "modules/prefsloader/ploader.h"

#include "modules/xmlutils/xmlparser.h"
#include "modules/xmlutils/xmltoken.h"
#include "modules/xmlutils/xmltokenhandler.h"

/**
 * A loading unit for xml preference data.
 */
class PrefsLoader : public PLoader, public XMLTokenHandler, public XMLParser::Listener
{
public:
	PrefsLoader(OpEndChecker *end);
	virtual ~PrefsLoader();

    /** Construct preference loader object from open URL connection
     *
     *  Use this when the preference data is reached
     *  from a link in a web page.
     *
     *  @param url     Open url to xml preference data.
     *  @param end     Callback object, if you want to interfere with the parsing.
     *  @return        OK if nothing failed during initialization
     */
	OP_STATUS Construct(URL url);

    /** Construct loader object with predefined Opera preference source
     *
     *  Use this when loading the preferences is initiated from the user interface. The Constructor will
     *  open an URL to a prefs server with a query argument containing the requested host name.
     *
     *  @param host    The hostname of the site for which the preference data will be valid.
     *  @param end     Callback object, if you want to interfere with the parsing.
     *  @return        OK if nothing failed during initialization
     */
	OP_STATUS Construct(const OpStringC &host);

    /** Start loading preference data
     *
     *  Start loading data from the preference server. If the constructor was provided with an EndChecker object, an initial
     *  parsing is done to extract the host names of the loaded preferences. These are passed as an argument to the endchecker.
     *
     *  @return        OK if nothing failed
     */
	OP_STATUS StartLoading();

    /** Callback for data received on the url
     *
     *  Implementing  MessageObject method.
     *
     */
	virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

private:
	typedef enum
	{
		ELM_UNKNOWN,
		ELM_TOP,
		ELM_PREFERENCES, // XML root element
		ELM_HOST,        // Identify a host with ssp
		ELM_HOSTNAMES,   // deprecated
		ELM_SECTION,     // Opera.ini preference section
		ELM_PREF,        // A single preference with string or integer argument
		ELM_FILE,        // A preference with a file name argument
		ELM_DELETE_FILE  // A utility file that has become obsolete.
	} ElementType;

	typedef enum
	{
		ATT_UNKNOWN,
		ATT_NAME,       // Name of host, section or preference
		ATT_VALUE,      // Value of preference
		ATT_FROM,       // Url from where to download a utility file: .css, .jss etc
		ATT_TO,         // Synonym to value, used by ELM_FILE, local file name of preference utility file
		ATT_CLEAN,      // An instruction to remove all non-user preferences on a host
		ATT_CLEAN_ALL,  // An instruction to remove all non-user preferences on all hosts
	} AttributeType;

	// Loading data

    /**
	 * The url from where preference data is downloaded
	 */
	URL_InUse            m_serverURL;

    /**
	 * Data descriptor containing the preference data before parsing
	 */
    URL_DataDescriptor * m_descriptor;

	// Parsing data
	XMLParser          * m_prl_parser;
	BOOL                 m_parse_error;
	ElementType          m_elm_type[8];
	int                  m_cur_level;

    /**
	 * The number of times we have been running the parsing. First time is for extracting host names.
	 * Second time is for reading preferences.
	 */
    int                  m_parseno;

    /**
	 * The name of the .ini preference section being parsed : User Prefs, Multimedia, etc.
	 */
	OpString8            m_current_section;

    /**
	 * The name of the site being overridden.
	 */
    OpString             m_current_host;

    /**
	 * An aggregate of all hostnames in the preference listing.
	 * Used for user interaction.
	 */
	OpString             m_hostnames;

    /**
     * TRUE if primary parsing found a clean_all attribute; this means that
     * all existing overridden hostnames will also be affected by the update
     * in addition to the hosts that preliminary parsing collected in
     * the m_hostnames string.
     */
    BOOL		m_clean_all_update;

    /**
	 * A callback wich will check whether to cancel the preference installation
	 */
	OpEndChecker       * m_checker;

    /**
	 * A flag set if the parsing is blocked (to avoid interference from recursive calls)
	 */
    BOOL                 m_parse_blocked;

	OP_STATUS     FinishLoading(URL_ID url_id);
	void          LoadData(URL_ID url_id);

	// Parsing methods
	BOOL          ParseResponse(const uni_char *buffer, int length, BOOL more);
	ElementType   GetElmType(const uni_char *name, int name_len);
	AttributeType GetAttType(XMLToken::Attribute *att);

    /**
	 * Checks the preference name against a hardcoded list of preferences.
	 * We will only update preferences mentioned in this list, and we will
     * not store this list in any data file outside the source.
     *
     * @return TRUE if the pref is accepted for download.
	 */
    BOOL          IsAcceptedPref(XMLToken::Attribute *att);

    /**
	 * Checks the suggested file name for occurences of "..",
     * to avoid saving the file in a location outside of the download directory.
     *
     * @return TRUE if the file name is in a safe location.
	 */
    BOOL          IsAcceptedFile(OpString &name);

	virtual Result HandleToken(XMLToken &token);
	virtual void  Continue(XMLParser *) {}
	virtual void  Stopped(XMLParser *) {};
   	void          HandleStartElement(const uni_char *name, int name_len, XMLToken::Attribute *atts, int atts_len);
	void          HandleEndElement(const uni_char *name, int name_len);
};

#endif
