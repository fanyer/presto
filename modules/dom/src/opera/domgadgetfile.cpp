/* -*- mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2007-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Lasse Magnussen lasse@opera.com
 */
#include "core/pch.h"

#ifdef DOM_GADGET_FILE_API_SUPPORT

#include "modules/dom/src/opera/domgadgetfile.h"

#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/dom/src/domobj.h"
#include "modules/dom/src/domevents/domeventtarget.h"
#include "modules/dom/src/domevents/domeventlistener.h"
#include "modules/dom/src/domglobaldata.h"
#include "modules/dom/src/domcore/node.h"
#include "modules/dom/src/js/window.h"
#include "modules/doc/frm_doc.h"
#include "modules/dochand/win.h"
#include "modules/encodings/decoders/inputconverter.h"
#include "modules/encodings/encoders/outputconverter.h"
#include "modules/formats/encoder.h"
#include "modules/formats/base64_decode.h"
#include "modules/util/zipload.h"
#include "modules/formats/uri_escape.h"
#include "modules/gadgets/OpGadget.h"
#include "modules/windowcommander/src/WindowCommander.h"
#include "modules/webserver/webserver_body_objects.h"
#include "modules/dom/src/opera/dombytearray.h"
#include "modules/dom/src/domwebserver/domwebserver.h"

#ifdef WEBSERVER_SUPPORT
# include "modules/webserver/webserver-api.h"
# include "modules/webserver/webserver_resources.h"
# include "modules/dom/src/domwebserver/domwebserver.h"
# ifdef GADGET_SUPPORT
#  include "modules/gadgets/OpGadgetManager.h"
# endif // GADGET_SUPPORT
#endif //WEBSERVER_SUPPORT



#ifdef SELFTEST
# include "modules/selftest/optestsuite.h"
#endif

#include "modules/url/loadhandler/url_lhsand.h"
#include "modules/dom/src/opera/domio.h"

#define THROW_FILEAPIEXCEPTION_IF_ERROR( expr )                                                                      \
	do {                                                                                                             \
		OP_STATUS THROW_FILEAPIEXCEPTION_IF_ERROR_TMP = expr;                                                        \
		if (OpStatus::IsError(THROW_FILEAPIEXCEPTION_IF_ERROR_TMP))                                                  \
			return DFA_Utils::ThrowFileAPIException(THROW_FILEAPIEXCEPTION_IF_ERROR_TMP, this_object, return_value); \
	} while(0)

#define DOM_FILE_CHECK_VALIDITY( file, allow, disallow )																		\
	do {																														\
		if (file && (!file->IsValid() || (allow && !file->HasCapability(allow)) || (file->HasCapability(disallow))))			\
			return ES_EXCEPT_SECURITY;																							\
	} while (0)

/************************************************************************/
/* class DOM_FileSystem                                                 */
/************************************************************************/

/* static */ OP_STATUS
DOM_FileSystem::Make(DOM_FileSystem*& new_obj, DOM_Runtime *origining_runtime, OpPersistentStorageListener *storage_listener)
{
	new_obj = OP_NEW(DOM_FileSystem, (storage_listener));
	if (new_obj == NULL)
		return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_ERROR(DOMSetObjectRuntime(new_obj, origining_runtime,
		origining_runtime->GetPrototype(DOM_Runtime::FILESYSTEM_PROTOTYPE), "FileSystem"));

	RETURN_IF_ERROR(new_obj->Initialize());

	new_obj->GetEnvironment()->AddFileObject(new_obj);

	return OpStatus::OK;
}

DOM_FileSystem::DOM_FileSystem(OpPersistentStorageListener *storage_listener)
	: m_mountpoints(NULL)
	  ,m_persistent_storage_listener(storage_listener)
	  ,m_browse_callback(NULL)
	  ,m_fd_persistent(FALSE)
	  ,m_fd_browse_type(UNKNOWN)
{
}

DOM_FileSystem::~DOM_FileSystem()
{
}

OP_STATUS
DOM_FileSystem::Initialize()
{
	RETURN_IF_ERROR(DOM_GadgetFile::MakeRoot(m_mountpoints, GetRuntime()));
	RETURN_IF_ERROR(ReadPersistentData());

#ifdef SELFTEST
	if (g_selftest.running)
	{
		RETURN_IF_LEAVE(AddFunctionL(addMountPoint, "addMountPoint", NULL));
		RETURN_IF_LEAVE(AddFunctionL(getSpecialFile, "getSpecialFile", NULL));
	}
#endif // SELFTEST

	return OpStatus::OK;
}

void
DOM_FileSystem::Cleanup()
{
	WindowCommander* wc = GetRuntime()->GetFramesDocument()->GetWindow()->GetWindowCommander();
	if (wc)
		wc->GetFileSelectionListener()->OnDomFilesystemFilesCancel(wc);
}

OP_STATUS
DOM_FileSystem::ReadPersistentData()
{
	UINT32 i = 0;
	OpString key;
	OpString data;

	if (!m_persistent_storage_listener)
		return OpStatus::OK;

	while (m_persistent_storage_listener->GetPersistentDataItem(i++, key, data) == OpStatus::OK)
	{
		DOM_GadgetFile *new_file;
		RETURN_IF_ERROR(DOM_GadgetFile::MakeMountPoint(new_file, GetRuntime(), m_mountpoints, key.CStr(), data.CStr(), m_persistent_storage_listener));
		RETURN_IF_ERROR(new_file->SetIsPersistent(TRUE));
		RETURN_IF_ERROR(AddMountPoint(new_file));
	};

	return OpStatus::OK;
}

void
DOM_FileSystem::OnFilesSelected(const OpAutoVector<OpString>* files)
{
	BOOL cancelled = TRUE;

	if (files && files->GetCount() == 1 && m_browse_callback)
	{
		const uni_char* path = files->Get(0)->CStr();
		BOOL exists = TRUE;

		if (m_fd_browse_type != BROWSE_FOR_SAVE)
		{
			OpFile dir;

			RETURN_VOID_IF_ERROR(dir.Construct(path, OPFILE_ABSOLUTE_FOLDER));
			RETURN_VOID_IF_ERROR(dir.Exists(exists));
		}

		if (exists)
		{
			DOM_GadgetFile *new_file;
			RETURN_VOID_IF_ERROR(DOM_GadgetFile::MakeMountPoint(new_file, GetRuntime(), m_mountpoints, m_fd_name.CStr(), path, m_persistent_storage_listener));
			RETURN_VOID_IF_ERROR(new_file->SetIsPersistent(m_fd_persistent));
			RETURN_VOID_IF_ERROR(AddMountPoint(new_file));

			ES_Value arguments[1];
			DOMSetObject(&arguments[0], new_file);
			RETURN_VOID_IF_ERROR(GetEnvironment()->GetAsyncInterface()->CallFunction(m_browse_callback, *this, 1, arguments, NULL, DOM_Object::GetCurrentThread(GetRuntime())));

			cancelled = FALSE;
		}
	}

	if (cancelled && m_browse_callback)
	{
		// user cancelled
		ES_Value arguments[1];
		DOMSetNull(&arguments[0]);
		RETURN_VOID_IF_ERROR(GetEnvironment()->GetAsyncInterface()->CallFunction(m_browse_callback, *this, 1, arguments, NULL, DOM_Object::GetCurrentThread(GetRuntime())));
	}
}

const OpFileSelectionListener::MediaType*
DOM_FileSystem::GetMediaType(unsigned int index)
{
	if (index == 0)
		return &m_fd_mediatype;
	else
		return NULL;
}

const uni_char*
DOM_FileSystem::GetInitialPath()
{
	return m_fd_location.CStr();
}

OpFileSelectionListener::DomFilesystemCallback::SelectionType
DOM_FileSystem::GetSelectionType()
{
	switch (m_fd_browse_type)
	{
	case BROWSE_FOR_DIRECTORY:
		return OpFileSelectionListener::DomFilesystemCallback::GET_MOUNTPOINT;
	default:
	case BROWSE_FOR_FILE:
		return OpFileSelectionListener::DomFilesystemCallback::OPEN_FILE;
	case BROWSE_FOR_MULTIPLE_FILES:
		return OpFileSelectionListener::DomFilesystemCallback::OPEN_MULTIPLE_FILES;
	case BROWSE_FOR_SAVE:
		return OpFileSelectionListener::DomFilesystemCallback::SAVE_FILE;
	}
}

const uni_char*
DOM_FileSystem::GetCaption()
{
	return m_fd_name.CStr();
}

/* virtual */ ES_GetState
DOM_FileSystem::GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_mountPoints:
		DOMSetObject(value, m_mountpoints);
		return GET_SUCCESS;
	}

	return DOM_Object::GetName(property_name, value, origining_runtime);
}

/* virtual */ ES_PutState
DOM_FileSystem::PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_mountPoints:
		return PUT_READ_ONLY;
	}

	return DOM_Object::PutName(property_name, value, origining_runtime);
}

/* virtual */ void
DOM_FileSystem::GCTrace()
{
	GCMark(m_browse_callback);
	GCMark(m_mountpoints);
}

OP_STATUS
DOM_FileSystem::AddMountPoint(DOM_GadgetFile *mp)
{
	return m_mountpoints->AddNode(mp);
}

OP_STATUS
DOM_FileSystem::RemoveMountPoint(DOM_GadgetFile *mp)
{
	OP_ASSERT(!mp->HasCapability(DOM_GadgetFile::ROOT));	// It's illegal to explicitly remove the root entry.

	// Make mp invalid as a mp
	mp->SetIsPersistent(FALSE);
	mp->MakeInvalid();
	return m_mountpoints->DeleteNode(mp, TRUE);
}

DOM_GadgetFile*
DOM_FileSystem::GetMountPoint(const uni_char *name)
{
	DOM_GadgetFile *f;

	for (int i = 0; (f = m_mountpoints->GetNode(i)) != NULL; i++)
		if (uni_strcmp(f->Name(), name) == 0)
			return f;

	return NULL;
}

OP_STATUS
DOM_FileSystem::RemoveMountPoints()
{
	return m_mountpoints->DeleteNodes();
}

/* static */ int
DOM_FileSystem::addMountPoint(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value *return_value, DOM_Runtime *origining_runtime)
{
	DOM_THIS_OBJECT(fs, DOM_TYPE_FILESYSTEM, DOM_FileSystem);

	const uni_char *path = (argc > 0 && argv[0].type == VALUE_STRING) ? argv[0].value.string : NULL;
	const uni_char *name = (argc > 1 && argv[1].type == VALUE_STRING) ? argv[1].value.string : NULL;
	BOOL persistent = argc > 2 && argv[2].type == VALUE_BOOLEAN && argv[2].value.boolean;

	if (path)
	{
		DOM_GadgetFile *file;
		CALL_FAILED_IF_ERROR(DOM_GadgetFile::MakeMountPoint(file, fs->GetRuntime(), fs->m_mountpoints, name, path, fs->m_persistent_storage_listener));
		CALL_FAILED_IF_ERROR(file->SetIsPersistent(persistent));
		CALL_FAILED_IF_ERROR(fs->AddMountPoint(file));

		DOMSetObject(return_value, file);
	}
	else
		DOMSetNull(return_value);

	return ES_VALUE; // Returns DOM_GadgetFile
}

/* static */ int
DOM_FileSystem::getSpecialFile(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value *return_value, DOM_Runtime *origining_runtime)
{
	int type = (argc > 0 && argv[0].type == VALUE_NUMBER) ? (int) argv[0].value.number : 1;

	DOM_GadgetFile *obj;

	switch (type)
	{
	case 1:
		CALL_FAILED_IF_ERROR(DOM_GadgetFile::MakeVirtual(obj, origining_runtime));
		break;
	case 2:
		CALL_FAILED_IF_ERROR(DOM_GadgetFile::MakeRoot(obj, origining_runtime));
		break;
	default:
		obj = NULL;
	}

	DOMSetObject(return_value, obj);
	return ES_VALUE;

}

/* static */ int
DOM_FileSystem::removeMountPoint(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value *return_value, DOM_Runtime *origining_runtime)
{
	DOM_THIS_OBJECT(fs, DOM_TYPE_FILESYSTEM, DOM_FileSystem);

	if (argc < 1)
		return DOM_CALL_FILEAPIEXCEPTION(WRONG_ARGUMENTS_ERR);

	DOM_GadgetFile *file = NULL;

	if (argv[0].type == VALUE_OBJECT)
	{
		DOM_Object *obj = DOM_GetHostObject(argv[0].value.object);
		if (obj && obj->IsA(DOM_TYPE_GADGETFILE))
			file = static_cast<DOM_GadgetFile*>(obj);
	}
	else if (argv[0].type == VALUE_STRING && argv[0].value.string)
	{
		OpString name;
		CALL_FAILED_IF_ERROR(DFA_Utils::SecurePathConcat(name, argv[0].value.string, TRUE));

		DOM_GadgetFile *f;
		for (int i = 0; (f = fs->m_mountpoints->GetNode(i)) != NULL; i++)
		{
			if (name.Compare(f->Name()) == 0)
			{
				file = f;
				break;
			}
		}
	}

	if (file)
	{
		if (file->HasCapability(DOM_GadgetFile::ROOT)) // It's not allowed to remove the root entry, throw exception
			return ES_EXCEPT_SECURITY;

		CALL_FAILED_IF_ERROR(fs->RemoveMountPoint(file));
	}

	return ES_FAILED; // no return value
}

