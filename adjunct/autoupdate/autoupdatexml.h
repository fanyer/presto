/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Michal Zajaczkowski
 */

#ifdef AUTO_UPDATE_SUPPORT

#ifndef AUTOUPDATE_XML_H
#define AUTOUPDATE_XML_H

#include "modules/util/opstrlst.h"
#include "modules/xmlutils/xmlfragment.h"
#include "adjunct/autoupdate/additionchecker.h"

class AdditionCheckerItem;

/**
 * This was formerly part of the AutoUpdateURL class.
 * The AutoUpdateXML class serves the purpose of generating the autoupdate request XML. This is the only way the request XML should be generated, 
 * this class should contain all the logic needed to construct a valid XML request for the needed request type.
 *
 * Since the autoupdate server does two tasks that are quite different, i.e. serving browser and resources updates and resolving additions, the logic
 * used to generate the request XML is a bit complicated.
 * If you want to request a browser update XML, you expect to get back a browser package, if a newer one is available on the server, as well as resources
 * update. Use update levels 1 to 3 for this purpose.
 * If you want to request an addition check XML, use the update level that is proper for the given addition type, i.e. from the level 4 up.
 * Additionally, if you want to request an addition check XML, you need to specify addition items that will be encoded in the XML, i.e. a list of plugin
 * with their mimetypes. These need to be added to the AutoUpdateXML internal list using the AddRequestItem() method before a call to GetRequestXML() is made.
 *
 * Since this class is meant to exist in one copy held by the AutoUpdateManager, you should always call ClearRequestItems() before requesting a new XML,
 * especially before adding new request items.
 *
 * Note that this class should be used internally by the autoupdate module and you shouldn't use it directly.
 *
 */
class AutoUpdateXML
{
public:
	/*
	 * _DO_NOT_CHANGE_ the values for this enum, they represent the numerical update levels recognized by the autoupdate server.
	 * In case you want to add a new addition type, you need to agree that with the core services team and append a new update
	 * level here.
	 * Also, the AdjustUpdateLevel() method should be aligned with the changes, as well as the actual XML generation algorithm, see AppendXMLOpera().
	 *
	 * A value from this enum is used as a parameter to the GetRequestXML() calls.
	 *
	 * The actual behaviour of the autoupdate server may vary due to server changes.
	 **/
	enum AutoUpdateLevel {
		/**
		 * In case the GetRequestXML() method is called with the value UpdateLevelChooseAuto (default parameter value), this class attempts to recognize
		 * the needed update level automatically. This should work fine most of the time.
		 */
		UpdateLevelChooseAuto			= 0,
		/**
		 * Request browser update only, no resources (browser.js, override_downloaded.ini, dictionaries.xml, language files, ...).
		 */
		UpdateLevelUpgradeCheck			= 1,
		/**
		 * Request resources update only (browser.js, ...). NOTE, that this has nothing to do with additions, however installed language files
		 * will be tracked as resources and will be updated with update levels 1 and 2.
		 */
		UpdateLevelResourceCheck		= 2,
		/**
		 * This update level means requesting an update of the browser package, if any is available, as well as updates to the resources tracked by the
		 * autoupdate mechanism (browser.js, ...)
		 * UpdateLevelDefaultCheck is the level that is chosen when the request XML is generated with the UpdateLevelChooseAuto value and no request items
		 * have been added to the request.
		 */
		UpdateLevelDefaultCheck			= 3,
		/**
		 * Used to resolve language files (dictionaries). This level is used to check availablity of language files that we want to install.
		 */
		UpdateLevelInstallDictionaries	= 4,
		/**
		 * Used to resolve plugin files that we want to install.
		 */
		UpdateLevelInstallPlugins		= 5,
		/**
		 * Used to get country code for computer's IP address.
		 */
		UpdateLevelCountryCheck			= 6,
	};

	// Introduced by DSK-344553, we need to distinguish XML requests sent for the "main", periodical update checks, and any other requests.
	enum RequestType {
		// Request sent from the main autoupdate mechanism
		RT_Main,
		// Any other request (additions, VersionChecker, ...)
		RT_Other
	};

	enum TimeStampItem
	{
		TSI_BrowserJS,
		TSI_OverrideDownloaded,
		TSI_DictionariesXML,
		TSI_HardwareBlocklist,
		TSI_HandlersIgnore,
		TSI_LastUpdateCheck,

		TSI_LAST_ITEM
	};

	AutoUpdateXML();
	~AutoUpdateXML();

