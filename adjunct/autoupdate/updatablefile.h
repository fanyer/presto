/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Marius Blomli, Michal Zajaczkowski
 */

#ifndef _UPDATEABLEFILE_H_INCLUDED_
#define _UPDATEABLEFILE_H_INCLUDED_

#ifdef AUTO_UPDATE_SUPPORT

#include "adjunct/autoupdate/updatableresource.h"
#include "adjunct/autoupdate/autoupdater.h"
#include "modules/prefsloader/prefsloadmanager.h"

#ifdef PLUGIN_AUTO_INSTALL
#include "adjunct/quick/managers/PluginInstallManager.h"
#endif // PLUGIN_AUTO_INSTALL

/**
 * This class is a representation of a local file resource that is to be
 * kept up to date by the auto update system.
 */
class UpdatableFile:
	public UpdatableResource,
	public FileDownloader
{
public:

	UpdatableFile();
	/**
	 * Gets the class of resource
	 *
	 * @returns - ResourceClass::File
	 */
	virtual ResourceClass GetResourceClass() { return File; }

	/**
	 * Override this method to implement the way that your file resource is checked after being downloaded.
	 * The default implementation found in UpdatebleFile::CheckResource() checks the SHA checksum that should
	 * be sent from the server along with each file resource.
	 *
	 * @returns - TRUE is case the file was checked to be OK, FALSE otherwise
	 */
	virtual BOOL CheckResource();

	/**
	 * Implement this method to ensure that the attribute list sent from the server along with your file resource
	 * is complete, i.e. that the server has sent all the XML elements that are essential for the given type of file
	 * resource.
	 * This method is called by the UpdatableResourceFactory just after the XML fragment containing the description
	 * of the given file is fully parsed. In case this method returns FALSE, the file resource is dropped, i.e. the
	 * UpdatableResourceFactory refuses to construct it.
	 *
	 * @returns - TRUE if the file resource has all the needed attributes, FALSE otherwise
	 */
	virtual BOOL VerifyAttributes() = 0;

	/**
	 * Constructs the full local filename for the given file resource, i.e. the filename containing the full path to the file.
	 * Utilizes the folder name returned by GetDownloadFolder() and the filename sent from the autoupdate server within the 
	 * <name> XML element. This file name will determine the download location for the given file resource.
	 *
	 * @param full_filename - an OpString reference that will be filled with the full file name including the path
	 *
	 * @returns - OpStatus::OK if the file name was construced OK, ERR otherwise.
	 */
	OP_STATUS GetDownloadFullName(OpString& full_filename);

	/**
	 * Specifies the folder for the given file resource. The default implementation sets the folder to OPFILE_TEMPDOWNLOAD_FOLDER,
	 * this method can be overriden in order to have the file downloaded to some other folder.
	 *
	 * @param download_folder - an OpString reference that will contain the folder name string
	 *
	 * @returns - OpStatus::OK if the string was set successfully, ERR otherwise.
	 */
	virtual OP_STATUS GetDownloadFolder(OpString& download_folder);

	/**
	 * Starts the download of the given file resource. Notify the given FileDownloadListener implementation about the progress.
	 *
	 * @param listener - The FileDownloadListener implementation that will receive the progress events.
	 *
	 * @returns - OpStatus::OK in case the download started OK, error code otherwise.
	 */
	OP_STATUS StartDownloading(FileDownloadListener* listener);

protected:

	/**
	 * In order to report the file resource download progress, Opera needs to know the resource size. This should come from the 
	 * HTTP headers sent by the server hosting the download, however if the size received that way is zero, we need a better 
	 * way to know it, thus this method is called then.
	 * The default implementation uses the <size> XML element content to determine the download size. This is not always the
	 * best idea, i.e. with the plugin downloads that are hosted on vendors' servers this element may not contain the lastest
	 * up-to-date information.
	 * If your UpdatableFile implementation has a good way of knowing the download size, implement this method.
	 *
	 * @returns - the download size in bytes, best guess.
	 */
	virtual OpFileLength GetOverridedFileLength();
};