/* static */ int
DOM_FileSystem::browseFor(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value *return_value, DOM_Runtime *origining_runtime, int data)
{
	DOM_THIS_OBJECT(fs, DOM_TYPE_FILESYSTEM, DOM_FileSystem);
	DOM_CHECK_ARGUMENTS("ssO|bb");

	const uni_char *name     = argv[0].value.string;
	const uni_char *location = argv[1].value.string;
	fs->m_browse_callback    = (argv[2].type == VALUE_OBJECT ? argv[2].value.object : NULL);
	fs->m_fd_persistent      = (argc > 3 && argv[3].value.boolean);
	BOOL multiple            = (argc > 4 && argv[4].value.boolean);

	if (JS_Window::IsUnrequestedThread(GetCurrentThread(origining_runtime)))
		return ES_FAILED;

	if (name[uni_strcspn(name, UNI_L("<>:/\"|?*^`{}!/"))])
		return DOM_CALL_DOMEXCEPTION(INVALID_CHARACTER_ERR);

	switch (data)
	{
	default:
	case 0: /* browseForDirectory */
		fs->m_fd_browse_type = BROWSE_FOR_DIRECTORY;
		break;
	case 1: /* browseForFile */
		fs->m_fd_browse_type = multiple ? BROWSE_FOR_MULTIPLE_FILES : BROWSE_FOR_FILE;
		break;
	case 2: /* browseForSave */
		fs->m_fd_browse_type = BROWSE_FOR_SAVE;
		break;
	}

	fs->m_fd_name.Empty();
	fs->m_fd_location.Empty();
	CALL_FAILED_IF_ERROR(DFA_Utils::SecurePathConcat(fs->m_fd_name, name, TRUE));
	CALL_FAILED_IF_ERROR(DFA_Utils::SecurePathConcat(fs->m_fd_location, location));
	WindowCommander* wc = fs->GetRuntime()->GetFramesDocument()->GetWindow()->GetWindowCommander();
	wc->GetFileSelectionListener()->OnDomFilesystemFilesRequest(wc, fs);

	return ES_FAILED; // no return value
}

/* static */ int
DOM_FileSystem::mountSystemDirectory(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value *return_value, DOM_Runtime *origining_runtime)
{
	DOM_THIS_OBJECT(fs, DOM_TYPE_FILESYSTEM, DOM_FileSystem);

	const uni_char *folder = (argc > 0 && argv[0].value.string) ? argv[0].value.string : UNI_L("");
	const uni_char *name = (argc > 1 && argv[1].value.string) ? argv[1].value.string : folder;

	OpString escaped_name;
	CALL_FAILED_IF_ERROR(DFA_Utils::EscapeString(escaped_name, name));
	name = escaped_name.CStr();

	DOM_GadgetFile* mp;
	CALL_FAILED_IF_ERROR(fs->m_mountpoints->FindNode(mp, name, 0));

	if (mp)
	{
		if (mp->Name() && uni_strcmp(mp->Name(), name) == 0)
		{
			if (mp->SysFolder() && uni_strcmp(mp->SysFolder(), folder) == 0)
			{
				// re-mounting
				DOMSetObject(return_value, mp);
				return ES_VALUE;
			}
			else if (mp->SysFolder())
				return DOM_CALL_FILEAPIEXCEPTION(GENERIC_ERR);
		}
	}

	if (!fs->m_persistent_storage_listener)
		return DOM_CALL_FILEAPIEXCEPTION(RESOURCE_UNAVAILABLE_ERR);

	OpString path;
	BOOL read_only = FALSE;

	if (uni_str_eq("storage", folder))
	{
		CALL_FAILED_IF_ERROR(fs->m_persistent_storage_listener->GetStoragePath(path));
		CALL_FAILED_IF_ERROR(path.Append("/storage"));
	}
	else if (uni_str_eq("application", folder))
	{
		read_only = TRUE;
		CALL_FAILED_IF_ERROR(fs->m_persistent_storage_listener->GetApplicationPath(path));
	}
#if defined(WEBSERVER_SUPPORT) && defined(GADGET_SUPPORT)
	else if (uni_str_eq("rootService", folder))
	{
		read_only = TRUE;

		UINT i;
		for (i = 0; i < g_gadget_manager->NumGadgets(); i++)
		{
			OpGadget* gadget = g_gadget_manager->GetGadget(i);
			if (gadget && gadget->IsRootService())
			{
				CALL_FAILED_IF_ERROR(path.Set(gadget->GetGadgetPath()));
			}
		}
	}
#endif // WEBSERVER_SUPPORT && GADGET_SUPPORT
	else if (uni_str_eq("shared", folder))
	{
		OpGadget *gadget = origining_runtime->GetFramesDocument()->GetWindow()->GetGadget();
		if (gadget)
			CALL_FAILED_IF_ERROR(path.Set(gadget->GetSharedFolder()));
		else
			return DOM_CALL_FILEAPIEXCEPTION(GENERIC_ERR);
	}

	if (path.IsEmpty())
	{
		// No path was returned, just return null
		mp = NULL;
	}
	else
	{
		CALL_FAILED_IF_ERROR(DOM_GadgetFile::MakeMountPoint(mp, fs->GetRuntime(), fs->m_mountpoints, name, path.CStr(), fs->m_persistent_storage_listener));
		CALL_FAILED_IF_ERROR(fs->AddMountPoint(mp));
		CALL_FAILED_IF_ERROR(mp->SetSysFolder(folder));

		if (read_only)
			mp->SetCapability(DOM_GadgetFile::READONLY);
	}

	DOMSetObject(return_value, mp);
	return ES_VALUE;
}

/************************************************************************/
/* class DOM_GadgetFile                                                       */
/************************************************************************/

/* static */ OP_STATUS
DOM_GadgetFile::Make(DOM_GadgetFile*& new_obj, DOM_Runtime *origining_runtime, DOM_GadgetFile *parent)
{
	new_obj = OP_NEW(DOM_GadgetFile, (parent));

	if (!new_obj)
		return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_ERROR(DOMSetObjectRuntime(new_obj, origining_runtime, origining_runtime->GetPrototype(DOM_Runtime::GADGETFILE_PROTOTYPE), "GadgetFile"));

	new_obj->GetEnvironment()->AddFileObject(new_obj);

	return OpStatus::OK;
}

/* static */ OP_STATUS
DOM_GadgetFile::MakeMountPoint(DOM_GadgetFile*& new_obj, DOM_Runtime *origining_runtime, DOM_GadgetFile *parent, const uni_char *name, const uni_char *path, OpPersistentStorageListener *listener)
{
	RETURN_IF_ERROR(DOM_GadgetFile::Make(new_obj, origining_runtime, parent));
	RETURN_IF_ERROR(new_obj->Initialize(name, path));

	RETURN_IF_ERROR(MountPoint::Make(new_obj->m_mp_handler, name, new_obj->m_win_id, path));

	OpStatus::Ignore(new_obj->UpdateFileType());
	new_obj->SetCapability(MOUNTPOINT | DIRECTORY);
	new_obj->SetPersistentStorageListener(listener);

	return OpStatus::OK;
}

/* static */ OP_STATUS
DOM_GadgetFile::MakePhysical(DOM_GadgetFile*& new_obj, DOM_Runtime *origining_runtime, DOM_GadgetFile *parent, const uni_char *name, const uni_char *path)
{
	RETURN_IF_ERROR(DOM_GadgetFile::Make(new_obj, origining_runtime, parent));
	RETURN_IF_ERROR(new_obj->Initialize(name, path));

	OpStatus::Ignore(new_obj->UpdateFileType());

	return OpStatus::OK;
}

/* static */ OP_STATUS
DOM_GadgetFile::MakeVirtual(DOM_GadgetFile*& new_obj, DOM_Runtime *origining_runtime)
{
	RETURN_IF_ERROR(DOM_GadgetFile::Make(new_obj, origining_runtime, NULL));
	RETURN_IF_ERROR(new_obj->Initialize(UNI_L(""), UNI_L("")));

	new_obj->SetCapabilities(VIRTUAL | DIRECTORY);

	return OpStatus::OK;
}

/* static */ OP_STATUS
DOM_GadgetFile::MakeRoot(DOM_GadgetFile*& new_obj, DOM_Runtime *origining_runtime)
{
	RETURN_IF_ERROR(DOM_GadgetFile::Make(new_obj, origining_runtime, NULL));
	RETURN_IF_ERROR(new_obj->Initialize(UNI_L(""), UNI_L("/")));

	new_obj->SetCapabilities(ROOT | DIRECTORY);

	return OpStatus::OK;
}

DOM_GadgetFile::DOM_GadgetFile(DOM_GadgetFile *parent)
	: m_capabilities(NONE)
	, m_persistent(FALSE)
	, m_persistent_storage_listener(NULL)
	, m_parent(parent)
	, m_meta_data(NULL)
#ifdef WEBSERVER_SUPPORT
	, m_res_desc(NULL)
#endif // WEBSERVER_SUPPORT
	, m_win_id(0)
	, m_mp_handler(NULL)
	, m_folder_lister(NULL)
{
}

OP_STATUS
DOM_GadgetFile::Initialize(const uni_char *name, const uni_char *path)
{
	if (name)
		RETURN_IF_ERROR(DFA_Utils::SecurePathConcat(m_name, name, FALSE));

	if (path)
		RETURN_IF_ERROR(DFA_Utils::SecurePathConcat(m_path, path, FALSE));

	FramesDocument *frm_doc = GetFramesDocument();
	m_win_id = frm_doc->GetWindow()->Id();

	return OpStatus::OK;
}

DOM_GadgetFile::~DOM_GadgetFile()
{
	DOM_FileBase::Out();

	MakeInvalid();

	OP_DELETE(m_folder_lister);
}

/* virtual */ void
DOM_GadgetFile::GCTrace()
{
	GCMark(event_target);

	for (UINT32 i = 0; i < m_nodes.GetCount(); i++)
		GCMark(m_nodes.Get(i));

	GCMark(m_parent);
	GCMark(m_meta_data);
}