	/**
	 * Needs to be called before any operation on the AutoUpdateXML class is done. Initializes internal data and needs to succeed.
	 *
	 * @return OpStatus::OK if everything went OK, ERR otherwise.
	 */
	OP_STATUS Init();

	/**
	 * In case you want to generate an autoupdate request XML containing description of additions that you want to resolve, you need
	 * to pass the addition items to this class. Append as many addition items as you like before calling GetRequestXML(), make sure
	 * to ClearRequestItems() before you start adding new items in order to not to mix old items with the new ones.
	 *
	 * Since the autoupdate server can only handle on type of additions per one XML request (according to the update level chosen), you
	 * cannot mix different addition types with one call to GetRequestXML(), therefore you cannot mix the types while adding them here
	 * or the call to GetRequestXML() will fail.
	 *
	 * @param item - the addition checker item that will be added to the item list used by the next call to GetRequestXML(). Note that
	 *               this class doesn't take the ownership of the addition checker items passed with this call.
	 *
	 * @return - OpStatus::OK if adding the new addition item succeeded, ERR otherwise.
	 */
	OP_STATUS AddRequestItem(AdditionCheckerItem* item);

	/**
	 * Clears the current addition item list. Needs to be called before new items are added.
	 */
	void	ClearRequestItems();

	/**
	 * Construct and return the autoupdate request XML.
	 * If you want the XML for update levels 1-3, make sure the addition items list is empty before calling this method (use ClearRequestItems()), and 
	 * set the update level manually via the level parameter, or use the UpdateLevelChooseAuto value to use the default level 3.
	 * If you want the XML for addition check, add addition items using AddRequestItem() and use UpdateLevelChooseAuto in order to leave choosing the proper
	 * update level to this class internal logic (see AdjustUpdateLevel()).
	 *
	 * @param xml_string - reference to an OpString8 object that will receive the constructed request XML that should be sent directly to the autoupdate server.
	 * @param level - autoupdate level that should be used to construct the XML. Should be left with the default value UpdateLevelChooseAuto unless you want to
	 *                explicitly choose a different level between 1 and 3.
	 * @param request_type - see DSK-344553, this should be set correctly to distinguish regular periodic update checks from any other XML requests.
	 * @param used_level - On exit contains the update lavel determined and used to create the request XML.
	 *
	 * @return - OpStatus::OK if everything went OK, ERR if the addition items are not consistent according to their type, the update level does not fit the
	 *           addition items or if any other problem occurred.
	 */
	OP_STATUS GetRequestXML(OpString8 &xml_string, AutoUpdateLevel level = UpdateLevelChooseAuto, RequestType request_type = RT_Other, AutoUpdateLevel* used_level = NULL);

	/**
	 * This method has been introduced as a result of DSK-336588.
	 * If any of the resource timestamps are set to 0, we need to trigger an immediate resource update check after the browser starts.
	 * The browserjs timestamp will be set 0 to browser upgrade.
	 * Note that AutoUpdateXML::GetTimeStamp(TSI_HandlersIgnore) seems to return 0 even after
	 * an update check, perhaps because we don't always get the resource it describes.
	 * Note that if the resources list found in AutoUpdateXML::NeedsResourceCheck() includes
	 * resources that don't get updated with the update check, we might end up sending the
	 * resource check each and every time Opera starts.
	 *
	 * @return - TRUE if a resource update check is needed, FALSE otherwise.
	 */
	BOOL	NeedsResourceCheck();

protected:
	OP_STATUS SetPref(int which, int value);

	virtual int GetTimeStamp(TimeStampItem item);

