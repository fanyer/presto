/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Manuela Hutter (manuelah)
 */

#ifndef WEBSERVER_SERVICE_CONTROLLER
#define WEBSERVER_SERVICE_CONTROLLER

#ifdef GADGET_SUPPORT
#ifdef WEBSERVER_SUPPORT

#include "modules/windowcommander/OpTransferManager.h"
#include "adjunct/quick/hotlist/HotlistModelItem.h"
#include "adjunct/quick/webserver/controller/WebServerServiceSettingsContext.h"
#include "adjunct/quick/webserver/controller/WebServerServiceDownloadContext.h"

class UniteServiceModelItem;
class URLInformation;
class WebServerServiceSettingsContext;
class OpTreeView;


/***********************************************************************************
 **  @class	WebServerServiceController
 **	@brief	Controller that handles all actions the dialog needs to perform (ie. Install)
 ************************************************************************************/
class WebServerServiceController
{
public:
	static WebServerServiceSettingsContext*	CreateServiceContextFromPath(const OpStringC & file_path, const OpStringC & remote_location);
	static WebServerServiceSettingsContext*	CreateServiceContextFromHotlistItem(UniteServiceModelItem * item);

	WebServerServiceController();
	virtual ~WebServerServiceController();

	OP_STATUS	ConfigurePreinstalledService(UniteServiceModelItem * item);

	OP_STATUS	ChangeServiceSettings(const WebServerServiceSettingsContext & service_context);

	BOOL		IsValidServiceNameInURL(const OpStringC & service_address);
	BOOL		IsValidTrustedServer(const OpStringC & server_url);
	void		AddTrustedServer(const OpStringC & server_url);

	BOOL		IsServicePreInstalled(const WebServerServiceSettingsContext & service_context);

#ifdef SELFTEST
	OpHashTable*	GetItemsListenersTable() { return &m_items_listeners_table; }
#endif // SELFTEST

	// call to ensure the service name (used in the URL) is unique among all services
	BOOL				IsServiceNameUnique(const OpStringC& current_service_identifier, const OpStringC& service_name_in_url);

protected:
	OpTreeView *		GetUniteItemsTreeView();

private:
	enum VersionType
	{
		NoVersionFound,
		NewerVersion,
		SameVersion,
		OlderVersion,
		OlderVersionUnconfigured
	};

	enum InstallType
	{
		InstallTypeNormal,
		InstallTypeUpgrade,
		InstallTypeSaveOnly
	};

	static OP_STATUS	GetDowloadLocationForURL(const URL & url, OpString & location);
	OP_STATUS			MoveServiceToDestination(WebServerServiceSettingsContext & service_context, BOOL copy_only = FALSE);
	void				GetUpgradableGadgets(OpVector<GadgetModelItem>& upgradable_gadgets, const WebServerServiceSettingsContext & service_context);
	VersionType			IsServiceInstalled(const WebServerServiceSettingsContext & service_context);
	OpHashTable	m_items_listeners_table; // table that holds download items and their ServiceDownloadListeners

	OpAutoVector<WebServerServiceDownloadContext> m_pending_installs;
};

#endif // WEBSERVER_SUPPORT
#endif // GADGET_SUPPORT

#endif // WEBSERVER_SERVICE_CONTROLLER