/* virtual */ ES_GetState
DOM_GadgetFile::GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_readOnly:
		if (HasCapability(VIRTUAL|ROOT|READONLY))
			DOMSetBoolean(value, TRUE);
		else
		{
			OpString path;
			OpFile opfile;

			if (OpStatus::IsSuccess(GetPhysicalPath(path)) &&
				OpStatus::IsSuccess(opfile.Construct(path.CStr(), OPFILE_ABSOLUTE_FOLDER)))
			{
				DOMSetBoolean(value, !opfile.IsWritable());
			}
			else
				DOMSetNull(value);
		}
		return GET_SUCCESS;

	case OP_ATOM_exists:
		if (HasCapability(VIRTUAL | ROOT))
			DOMSetBoolean(value, FALSE);
		else
		{
			OpString path;
			OpFile opfile;
			BOOL exists = FALSE;

			if (OpStatus::IsSuccess(GetPhysicalPath(path)) &&
				OpStatus::IsSuccess((opfile.Construct(path.CStr(), OPFILE_ABSOLUTE_FOLDER))))
			{
				GET_FAILED_IF_ERROR(opfile.Exists(exists));
			}
			DOMSetBoolean(value, exists);
		}
		return GET_SUCCESS;

	case OP_ATOM_isFile:
		DOMSetBoolean(value, IsFile() && Exists());
		return GET_SUCCESS;

	case OP_ATOM_isDirectory:
		DOMSetBoolean(value, IsDirectory() && Exists());
		return GET_SUCCESS;

	case OP_ATOM_isArchive:
		DOMSetBoolean(value, IsArchive() && Exists());
		return GET_SUCCESS;

	case OP_ATOM_isHidden:
		DOMSetBoolean(value, IsHidden() && Exists());
		return GET_SUCCESS;

	case OP_ATOM_created:
		if (value)
		{
			OpString path;
			OpFile opfile;
			OpFileInfo info;

			info.flags = OpFileInfo::CREATION_TIME;
			info.creation_time = 0;

			if (OpStatus::IsSuccess(GetPhysicalPath(path)) &&
				OpStatus::IsSuccess(opfile.Construct(path.CStr(), OPFILE_ABSOLUTE_FOLDER)) &&
				OpStatus::IsSuccess(opfile.GetFileInfo(&info)))
			{
				ES_Value date_value;
				// double arithmetics
				DOMSetNumber(&date_value, static_cast<double>(info.creation_time * 1000.0));
				ES_Object *date_obj;
				GET_FAILED_IF_ERROR(GetRuntime()->CreateNativeObject(date_value, ENGINE_DATE_PROTOTYPE, &date_obj));
				DOMSetObject(value, date_obj);
			}
			else
				DOMSetNull(value);
		}
		return GET_SUCCESS;

	case OP_ATOM_modified:
		if (value)
		{
			OpString path;
			OpFile opfile;
			OpFileInfo info;

			info.flags = OpFileInfo::LAST_MODIFIED;
			info.last_modified = 0;

			if (OpStatus::IsSuccess(GetPhysicalPath(path)) &&
				OpStatus::IsSuccess(opfile.Construct(path.CStr(), OPFILE_ABSOLUTE_FOLDER)) &&
				OpStatus::IsSuccess(opfile.GetFileInfo(&info)))
			{
				ES_Value date_value;
				// double arithmetics
				DOMSetNumber(&date_value, static_cast<double>(info.last_modified * 1000.0));
				ES_Object *date_obj;
				GET_FAILED_IF_ERROR(GetRuntime()->CreateNativeObject(date_value, ENGINE_DATE_PROTOTYPE, &date_obj));
				DOMSetObject(value, date_obj);
			}
			else
				DOMSetNull(value);
		}
		return GET_SUCCESS;

	case OP_ATOM_length:
		DOMSetNumber(value, m_nodes.GetCount());
		return GET_SUCCESS;

	case OP_ATOM_fileSize:
		if (HasCapability(FILE))
		{
			OpString path;
			OpFile file;
			OpFileLength len;

			if (OpStatus::IsSuccess(GetPhysicalPath(path)) &&
				OpStatus::IsSuccess(file.Construct(path.CStr(), OPFILE_ABSOLUTE_FOLDER)) &&
				OpStatus::IsSuccess(file.GetFileLength(len)))
			{
				DOMSetNumber(value, static_cast<double>(len));
			}
			else
				DOMSetNull(value);
		}
		else
			DOMSetNumber(value, 0);
		return GET_SUCCESS;

	case OP_ATOM_name:
		GET_FAILED_IF_ERROR(DFA_Utils::SetEscapedString(this, value, m_name.CStr()));
		return GET_SUCCESS;

	case OP_ATOM_path:
		if (value)
		{
			OpString path;
			GET_FAILED_IF_ERROR(path.Set(UNI_L("mountpoint:/")));
			GET_FAILED_IF_ERROR(GetVisiblePath(path));
			GET_FAILED_IF_ERROR(DFA_Utils::SetEscapedString(this, value, path.CStr()));
		}
		return GET_SUCCESS;

	case OP_ATOM_parent:
		DOMSetObject(value, m_parent);
		return GET_SUCCESS;

	case OP_ATOM_metaData:
		DOMSetObject(value, m_meta_data);
		return GET_SUCCESS;

	case OP_ATOM_maxPathLength:
		if (value)
		{
			OpString path;
			GET_FAILED_IF_ERROR(GetPhysicalPath(path));

			INT32 maxlen = _MAX_PATH - path.Length();

			DOMSetNumber(value, maxlen);
		}
		return GET_SUCCESS;

	case OP_ATOM_nativePath:
		if (value)
		{
			if (!IsSystem())
			{
				TempBuffer *buffer = GetEmptyTempBuf();
				OpString path;

				GET_FAILED_IF_ERROR(GetPhysicalPath(path));

				for (uni_char* t = path.CStr(); t && *t; t++)	// De-normalize the string. Back to the platform standard
					if (*t == '/')
						*t = PATHSEPCHAR;

				GET_FAILED_IF_ERROR(buffer->Append(path.CStr()));

				DOMSetString(value, buffer);
			}
			else
				DOMSetString(value, UNI_L(""));
		}
		return GET_SUCCESS;
	}

	return DOM_Object::GetName(property_name, value, origining_runtime);
}

/* virtual */ ES_PutState
DOM_GadgetFile::PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_parent:
	case OP_ATOM_readOnly:
	case OP_ATOM_exists:
	case OP_ATOM_isFile:
	case OP_ATOM_isDirectory:
	case OP_ATOM_isArchive:
	case OP_ATOM_isHidden:
	case OP_ATOM_name:
	case OP_ATOM_path:
	case OP_ATOM_fileSize:
	case OP_ATOM_length:
	case OP_ATOM_maxPathLength:
	case OP_ATOM_nativePath:
		return PUT_READ_ONLY;

#ifdef SUPPORT_OPFILEINFO_CHANGE
	case OP_ATOM_created:
		if (value->type == VALUE_OBJECT)
		{
			if (op_strcmp(ES_Runtime::GetClass(value->value.object), "Date") != 0)
				return PUT_FAILED;

			OpString path;
			OpFile opfile;
			OP_BOOLEAN result;

			ES_Value val;
			PUT_FAILED_IF_ERROR(result = GetRuntime()->GetNativeValueOf(value->value.object, &val));

			if (result == OpBoolean::IS_TRUE)
			{
				OpFileInfoChange change;
				change.flags = OpFileInfoChange::CREATION_TIME;
				change.creation_time = static_cast<time_t>(val.value.number / 1000.0);

				PUT_FAILED_IF_ERROR(GetPhysicalPath(path));
				PUT_FAILED_IF_ERROR(opfile.Construct(path.CStr(), OPFILE_ABSOLUTE_FOLDER));
				PUT_FAILED_IF_ERROR(opfile.ChangeFileInfo(&change));
			}

			return PUT_SUCCESS;
		}
		else
			return PUT_FAILED;

	case OP_ATOM_modified:
		if (value->type == VALUE_OBJECT)
		{
			if (op_strcmp(ES_Runtime::GetClass(value->value.object), "Date") != 0)
				return PUT_FAILED;

			OpString path;
			OpFile opfile;

			ES_Value val;
			PUT_FAILED_IF_ERROR(GetRuntime()->GetNativeValueOf(value->value.object, &val));

			OpFileInfoChange change;
			change.flags = OpFileInfoChange::LAST_MODIFIED;
			change.last_modified = static_cast<time_t>(val.value.number / 1000.0);

			PUT_FAILED_IF_ERROR(GetPhysicalPath(path));
			PUT_FAILED_IF_ERROR(opfile.Construct(path.CStr(), OPFILE_ABSOLUTE_FOLDER));
			PUT_FAILED_IF_ERROR(opfile.ChangeFileInfo(&change));

			return PUT_SUCCESS;
		}
		else
			return PUT_FAILED;
#endif // SUPPORT_OPFILEINFO_CHANGE

	case OP_ATOM_metaData:
		if (IsMountPoint())
			return PUT_READ_ONLY;
		else if (value->type == VALUE_OBJECT)
		{
			m_meta_data = DOM_GetHostObject(value->value.object);
			return m_meta_data ? PUT_SUCCESS : PUT_FAILED;
		}
		return PUT_FAILED;
	}

	return DOM_Object::PutName(property_name, value, origining_runtime);
}

/* virtual */ ES_GetState
DOM_GadgetFile::GetIndex(int property_index, ES_Value *value, ES_Runtime *origining_runtime)
{
	if (property_index >= 0 && property_index < (int) m_nodes.GetCount())
	{
		DOMSetObject(value, m_nodes.Get(property_index));
		return GET_SUCCESS;
	}

	return DOM_Object::GetIndex(property_index, value, origining_runtime);
}

OP_STATUS
DOM_GadgetFile::DeleteNode(DOM_GadgetFile *dom_file, BOOL recursive)
{
	if (dom_file->m_nodes.GetCount() > 0)
	{
		if (recursive)
			for (UINT32 i = 0; i < m_nodes.GetCount(); i++)
			{
				RETURN_IF_ERROR(m_nodes.Get(i)->DeleteNodes(TRUE));
				m_nodes.Remove(i);
			}
	}
	else
		m_nodes.RemoveByItem(dom_file);

	return OpStatus::OK;
}

OP_STATUS
DOM_GadgetFile::DeleteNodes(BOOL recursive)
{
	if (recursive)
		for (UINT32 i = 0; i < m_nodes.GetCount(); i++)
			RETURN_IF_ERROR(m_nodes.Get(i)->DeleteNodes(TRUE));

	m_nodes.Clear();

	return OpStatus::OK;
}

OP_STATUS
DOM_GadgetFile::FindNode(DOM_GadgetFile*& dom_file, const uni_char *path, INT32 level, const uni_char **rest, DOM_GadgetFile **last_node)
{
	dom_file = NULL;

	if (!path || *path == 0)
		return OpStatus::OK;

	if (*path == '/')
		if (m_parent)
			return m_parent->FindNode(dom_file, path, level, rest, last_node);
		else
			path++;

	const uni_char *end = uni_strchr(path, '/');
	if (!end)
		end = uni_strchr(path, 0);

	if (path != end)
	{
		OpString tmp;
		RETURN_IF_ERROR(tmp.Set(path, end - path));

		for (UINT32 i = 0; i < m_nodes.GetCount(); i++)
		{
			DOM_GadgetFile *current_node = m_nodes.Get(i);

			if (current_node->m_name.Compare(tmp) == 0)
			{
				if (end && *end && level != 0)
				{
					if (rest)
						*rest = end+1;

					if (last_node)
						*last_node = current_node;

					return current_node->FindNode(dom_file, end+1, --level, rest, last_node);
				}
				else
				{
					dom_file = m_nodes.Get(i);
					return OpStatus::OK;
				}
			}
		}
	}
	else if (!m_parent)
		// We're the root object, return ourself
		dom_file = this;

	return OpStatus::OK;
}

DOM_GadgetFile*
DOM_GadgetFile::FindParentNode(INT32 capability)
{
	DOM_GadgetFile* obj = this;
	while (obj && !obj->HasCapability(capability))
	{
		obj = obj->m_parent;
	}

	return obj;
}

OP_STATUS
DOM_GadgetFile::ParseNodes(DOM_GadgetFile*& last_node, const uni_char *path)
{
	// Expects a string looking like "somefile", "somedir/", "somedir/somefile" or "somedir/somedir/" etc.

	if (!path ||* path == 0)
		return OpStatus::OK;

	BOOL isDirectory = FALSE;
	const uni_char *end = uni_strchr(path, '/');

	if (end)
		isDirectory = TRUE;
	else
		end = uni_strchr(path, 0);

	if (end)
	{
		OpString tmp;
		RETURN_IF_ERROR(tmp.Set(path, end - path));

		RETURN_IF_ERROR(FindNode(last_node, tmp.CStr()));

		if (!last_node)
		{
			OpString new_path;
			RETURN_IF_ERROR(GetPhysicalPath(new_path));
			RETURN_IF_ERROR(DFA_Utils::SecurePathConcat(new_path, tmp.CStr()));

			RETURN_IF_ERROR(DOM_GadgetFile::MakePhysical(last_node, GetRuntime(), this, tmp.CStr(), new_path.CStr()));
			last_node->SetCapability(isDirectory ? DIRECTORY : FILE);
		}

		if (*end)
			RETURN_IF_ERROR(last_node->ParseNodes(last_node, end + 1));
	}

	return OpStatus::OK;
}

OP_STATUS
DOM_GadgetFile::GetPhysicalPath(OpString& path)
{
	return path.Set(m_path);
}

OP_STATUS
DOM_GadgetFile::GetVisiblePath(OpString& path)
{
	return BuildPath(path);
}

OP_STATUS
DOM_GadgetFile::BuildPath(OpString& path)
{
	if (m_parent)
		RETURN_IF_ERROR(m_parent->BuildPath(path));

	if (!m_name.IsEmpty())
	{
		RETURN_IF_ERROR(path.Append(UNI_L("/")));
		RETURN_IF_ERROR(path.Append(m_name));
	}

	return OpStatus::OK;
}

OP_STATUS
DOM_GadgetFile::EnsurePathExists()
{
	OpString path;
	RETURN_IF_ERROR(GetPhysicalPath(path));

	if (path.HasContent())
	{
		OpFile opfile;
		BOOL exists = FALSE;
		RETURN_IF_ERROR(opfile.Construct(path.CStr(), OPFILE_ABSOLUTE_FOLDER));
		if (OpStatus::IsSuccess(opfile.Exists(exists)) && !exists)
			RETURN_IF_ERROR(opfile.MakeDirectory());
	}

	return OpStatus::OK;
}

OP_STATUS
DOM_GadgetFile::UpdateFileType()
{
	if (HasCapability(ROOT | MOUNTPOINT | VIRTUAL) || m_path.IsEmpty())
		return OpStatus::OK;

	OpString path;
	OpFile file;
	BOOL exists = FALSE;

	RETURN_IF_ERROR(GetPhysicalPath(path));
	if (OpStatus::IsSuccess(file.Construct(path.CStr(), OPFILE_ABSOLUTE_FOLDER)))
		OpStatus::Ignore(file.Exists(exists));

	if (!exists)
		return OpStatus::OK;

	OpFileInfo fileinfo;
	fileinfo.flags = OpFileInfo::HIDDEN | OpFileInfo::MODE;

	if (OpStatus::IsError(file.GetFileInfo(&fileinfo)))
		return OpStatus::OK;

	ClearCapability(DIRECTORY | FILE | ARCHIVE);

	if (fileinfo.hidden)
		SetCapability(HIDDEN);

	if (fileinfo.mode == OpFileInfo::DIRECTORY)
	{
		SetCapability(DIRECTORY);
	}
	else if (fileinfo.mode == OpFileInfo::FILE)
	{
		SetCapability(FILE);

		BOOL isArchive = FALSE;
		RETURN_IF_ERROR(CheckIsArchive(isArchive));
		if (isArchive)
			SetCapability(ARCHIVE);
	}

	return OpStatus::OK;
}

