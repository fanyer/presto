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
 * @file prefsfileloader.h
 * Object administrating the downloading of preference utility files.
 *
 * The classes in this file are only for internal use in the prefsload submodule.  
 * They should not be used outside of the module. The result would be unpredictable.
 * Interact with the submodule only through g_PrefsLoadManager.
 */

#if !defined PREFSFILELOADER_H && defined PREFS_FILE_DOWNLOAD
#define PREFSFILELOADER_H

#include "modules/prefsloader/ploader.h"
#include "modules/url/url2.h"
#include "modules/util/opfile/opfile.h"

/**
 * A loading unit for preference utility files: files referred to in .ini files.
 */
class PrefsFileLoader : public PLoader
{
public:
    /** Construct loader object for a data file mentioned in the preferences 
     *  
     *  Use this when parsing of site specific preferences has reached an element of type FILE.
     *
     *  @param host    The host name of the site that needs this preference.
     *  @param section The prefs section where the file is mentioned.
     *  @param pref    The preference that referres to the file.
     *  @param from    Where to find the file on the server side.
     *  @param to      Where to put the file on the client side.
	 *  @return        OK if initialization was successful. 
     */
	OP_STATUS Construct(const OpStringC &host, const OpStringC8 &section, const OpStringC8 &pref, const OpStringC &from, const OpStringC &to);
    
     /** Start loading the file
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

	// Loading data

    /** 
     *  The name of the host being overridden.
     */
    OpString             m_host;

    /** 
     *  The section of the overridden preference.
     */
	OpString8            m_section;

    /** 
     *  The name of the overridden preference.
     */
    OpString8            m_pref;

    /** 
     *  The value of the overridden preference, which is a local file.
     */
    OpFile               m_file;

    /** 
     *  The url that is the source of the file being downloaded.
     */
    URL_InUse            m_fileURL;

    /** 
     *  Flag indicating that the preference will get a new value.
     */
    BOOL                 m_setPref;

    /** 
     *  The downloading is finished. Close and dismantle.
     */
	OP_STATUS   FinishLoading(int url_id);

    /** 
     *  Data is received. Do what you want with it.
     */
	void        LoadData(int url_id);

};

#endif
