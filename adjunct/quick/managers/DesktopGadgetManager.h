// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
//
// Copyright (C) 2007 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Huib Kleinhout
//

#ifndef __DESKTOPGADGET_MANAGER_H__
#define __DESKTOPGADGET_MANAGER_H__

#ifdef GADGET_SUPPORT

#include "adjunct/quick/hotlist/HotlistModel.h"
#include "adjunct/quick/managers/DesktopManager.h"

#ifdef WEBSERVER_SUPPORT
#include "adjunct/quick/webserver/controller/WebServerServiceController.h"
#endif // WEBSERVER_SUPPORT

#ifdef GADGET_UPDATE_SUPPORT
#include "modules/hardcore/timer/optimer.h"
#include "modules/windowcommander/OpWindowCommanderManager.h"
#endif //GADGET_UPDATE_SUPPORT

#define g_desktop_gadget_manager (DesktopGadgetManager::GetInstance())

class OpGadget;
class OpGadgetClass;
#ifdef GADGET_UPDATE_SUPPORT
#include "adjunct/autoupdate_gadgets/GadgetUpdateController.h"
#endif //GADGET_UPDATE_SUPPORT

#define HOME_APP_ID "http://unite.opera.com/bundled/home/"
#define HOME_APP	UNI_L("home.ua")

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

class DesktopGadgetManager 
	: public DesktopManager<DesktopGadgetManager>
#ifdef WEBSERVER_SUPPORT
	, public MessageObject
	, public DesktopWindowListener
#endif // WEBSERVER_SUPPORT
{
public:
	DesktopGadgetManager();
	~DesktopGadgetManager();

	virtual OP_STATUS		Init() { return OpStatus::OK; }

	/**
	 * Scans the address for a gadget, and tries to open it if found.
	 *
	 * @param address		Local address that potentially contains a gadget. Accepted gadget addresses are:
	 *						<somedir>/config.xml; <somedir> (and somedir contains config.xml file); <somedir>/<widgetname>.zip
	 * @return				Returns TRUE when a gadget was found and successfully opened.
	 */
    BOOL 					OpenIfGadgetFolder (const OpStringC &address, BOOL open_service_page = FALSE);
	
	BOOL                    IsUniqueServiceName(const OpStringC& service_name, GadgetModelItem* gmi);

	OP_STATUS               UninstallGadget(OpGadget* gadget);
	OP_STATUS				PlaceFileInWidgetsFolder(const OpStringC& src_path,OpString& dest_file,BOOL delete_source = TRUE);

	void					VerifyGadgetInstallations();

#ifdef WEBSERVER_SUPPORT
	OP_STATUS				AddAutoStartService(const OpStringC& service_id);
	OP_STATUS				RemoveAutoStartService(const OpStringC& service_id);
	void					StartAutoStartServices();
	void					SetRootServiceWindow(DesktopGadget *gadget);

	/**
	 *  Wraps showing gadget settings
	 */
	OP_STATUS				ShowUniteServiceSettings(HotlistModelItem* item);

	// Stops all unite services and adds them to the autostart list so they will be auto started when the webserver is restarted
	void					StopUniteServices();
	
	// Installs the root service gadget
	OP_STATUS				InstallRootService(BOOL try_force_update = FALSE);
	virtual OP_STATUS		OpenRootService(BOOL open_home_page);

	virtual void			CloseRootServiceWindow(BOOL immediately = FALSE, BOOL user_initiated = FALSE, BOOL force = TRUE);

	OP_STATUS				ConfigurePreinstalledService(UniteServiceModelItem * item);
	UniteServiceModelItem *	FindHigherOrEqualUnconfiguredVersion(const OpStringC & widget_id, const GadgetVersion & version);
	UniteServiceModelItem *	FindUniteModelItem(const OpStringC & widget_id);
#endif // WEBSERVER_SUPPORT

	// Gets an icon for a gadget at a set size or larger
	OP_STATUS				GetGadgetIcon(OpGadgetClass* op_gadget, Image &image, UINT32 max_size = 32);

	OpGadget *				GetInstalledVersionOfGadget(const OpStringC &service_path);
	virtual BOOL			IsGadgetInstalled(const OpStringC &service_path) { return GetInstalledVersionOfGadget(service_path) != NULL; }
	virtual BOOL			IsGadgetInstalledAtSameLocation(const OpStringC &service_path, BOOL unite_service);

	GadgetModelItem*		FindGadgetInstallation(const OpStringC &address, BOOL unite_service);
	GadgetInstallationType	GetGadgetInstallationTypeForPath(const OpStringC & gadget_path);
	
	/**
	*	Retrives home app gadget, uses HotList for that
	*/
	OpGadget*				GetRootGadget();

#ifdef WEBSERVER_SUPPORT
	/* From MessageObject: */
	virtual void HandleCallback (OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

	BOOL					IsUniteServicePath(const OpStringC& address); 
	virtual INT32			GetRunningServicesCount();

	WebServerServiceController *	GetServiceController() { return &m_service_controller; }

	OP_STATUS GetUpdateFileName(OpGadget& gadget, OpString& file_name);

	/**
	  * This method should be called to obtain path to a file which contains download url
	  * of gadget's update. This file is located in extension's profile directory.
	  *
	  * @param[in]  gadget gadget for which update's download url we want to get
	  * @param[out] file_name OpString where path to file containing update's download url will be copied
	  * @return     status of the operation. If any error is returned caller should not depend
	  *             on the content of \param file_name
	  */
	OP_STATUS GetUpdateUrlFileName(OpGadget& gadget, OpString& file_name);

protected:
	// DesktopWindowListener implementation
	virtual void OnDesktopWindowClosing(DesktopWindow* desktop_window, BOOL user_initiated);

#endif // WEBSERVER_SUPPORT

private:
	static const uni_char UPDATE_FILENAME[];
	static const uni_char UPDATE_URL_FILENAME[];

	/**
	 * Builds the path to the root widget.
	 */
	OP_STATUS GetRootServiceAddress(OpString& address);

	DesktopGadget*			m_root_service_window;	// Root service window
	
#ifdef WEBSERVER_SUPPORT
	WebServerServiceController	m_service_controller;
	OpINT32Set					m_running_services;
#endif // WEBSERVER_SUPPORT
};

#endif // GADGET_SUPPORT
#endif // __DESKTOPGADGET_MANAGER_H__