BOOL
DOM_GadgetFile::IsInArchive()
{
	if (m_capabilities & ARCHIVE)
		return TRUE;
	else if (m_parent)
		return m_parent->IsInArchive();

	return FALSE;
}

OP_STATUS
DOM_GadgetFile::CheckIsArchive(BOOL& archive)
{
	archive = FALSE;
	OpString path;

	RETURN_IF_ERROR(GetPhysicalPath(path));
	if (!path.IsEmpty())
	{
		OpZip zipFile;
		if (OpStatus::IsSuccess(zipFile.Open(&path, FALSE)))
			archive = TRUE;
	}

	return OpStatus::OK;
}

BOOL
DOM_GadgetFile::IsReadOnly()
{
	return FindParentNode(READONLY) ? TRUE : FALSE;
}

BOOL
DOM_GadgetFile::IsSystem()
{
	return FindParentNode(SYSTEM) ? TRUE : FALSE;
}

BOOL
DOM_GadgetFile::Exists()
{
	OpString path;
	OpFile file;
	BOOL exists = FALSE;

	if (OpStatus::IsSuccess(GetPhysicalPath(path)) &&
		OpStatus::IsSuccess(file.Construct(path.CStr(), OPFILE_ABSOLUTE_FOLDER)) &&
		OpStatus::IsSuccess(file.Exists(exists))
	   )
		return exists;
	else
		return FALSE;

}

OP_STATUS
DOM_GadgetFile::GetMode(OpFileInfo::Mode& mode)
{
	OpString path;
	OpFile file;

	RETURN_IF_ERROR(GetPhysicalPath(path));
	RETURN_IF_ERROR(file.Construct(path.CStr(), OPFILE_ABSOLUTE_FOLDER));
	RETURN_IF_ERROR(file.GetMode(mode));

	return OpStatus::OK;
}

OP_STATUS
DOM_GadgetFile::SetIsPersistent(BOOL persistent)
{
	if (persistent && !m_persistent)
		RETURN_IF_ERROR(AddToPersistentData());
	else if (m_persistent && !persistent)
		RETURN_IF_ERROR(RemoveFromPersistentData());

	m_persistent = persistent;

	return OpStatus::OK;
}

OP_STATUS
DOM_GadgetFile::AddToPersistentData()
{
	if (m_persistent_storage_listener && IsMountPoint() && m_name.HasContent() && m_path.HasContent())
		return m_persistent_storage_listener->SetPersistentData(UNI_L("MountPoints"), m_name.CStr(), m_path.CStr());

	return OpStatus::OK;
}

OP_STATUS
DOM_GadgetFile::RemoveFromPersistentData()
{
	if (m_persistent_storage_listener && IsMountPoint() && m_name.HasContent())
		return m_persistent_storage_listener->DeletePersistentData(UNI_L("MountPoints"), m_name.CStr());

	return OpStatus::OK;
}

OP_STATUS
DOM_GadgetFile::InternalResolve(DOM_GadgetFile*& dom_file, const uni_char *path_arg)
{
	// some special cases:
	if (uni_str_eq(".", path_arg) || *path_arg == 0)
	{
		dom_file = this;
		return OpStatus::OK;
	}
	else if (uni_str_eq("..", path_arg))
	{
		dom_file = m_parent;
		return dom_file ? OpStatus::OK : OpStatus::ERR; // if no parent; security violation
	}
	else if (uni_strncmp("mountpoint:/", path_arg, 12) == 0)
	{
		path_arg += 12;
		DOM_GadgetFile *obj = FindParentNode(ROOT);
		if (obj)
			return obj->InternalResolve(dom_file, path_arg);
		else
			return OpStatus::ERR; // security violation
	}

	OpString opstr_clean_path;
	RETURN_IF_ERROR(DFA_Utils::SecurePathConcat(opstr_clean_path, path_arg));

	const uni_char *clean_path = opstr_clean_path.CStr();

	const uni_char* rest = NULL;
	DOM_GadgetFile *last_entry = NULL;

	RETURN_IF_ERROR(FindNode(dom_file, clean_path, -1, &rest, &last_entry));

	if (!dom_file)
	{
		if ((last_entry && rest && *rest && last_entry->HasCapability(ROOT)) ||
			HasCapability(ROOT) && !last_entry)
			return OpStatus::ERR; // tried to resolve some unexisting mountpoint

		if (last_entry && last_entry->HasCapability(DIRECTORY))
		{
			if (rest && *rest)
				RETURN_IF_ERROR(last_entry->ParseNodes(dom_file, rest));
			else
				dom_file = last_entry;
		}
		else
			RETURN_IF_ERROR(ParseNodes(dom_file, clean_path));
	}

	return OpStatus::OK;
}

OP_STATUS
DOM_GadgetFile::Invalidate()
{
	return UpdateFileType();
}

void
DOM_GadgetFile::MakeInvalid()
{
	SetCapabilities(NONE);
	m_name.Empty();
	m_path.Empty();
	m_sys_folder.Empty();
	m_persistent_storage_listener = NULL;
	m_parent = NULL;
	m_meta_data = NULL;
	OP_DELETE(m_mp_handler);
	m_mp_handler = NULL;
}

void
DOM_GadgetFile::Cleanup()
{
	MakeInvalid();
}

#ifdef WEBSERVER_SUPPORT
OP_BOOLEAN
DOM_GadgetFile::Share(DOM_WebServer *webserver, const uni_char *uri)
{
	if (m_res_desc != NULL)
		return OpBoolean::IS_TRUE;

	if (!webserver->GetSubServer())
		return OpStatus::ERR;

	OpString path;
	RETURN_IF_ERROR(GetPhysicalPath(path));
	if (path.IsEmpty())
		return OpBoolean::IS_FALSE;

	m_res_desc = WebserverResourceDescriptor_Static::Make(path, uri);

	OP_BOOLEAN status = OpStatus::ERR_NO_MEMORY;
	if (!m_res_desc || OpStatus::IsError(status = webserver->GetSubServer()->AddSubserverResource(m_res_desc)) || status == OpBoolean::IS_TRUE)
	{
		OP_DELETE(m_res_desc);
		m_res_desc = NULL;
		return status;
	}

	return OpBoolean::IS_FALSE;
}

OP_STATUS
DOM_GadgetFile::Unshare(DOM_WebServer *webserver)
{
	if (m_res_desc)
	{
		if (webserver->GetSubServer() != NULL)
		{
			WebserverResourceDescriptor_Base *resource = webserver->GetSubServer()->RemoveSubserverResource(m_res_desc);
			OP_DELETE(resource);
		}
		m_res_desc = NULL;
	}

	return OpStatus::OK;
}

OP_STATUS
DOM_GadgetFile::UnshareSelf()
{
	if (m_win_id)
	{
		WebSubServer *subserver;

		if (m_res_desc && g_webserver && (subserver = g_webserver->WindowHasWebserverAssociation(m_win_id)) != NULL)
		{
			WebserverResourceDescriptor_Base *resource = subserver->RemoveSubserverResource(m_res_desc);
			OP_DELETE(resource);
			m_res_desc = NULL;
		}
	}

	return OpStatus::OK;
}
#endif // WEBSERVER_SUPPORT

OP_BOOLEAN
DOM_GadgetFile::ProcessNextFile()
{
	double start = g_op_time_info->GetRuntimeMS();

	while ( g_op_time_info->GetRuntimeMS() - start < DOM_FILE_REFRESH_LENGTH_LIMIT)
	{
		if (m_folder_lister->Next())
		{
			const uni_char *name = m_folder_lister->GetFileName();
			if (uni_str_eq(name, ".") || uni_str_eq(name, ".."))
				continue;

			OpString path;
			RETURN_IF_ERROR(GetPhysicalPath(path));
			RETURN_IF_ERROR(DFA_Utils::SecurePathConcat(path, name));

			DOM_GadgetFile *dom_file;
			RETURN_IF_ERROR(DOM_GadgetFile::MakePhysical(dom_file, GetRuntime(), this, name, path.CStr()));
			RETURN_IF_ERROR(AddNode(dom_file));
		}
		else
		{
			OP_DELETE(m_folder_lister); m_folder_lister = NULL;
			return OpBoolean::IS_TRUE;
		}
	}

	return OpBoolean::IS_FALSE;
}

/* static */ int
DOM_GadgetFile::open(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value *return_value, DOM_Runtime *origining_runtime)
{
	DOM_THIS_OBJECT(file, DOM_TYPE_GADGETFILE, DOM_GadgetFile);
	DOM_FILE_CHECK_VALIDITY(file, 0, ROOT);

	DOM_Object          *obj_arg = (argc > 0 && argv[0].type == VALUE_OBJECT) ? DOM_GetHostObject(argv[0].value.object) : NULL;
	const uni_char *file_str_arg = (argc > 0 && argv[0].type == VALUE_STRING) ? argv[0].value.string : NULL;
	const uni_char  *mode_string = (argc > 1 && argv[1].type == VALUE_STRING) ? argv[1].value.string : NULL;
	INT				 mode_number = (argc > 1 && argv[1].type == VALUE_NUMBER) ? static_cast<INT>(argv[1].value.number) : 0;
	//BOOL                  async = (argc > 2 && argv[2].type == VALUE_BOOLEAN) ? argv[2].value.boolean : FALSE;
	//BOOL     create_directories = (argc > 3 && argv[3].type == VALUE_BOOLEAN) ? argv[3].value.boolean : FALSE;

	if (!file_str_arg && (argv[0].type == VALUE_NULL || obj_arg == NULL))
		obj_arg = file;

	if ((!file_str_arg && !obj_arg) || (!mode_string && !mode_number))
		return DOM_CALL_FILEAPIEXCEPTION(WRONG_ARGUMENTS_ERR);

	DOM_GadgetFile *file_obj_arg = NULL;

	if (obj_arg)
	{
		if (obj_arg->IsA(DOM_TYPE_GADGETFILE))
			file_obj_arg = static_cast<DOM_GadgetFile*>(obj_arg);
		else
			return DOM_CALL_FILEAPIEXCEPTION(WRONG_TYPE_OF_OBJECT_ERR);
	}
	else
	{
		OpString safePath;
		CALL_FAILED_IF_ERROR(DFA_Utils::SecurePathConcat(safePath, file_str_arg, TRUE));
		OP_STATUS err = file->InternalResolve(file_obj_arg, safePath.CStr());
		if (OpStatus::IsMemoryError(err))
			return ES_NO_MEMORY;
		else if (OpStatus::IsError(err))
			return ES_EXCEPT_SECURITY;
	}

	int filemode = 0;

	if (mode_string)
	{
		while (*mode_string)
		{
			switch (*mode_string)
			{
			case 'r':
				filemode |= OPFILE_READ;
				break;
			case 'w':
				filemode |= OPFILE_WRITE;
				break;
			case 'a':
				filemode |= OPFILE_APPEND;
				break;
			case 't':
				filemode |= OPFILE_TEXT;
				break;
			case 'c':
				break; // just ignore
			case '+':
				if (filemode == OPFILE_WRITE)
				{
					filemode = OPFILE_OVERWRITE;
					break;
				}
				else if (filemode == OPFILE_READ)
				{
					filemode = OPFILE_UPDATE;
					break;
				}
				else if (filemode == OPFILE_APPEND)
				{
					filemode |= OPFILE_APPENDREAD;
					break;
				}
				// follow through
			default:
				return DOM_CALL_FILEAPIEXCEPTION(WRONG_ARGUMENTS_ERR);
			}

			mode_string++;
		}
	}
	else if (mode_number)
	{
		if (mode_number & DOM_FileMode::DOM_FILEMODE_READ)
			filemode |= OPFILE_READ;
		if (mode_number & DOM_FileMode::DOM_FILEMODE_WRITE)
			filemode |= OPFILE_WRITE;
		if (mode_number & DOM_FileMode::DOM_FILEMODE_APPEND)
			filemode |= OPFILE_APPEND;
		if (mode_number & DOM_FileMode::DOM_FILEMODE_UPDATE)
			filemode |= OPFILE_UPDATE;
	}

	if (!filemode)
		filemode = OPFILE_READ;

	// Throw exception if an illegal combination of file open flags has been used
	if (
		((filemode & OPFILE_UPDATE) && (filemode & ~OPFILE_UPDATE)) ||								// UPDATE with any other flag
		((filemode & OPFILE_APPEND) && (filemode & OPFILE_WRITE)) ||								// APPEND with WRITE
		((filemode & OPFILE_APPEND) && (filemode & OPFILE_READ) && (filemode & OPFILE_WRITE)) ||	// APPEND with OVERWRITE (READ+WRITE)
		((filemode & OPFILE_SHAREDENYREAD) && (filemode & OPFILE_READ))								// SHAREDENYREAD with READ
		)
		return DOM_CALL_FILEAPIEXCEPTION(WRONG_ARGUMENTS_ERR);

	if ((filemode & (OPFILE_WRITE|OPFILE_UPDATE|OPFILE_APPEND)) && file->IsReadOnly())
		return DOM_CALL_FILEAPIEXCEPTION(NO_ACCESS_ERR);

	filemode |= OPFILE_COMMIT;

	DOM_FileStream *fs;
	CALL_FAILED_IF_ERROR(DOM_FileStream::Make(fs, file->GetRuntime(), file));

	OpFile *opfile = fs->GetFile();

	OpString path;
	OP_STATUS err;

	THROW_FILEAPIEXCEPTION_IF_ERROR(file_obj_arg->GetPhysicalPath(path));
	THROW_FILEAPIEXCEPTION_IF_ERROR(opfile->Construct(path.CStr(), OPFILE_ABSOLUTE_FOLDER));
	if ((err = opfile->Open(filemode)) == OpStatus::ERR)
		return DOM_CALL_FILEAPIEXCEPTION(WRONG_ARGUMENTS_ERR);
	else
		THROW_FILEAPIEXCEPTION_IF_ERROR(err);

	CALL_FAILED_IF_ERROR(file_obj_arg->Invalidate());

	DOMSetObject(return_value, fs);
	return ES_VALUE;
}

