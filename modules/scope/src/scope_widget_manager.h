/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Author: Jan Borsodi, Anders Ruud
*/

#ifndef SCOPE_WIDGET_MANAGER_H
#define SCOPE_WIDGET_MANAGER_H

#ifdef SCOPE_WIDGET_MANAGER_SUPPORT

#include "modules/scope/src/scope_service.h"
#include "modules/scope/scope_readystate_listener.h"
#include "modules/util/adt/opvector.h"
#include "modules/ecmascript_utils/esdebugger.h"
#include "modules/doc/frm_doc.h"
#include "modules/gadgets/OpGadgetManager.h"

#include "modules/scope/src/generated/g_scope_widget_manager_interface.h"

class OpGadgetClass;

class OpScopeWidgetManager
	: public OpScopeWidgetManager_SI
	, public OpGadgetInstallListener
	, public OpGadgetListener
{
public:
	OpScopeWidgetManager();
	virtual ~OpScopeWidgetManager();

	/**
	 * Called when service is first used.
	 * Registers listeners in the gadget manager
	 */
	virtual OP_STATUS OnServiceEnabled();
	/**
	 * Called when service is no longer in use.
	 * Resets the installer, download and uninstall requests and resets
	 * ID counter to 1.
	 */
	virtual OP_STATUS OnServiceDisabled();

	/* From OpGadgetInstallListener */
	virtual void HandleGadgetInstallEvent(GadgetInstallEvent& evt);

	/* From OpGadgetListener */
	virtual void OnGadgetUpgraded(const OpGadgetStartStopUpgradeData& data);
	virtual void OnGadgetInstanceCreated(const OpGadgetInstanceData& data);
	virtual void OnGadgetInstanceRemoved(const OpGadgetInstanceData& data);
	virtual void OnGadgetUpdateReady(const OpGadgetUpdateData& data);
	virtual void OnGadgetUpdateError(const OpGadgetErrorData& data);

	// We're not interested in these, empty implementation
	virtual void OnGadgetDownloadPermissionNeeded(const OpGadgetDownloadData&, GadgetDownloadPermissionCallback *) {}
	virtual void OnGadgetDownloaded(const OpGadgetDownloadData&) {}
	virtual void OnGadgetInstalled(const OpGadgetInstallRemoveData&) {}
	virtual void OnGadgetSignatureVerified(const OpGadgetSignatureVerifiedData& data) {}
	virtual void OnGadgetRemoved(const OpGadgetInstallRemoveData&) {}
	virtual void OnRequestRunGadget(const OpGadgetDownloadData&) {}
	virtual void OnGadgetStarted(const OpGadgetStartStopUpgradeData&) {}
	virtual void OnGadgetStartFailed(const OpGadgetStartFailedData& data) {}
	virtual void OnGadgetStopped(const OpGadgetStartStopUpgradeData&) {}
	virtual void OnGadgetDownloadError(const OpGadgetErrorData&) {}

	// Request/Response functions
	virtual OP_STATUS DoGetSettings(Settings &out);
	virtual OP_STATUS DoListWidgets(WidgetList &out);
	virtual OP_STATUS DoOpenWidget(const OpenWidgetArg &in);
	virtual OP_STATUS DoCloseWidget(const CloseWidgetArg &in);
	virtual OP_STATUS DoCreateInstaller(const CreateInstallerArg &in, Installer &out);
	virtual OP_STATUS DoUpdateInstaller(const UpdateInstallerArg &in);
	virtual OP_STATUS DoRemoveInstaller(const RemoveInstallerArg &in);
	virtual OP_STATUS DoInstallWidget(const InstallWidgetArg &in, WidgetInstallResult &out);
	virtual OP_STATUS DoSetExtensionFlag(const SetExtensionFlagArg &in);
	virtual OP_STATUS DoGetExtensionFlag(const GetExtensionFlagArg &in, ExtensionFlag &out);
	virtual OP_STATUS DoUpdateWidget(const UpdateWidgetArg &in, unsigned int async_tag);
	virtual OP_STATUS DoInstallWidgetByURL(const InstallWidgetByURLArg &in, unsigned int async_tag);
	virtual OP_STATUS DoUninstallWidget(const UninstallWidgetArg &in, unsigned int async_tag);

private:
	/**
	 * Exports @c OpGadget and @c OpGadgetClass information to the @c Widget message.
	 * @note Calls ExportWidget with the OpGadgetClass object.
	 */
	OP_STATUS ExportWidget(Widget &widget, OpGadget *gadget);
	/**
	 * Exports @c OpGadgetClass information to the @c Widget message.
	 */
	OP_STATUS ExportWidget(Widget &widget, OpGadgetClass *klass);

	void SendWidgetUpgradedResult(OpGadgetClass* klass, WidgetUpdateResult::Result res);

private:
	friend class OpScopeWidgetListener;

	// Callbacks from the gadgets module

	/**
	 * Called when a gadget instance (OpGadget *) has been created. Gadget
	 * instances are exposed to the clients as widgets, gadget classes are
	 * never exposed.
	 * @param gadget The gadget instance that was created.
	 */
	OP_STATUS OnWidgetInstalled(OpGadget *gadget);

	/**
	 * Called when a gadget instance (OpGadget *) has been upgraded. Gadget
	 * instances are exposed to the clients as widgets, gadget classes are
	 * never exposed.
	 * @param original_klass The gadget class used by the gadget instance
	 *                       before the upgrade.
	 * @param gadget The gadget instance that was upgraded.
	 */
	OP_STATUS OnWidgetUpgraded(OpGadgetClass *original_klass, OpGadget *gadget);

	/**
	 * Called when a gadget instance (OpGadget *) has been destroyed. Gadget
	 * instances are exposed to the clients as widgets, gadget classes are
	 * never exposed.
	 * @param gadget The gadget instance that was destroyed.
	 */
	OP_STATUS OnWidgetUninstalled(OpGadget *gadget);

	/**
	 * Called when there is a change in the installation/download process for
	 * widgets installed from a URL (InstallWidgetByURL).
	 */
	OP_STATUS OnWidgetInstallResult(OpGadgetInstallListener::Event evt, OpGadgetDownloadObject *obj);

	/**
	 * A request for installing a new widget. @a id is the unique ID which
	 * is sent through scope to the client.
	 * If the installer is meant to be used as an upgrade to an existing
	 * widget the @a widget_id field will be set.
	 *
	 * @a install_object is the object which handles updating of widget data
	 * and is sent to the gadgets module to be either installed or used for
	 * upgrading an existing widget.
	 */
	class InstallerObject
	{
	public:
		OP_STATUS Construct(unsigned id, const OpString &widget_id, GadgetClass installer_type);

		/**
		 * Opens the installer object by allowing data to be sent to it.
		 * @see OpGadgetInstallObject::Open
		 */
		OP_STATUS Open();
		/**
		 * Appends binary data to the installer object.
		 * @see OpGadgetInstallObject::AppendData.
		 */
		OP_STATUS AppendData(const ByteBuffer &data);
		/**
		 * Closes an open installer object.
		 * @see OpGadgetInstallObject::Close
		 */
		OP_STATUS Close();
		/**
		 * Checks whether the current installer contains a valid object.
		 * @see OpGadgetInstallObject::IsValidWidget
		 */
		BOOL IsValidWidget();
		/**
		 * Tells the installer object to update the stored filename.
		 * @see OpGadgetInstallObject::Update
		 */
		OP_STATUS Update();

		/**
		 * @return The unique ID for this installer.
		 */
		unsigned GetID() const { return id; }
		/**
		 * @return The ID of the widget which is to be upgraded, or empty
		 * string if the installer is meant to install a new widget.
		 */
		const OpString &GetWidgetID() const { return widget_id; }
		/**
		 * @return The gadget install object, this installer still has ownership of the object.
		 */
		OpGadgetInstallObject *GetInstallObject() const { return install_object.get(); }
		/**
		 * Releases ownership of the gadget install object and returns it.
		 */
		OpGadgetInstallObject *ReleaseInstallObject() { return install_object.release(); }

	private:
		unsigned id;
		OpAutoPtr<OpGadgetInstallObject> install_object;
		OpString widget_id;
	};

	/**
	 * Finds the installer which corresponds to the id @a installer_id and returns it.
	 *
	 * @return The installer object or @c NULL if no installer was found.
	 */
	InstallerObject *FindInstaller(unsigned installer_id);
	/**
	 * Releases the installer corresponding to the id @a installer_id from the queue.
	 *
	 * @return The installer object or @c NULL if no installer was found.
	 */
	InstallerObject *ReleaseInstaller(unsigned installer_id);

	/**
	 * Object to handle command-tag for update information and widget update.
	 */
	class CheckForUpdateObject
	{
	public:
		CheckForUpdateObject()
			: tag(0)
		{
		}
		OP_STATUS Construct(const uni_char* s, unsigned t) { RETURN_IF_ERROR(widget_id.Set(s)); tag=t; return OpStatus::OK; }
		/**
		 * @return The gadget identifier
		 */
		const uni_char *GetGadgetID() { return widget_id.CStr(); }
		/**
		 * @return the tag number of the command that requested the update.
		 */
		unsigned GetTag() const { return tag; }
	private:
		unsigned tag;
		OpString widget_id;
	};

	/**
	 * Finds the updater which corresponds to the widget identifier @a widget_id returns it and removes it from the vector.
	 * @param widget_id The widget id of the Widget that is Updated
	 * @return The installer object or @c NULL if no installer was found.
	 */
	CheckForUpdateObject *FindAndRelaseUpdateObject(const uni_char* widget_id);

	/**
	 * A request for installing a widget from a URL. The actual download
	 * and installation is handled by the gadgets module
	 * (via OpGadgetDownloadObject).
	 */
	class DownloadObject
	{
	public:
		DownloadObject()
			: download(NULL), tag(0)
		{
		}
		DownloadObject(OpGadgetDownloadObject *obj, unsigned tag)
			: download(obj), tag(tag)
		{
		}

		/**
		 * @return The download object or @c NULL if no object is set.
		 */
		OpGadgetDownloadObject *GetDownloadObject() const { return download; }
		/**
		 * @return the tag number of the command that requested the installation.
		 */
		unsigned GetTag() const { return tag; }

	private:
		OpGadgetDownloadObject *download;
		unsigned tag;
	};

	/**
	 * A request for uninstalling a widget.
	 */
	class UninstallRequest
	{
	public:
		UninstallRequest()
			: gadget(NULL), tag(0)
		{
		}
		UninstallRequest(OpGadget *gadget, unsigned tag)
			: gadget(gadget), tag(tag)
		{
		}

		/**
		 * @return The gadget object which was requested to be uninstalled or @c NULL if no gadget was set.
		 */
		OpGadget *GetGadget() const { return gadget; }
		/**
		 * @return The tag number of the command that requested the uninstallation.
		 */
		unsigned GetTag() const { return tag; }

	private:
		OpGadget *gadget;
		unsigned tag;
	};

	/* Helper classes */
	friend class ST_scopewidgetmanager;

	/**
	 * The current available ID for installers. Starts at 1 when service is enabled.
	 */
	unsigned id_counter;
	/**
	 * The currently active installer objects.
	 */
	OpVector<InstallerObject> install_objects;
	/**
	 * The currently active download requests.
	 */
	OpValueVector<DownloadObject> download_objects;
	/**
	 * The currently active uninstall requests.
	 */
	OpValueVector<UninstallRequest> uninstall_requests;
	/**
	 * Set of gadgets being checked for updates with async tags
	 */
	OpAutoVector<CheckForUpdateObject> check_update_requests;
};

#endif // SCOPE_WIDGET_MANAGER_SUPPORT

#endif // SCOPE_WIDGET_MANAGER_H
