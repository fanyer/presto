/* -*- mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2007 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** @author Lasse Magnussen lasse@opera.com
*/

#ifndef MODULES_DOM_DOMFILE_H
#define MODULES_DOM_DOMFILE_H

#ifdef DOM_GADGET_FILE_API_SUPPORT

#include "modules/dom/src/domobj.h"
#include "modules/dom/src/domevents/domeventtarget.h"
#include "modules/util/opfile/opfile.h"
#include "modules/util/adt/bytebuffer.h"
#include "modules/util/adt/opvector.h"
#include "modules/webserver/webserver_callbacks.h"
#include "modules/ecmascript_utils/esasyncif.h"
#include "modules/gadgets/OpGadget.h"
#include "modules/windowcommander/OpWindowCommander.h"

class DOM_GadgetFile;
class DOM_FileList;
class DOM_WebServer;
class WebserverFileSandbox;
class MountPoint;
class WebserverBodyObject_Base;
class OpPersistentStorageListener;
class WebserverResourceDescriptor_Base;
class MessageHandler;
class OpFolderLister;
class ES_Thread;

/************************************************************************/
/* class DOM_FileBase                                                   */
/************************************************************************/

class DOM_FileBase : public Link
{
public:
	virtual ~DOM_FileBase() { Out(); }
	virtual void Cleanup() = 0;
};

/************************************************************************/
/* class DOM_FileSystem                                                 */
/************************************************************************/

class DOM_FileSystem
	: public DOM_Object
	, public DOM_FileBase
	, public OpFileSelectionListener::DomFilesystemCallback
{
public:
	static OP_STATUS Make(DOM_FileSystem *&new_obj, DOM_Runtime *origining_runtime, OpPersistentStorageListener *storage_listener);

	virtual ES_GetState GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);

	virtual BOOL IsA(int type) { return type == DOM_TYPE_FILESYSTEM || DOM_Object::IsA(type); }
	virtual void GCTrace();

	OP_STATUS AddMountPoint(DOM_GadgetFile *mp);
	OP_STATUS RemoveMountPoint(DOM_GadgetFile *mp);
	DOM_GadgetFile *GetMountPoint(const uni_char *name);
	OP_STATUS RemoveMountPoints();

	// From DomFilesystemCallback
	void OnFilesSelected(const OpAutoVector<OpString>* files);
	const OpFileSelectionListener::MediaType* GetMediaType(unsigned int index);
	const uni_char* GetInitialPath();
	OpFileSelectionListener::DomFilesystemCallback::SelectionType GetSelectionType();
	const uni_char* GetCaption();

	void Cleanup(); // Called on DOM_EnvironmentImpl::BeforeDestroy and BeforeUnload

	// Debug functions (only available for selftests)
	DOM_DECLARE_FUNCTION(addMountPoint);
	DOM_DECLARE_FUNCTION(getSpecialFile);

	// Functions
	DOM_DECLARE_FUNCTION(removeMountPoint);
	DOM_DECLARE_FUNCTION(mountSystemDirectory);
	enum {
		FUNCTIONS_removeMountPoint = 1,
		FUNCTIONS_mountSystemDirectory,
		FUNCTIONS_ARRAY_SIZE
	};

	DOM_DECLARE_FUNCTION_WITH_DATA(browseFor);
	enum {
		FUNCTIONS_browseForDirectory = 1,
		FUNCTIONS_browseForFile,
		FUNCTIONS_browseForSave,
		FUNCTIONS_WITH_DATA_ARRAY_SIZE
	};


protected:
	DOM_FileSystem(OpPersistentStorageListener *storage_listener);

	virtual ~DOM_FileSystem();
	OP_STATUS Initialize();
	OP_STATUS ReadPersistentData();

private:
	typedef enum {
		UNKNOWN,
		BROWSE_FOR_DIRECTORY,
		BROWSE_FOR_FILE,
		BROWSE_FOR_MULTIPLE_FILES,
		BROWSE_FOR_SAVE
	} BrowseType;

	DOM_GadgetFile *m_mountpoints;
	OpPersistentStorageListener *m_persistent_storage_listener;
	ES_Object *m_browse_callback;

	// for the file dialogs:
	OpString m_fd_name;
	OpString m_fd_location;
	BOOL m_fd_persistent;
	OpFileSelectionListener::MediaType m_fd_mediatype;
	BrowseType m_fd_browse_type;
};