/* static */ int
DOM_GadgetFile::copyTo(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value *return_value, DOM_Runtime *origining_runtime)
{
	DOM_THIS_OBJECT(file, DOM_TYPE_GADGETFILE, DOM_GadgetFile);
	DOM_FILE_CHECK_VALIDITY(file, 0, ROOT);

	DOM_Object     *obj_arg = (argc > 0 && argv[0].type == VALUE_OBJECT) ? DOM_GetHostObject(argv[0].value.object) : NULL;
	const uni_char *str_arg = (argc > 0 && argv[0].type == VALUE_STRING) ? argv[0].value.string : NULL;
	BOOL          overwrite = (argc > 1 && argv[1].type == VALUE_BOOLEAN && argv[1].value.boolean);
	//ES_Object *callback = (argc > 2 && argv[2].type == VALUE_OBJECT) ? argv[2].value.object  : NULL;	// async

	DOM_GadgetFile *dom_file = NULL;

	if (str_arg)
	{
		OpString safePath;
		CALL_FAILED_IF_ERROR(DFA_Utils::SecurePathConcat(safePath, str_arg, TRUE));
		CALL_FAILED_IF_ERROR(file->InternalResolve(dom_file, safePath.CStr()));
	}
	else if (obj_arg && obj_arg->IsA(DOM_TYPE_GADGETFILE))
	{
		OpString path;
		DOM_GadgetFile *arg = static_cast<DOM_GadgetFile*>(obj_arg);
		CALL_FAILED_IF_ERROR(arg->GetPhysicalPath(path));
		CALL_FAILED_IF_ERROR(DOM_GadgetFile::MakePhysical(dom_file, origining_runtime, file, arg->Name(), path.CStr()));
	}
	else
		return DOM_CALL_FILEAPIEXCEPTION(WRONG_ARGUMENTS_ERR);

	OpString destinationPath;
	CALL_FAILED_IF_ERROR(dom_file->GetPhysicalPath(destinationPath));

	// Ensure that the parent directory exists
	if (dom_file->IsFile() && dom_file->m_parent)
	{
		THROW_FILEAPIEXCEPTION_IF_ERROR(dom_file->m_parent->EnsurePathExists());
	}
	else if (dom_file->IsDirectory())
	{
		THROW_FILEAPIEXCEPTION_IF_ERROR(dom_file->EnsurePathExists());
		CALL_FAILED_IF_ERROR(DFA_Utils::SecurePathConcat(destinationPath, file->Name()));
	}

	OpString sourcePath;
	CALL_FAILED_IF_ERROR(file->GetPhysicalPath(sourcePath));

	OpFile src;
	THROW_FILEAPIEXCEPTION_IF_ERROR(src.Construct(sourcePath.CStr(), OPFILE_ABSOLUTE_FOLDER));

	OpFile dst;
	THROW_FILEAPIEXCEPTION_IF_ERROR(dst.Construct(destinationPath.CStr(), OPFILE_ABSOLUTE_FOLDER));
	THROW_FILEAPIEXCEPTION_IF_ERROR(dst.CopyContents(&src, !overwrite));	// !overwrite == fail_if_exists

	dom_file->Invalidate();

	DOMSetObject(return_value, dom_file);
	return ES_VALUE;
}

/* static */ int
DOM_GadgetFile::moveTo(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value *return_value, DOM_Runtime *origining_runtime)
{
	DOM_THIS_OBJECT(file, DOM_TYPE_GADGETFILE, DOM_GadgetFile);
	DOM_FILE_CHECK_VALIDITY(file, 0, ROOT);

	int res = copyTo(this_object, argv, argc, return_value, origining_runtime);
	if (res != ES_VALUE)
		return res; // exception occurred

	OpString sourcePath;
	CALL_FAILED_IF_ERROR(file->GetPhysicalPath(sourcePath));

	OpFile src;
	THROW_FILEAPIEXCEPTION_IF_ERROR(src.Construct(sourcePath.CStr(), OPFILE_ABSOLUTE_FOLDER));

	THROW_FILEAPIEXCEPTION_IF_ERROR(src.Delete(FALSE));

	file->Invalidate();

	// copyTo set the return_value
	return ES_VALUE;
}

/* static */ int
DOM_GadgetFile::createDirectory(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value *return_value, DOM_Runtime *origining_runtime)
{
	DOM_THIS_OBJECT(file, DOM_TYPE_GADGETFILE, DOM_GadgetFile);
	DOM_FILE_CHECK_VALIDITY(file, 0, ROOT);

	DOM_Object     *obj_arg = (argc > 0 && argv[0].type == VALUE_OBJECT) ? DOM_GetHostObject(argv[0].value.object) : NULL;
	const uni_char *str_arg = (argc > 0 && argv[0].type == VALUE_STRING) ? argv[0].value.string : NULL;
	//BOOL     create_parents = (argc > 1 && argv[1].type == VALUE_BOOLEAN) ? argv[1].value.string : FALSE;

	DOM_GadgetFile *dom_file = NULL;

	if (str_arg)
	{
		OpString safePath;
		CALL_FAILED_IF_ERROR(DFA_Utils::SecurePathConcat(safePath, str_arg, TRUE));

		if (safePath.IsEmpty() || OpStatus::IsError(file->InternalResolve(dom_file, safePath.CStr())))
			return DOM_CALL_FILEAPIEXCEPTION(WRONG_ARGUMENTS_ERR);
	}
	else if (obj_arg && obj_arg->IsA(DOM_TYPE_GADGETFILE))
	{
		dom_file = static_cast<DOM_GadgetFile*>(obj_arg);
	}
	else
		return DOM_CALL_FILEAPIEXCEPTION(WRONG_ARGUMENTS_ERR);

	if (dom_file)
	{
		if (!dom_file->Exists())
		{
			THROW_FILEAPIEXCEPTION_IF_ERROR(dom_file->EnsurePathExists());
			dom_file->Invalidate();
		}
		else
			return DOM_CALL_FILEAPIEXCEPTION(FILE_ALREADY_EXISTS_ERR);
	}
	else
		return DOM_CALL_FILEAPIEXCEPTION(GENERIC_ERR);

	DOMSetObject(return_value, dom_file);
	return ES_VALUE;
}

/* static */ int
DOM_GadgetFile::deleteDirectory(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value *return_value, DOM_Runtime *origining_runtime)
{
	DOM_THIS_OBJECT(file, DOM_TYPE_GADGETFILE, DOM_GadgetFile);
	DOM_FILE_CHECK_VALIDITY(file, 0, ROOT);

	DOM_Object *    obj_arg = (argc > 0 && argv[0].type == VALUE_OBJECT) ? DOM_GetHostObject(argv[0].value.object) : NULL;
	const uni_char *str_arg = (argc > 0 && argv[0].type == VALUE_STRING) ? argv[0].value.string : NULL;
	BOOL      recursive_arg = (argc > 1 && argv[1].type == VALUE_BOOLEAN && argv[1].value.boolean);

	DOM_GadgetFile *dom_file = NULL;
	OpString path;

	if (str_arg)
	{
		CALL_FAILED_IF_ERROR(file->GetPhysicalPath(path));
		CALL_FAILED_IF_ERROR(DFA_Utils::SecurePathConcat(path, str_arg, TRUE));
	}
	else if (obj_arg && obj_arg->IsA(DOM_TYPE_GADGETFILE))
	{
		dom_file = static_cast<DOM_GadgetFile*>(obj_arg);
		CALL_FAILED_IF_ERROR(dom_file->GetPhysicalPath(path));
	}
	else
		return DOM_CALL_FILEAPIEXCEPTION(WRONG_ARGUMENTS_ERR);

	BOOL result = FALSE;
	OpFile opfile;
	THROW_FILEAPIEXCEPTION_IF_ERROR(opfile.Construct(path.CStr(), OPFILE_ABSOLUTE_FOLDER));
	if (OpStatus::IsSuccess(opfile.Delete(recursive_arg)))
		result = TRUE;

	if (dom_file)
		dom_file->Invalidate();
	else
		file->Invalidate();

	DOMSetBoolean(return_value, result);
	return ES_VALUE;
}

/* static */ int
DOM_GadgetFile::deleteFile(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value *return_value, DOM_Runtime *origining_runtime)
{
	DOM_THIS_OBJECT(file, DOM_TYPE_GADGETFILE, DOM_GadgetFile);
	DOM_FILE_CHECK_VALIDITY(file, 0, ROOT);

	DOM_Object *    obj_arg = (argc > 0 && argv[0].type == VALUE_OBJECT)  ? DOM_GetHostObject(argv[0].value.object) : NULL;
	const uni_char *str_arg = (argc > 0 && argv[0].type == VALUE_STRING)  ? argv[0].value.string : NULL;

	OpString path;

	if (str_arg)
	{
		CALL_FAILED_IF_ERROR(file->GetPhysicalPath(path));
		CALL_FAILED_IF_ERROR(DFA_Utils::SecurePathConcat(path, str_arg, TRUE));
	}
	else if (obj_arg && obj_arg->IsA(DOM_TYPE_GADGETFILE))
	{
		DOM_GadgetFile *dom_file = static_cast<DOM_GadgetFile*>(obj_arg);
		CALL_FAILED_IF_ERROR(dom_file->GetPhysicalPath(path));
	}
	else
		return DOM_CALL_FILEAPIEXCEPTION(WRONG_ARGUMENTS_ERR);

	BOOL result = FALSE;
	OpFile opfile;
	THROW_FILEAPIEXCEPTION_IF_ERROR(opfile.Construct(path.CStr(), OPFILE_ABSOLUTE_FOLDER));
	if (OpStatus::IsSuccess(opfile.Delete(FALSE)))
		result = TRUE;

	DOMSetBoolean(return_value, result);
	return ES_VALUE;
}

/* static */ int
DOM_GadgetFile::refresh(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value *return_value, DOM_Runtime *origining_runtime)
{
	DOM_THIS_OBJECT(file, DOM_TYPE_GADGETFILE, DOM_GadgetFile);
	DOM_FILE_CHECK_VALIDITY(file, 0, 0);

	OpString destinationPath;
	CALL_FAILED_IF_ERROR(file->GetPhysicalPath(destinationPath));

	if (argc < 0)
	{
		OP_BOOLEAN result;
		CALL_FAILED_IF_ERROR(result = file->ProcessNextFile());
		if (result == OpBoolean::IS_TRUE)
			return ES_FAILED;
		else
			return (ES_SUSPEND | ES_RESTART);
	}

	for (uni_char* t = destinationPath.CStr(); t && *t; t++)	// De-normalize the string. Back to the platform standard
	{
		if (*t == '/')
			*t = PATHSEPCHAR;
	}

	if (file->IsVirtual() || file->IsRoot())
		return ES_FAILED; // no return value
#ifdef ZIPFILE_DIRECT_ACCESS_SUPPORT
	else if (file->IsInArchive())
	{
		file->DeleteNodes(TRUE);

		OP_DELETE(file->m_folder_lister); file->m_folder_lister = NULL;
		CALL_FAILED_IF_ERROR(OpZipFolderLister::Create(&file->m_folder_lister));
		CALL_FAILED_IF_ERROR(file->m_folder_lister->Construct(destinationPath.CStr(), UNI_L("*")));

		OP_BOOLEAN result;
		CALL_FAILED_IF_ERROR(result = file->ProcessNextFile());
		if (result == OpBoolean::IS_TRUE)
			return ES_FAILED;
		else
			return (ES_SUSPEND | ES_RESTART);
	}
#endif // ZIPFILE_DIRECT_ACCESS_SUPPORT
	else if (file->IsMountPoint() || file->IsDirectory())
	{
#ifdef DIRECTORY_SEARCH_SUPPORT
		file->DeleteNodes(FALSE);

		OpFile opfile;
		THROW_FILEAPIEXCEPTION_IF_ERROR(opfile.Construct(destinationPath.CStr(), OPFILE_ABSOLUTE_FOLDER));

		OP_DELETE(file->m_folder_lister); file->m_folder_lister = NULL;
		file->m_folder_lister = opfile.GetFolderLister(OPFILE_ABSOLUTE_FOLDER, UNI_L("*"), destinationPath.CStr());
		if (!file->m_folder_lister)
			return ES_NO_MEMORY;

		OP_BOOLEAN result;
		CALL_FAILED_IF_ERROR(result = file->ProcessNextFile());
		if (result == OpBoolean::IS_TRUE)
			return ES_FAILED;
		else
			return (ES_SUSPEND | ES_RESTART);

#endif // DIRECTORY_SEARCH_SUPPORT
	}
	else
		return ES_FAILED;
}

