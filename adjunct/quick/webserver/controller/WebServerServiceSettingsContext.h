/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Manuela Hutter (manuelah)
 */

#ifndef WEBSERVER_SERVICE_SETTINGS_CONTEXT
#define WEBSERVER_SERVICE_SETTINGS_CONTEXT

#ifdef GADGET_SUPPORT
#ifdef WEBSERVER_SUPPORT

#include "adjunct/quick/windows/DesktopGadget.h"

class OpGadgetClass;
class OpGadget;

/*
** Type of folder hint
*/
enum FolderHintType
{
	FolderHintPictures,
	FolderHintMusic,
	FolderHintVideo,
	FolderHintDocuments,
	FolderHintDownloads, 
	FolderHintDesktop,
	FolderHintDefault,
	FolderHintPublic,
	FolderHintNone
};

enum UniteAppInstallationType
{
	LocalPackaged,	// drag & drop of a unite application stored locally. app should be copied to the widgets directory.
	LocalConfigXML,	// drag & drop of a config.xml file. app should be started from the location of the config.xml. no copying/moving.
	RemoteURL		// installation from a URL
};

#ifdef GADGETS_CAP_HAS_FEATURES
/***********************************************************************************
**  @class	WebServerServiceFolderHintTranslator
**	@brief	Transforms a folder hint structure into a platform path.
************************************************************************************/
class WebServerServiceFolderHintTranslator
{
public:
	WebServerServiceFolderHintTranslator(OpFolderManager * folder_manager);

	OP_STATUS			SetServiceClass(OpGadgetClass * service_class);
	const OpString &	GetFolderHintPath() const;
	FolderHintType		GetFolderHintType() const;

private:
	OP_STATUS			SetFolderPath(OpFileFolder folder);

	OpFolderManager *	m_folder_manager;
	OpString			m_folder_hint;
	FolderHintType		m_folder_hint_type;
};
#endif //  GADGETS_CAP_HAS_FEATURES

/***********************************************************************************
**  @class	WebServerServiceSettingsContext
**	@brief	Service context. ie. the name of the widget, its icon, ..
************************************************************************************/
class WebServerServiceSettingsContext
{
public:

	static WebServerServiceSettingsContext * CreateContextFromService(OpGadget * service_class);
	static WebServerServiceSettingsContext * CreateContextFromServiceClass(OpGadgetClass * service_class);

	WebServerServiceSettingsContext();

	// Getters
	const OpStringC &	GetFriendlyName() const;
	const OpStringC &	GetServiceNameInURL() const;
	const OpStringC &	GetServiceDescription() const;
	const Image &		GetServiceImage() const;
	FolderHintType		GetSharedFolderHintType() const;
	const OpStringC &	GetSharedFolderHint() const;
	const OpStringC &	GetSharedFolderPath() const;
	const OpStringC &	GetRemoteDownloadLocation() const;
	const OpStringC &	GetWidgetID() const;
	const OpStringC &	GetLocalDownloadPath() const;
	const OpStringC &   GetServiceIdentifier() const;
	const GadgetVersion & GetVersion() const { return m_version; }
	INT32				GetHotlistID() const { return m_hotlist_id; }
	GadgetInstallationType	GetInstallationType() const { return m_installation_type; };
	BOOL                GetIsPreinstalledService() const { return m_is_preinstalled; } 

#ifdef WEBSERVER_CAP_SET_VISIBILITY
	BOOL				IsVisibleASD() const		{ return m_asd_visibility; }
	BOOL				IsVisibleUPNP() const		{ return m_upnp_visibility; }
	BOOL				IsVisibleRobots() const		{ return m_robots_visibility; }
#endif //WEBSERVER_CAP_SET_VISIBILITY

	// Setters
	OP_STATUS			SetFriendlyName(const OpStringC & friendly_name);
	OP_STATUS			SetServiceNameInURL(const OpStringC & service_name_in_url);
	OP_STATUS			SetServiceDescription(const OpStringC & service_description);
	OP_STATUS			SetServiceImage(const Image & service_image);
	void				SetSharedFolderHintType(FolderHintType shared_folder_hint_type);
	OP_STATUS			SetSharedFolderHint(const OpStringC & shared_folder_hint);
	OP_STATUS			SetSharedFolderPath(const OpStringC & shared_folder_path);
	OP_STATUS			SetRemoteDownloadLocation(const OpStringC & download_url);
	OP_STATUS			SetWidgetID(const OpStringC & widget_id);
	OP_STATUS			SetLocalDownloadPath(const OpStringC & local_path);
	OP_STATUS           SetServiceIdentifier(const OpStringC& service_id);
	OP_STATUS			SetVersionString(const OpStringC& version_str) { return m_version.Set(version_str); }
	void				SetHotlistID(INT32 hotlist_id) { m_hotlist_id = hotlist_id; }
	void				SetInstallationType(GadgetInstallationType installation_type) { m_installation_type = installation_type; }
	void                SetIsPreinstalledService() { m_is_preinstalled = TRUE;}
	
#ifdef WEBSERVER_CAP_SET_VISIBILITY
	void				SetVisibleASD(BOOL asd_visibility) { m_asd_visibility = asd_visibility; }
	void				SetVisibleUPNP(BOOL upnp_visibility) { m_upnp_visibility = upnp_visibility; }
	void				SetVisibleRobots(BOOL robots_visibility) { m_robots_visibility = robots_visibility; }
#endif // WEBSERVER_CAP_SET_VISIBILITY

private:
	OpString		m_friendly_name;			// The "friendly" name as shown in the panel
	OpString		m_service_name_in_url;		// The service name shown in the URL
	OpString		m_service_description;		// description from config.xml
	Image			m_service_image;			// the image of the service (32x32 pixels version)
	FolderHintType m_shared_folder_hint_type; // type of shared folder
	OpString		m_shared_folder_hint;		// reading the "folderhint" feature from the gadget
	OpString		m_shared_folder_path;		// the path the widget can read/write information to
	OpString		m_download_url;				// where the service is downloaded from 
	OpString		m_widget_id;				// the id of the widget as specified in http://www.w3.org/TR/widgets/#the-widget-element, used for checking for duplicates on installation
	OpString		m_local_path;				// the path the service is saved to locally (ie. the profile/widgets directory)
	OpString		m_service_identifier;		// to identify a service in the gadgets manager
	GadgetVersion	m_version;					/// the gadget version
	INT32			m_hotlist_id;				// ID of the gadget in the hotlist model
	GadgetInstallationType	m_installation_type;
	BOOL			m_is_preinstalled; 

#ifdef WEBSERVER_CAP_SET_VISIBILITY
	BOOL	m_asd_visibility;
	BOOL	m_upnp_visibility;
	BOOL	m_robots_visibility;
#endif // WEBSERVER_CAP_SET_VISIBILITY
};

#endif // WEBSERVER_SUPPORT
#endif // GADGET_SUPPORT

#endif // WEBSERVER_SERVICE_SETTINGS_CONTEXT