/**
 *
 * UpdatablePackage
 *
 */
class UpdatablePackage : public UpdatableFile
{
public:
	UpdatablePackage() {}
	virtual ~UpdatablePackage() {}
	
	/**
	 * Implementing UpdatableResource API.
	 */
	virtual UpdatableResourceType	GetType() { return RTPackage; }
	virtual OP_STATUS				UpdateResource();
	virtual BOOL					CheckResource();
	virtual OP_STATUS				Cleanup();
	virtual const uni_char*			GetResourceName() { return UNI_L("Package"); }
	virtual BOOL					UpdateRequiresUnpacking();
	virtual BOOL					UpdateRequiresRestart() { return TRUE; }
	virtual BOOL					GetShowNotification();
	virtual BOOL					VerifyAttributes();

	OP_STATUS GetPackageVersion(OperaVersion& version);
};

/**
 *
 * UpdatableSpoof
 *
 */
class UpdatableSpoof : public UpdatableFile
{
public:
	UpdatableSpoof() {}
	virtual ~UpdatableSpoof() {}

	/**
	 * Implementing UpdatableResource API.
	 */
	virtual UpdatableResourceType	GetType() { return RTSpoofFile; }
	virtual OP_STATUS				UpdateResource();
	virtual BOOL					CheckResource();
	virtual OP_STATUS				Cleanup();
	virtual const uni_char*			GetResourceName() { return UNI_L("Spoof"); }
	virtual BOOL					UpdateRequiresUnpacking() { return FALSE; }
	virtual BOOL					UpdateRequiresRestart() { return FALSE; }
	virtual BOOL					VerifyAttributes();

private:
	class EndChecker: public OpEndChecker
	{
	public:
		/**
		 * Always accept spoof lists Opera downloads automatically: return FALSE
		 */
		BOOL IsEnd(OpStringC info) { return FALSE; }
		void Dispose() { OP_DELETE(this); }
	};
};


/**
 *
 * UpdatableBrowserJS
 *
 */
class UpdatableBrowserJS : public UpdatableFile
{
public:
	UpdatableBrowserJS() {}
	virtual ~UpdatableBrowserJS() {}
	
	/**
	 * Implementing UpdatableResource API.
	 */
	virtual UpdatableResourceType	GetType() { return RTBrowserJSFile; }
	virtual OP_STATUS				UpdateResource();
	virtual BOOL					CheckResource();
	virtual OP_STATUS				Cleanup();
	virtual const uni_char*			GetResourceName() { return UNI_L("Browser JS"); }
	virtual BOOL					UpdateRequiresUnpacking() { return FALSE; }
	virtual BOOL					UpdateRequiresRestart() { return FALSE; }
	virtual BOOL					VerifyAttributes();
};

/**
 *
 * UpdatableDictionary
 *
 */
class UpdatableDictionary : public UpdatableFile
{
public:
	UpdatableDictionary() {}
	virtual ~UpdatableDictionary() {}
	
	/**
	 * Implementing UpdatableResource API.
	 */
	virtual UpdatableResourceType	GetType() { return RTDictionary; }
	virtual OP_STATUS				UpdateResource();
	virtual BOOL					CheckResource();
	virtual OP_STATUS				Cleanup();
	virtual const uni_char*			GetResourceName() { return UNI_L("Dictionary"); }
	virtual BOOL					UpdateRequiresUnpacking() { return FALSE; }
	virtual BOOL					UpdateRequiresRestart() { return FALSE; }
	virtual URAttr					GetResourceKey() { return URA_ID; }
	virtual BOOL					VerifyAttributes();
};

/**
 *
 * UpdatableDictionaryXML
 *
 */
class UpdatableDictionaryXML : public UpdatableFile
{
public:
	UpdatableDictionaryXML() {}
	virtual ~UpdatableDictionaryXML() {}
	