/* static */ int
DOM_GadgetFile::resolve(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value *return_value, DOM_Runtime *origining_runtime)
{
	DOM_THIS_OBJECT(file, DOM_TYPE_GADGETFILE, DOM_GadgetFile);
	DOM_FILE_CHECK_VALIDITY(file, 0, 0);
	DOM_CHECK_ARGUMENTS("s");

	OpString safePath;
	CALL_FAILED_IF_ERROR(DFA_Utils::SecurePathConcat(safePath, argv[0].value.string, TRUE));

	DOM_GadgetFile *dom_file;
	OP_STATUS err = file->InternalResolve(dom_file, safePath.CStr());
	if (OpStatus::IsMemoryError(err))
		return ES_NO_MEMORY;
	else if (OpStatus::IsError(err))
		return ES_EXCEPT_SECURITY;

	DOMSetObject(return_value, dom_file);
	return ES_VALUE; // Return DOM_GadgetFile
}

/* static */ int
DOM_GadgetFile::sort(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value *return_value, DOM_Runtime *origining_runtime)
{
	DOM_THIS_OBJECT_UNUSED(DOM_TYPE_GADGETFILE);
//	DOM_FILE_CHECK_VALIDITY(file, 0, ROOT);

	ES_Object *array = NULL;

#if 0 // NOT YET
	UINT32 nodes = file->m_nodes.GetCount();
	if (nodes > 0)
	{
		CALL_FAILED_IF_ERROR(file->GetRuntime()->CreateNativeArrayObject(&array, nodes));
		for (UINT32 i = 0; i < nodes; i++)
		{
			DOM_GadgetFile *node = file->m_nodes.Get(i);

			ES_Value val;
			val.type = VALUE_OBJECT;
			val.value.object = node->GetNativeObject();
			CALL_FAILED_IF_ERROR(file->GetRuntime()->PutIndex(array, i, val));
		}
	}

#endif // 0 NOT YET

	DOMSetObject(return_value, array);
	return ES_VALUE;
}

/* static */ int
DOM_GadgetFile::toString(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value *return_value, DOM_Runtime *origining_runtime)
{
	DOM_THIS_OBJECT_OPTIONAL(file, DOM_TYPE_GADGETFILE, DOM_GadgetFile);

	if (file)
	{
		OpString path;

		CALL_FAILED_IF_ERROR(path.Set(UNI_L("mountpoint:/")));
		CALL_FAILED_IF_ERROR(file->GetVisiblePath(path));
		CALL_FAILED_IF_ERROR(DFA_Utils::SetEscapedString(file, return_value, path.CStr()));

		return ES_VALUE;
	}
	else
		return DOM_toString(this_object, argv, argc, return_value, origining_runtime);
}

/************************************************************************/
/* class DOM_FileStream                                                 */
/************************************************************************/

/* static */ OP_STATUS
DOM_FileStream::Make(DOM_FileStream*& file, DOM_Runtime *runtime, DOM_GadgetFile *parent)
{
	RETURN_IF_ERROR(DOMSetObjectRuntime(file = OP_NEW(DOM_FileStream, (parent)), runtime, runtime->GetPrototype(DOM_Runtime::FILESTREAM_PROTOTYPE), "FileStream"));
	return file->Initialize();
}

OP_STATUS
DOM_FileStream::Initialize()
{
	RETURN_IF_ERROR(m_encoding.Set(UNI_L("utf-8")));
	RETURN_IF_ERROR(m_newline.Set(UNI_L(NEWLINE)));
	return OpStatus::OK;
}

DOM_FileStream::DOM_FileStream(DOM_GadgetFile *parent)
	: m_parent(parent)
{
}

/* virtual */
DOM_FileStream::~DOM_FileStream()
{
	if (m_file.IsOpen())
		m_file.Close();
}

/* virtual */ void
DOM_FileStream::GCTrace()
{
	GCMark(event_target);
	GCMark(m_parent);
}

/* virtual */ ES_GetState
DOM_FileStream::GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_position:
		if (value)
		{
			OpFileLength file_pos;
			if (m_file.IsOpen())
				GET_FAILED_IF_ERROR(m_file.GetFilePos(file_pos));
			else
				file_pos = 0;

			DOMSetNumber(value, static_cast<double>(file_pos));
		}
		return GET_SUCCESS;

	case OP_ATOM_bytesAvailable:
		if (value)
		{
			OpFileLength pos;
			OpFileLength size;
			GET_FAILED_IF_ERROR(m_file.GetFilePos(pos));
			GET_FAILED_IF_ERROR(m_file.GetFileLength(size));

			DOMSetNumber(value, static_cast<double>(size - pos));
		}
		return GET_SUCCESS;

	case OP_ATOM_eof:
		DOMSetBoolean(value, IsReadable() && m_file.Eof());
		return GET_SUCCESS;

	case OP_ATOM_encoding:
		DOMSetString(value, m_encoding.CStr());
		return GET_SUCCESS;

	case OP_ATOM_fileInstance:
		DOMSetObject(value, m_parent);
		return GET_SUCCESS;

	case OP_ATOM_systemNewLine:
		DOMSetString(value, UNI_L(NEWLINE));
		return GET_SUCCESS;

	case OP_ATOM_newLine:
		DOMSetString(value, m_newline.CStr());
		return GET_SUCCESS;
	}

	return DOM_Object::GetName(property_name, value, origining_runtime);
}

/* virtual */ ES_PutState
DOM_FileStream::PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_bytesAvailable:
	case OP_ATOM_eof:
	case OP_ATOM_systemNewLine:
		return PUT_READ_ONLY;

	case OP_ATOM_position:
		if (value->type != VALUE_NUMBER)
			return PUT_NEEDS_NUMBER;
		else if (!op_isnan(value->value.number))
		{
			OpFileLength file_pos;
			if (value->value.number < 0)
				file_pos = 0;
			else
			{
				OpFileLength file_length;
				PUT_FAILED_IF_ERROR(m_file.GetFileLength(file_length));

				if (op_isinf(value->value.number))
					file_pos = file_length;
				else
				{
					file_pos = static_cast<OpFileLength>(value->value.number);
					if (file_pos > file_length)
						file_pos = file_length;
				}
			}
			PUT_FAILED_IF_ERROR(m_file.SetFilePos(file_pos));
		}
		return PUT_SUCCESS;

	case OP_ATOM_encoding:
		if (value->type == VALUE_STRING)
			PUT_FAILED_IF_ERROR(m_encoding.Set(value->value.string));
		return PUT_SUCCESS;

	case OP_ATOM_newLine:
		if (value->type == VALUE_STRING)
		{
			const uni_char *newline = value->value.string;
			if (uni_strcmp(newline, UNI_L("\n")) == 0 || uni_strcmp(newline, UNI_L("\r\n")) == 0)
				PUT_FAILED_IF_ERROR(m_newline.Set(value->value.string));
			else
				return DOM_PUTNAME_DOMEXCEPTION(INVALID_CHARACTER_ERR);
		}
		return PUT_SUCCESS;
	}

	return DOM_Object::PutName(property_name, value, origining_runtime);
}

OP_STATUS
DOM_FileStream::GetFileName(OpString& filename)
{
	RETURN_IF_ERROR(filename.Set(m_file.GetFullPath()));
	return OpStatus::OK;
}

BOOL DOM_FileStream::IsReadable()
{
	if (m_file.IsOpen())
		return TRUE;

	return FALSE;
}

BOOL DOM_FileStream::IsWritable()
{
	if (m_parent && m_parent->IsReadOnly())
		return FALSE;

	if (m_file.IsOpen() && m_file.IsWritable())
		return TRUE;

	return FALSE;
}

BOOL DOM_FileStream::IsOpen()
{
	if (m_file.IsOpen())
		return TRUE;

	return FALSE;
}

/* static */ int
DOM_FileStream::close(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value *return_value, DOM_Runtime *origining_runtime)
{
	DOM_THIS_OBJECT(fs, DOM_TYPE_FILESTREAM, DOM_FileStream);
	DOM_CHECK_ARGUMENTS("");

	if (fs->IsOpen())
	{
		THROW_FILEAPIEXCEPTION_IF_ERROR(fs->m_file.Close());
	}

	return ES_FAILED;
}

/* static */ int
DOM_FileStream::read(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value *return_value, DOM_Runtime *origining_runtime)
{
	DOM_THIS_OBJECT(fs, DOM_TYPE_FILESTREAM, DOM_FileStream);
	DOM_CHECK_ARGUMENTS("n");

	OpString8 charset;
	CALL_FAILED_IF_ERROR(charset.Set((argc > 1 && argv[1].type == VALUE_STRING) ? argv[1].value.string : fs->m_encoding.CStr()));

	size_t max_length = static_cast<size_t>(argv[0].value.number);

	if (max_length > 104857600)
		return ES_FAILED;

	if (!fs->IsReadable())
		return ES_FAILED;

	if (fs->m_file.Eof())
	{
		DOMSetNull(return_value);
		return ES_VALUE;
	}

	InputConverter *converter;
	CALL_FAILED_IF_ERROR(InputConverter::CreateCharConverter(charset.CStr(), &converter));

	OpAutoPtr<InputConverter> converter_anchor(converter);

	TempBuffer *buffer = fs->GetEmptyTempBuf();
	CALL_FAILED_IF_ERROR(buffer->Expand(max_length + 1));
	uni_char *return_buffer = buffer->GetStorage();

	size_t input_max_length = max_length;
	if (input_max_length < 6)
		input_max_length = 6;

	if (input_max_length % 2)
		input_max_length++;

	OpString8 text;

	if (!text.Reserve(input_max_length + 1))
		return ES_NO_MEMORY;

	OpFileLength bytes_read;
	int bytes_converted;
	int total_bytes_converted = 0;
	unsigned int total_written_to_buffer = 0;
	unsigned int total_in_buffer = 0;
	OpFileLength file_pos;
	GET_FAILED_IF_ERROR(fs->m_file.GetFilePos(file_pos));

	while (total_written_to_buffer < max_length && !fs->m_file.Eof())
	{
		THROW_FILEAPIEXCEPTION_IF_ERROR(fs->m_file.Read(text.DataPtr() + total_in_buffer, input_max_length - total_in_buffer, &bytes_read));
		unsigned int truncated_bytes_read = static_cast<unsigned int>(bytes_read);
		OP_ASSERT(truncated_bytes_read == bytes_read);

		total_written_to_buffer += converter->Convert(text.DataPtr(), truncated_bytes_read, return_buffer + total_written_to_buffer, (max_length - total_written_to_buffer) * sizeof(uni_char), &bytes_converted) / sizeof(uni_char);

		total_bytes_converted += bytes_converted;
		return_buffer[total_written_to_buffer] = '\0';

		//memove the remaining uncoverted data to the start of the input buffer
		total_in_buffer = truncated_bytes_read - bytes_converted;
		op_memmove(text.DataPtr(), text.DataPtr() + bytes_converted, total_in_buffer);
	}

	//set file position to the end of the bytes consumed to convert the resulting string.
	file_pos += total_bytes_converted;
	fs->m_file.SetFilePos(file_pos);

	OP_ASSERT(total_written_to_buffer <= max_length);

	DOMSetString(return_value, buffer);
	return ES_VALUE;
}

/* static */ int
DOM_FileStream::readLine(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value *return_value, DOM_Runtime *origining_runtime)
{
	DOM_THIS_OBJECT(fs, DOM_TYPE_FILESTREAM, DOM_FileStream);
	DOM_CHECK_ARGUMENTS("");

	OpString8 charset;
	CALL_FAILED_IF_ERROR(charset.Set((argc > 0 && argv[0].type == VALUE_STRING) ? argv[0].value.string : fs->m_encoding.CStr()));

	if (!fs->IsReadable())
		return ES_FAILED;

	if (fs->m_file.Eof())
	{
		DOMSetNull(return_value);
		return ES_VALUE;
	}

	OpString8 line;
	THROW_FILEAPIEXCEPTION_IF_ERROR(fs->m_file.ReadLine(line));

	int dest_len = UNICODE_SIZE(line.Length() + 1);
	TempBuffer *buffer = fs->GetEmptyTempBuf();
	CALL_FAILED_IF_ERROR(buffer->Expand(dest_len));

	int bytes_converted;
	InputConverter *converter;
	CALL_FAILED_IF_ERROR(InputConverter::CreateCharConverter(charset.CStr(), &converter));
	OpAutoPtr<InputConverter> converter_anchor(converter);
	int total_written_to_buffer = UNICODE_DOWNSIZE(converter->Convert(line.CStr(), line.Length(), buffer->GetStorage(), buffer->GetCapacity(), &bytes_converted));
	if (total_written_to_buffer < 0)
		return ES_NO_MEMORY;
	else
		buffer->GetStorage()[total_written_to_buffer] = '\0';

	DOMSetString(return_value, buffer);
	return ES_VALUE;
}