	/**
	 * Internal information collected by the Collect*() and UpdateFragileData() methods.
	 * These might be overriden by the ST_AutoUpdateXML class.
	 *
	 * See also the EnsureStringsHaveBuffers() method.
	 */
	OpString	m_opera_version;	// Version of the currently running Opera application
	OpString	m_build_number;		// Build number of the currently running Opera application
	OpString	m_language;			// Language (code) of the currently running Opera application
	OpString	m_edition;			// Special build edition (e.g. t-online) ([ISP] Id=)
	OpString	m_os_name;			// Name of the operating system, i.e. Windows, MacOS, Linux, FreeBSD, as defined by http://www.opera.com/download/index.dml?custom=yes
	OpString	m_os_version;		// Operating system version (e.g. 10.4.3, 5.1)
	OpString	m_architecture;		// Architecture, i386/ppc ...
	OpString	m_package_type;		// Type of package installed (*nix only), i.e. static, shared, gcc3, gcc4
	OpString	m_gcc_version;		// gcc version (*nix only)
	OpString	m_qt_version;		// QT version (*nix only)
	int			m_timestamp_browser_js;					// Timestamp of the current browser.js file
	int			m_timestamp_override_downloaded_ini;	// Timestamp of the current override_downloaded.ini (Site Prefs) file
	int			m_timestamp_dictionaries_xml;			// Timestamp of the current dictionaries.xml file
	int			m_timestamp_hardware_blocklist;			// Timestamp of the current hardware blocklist file (newest if several)
	int			m_timestamp_handlers_ignore;			// Timestamp of the current handlers-ignore.ini file
	int			m_time_since_last_check;				// Time since last check (i.e. current_time - last_update_check_time). This should not be affected by addition requests.
	int			m_download_all_snapshots;				// Set to download all snapshots

	/**
	 * Addition checker items that will be resolved against the autoupdate server are stored here.
	 */
	OpVector<AdditionCheckerItem>	m_items;

private:
	// Autoupdate debugging in the live client-server environment needs marking some flags
	// These are treated as bit fields!
	enum DirtyFlag {
		// The Time Since Last Check calculated to be negative
		DF_TSLC = 0x01,
		// operaprefs.ini could not be written into (or: could not save a pref value)
		DF_OPERAPREFS = 0x02,
		// First Used Version was empty and we tried to change it to "Old Installation"
		DF_FIRSTRUNVER = 0x04,
		// First Run Timestamp was 0 and we tried to set it to 1
		DF_FIRSTRUNTS = 0x08,
		// The path of the disk image of the running binary starts with what is the system temp dir, for example.
		// the binary was loaded from "c:\tmp\VsourcesDebug\Opera.exe".
		// If this flag is set then we probably have problems related to DSK-345746, DSK-345431.
		DF_RUNFROMTEMP = 0x10
	};

	/**
	 * The current autoupdate XML schema version, i.e. "1.0".
	 */
	static const uni_char* AUTOUPDATE_SCHEMA_VERSION;
	/**
	 * If the opera:config#AutoUpdate|SaveAutoUpdateXML preference is set, the request XML is saved as a file with the name and folder specified by the
	 * following two consts.
	 */
	static const uni_char* AUTOUPDATE_REQUEST_FILE_NAME;
	static const OpFileFolder AUTOUPDATE_REQUEST_FILE_FOLDER;

	/**
	 * Chooses the update level according to the current content of the addition items list contained in the m_items vector.
	 * Checks the contents of the m_items vector and verifies it agains the update level passed with the call.
	 * Adjusts the update level in case the passed level is 0 (UpdateLevelChooseAuto).
	 *
	 * @param level - The update level that we want to adjust and/or verify. In case the passed level value is UpdateLevelChooseAuto, it gets
	 *                adjusted to a correct level basing on the contents of the m_items vector. In case the passed level value is anything else,
	 *                it gets verified against the contents of the vector.
	 *
	 * @returns - OpStatus::OK in case the level was adjusted/verified succesfully.
	 * 
	 */
	OP_STATUS	AdjustUpdateLevel(AutoUpdateLevel& level);

	/**
	 * Called during Init(), collects the platform data that is not going to change, i.e. OS name, version, computer architecture and so.
	 *
	 * @returns OpStatus::OK if everything went OK, ERR otherwise.
	 */
	OP_STATUS	CollectPlatformData();

	/**
	 * Called during Init(), collects the build data that is not going to change, i.e. Opera version, build number and so. 
	 *
	 * @returns OpStatus::OK if everything went OK, ERR otherwise.
	 */
	OP_STATUS	CollectBuildData();

	/**
	 * Verifies the existence of the resource files (currently browser.js, override_downloaded.ini, dictionaries.xml). In case any of the files
	 * doesn't exist in the filesystem, the corresponding preference representing the timestamp of that resource is set to 0.
	 * The preferences are: PrefsCollectionUI::BrowserJSTime, PrefsCollectionUI::SpoofTime, PrefsCollectionUI::DictionaryTime, PrefsCollectionUI::HardwareBlocklistTime, PrefsCollectionUI::HandlersIgnoreTime.
	 * The preference values are set the value received from the autoupdate server during updating of the resources.
	 *
	 * @returns - OpStatus::OK if everything went OK, ERR if it was not possible to set a new value of any of the preferences.
	 */
	OP_STATUS	CheckResourceFiles();

