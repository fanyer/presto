/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Adam Minchinton, Michal Zajaczkowski
*/

#ifndef _ST_AUTOUPDATEXML_H_INCLUDED_
#define _ST_AUTOUPDATEXML_H_INCLUDED_

#ifdef AUTO_UPDATE_SUPPORT
#ifdef SELFTEST

#include "adjunct/autoupdate/autoupdatexml.h"

/**
 * The ST_AutoUpdateXML class extends the AutoUpdateXML in order to give access to functionality that the selftests need,
 * mainly getting/setting the internal data of the AutoUpdateXML class.
 */
class ST_AutoUpdateXML : public AutoUpdateXML
{
public:
	ST_AutoUpdateXML();
	/**
	 * Used to override the version value sent to the server.
	 *
	 * @param version to set
	 * @return OK if the value is successfully set.
	*/
	OP_STATUS SetVersion(const OpStringC8 &version);

	/**
	 * Used to override the bulid number value sent to the server.
	 *
	 * @param build number to set
	 * @return OK if the value is successfully set.
	*/
	OP_STATUS SetBuildNum(const OpStringC8 &buildnum);

	OP_STATUS GetVersion(OpString& version) { return version.Set(m_opera_version); }
	OP_STATUS GetBuildNum(OpString& buildnum) { return buildnum.Set(m_build_number); }

	/**
	 * Used to override the language value sent to the server. 
	 *
	 * @param language to set (2 char code)
	 * @return OK if the value is successfully set.
	*/
	OP_STATUS SetLang(const OpStringC8 &lang) { return m_language.Set(lang); }

	/**
	 * Used to override the edition value sent to the server. 
	 *
	 * @param Special edition to set
	 * @return OK if the value is successfully set.
	*/
	OP_STATUS SetEdition(const OpStringC8 &ed) { return m_edition.Set(ed); }

	/**
	 * Used to override the operating system name value sent to the server. 
	 * (defined by http://www.opera.com/download/index.dml?custom=yes)
	 *
	 * @param operating system name to set
	 * @return OK if the value is successfully set.
	*/
	OP_STATUS SetOSName(const OpStringC8 &os_name) { return m_os_name.Set(os_name); }

	/**
	 * Used to override the operating system version value sent to the server. 
	 *
	 * @param operating system version to set
	 * @return OK if the value is successfully set.
	*/
	OP_STATUS SetOSVersion(const OpStringC8 &os_version) { return m_os_version.Set(os_version); }

	virtual int GetTimeStamp(TimeStampItem item);
	virtual OP_STATUS SetTimeStamp(TimeStampItem item, int timestamp);

protected:
	int m_overrided_timestamps[TSI_LAST_ITEM];
};

#endif // SELFTEST
#endif // AUTO_UPDATE_SUPPORT

#endif // _ST_AUTOUPDATEXML_H_INCLUDED_