/* static */ int
DOM_FileStream::readBytes(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value *return_value, DOM_Runtime *origining_runtime)
{
	DOM_THIS_OBJECT(fs, DOM_TYPE_FILESTREAM, DOM_FileStream);
	DOM_CHECK_ARGUMENTS("n");

	if (!fs->IsReadable())
		return ES_FAILED;

	if (fs->m_file.Eof())
	{
		DOMSetNull(return_value);
		return ES_VALUE;
	}

	int size = static_cast<unsigned int>(argv[0].value.number);
	int offset = (argc > 1 && argv[1].type == VALUE_NUMBER) ? int (argv[1].value.number) : -1;

	if (offset >= 0)
		THROW_FILEAPIEXCEPTION_IF_ERROR(fs->GetFile()->SetFilePos(offset));

	DOM_ByteArray *binary_object;
	THROW_FILEAPIEXCEPTION_IF_ERROR(DOM_ByteArray::MakeFromFile(binary_object, fs->GetRuntime(), fs->GetFile(), size));

	DOMSetObject(return_value, binary_object);
	return ES_VALUE;
}

/* static */ int
DOM_FileStream::readBase64(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value *return_value, DOM_Runtime *origining_runtime)
{
	DOM_THIS_OBJECT(fs, DOM_TYPE_FILESTREAM, DOM_FileStream);
	DOM_CHECK_ARGUMENTS("n");

	if (!fs->IsReadable())
		return ES_FAILED;

	if (fs->m_file.Eof())
	{
		DOMSetNull(return_value);
		return ES_VALUE;
	}

#ifdef URL_UPLOAD_BASE64_SUPPORT
	OpFileLength len = (OpFileLength) argv[0].value.number;
	OpFileLength bytes_read;
	char *src = OP_NEWA(char, (INT32)len);
	if (!src)
		return ES_NO_MEMORY;
	ANCHOR_ARRAY(char, src);
	THROW_FILEAPIEXCEPTION_IF_ERROR(fs->m_file.Read(src, len, &bytes_read));

	char *target = NULL;
	int target_len = 0;
	if (MIME_Encode_SetStr(target, target_len, src, (INT32)bytes_read, NULL, GEN_BASE64) != MIME_NO_ERROR)
		return ES_FAILED;
	ANCHOR_ARRAY(char, target);

	TempBuffer *buffer = GetEmptyTempBuf();
	CALL_FAILED_IF_ERROR(buffer->Append(target, target_len));

	DOMSetString(return_value, buffer);
	return ES_VALUE;
#else // URL_UPLOAD_BASE64_SUPPORT
	return ES_FAILED;
#endif // URL_UPLOAD_BASE64_SUPPORT
}

/* static */ int
DOM_FileStream::write(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value *return_value, DOM_Runtime *origining_runtime)
{
	DOM_THIS_OBJECT(fs, DOM_TYPE_FILESTREAM, DOM_FileStream);
	DOM_CHECK_ARGUMENTS("s");

	OpString8 charset;
	CALL_FAILED_IF_ERROR(charset.Set((argc > 1 && argv[1].type == VALUE_STRING) ? argv[1].value.string : fs->m_encoding.CStr()));

	if (!fs->IsWritable())
		THROW_FILEAPIEXCEPTION_IF_ERROR(OpStatus::ERR_NO_ACCESS);

	OpString8 utf8;
	CALL_FAILED_IF_ERROR(utf8.SetUTF8FromUTF16(argv[0].value.string));
	THROW_FILEAPIEXCEPTION_IF_ERROR(fs->m_file.Write(utf8.CStr(), utf8.Length()));

	DOMSetBoolean(return_value, TRUE);
	return ES_VALUE;
}

/* static */ int
DOM_FileStream::writeLine(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value *return_value, DOM_Runtime *origining_runtime)
{
	DOM_THIS_OBJECT(fs, DOM_TYPE_FILESTREAM, DOM_FileStream);
	DOM_CHECK_ARGUMENTS("s");

	OpString8 charset;
	CALL_FAILED_IF_ERROR(charset.Set((argc > 1 && argv[1].type == VALUE_STRING) ? argv[1].value.string : fs->m_encoding.CStr()));

	if (!fs->IsWritable())
		THROW_FILEAPIEXCEPTION_IF_ERROR(OpStatus::ERR_NO_ACCESS);

	// TODO: Could this be optimized to convert data + newline without first concatenating?
	OpString data;
	CALL_FAILED_IF_ERROR(data.SetConcat(argv[0].value.string, fs->m_newline.CStr()));

	OutputConverter *converter;
	CALL_FAILED_IF_ERROR(OutputConverter::CreateCharConverter(charset.CStr(), &converter));

	TempBuffer buffer;
	const unsigned char *src = (const unsigned char *) data.CStr();
	unsigned char *dest;
	int src_len = uni_strlen(data.CStr()) * sizeof(uni_char);
	int dest_len = 0, read = 0, src_ptr = 0;
	int dest_ptr = 0;

	while (src_ptr < src_len)
	{
		if (OpStatus::IsError(buffer.Expand(dest_len > src_len ? dest_len + 256 : src_len)))
		{
			OP_DELETE(converter);
			return ES_NO_MEMORY;
		}

		dest = (unsigned char *) buffer.GetStorage();
		dest_len = buffer.GetCapacity() * sizeof(uni_char);

		int result = converter->Convert(src + src_ptr, src_len - src_ptr, dest + dest_ptr, dest_len - dest_ptr, &read);

		if (result < 0)
		{
			OP_DELETE(converter);
			return ES_NO_MEMORY;
		}

		src_ptr += read;
		dest_ptr += result;
	}

	OP_DELETE(converter);

	THROW_FILEAPIEXCEPTION_IF_ERROR(fs->m_file.Write(buffer.GetStorage(), dest_ptr));

	DOMSetBoolean(return_value, TRUE);
	return ES_VALUE;
}

/* static */ int
DOM_FileStream::writeObject(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value *return_value, DOM_Runtime *origining_runtime)
{
	DOM_THIS_OBJECT(fs, DOM_TYPE_FILESTREAM, DOM_FileStream);
	DOM_CHECK_ARGUMENTS("o");

	if (!fs->IsWritable())
		THROW_FILEAPIEXCEPTION_IF_ERROR(OpStatus::ERR_NO_ACCESS);

	DOM_Object *obj = DOM_GetHostObject(argv[0].value.object);
	WebserverBodyObject_Base *body_object = NULL;
	DOM_GadgetFile *file_object = NULL;

#ifdef WEBSERVER_SUPPORT
# ifdef WEBSERVER_FILE_ENABLED
	if (obj && obj->IsA(DOM_TYPE_WEBSERVER_FILE))
	{
		DOM_WebServerFile *webserver_file =  static_cast<DOM_WebServerFile*>(obj);

		OpString filename;
		CALL_FAILED_IF_ERROR(webserver_file->GetFileName(filename));

		WebserverBodyObject_File *file_object;
		THROW_FILEAPIEXCEPTION_IF_ERROR(WebserverBodyObject_File::Make(file_object, filename));

		body_object = file_object;
	}
	else
# endif // WEBSERVER_FILE_ENABLED
	if (obj && obj->IsA(DOM_TYPE_WEBSERVER_UPLOADED_FILE))
	{
		DOM_WebServerUploadedFile *uploaded_file =  static_cast<DOM_WebServerUploadedFile*>(obj);

		WebserverBodyObject_UploadedFile *uploaded_file_object;
		THROW_FILEAPIEXCEPTION_IF_ERROR((WebserverBodyObject_UploadedFile::Make(uploaded_file_object, uploaded_file->GetUploadedFilePointer())));
		body_object = uploaded_file_object;
	}
	else
#endif // WEBSERVER_SUPPORT

	if (obj && obj->IsA(DOM_TYPE_GADGETFILE))
		file_object = static_cast<DOM_GadgetFile *>(obj);
	else if (obj && obj->IsA(DOM_TYPE_WEBSERVER_BINARY_OBJECT))
	{
		DOM_ByteArray *binary_object =  static_cast<DOM_ByteArray*>(obj);
		WebserverBodyObject_Binary *body_binary_object;

		THROW_FILEAPIEXCEPTION_IF_ERROR(WebserverBodyObject_Binary::Make(body_binary_object, binary_object->GetDataPointer(), binary_object->GetDataSize()));
		body_object = body_binary_object;
	}
	else if (obj && obj->IsA(DOM_TYPE_HTML_ELEMENT))
	{
		DOM_HTMLElement *domhtml_element = static_cast<DOM_HTMLElement*>(obj);
		HTML_Element *html_element = domhtml_element->GetThisElement();

#ifdef CANVAS_SUPPORT
		if (html_element->Type() == HE_CANVAS)
			static_cast<DOM_HTMLCanvasElement *>(obj)->Invalidate();
		else
#endif // CANVAS_SUPPORT
			if (html_element->Type() != HE_IMG)
				return DOM_CALL_FILEAPIEXCEPTION(TYPE_NOT_SUPPORTED_ERR);

		CALL_FAILED_IF_ERROR(WebserverBodyObject_Base::MakeBodyObjectFromHtml(body_object, html_element));
	}
	else
		return DOM_CALL_FILEAPIEXCEPTION(TYPE_NOT_SUPPORTED_ERR);

	if (body_object)
	{
		OpAutoPtr<WebserverBodyObject_Base> body_object_anchor(body_object);
		THROW_FILEAPIEXCEPTION_IF_ERROR(fs->WriteBodyObjectToFile(body_object));
	}
	else if (file_object)
	{
		OpString path;
		CALL_FAILED_IF_ERROR(file_object->GetPhysicalPath(path));

		OpFile file;
		THROW_FILEAPIEXCEPTION_IF_ERROR(file.Construct(path.CStr()));
		THROW_FILEAPIEXCEPTION_IF_ERROR(fs->WriteFileToFile(&file));
	}

	return ES_FAILED; // no return value
}

/* static */ int
DOM_FileStream::writeBase64(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value *return_value, DOM_Runtime *origining_runtime)
{
	DOM_THIS_OBJECT(fs, DOM_TYPE_FILESTREAM, DOM_FileStream);
	DOM_CHECK_ARGUMENTS("s");

	if (!fs->IsWritable())
		THROW_FILEAPIEXCEPTION_IF_ERROR(OpStatus::ERR_NO_ACCESS);

	// convert to 8 bit
	OpString8 encoded;
	CALL_FAILED_IF_ERROR(encoded.Set(argv[0].value.string));
	unsigned long length = encoded.Length();
	OP_ASSERT(length);

	// decoded base64 data is 3/4 the size of encoded. add one for
	// round-off errors. Might waste a little space, since base64 often
	// has padding, and no efford is made to calculate size without padding.
	unsigned long decode_buffer_length = (length * 3 / 4) + 1;

	// allocate decoding buffer
	unsigned char *decoded = OP_NEWA(unsigned char, decode_buffer_length);
	if (!decoded)
		return ES_NO_MEMORY;

	// decode
	BOOL warning = FALSE;
	unsigned long offset = 0;

	UINT decoded_length = GeneralDecodeBase64((const unsigned char*)encoded.CStr(), length, offset, decoded, warning, decode_buffer_length);
	ANCHOR_ARRAY(unsigned char, decoded);
	OP_ASSERT(decoded_length <= decode_buffer_length);
	if (warning || offset != length)
		return ES_FAILED;

	THROW_FILEAPIEXCEPTION_IF_ERROR(fs->m_file.Write(decoded, decoded_length));

	return ES_FAILED;
}