/************************************************************************/
/* class DOM_GadgetFile                                                       */
/************************************************************************/

class DOM_GadgetFile
	: public DOM_FileBase
	, public DOM_Object
	, public DOM_EventTargetOwner
{
public:
	virtual ~DOM_GadgetFile();

	static OP_STATUS MakeMountPoint(DOM_GadgetFile *&new_obj, DOM_Runtime *origining_runtime, DOM_GadgetFile *parent, const uni_char *name, const uni_char *path, OpPersistentStorageListener *listener);
	static OP_STATUS MakePhysical(DOM_GadgetFile *&new_obj, DOM_Runtime *origining_runtime, DOM_GadgetFile *parent, const uni_char *name, const uni_char *path);
	static OP_STATUS MakeVirtual(DOM_GadgetFile *&new_obj, DOM_Runtime *origining_runtime);
	static OP_STATUS MakeRoot(DOM_GadgetFile *&new_obj, DOM_Runtime *origining_runtime);

	virtual ES_GetState GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);

	virtual ES_GetState GetIndex(int property_index, ES_Value *value, ES_Runtime *origining_runtime);

	virtual BOOL IsA(int type) { return type == DOM_TYPE_GADGETFILE || DOM_Object::IsA(type); }
	virtual void GCTrace();

	// From DOM_EventTargetOwner
	DOM_Object *GetOwnerObject() { return this; }

	OP_STATUS GetPhysicalPath(OpString& path);
	OP_STATUS GetVisiblePath(OpString& path);

	BOOL IsPersistent() { return m_persistent; }
	OP_STATUS SetIsPersistent(BOOL persistent);
	void SetPersistentStorageListener(OpPersistentStorageListener *storage_listener) { m_persistent_storage_listener = storage_listener; }

	BOOL IsMountPoint() { return (m_capabilities & MOUNTPOINT) != 0; }
	BOOL IsVirtual() { return (m_capabilities & VIRTUAL) != 0; }
	BOOL IsArchive() { return (m_capabilities & ARCHIVE) != 0; }
	BOOL IsInArchive();
	BOOL IsDirectory() { return (m_capabilities & DIRECTORY) != 0; }
	BOOL IsFile() { return (m_capabilities & FILE) != 0; }
	BOOL IsSymlink() { return (m_capabilities & SYMLINK) != 0; }
	BOOL IsRoot() { return (m_capabilities & ROOT) != 0; }
	BOOL IsReadOnly();
	BOOL IsSystem();
	BOOL IsHidden() { return (m_capabilities & HIDDEN) != 0; }

	BOOL Exists();

	OP_STATUS GetMode(OpFileInfo::Mode& mode);

	OP_STATUS AddNode(DOM_GadgetFile *new_file) { return m_nodes.Add(new_file); }
	OP_STATUS DeleteNode(DOM_GadgetFile *file, BOOL recursive = FALSE);
	OP_STATUS DeleteNodes(BOOL recursive = FALSE);
	DOM_GadgetFile *GetNode(UINT32 index) { return m_nodes.Get(index); }
	OP_STATUS FindNode(DOM_GadgetFile *&dom_file, const uni_char *path_arg, INT32 level = -1, const uni_char **rest = NULL, DOM_GadgetFile **last_node = NULL);
	DOM_GadgetFile *FindParentNode(INT32 capability);

	const uni_char *Name() { return m_name.CStr(); }
	const uni_char *SysFolder() { return m_sys_folder.CStr(); }

	OP_STATUS SetSysFolder(const uni_char* folder) { m_capabilities |= SYSTEM; return m_sys_folder.Set(folder); }

	void SetMetaData(DOM_Object *meta_data) { m_meta_data = meta_data; }
	DOM_Object *GetMetaData() { return m_meta_data; }

	void MakeInvalid();	// invalidates this object. Used to prevent further use.
	BOOL IsValid() { return m_capabilities != NONE; }
	void Cleanup(); // Called on DOM_EnvironmentImpl::BeforeDestroy and BeforeUnload

	void SetCapabilities(INT32 cap) { m_capabilities = cap; }
	void SetCapability(INT32 cap) { m_capabilities |= cap; }
	void ClearCapability(INT32 cap) { m_capabilities &= ~cap; }
	INT32 GetCapabilities() { return m_capabilities; }
	BOOL HasCapability(INT32 cap) { return m_capabilities & cap ? TRUE : FALSE; }

