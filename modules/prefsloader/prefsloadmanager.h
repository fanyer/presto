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
 * @file prefsloadmanager.h
 * Framework for preference downloading.
 *
 * The classes in this file constitutes the interface of the prefsload submodule. No other classes defined 
 * in the submodule should be used outside of the module. The result would be unpredictable.
 * Interact with the class only through g_PrefsLoadManager.
 */

#if !defined PREFSLOADMANAGER_H && defined PREFS_DOWNLOAD
#define PREFSLOADMANAGER_H

#include "modules/url/url2.h"

/** Global PrefsLoadManager object (singleton). */
#define g_PrefsLoadManager (g_opera->prefsloader_module.PrefsLoadManager())

/**
 * Pure virtual class that provides the xml-parser with an external interupt.
 * Implement this to stop the xml parsing at a certain pattern.
 */
class OpEndChecker
{
public:
	/** Standard destructor. Needs to be virtual due to inheritance. */
	virtual ~OpEndChecker() {}

    /** End callback
     *
     *  Will be called when parser has received information of the scope of the preferences.
     *  Implement this callback to signal that the parsing of xml preferences should stop.
     *  The implementation will typically be some kind of user dialog.
     *
     *  @param info     A list of host names separated by ' '.
     *  @return         TRUE if the parsing should stop and no more preferences installed.
     */
	virtual BOOL IsEnd(OpStringC info) = 0;

    /** Dispose callback 
     * 
     *  Called when parsing is finished.
     *  Implement this callback to get rid of the object. Typically delete it.
     */
	virtual void Dispose() = 0;
};

/**
 * Manage loading preferences and utility files from preference server.
 */
class PrefsLoadManager
{
public:
    /** Init preference loading from URL
     *  
     *  Initiate loading preference data from an already open URL. Use this when the preference data is reached 
     *  from a link in a web page.
     *
     *  @param url     Open url to xml preference data.
     *  @param end     Callback object, if you want to interfere with the parsing.
     */
	OP_STATUS InitLoader(const URL url, OpEndChecker * end);

    /** Init preference loading from preference server
     *  
     *  Initiate loading preference data for a given host. Use this when the preference loading
     *  is initiated from the Opera UI. The url of the prefs server is found in the preferences.
     *
     *  @param host    The hostname of the site for wich the preference data will be valid.
     *  @param end     Callback object, if you want to interfere with the parsing.
     */
	OP_STATUS InitLoader(const OpStringC &host, OpEndChecker * end = NULL);

    /** 
     *  Remove all preference loading data.
     */
	void Cleanup(BOOL force = TRUE);

    friend class PrefsLoader;

protected:
#ifdef PREFS_FILE_DOWNLOAD
    /** Init file loading from preference server
     *  
     *  Initiate loading a file from the prefs server.
     *  Used when the file name is found in the xml preference data. May be js or css file.
     *
     *  @param host    The hostname of the site for which the file should be used.
     *  @param section The prefs section where the file is mentioned.
     *  @param pref    The preference that referres the file.
     *  @param from    The server side path of the file.
     *  @param to      The client side path of the file.
	 *  @return        OK if initialization was successful.
     */
    OP_STATUS InitFileLoader(const OpStringC &host, const OpStringC8 &section, const OpStringC8 &pref, const OpStringC &from, const OpStringC &to);
#endif

private:
	Head m_activeLoaders;
};

#endif