OP_STATUS
DOM_FileStream::WriteBodyObjectToFile(WebserverBodyObject_Base *body_object)
{
	char *data_pointer;
	int length;
	OP_BOOLEAN has_data_pointer;
	OP_BOOLEAN has_length;

	RETURN_IF_ERROR(body_object->InitiateGettingData());
	RETURN_IF_ERROR(has_data_pointer = body_object->GetDataPointer(data_pointer));
	RETURN_IF_ERROR(has_length = body_object->GetDataSize(length));

	OpFileLength file_length = length;
	if (has_data_pointer == OpBoolean::IS_TRUE && has_length == OpBoolean::IS_TRUE )
	{
		RETURN_IF_ERROR(m_file.Write(static_cast<void*>(data_pointer), file_length));
	}
	else
	{
		OP_BOOLEAN more_data = OpBoolean::IS_TRUE;
		OP_STATUS status;

		int data_chunk_length = 10240; /* ARRAY OK 2007-09-03 haavardm */
		char *data_chunk = OP_NEWA(char, data_chunk_length);
		if (data_chunk == NULL)
			return OpStatus::ERR_NO_MEMORY;

		int lengthRead = 0;

		while (more_data == OpBoolean::IS_TRUE)
		{
			lengthRead = 0;
			if (OpStatus::IsError(more_data = body_object->GetData(data_chunk_length, data_chunk, lengthRead)))
			{
				OP_DELETEA(data_chunk);
				return more_data;
			}

			OP_ASSERT(lengthRead <= data_chunk_length); /* in case of error in any the implementations of the pure virtual class WebserverBodyObject_Base */

			file_length = lengthRead;
			if (lengthRead > 0)
			{
				if (OpStatus::IsError(status = m_file.Write(static_cast<void*>(data_chunk), file_length)))
				{
					OP_DELETEA(data_chunk);
					return status;
				}
			}
		}
		OP_DELETEA(data_chunk);
	}

	return OpStatus::OK;
}

OP_STATUS
DOM_FileStream::WriteFileToFile(OpFile *src)
{
	if (!IsWritable())
		return OpStatus::ERR_NO_ACCESS;

	OpFile tmp;
	RETURN_IF_ERROR(tmp.Copy(&m_file));
	RETURN_IF_ERROR(tmp.CopyContents(src, FALSE));

	return OpStatus::OK;
}


/************************************************************************/
/* FUNCTIONS                                                            */
/************************************************************************/

DOM_FUNCTIONS_START(DOM_FileSystem)
	DOM_FUNCTIONS_FUNCTION(DOM_FileSystem, DOM_FileSystem::removeMountPoint, "removeMountPoint", "-")
	DOM_FUNCTIONS_FUNCTION(DOM_FileSystem, DOM_FileSystem::mountSystemDirectory, "mountSystemDirectory", "ss-")
DOM_FUNCTIONS_END(DOM_FileSystem)
DOM_FUNCTIONS_WITH_DATA_START(DOM_FileSystem)
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_FileSystem, DOM_FileSystem::browseFor, 0, "browseForDirectory", "ssob-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_FileSystem, DOM_FileSystem::browseFor, 1, "browseForFile", "ssobb-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_FileSystem, DOM_FileSystem::browseFor, 2, "browseForSave", "ssob-")
DOM_FUNCTIONS_WITH_DATA_END(DOM_FileSystem)

DOM_FUNCTIONS_START(DOM_GadgetFile)
	DOM_FUNCTIONS_FUNCTION(DOM_GadgetFile, DOM_GadgetFile::open, "open", "o--")
	DOM_FUNCTIONS_FUNCTION(DOM_GadgetFile, DOM_GadgetFile::copyTo, "copyTo", "obb-")
	DOM_FUNCTIONS_FUNCTION(DOM_GadgetFile, DOM_GadgetFile::moveTo, "moveTo", "obb-")
	DOM_FUNCTIONS_FUNCTION(DOM_GadgetFile, DOM_GadgetFile::createDirectory, "createDirectory", "--")
	DOM_FUNCTIONS_FUNCTION(DOM_GadgetFile, DOM_GadgetFile::deleteDirectory, "deleteDirectory", "--")
	DOM_FUNCTIONS_FUNCTION(DOM_GadgetFile, DOM_GadgetFile::deleteFile, "deleteFile", "--")
	DOM_FUNCTIONS_FUNCTION(DOM_GadgetFile, DOM_GadgetFile::refresh, "refresh", NULL)
	DOM_FUNCTIONS_FUNCTION(DOM_GadgetFile, DOM_GadgetFile::resolve, "resolve", "s-")
	DOM_FUNCTIONS_FUNCTION(DOM_GadgetFile, DOM_GadgetFile::sort, "sort", "-")
	DOM_FUNCTIONS_FUNCTION(DOM_GadgetFile, DOM_GadgetFile::toString, "toString", NULL)
DOM_FUNCTIONS_END(DOM_GadgetFile)
DOM_FUNCTIONS_WITH_DATA_START(DOM_GadgetFile)
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_GadgetFile, DOM_Node::accessEventListener, 0, "addEventListener", "s-b-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_GadgetFile, DOM_Node::accessEventListener, 1, "removeEventListener", "s-b-")
DOM_FUNCTIONS_WITH_DATA_END(DOM_GadgetFile)

DOM_FUNCTIONS_START(DOM_FileStream)
	DOM_FUNCTIONS_FUNCTION(DOM_FileStream, DOM_FileStream::close, "close", NULL)
	DOM_FUNCTIONS_FUNCTION(DOM_FileStream, DOM_FileStream::read, "read", "n-")
	DOM_FUNCTIONS_FUNCTION(DOM_FileStream, DOM_FileStream::readLine, "readLine", "-")
	DOM_FUNCTIONS_FUNCTION(DOM_FileStream, DOM_FileStream::readBytes, "readBytes", "n-")
	DOM_FUNCTIONS_FUNCTION(DOM_FileStream, DOM_FileStream::readBase64, "readBase64", "n-")
	DOM_FUNCTIONS_FUNCTION(DOM_FileStream, DOM_FileStream::write, "write", "s-")
	DOM_FUNCTIONS_FUNCTION(DOM_FileStream, DOM_FileStream::writeLine, "writeLine", "s-")
	DOM_FUNCTIONS_FUNCTION(DOM_FileStream, DOM_FileStream::writeObject, "writeBytes", "o-")
	DOM_FUNCTIONS_FUNCTION(DOM_FileStream, DOM_FileStream::writeObject, "writeFile", "o-")
	DOM_FUNCTIONS_FUNCTION(DOM_FileStream, DOM_FileStream::writeObject, "writeImage", "o-")
	DOM_FUNCTIONS_FUNCTION(DOM_FileStream, DOM_FileStream::writeBase64, "writeBase64", "s-")
DOM_FUNCTIONS_END(DOM_FileStream)
DOM_FUNCTIONS_WITH_DATA_START(DOM_FileStream)
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_FileStream, DOM_Node::accessEventListener, 0, "addEventListener", "s-b-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_FileStream, DOM_Node::accessEventListener, 1, "removeEventListener", "s-b-")
DOM_FUNCTIONS_WITH_DATA_END(DOM_FileStream)

/************************************************************************/
/* class DFA_Utils (DOM File API utils)                                 */
/************************************************************************/

/* static */ int
DFA_Utils::ThrowFileAPIException(OP_STATUS err, DOM_Object *this_object, ES_Value *return_value)
{
	switch (err)
	{
	case OpStatus::ERR_NO_MEMORY:
		return ES_NO_MEMORY;			// handled by dom core
	case OpStatus::ERR_NO_ACCESS:
		return DOM_CALL_FILEAPIEXCEPTION(NO_ACCESS_ERR);
	case OpStatus::ERR_FILE_NOT_FOUND:
		return DOM_CALL_FILEAPIEXCEPTION(FILE_NOT_FOUND_ERR);
	case OpStatus::ERR_NO_SUCH_RESOURCE:
		return DOM_CALL_FILEAPIEXCEPTION(RESOURCE_UNAVAILABLE_ERR);
	case OpStatus::ERR:
		return DOM_CALL_FILEAPIEXCEPTION(GENERIC_ERR);
	default:
		return DOM_CALL_FILEAPIEXCEPTION(TYPE_NOT_SUPPORTED_ERR);
	}
}

/* static */ int
DFA_Utils::SetEscapedString(DOM_Object *this_object, ES_Value *value, const uni_char *string)
{
	TempBuffer *buffer = this_object->GetEmptyTempBuf();
	if (!buffer)
		return OpStatus::ERR_NO_MEMORY;

	if (OpStatus::IsMemoryError(EscapeToBuffer(string, buffer)))
		return OpStatus::ERR_NO_MEMORY;

	this_object->DOMSetString(value, buffer);

	return OpStatus::OK;
}

/* static */ OP_STATUS
DFA_Utils::EscapeToBuffer(const uni_char *url, TempBuffer *buffer)
{
	OP_ASSERT(buffer);
	if (!url)
		return OpStatus::OK;

	if (UriEscape::CountEscapes(url, UriEscape::StandardUnsafe | UriEscape::Hash | UriEscape::Percent) == 0)
		return buffer->Append(url);

	OpString escaped;
	if (escaped.Reserve(uni_strlen(url) * 3 + 1) == NULL)
		return OpStatus::ERR_NO_MEMORY;

	UriEscape::Escape(escaped.CStr(), url, UriEscape::StandardUnsafe | UriEscape::Hash | UriEscape::Percent);

	RETURN_IF_ERROR(buffer->Append(escaped.CStr()));

	return OpStatus::OK;
}

/* static */ OP_STATUS
DFA_Utils::SecurePathConcat(OpString& safe_base_path, const uni_char *unsafe_base_path, BOOL isUrl)
{
	OpString lsubPath;
	RETURN_IF_ERROR(lsubPath.Set(unsafe_base_path));

	//RETURN_IF_ERROR(CheckAndFixDangerousPaths(safe_base_path, FALSE));
	RETURN_IF_ERROR(CheckAndFixDangerousPaths(lsubPath.CStr(), isUrl));

	int lengthSub =  lsubPath.Length();

	int safe_base_path_len = safe_base_path.Length();
	if ((safe_base_path_len > 0 && safe_base_path[safe_base_path_len - 1] != '/') && (lengthSub > 0 && lsubPath[0] != '/'))
		RETURN_IF_ERROR(safe_base_path.Append("/"));

	RETURN_IF_ERROR(safe_base_path.Append(lsubPath));
	RETURN_IF_ERROR(CheckAndFixDangerousPaths(safe_base_path.CStr(), FALSE));	// this is here to prevent someone from concating two partial paths that combined is unsafe

	return OpStatus::OK;
}

/* static */ OP_STATUS
DFA_Utils::CheckAndFixDangerousPaths(uni_char *path, BOOL isUrl)
{
	if (path == NULL || *path == 0)
		return OpStatus::OK;

	uni_char* pos = path;
	do
	{
		// normalize the path here (all \'s are turned in to /'s)
		if (*pos == '\\')
			*pos = '/';
	} while (*(++pos));

	if (isUrl)
	{
		// This double-unescape is quite likely a bug but has to be left
		// because of time constraints. It will turn %2525 in an url
		// into % instead of %25 which would be expected.
		// See CORE-26791.
		UriUnescape::ReplaceChars(path, UriUnescape::LocalfileUrlUtf8);
		UriUnescape::ReplaceChars(path, UriUnescape::All);
	}

	// remove all "/../" , "/./" and "//"

	uni_char *src;
	uni_char *dst;

	src = dst = path;

	while (*src)
	{
		*dst = *src;

		if (uni_strncmp(src, "/./", 3) == 0)
			src += 2;  // remove the "/."
		else if (uni_strncmp(src, "//", 2) == 0)
			src++; // remove the "/"
		else if (uni_strncmp(src, "/../", 4) == 0)
		{
			src += 3;	// remove the "/.."
			while (dst > path && *(--dst) != '/')
			{
				// rewind to the last '/' we passed
			}
		}
		else if (uni_strncmp(src, "./", 2) == 0)
			src += 2;		// remove the current dir reference (leading)
		else if (uni_strcmp(src, "/.") == 0)
			src += 2;		// remove the current dir reference (trailing)
		else
		{
			src++;
			dst++;
		}
	}

	*dst = '\0';

	OP_ASSERT(uni_strstr(path, UNI_L("./")) != path && uni_strstr(path, UNI_L("/..")) == NULL);

	return OpStatus::OK;
}

/* static */ OP_STATUS
DFA_Utils::UnescapeString(OpString& unescaped, const uni_char* escaped)
{
	RETURN_IF_ERROR(unescaped.Set(escaped));

	UriUnescape::ReplaceChars(unescaped.CStr(), UriUnescape::LocalfileUtf8);

	return OpStatus::OK;

}

/* static */ OP_STATUS
DFA_Utils::EscapeString(OpString& escaped, const uni_char* unescaped)
{
	if (UriEscape::CountEscapes(unescaped, UriEscape::StandardUnsafe) == 0)
		return escaped.Set(unescaped);

	OpString8 unescaped_utf8;
	RETURN_IF_ERROR(unescaped_utf8.SetUTF8FromUTF16(unescaped));

	OpString8 escaped_utf8;
	if (escaped_utf8.Reserve(unescaped_utf8.Length() * 3 + 1) == NULL)
		return OpStatus::ERR_NO_MEMORY;

	UriEscape::Escape(escaped_utf8.CStr(), unescaped_utf8.CStr(), UriEscape::StandardUnsafe);

	RETURN_IF_ERROR(escaped.SetFromUTF8(escaped_utf8.CStr()));

	return OpStatus::OK;
}

#endif // DOM_GADGET_FILE_API_SUPPORT