#ifdef WEBSERVER_SUPPORT
	OP_BOOLEAN Share(DOM_WebServer *webserver, const uni_char *uri);
	OP_STATUS Unshare(DOM_WebServer *webserver);
	OP_STATUS UnshareSelf();
#endif // WEBSERVER_SUPPORT

	// Functions
	DOM_DECLARE_FUNCTION(open);
	DOM_DECLARE_FUNCTION(copyTo);
	DOM_DECLARE_FUNCTION(moveTo);
	DOM_DECLARE_FUNCTION(createDirectory);
	DOM_DECLARE_FUNCTION(deleteDirectory);
	DOM_DECLARE_FUNCTION(deleteFile);
	DOM_DECLARE_FUNCTION(refresh);
	DOM_DECLARE_FUNCTION(resolve);
	DOM_DECLARE_FUNCTION(sort);
	DOM_DECLARE_FUNCTION(toString);
	enum {
		FUNCTIONS_open = 1,
		FUNCTIONS_copyTo,
		FUNCTIONS_moveTo,
		FUNCTIONS_createDirectory,
		FUNCTIONS_deleteDirectory,
		FUNCTIONS_deleteFile,
		FUNCTIONS_refresh,
		FUNCTIONS_resolve,
		FUNCTIONS_sort,
		FUNCTIONS_toString,
		FUNCTIONS_ARRAY_SIZE
	};

	enum {
		FUNCTIONS_WITH_DATA_addEventListenerNS = 1,
		FUNCTIONS_WITH_DATA_removeEventListenerNS,
		FUNCTIONS_WITH_DATA_ARRAY_SIZE
	};

	enum
	{
		NONE       = 0,
		MOUNTPOINT = 0x01,
		FILE       = 0x02,
		DIRECTORY  = 0x04,
		ARCHIVE    = 0x08,
		VIRTUAL    = 0x10,
		ROOT       = 0x20,
		SYMLINK    = 0x40,
		SYSTEM     = 0x80,
		READONLY   = 0x100,
		HIDDEN     = 0x200
	};

private:
	static OP_STATUS Make(DOM_GadgetFile *&new_obj, DOM_Runtime *origining_runtime, DOM_GadgetFile *parent);

	DOM_GadgetFile(DOM_GadgetFile *parent);

	OP_STATUS Initialize(const uni_char *name, const uni_char *path);

	OpPersistentStorageListener *GetPSListener() { return m_parent ? m_parent->GetPSListener() : m_persistent_storage_listener; }

	OP_STATUS ParseNodes(DOM_GadgetFile *&last_node, const uni_char *path);
	OP_STATUS UpdateFileType();
	OP_STATUS CheckIsArchive(BOOL& archive);
	OP_STATUS AddToPersistentData();
	OP_STATUS RemoveFromPersistentData();
	OP_STATUS InternalResolve(DOM_GadgetFile *&dom_file, const uni_char *path);
	OP_STATUS BuildPath(OpString& path);
	OP_STATUS EnsurePathExists();
	OP_STATUS Invalidate();	// refreshes internal states

	OP_BOOLEAN ProcessNextFile();

	INT32 m_capabilities;
	OpString m_name;
	OpString m_path;
	OpString m_sys_folder;

	BOOL m_persistent;
	OpPersistentStorageListener *m_persistent_storage_listener;

	OpVector<DOM_GadgetFile> m_nodes;
	DOM_GadgetFile *m_parent;
	DOM_Object *m_meta_data;
#ifdef WEBSERVER_SUPPORT
	WebserverResourceDescriptor_Base *m_res_desc;