	/**
	 * Update timestamps: resource timestamps, time since last check. Needs to be called each time before the request XML is generated since the 
	 * timestamps WILL change over time.
	 *
	 * @returns - OpStatus:OK if everything went OK, ERR otherwise.
	 */
	OP_STATUS	UpdateFragileData();

	/**
	 * Helper methods to generate the actual XML. Since this will change when new types are added, you should rememeber one thing mainly: don't add
	 * XML representing additions for update levels 1-3 unless the addition is already installed and should be tracked with autoupdate. Each such
	 * case should be agreed with the core services team, see updating installed dictionaries vs not-updating installed plugins.
	 */
	OP_STATUS	AppendXMLOperatingSystem(XMLFragment& fragment, AutoUpdateLevel level);
	OP_STATUS	AppendXMLPackage(XMLFragment& fragment);
	OP_STATUS	AppendXMLOpera(XMLFragment& fragment, AutoUpdateLevel level);
	OP_STATUS	AppendXMLDictionaries(XMLFragment& fragment, AutoUpdateLevel level);
	OP_STATUS	AppendXMLPlugins(XMLFragment& fragment, AutoUpdateLevel level);
	OP_STATUS	AppendXMLExistingDictionaries(XMLFragment& fragment, AutoUpdateLevel level);
	OP_STATUS	AppendXMLNewDictionaries(XMLFragment& fragment, AutoUpdateLevel level);

	/**
	 * Generate the XML string from the XMLFragment prepared with earlier calls to the AppendXML* methods.
	 */
	OP_STATUS	EncodeXML(XMLFragment& fragment, OpString8& xml_string);

	/**
	 * If the opera:config#AutoUpdate|SaveAutoUpdateXML preference is set, the autoupdate request XML is saved to a file.
	 * See AUTOUPDATE_REQUEST_FILE_NAME, AUTOUPDATE_REQUEST_FILE_FOLDER.
	 * Additionally in debug builds the file contents is opened in a new browser tab.
	 *
	 * @returns - OpStatus:OK if everthing went OK, ERR otherwise.
	 */
	OP_STATUS	DumpXML(OpString8& xml_string);

	/**
	 * Clears all dirty flags.
	 */
	void ClearDirty();

	/**
	 * Sets the dirty flag indicated by the parameter given.
	 *
	 * @param flag - The flag to set
	 */
	void SetDirty(DirtyFlag flag);

	/**
	 * This method implements the autoupdate server "debugging" realised via the <dirty> XML element sent with the
	 * autoupdate request. Due to certain problems that are hard to track, the client tries to do its bets to detect
	 * any problems that might arise during construction of the autoupdate XML request and sets appropiate flags, that
	 * are then sent to the autoupdate server. The flags are packed into an unsingned integer and sent as a plain
	 * decimal integer, i.e. <dirty>12</dirty>.
	 * To know the flags meaning, see enum DirtyFlag above.
	 *
	 * This method goes through all the places that are suspected to cause problems and tries to check and fix thigns, 
	 * lighting up the flags. Feel free to add any checks here, however do rememeber to sync whatever you do here with
	 * the Core Operations Services team, that does maintain the autoupdate server.
	 */
	void CheckDirtyness();

	/**
	 * It is possible to have a NULL CStr() pointer from any of the OpString objects above since the
	 * platform implementations might call Empty() on the OpStrings.
	 * That is wrong since the XML code uses strlen on the given strings later on and we can expect crashes,
	 * as it happens on the Linux platform.
	 *
	 * Therefore, we make sure that there is ALWAYS a pointer allocated within the OpStrings, even if the
	 * actual string content is empty.
	 * This method checks all the strings that collect platform data for buffer presence and sets an epmty buffer
	 * in case there is no buffer.
	 *
	 * REMEMBER to add OpString properties added to this class here if you are not sure that they will have a buffer at
	 * the time of the XML construction.
	 */
	void EnsureStringsHaveBuffers();

	/**
	 * Whether Init() was called OK.
	 */
	BOOL		m_inited;

	// DSK-344456 and DSK-344460 introduce the <dirty> XML element, that has the sole purpose of 
	// telling the autoupdate server that something has gone wrong during preparation of the XML
	// autoupdate request.
	// The dirty flags are stored here.
	unsigned int	m_dirty;
};

#endif // AUTOUPDATE_XML_H
#endif // AUTO_UPDATE_SUPPORT