	/**
	 * Implementing UpdatableResource API.
	 */
	virtual UpdatableResourceType	GetType() { return RTDictionary; }
	virtual OP_STATUS				UpdateResource();
	virtual BOOL					CheckResource();
	virtual OP_STATUS				Cleanup();
	virtual OpFileLength			GetSize() const { return 0; }
	virtual const uni_char*			GetResourceName() { return UNI_L("DictionaryXML"); }
	virtual BOOL					UpdateRequiresUnpacking() { return FALSE; }
	virtual BOOL					UpdateRequiresRestart() { return FALSE; }
	virtual BOOL					VerifyAttributes();
};

#ifdef PLUGIN_AUTO_INSTALL
/**
 *
 * UpdatablePlugin
 *
 */
class UpdatablePlugin:
	public UpdatableFile,
	public PIM_AsyncProcessRunnerListener
{
public:
	UpdatablePlugin();
	virtual ~UpdatablePlugin();
	/**
	* Implementing UpdatableResource API.
	*/
	virtual UpdatableResourceType				GetType()	{ return RTPlugin; }
	virtual OP_STATUS                           UpdateResource();
	virtual BOOL                                CheckResource();
	virtual OP_STATUS                           Cleanup();
	virtual const uni_char*						GetResourceName() { return UNI_L("Plugin"); }
	virtual BOOL                                UpdateRequiresRestart() { return FALSE; }
	virtual BOOL								UpdateRequiresUnpacking() { return FALSE; }
	virtual URAttr								GetResourceKey() { return URA_MIME_TYPE; }
	virtual BOOL								VerifyAttributes();

	/**
	* Extending API for UpdatablePlugin
	*/

	OP_STATUS									InstallPlugin(BOOL a_silent);
	OP_STATUS									CancelPluginInstallation();

	PIM_AsyncProcessRunner* GetAsyncProcessRunner() { return m_async_process_runner; }

	void SetAsyncProcessRunner(PIM_AsyncProcessRunner* runner) { m_async_process_runner = runner; }

	/**
	 * PIM_AsyncProcessRunnerListener
	 */
	void										OnProcessFinished(DWORD exit_code);

private:
	PIM_AsyncProcessRunner*						m_async_process_runner;

};
#endif // PLUGIN_AUTO_INSTALL

#ifdef VEGA_3DDEVICE
/**
 *
 * UpdatableHardwareBlocklist
 *
 */
class UpdatableHardwareBlocklist : public UpdatableFile
{
public:
	UpdatableHardwareBlocklist() {}
	virtual ~UpdatableHardwareBlocklist() {}

	/**
	 * Implementing UpdatableResource API.
	 */
	virtual UpdatableResourceType				GetType() { return RTHardwareBlocklist; }
	virtual OP_STATUS							UpdateResource();
	virtual BOOL								CheckResource();
	virtual const uni_char*						GetResourceName() { return UNI_L("HardwareBlocklist"); }
	virtual BOOL								UpdateRequiresUnpacking() { return FALSE; }
	virtual BOOL								UpdateRequiresRestart() { return FALSE; }
	virtual OP_STATUS							Cleanup();
	virtual BOOL								VerifyAttributes();
};
#endif // VEGA_3DDEVICE

/**
 *
 * UpdatableHandlersIgnore
 *
 */
class UpdatableHandlersIgnore : public UpdatableFile
{
public:
	UpdatableHandlersIgnore() {}
	virtual ~UpdatableHandlersIgnore() {}

	/**
	 * Implementing UpdatableResource API.
	 */
	virtual UpdatableResourceType				GetType() { return RTHandlersIgnore; }
	virtual OP_STATUS							UpdateResource();
	virtual const uni_char*						GetResourceName() { return UNI_L("HandlersIgnore"); }
	virtual BOOL								UpdateRequiresUnpacking() { return FALSE; }
	virtual BOOL								UpdateRequiresRestart() { return FALSE; }
	virtual OP_STATUS							Cleanup();
	virtual BOOL								VerifyAttributes();
};


#endif // AUTO_UPDATE_SUPPORT
#endif // _UPDATEABLEFILE_H_INCLUDED_