#endif // WEBSERVER_SUPPORT
	unsigned int m_win_id;
	MountPoint *m_mp_handler;
	OpFolderLister *m_folder_lister;
};

/************************************************************************/
/* class DOM_FileStream                                                 */
/************************************************************************/

class DOM_FileStream : public DOM_Object, public DOM_EventTargetOwner
{
public:
	static OP_STATUS Make(DOM_FileStream *&new_obj, DOM_Runtime *origining_runtime, DOM_GadgetFile *parent);

	virtual ES_GetState GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual BOOL IsA(int type) { return type == DOM_TYPE_FILESTREAM || DOM_Object::IsA(type); }
	virtual void GCTrace();

	// From DOM_EventTargetOwner
	DOM_Object *GetOwnerObject() { return this; }

	OP_STATUS GetFileName(OpString& filename);
	OP_STATUS WriteBodyObjectToFile(WebserverBodyObject_Base *body_object);
	OP_STATUS WriteFileToFile(OpFile *src);

	BOOL IsReadable();
	BOOL IsWritable();
	BOOL IsOpen();

	OpFile *GetFile(void) { return &m_file; }

	// Functions
	DOM_DECLARE_FUNCTION(close);
	DOM_DECLARE_FUNCTION(read);
	DOM_DECLARE_FUNCTION(readLine);
	DOM_DECLARE_FUNCTION(readBytes);
	DOM_DECLARE_FUNCTION(readBase64);
	DOM_DECLARE_FUNCTION(write);
	DOM_DECLARE_FUNCTION(writeLine);
	DOM_DECLARE_FUNCTION(writeObject);
	DOM_DECLARE_FUNCTION(writeBase64);
	enum {
		FUNCTIONS_close = 1,
		FUNCTIONS_read,
		FUNCTIONS_readLine,
		FUNCTIONS_readBytes,
		FUNCTIONS_readBase64,
		FUNCTIONS_write,
		FUNCTIONS_writeLine,
		FUNCTIONS_writeBytes,
		FUNCTIONS_writeFile,
		FUNCTIONS_writeImage,
		FUNCTIONS_writeBase64,
		FUNCTIONS_ARRAY_SIZE
	};

	enum {
		FUNCTIONS_WITH_DATA_addEventListenerNS = 1,
		FUNCTIONS_WITH_DATA_removeEventListenerNS,
		FUNCTIONS_WITH_DATA_ARRAY_SIZE
	};

private:
	DOM_FileStream(DOM_GadgetFile *parent);

	virtual ~DOM_FileStream();
	OP_STATUS Initialize();

	DOM_GadgetFile *m_parent;
	OpFile m_file;
	OpString m_encoding;
	OpString m_newline;
};

/************************************************************************/
/* struct DOM_FileMode                                                  */
/************************************************************************/

struct DOM_FileMode
{
	enum DOM_FILEMODE
	{
		DOM_FILEMODE_READ   = 1,	// External API. Do not change.
		DOM_FILEMODE_WRITE  = 2,
		DOM_FILEMODE_APPEND = 4,
		DOM_FILEMODE_UPDATE = 8
	};
};

/************************************************************************/
/* class DFA_Utils (DOM File API utils)                                 */
/************************************************************************/

class DFA_Utils
{
public:
	static int			ThrowFileAPIException(OP_STATUS err, DOM_Object *this_object, ES_Value *return_value);
	static int			SetEscapedString(DOM_Object *this_object, ES_Value *value, const uni_char *string);
	static OP_STATUS	EscapeToBuffer(const uni_char *url, TempBuffer *buffer);
	static OP_STATUS	SecurePathConcat(OpString& safe_base_path, const uni_char *unsafe_base_path, BOOL isUrl = FALSE);
	static OP_STATUS	CheckAndFixDangerousPaths(uni_char *filePathAndName, BOOL isURL);
	static OP_STATUS	UnescapeString(OpString& unescaped, const uni_char* escaped);
	static OP_STATUS	EscapeString(OpString& escaped, const uni_char* unescaped);
};

#endif // DOM_GADGET_FILE_API_SUPPORT

#endif // !MODULES_DOM_DOMFILE_H
