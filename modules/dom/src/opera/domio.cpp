/* -*- mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2007-2010 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** @author Lasse Magnussen lasse@opera.com
*/

#include "core/pch.h"

#if defined(DOM_GADGET_FILE_API_SUPPORT) || defined(WEBSERVER_SUPPORT)

#include "modules/dom/src/opera/domio.h"

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
#include "modules/formats/encoder.h"
#include "modules/formats/base64_decode.h"
#include "modules/util/zipload.h"
#include "modules/gadgets/OpGadget.h"

#ifdef WEBSERVER_SUPPORT
# include "modules/webserver/webserver-api.h"
# include "modules/webserver/webserver_resources.h"
# include "modules/dom/src/domwebserver/domwebserver.h"
# include "modules/webserver/webserver_body_objects.h"
# include "modules/prefs/prefsmanager/collections/pc_webserver.h"
#endif //WEBSERVER_SUPPORT

#ifdef SELFTEST
# include "modules/selftest/optestsuite.h"
#endif

#include "modules/dom/src/opera/domgadgetfile.h"

/************************************************************************/
/* class DOM_IO                                                         */
/************************************************************************/

/* static */ OP_STATUS
DOM_IO::Make(DOM_IO*& new_obj, DOM_Runtime *origining_runtime)
{
	new_obj = OP_NEW(DOM_IO, ());
	if (new_obj == NULL)
		return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_ERROR(DOMSetObjectRuntime(new_obj, origining_runtime, origining_runtime->GetPrototype(DOM_Runtime::IO_PROTOTYPE), "IO"));

	RETURN_IF_ERROR(new_obj->Initialize());

	TRAPD(status, new_obj->InstallFileObjectsL());
	RETURN_IF_ERROR(status);
#ifdef WEBSERVER_SUPPORT
	RETURN_IF_ERROR(new_obj->InstallWebserverObjects());
#endif // WEBSERVER_SUPPORT

	return OpStatus::OK;
}

OP_STATUS
DOM_IO::Initialize()
{
	OP_ASSERT(GetRuntime()); // Construct must be called after we've been added to the runtime
	OpGadget* gadget = GetFramesDocument()->GetWindow()->GetGadget();

	if (gadget)
		m_persistentStorageListener = gadget;
#ifdef WEBSERVER_SUPPORT
	else if (g_webserver && g_webserver->IsRunning() && g_webserver->WindowHasWebserverAssociation(GetFramesDocument()->GetWindow()->Id()))
		m_persistentStorageListener = g_webserver->WindowHasWebserverAssociation(GetFramesDocument()->GetWindow()->Id());
#endif
#ifdef SELFTEST
	else if (g_selftest.running && !m_persistentStorageListener)
		m_persistentStorageListener = this;
#endif // SELFTEST
	return OpStatus::OK;
}

void
DOM_IO::InstallFileObjectsL()
{
	if (AllowFileAPI(GetFramesDocument()))
	{
		DOM_Runtime *runtime = GetRuntime();
		ES_Value value;

		DOM_FileSystem *fs;
		LEAVE_IF_ERROR(DOM_FileSystem::Make(fs, runtime, m_persistentStorageListener));
		DOMSetObject(&value, fs);
		PutL("filesystem", value, PROP_DONT_DELETE | PROP_READ_ONLY);

		DOM_Object *fmode = OP_NEW(DOM_Object, ());
		DOMSetObjectRuntimeL(fmode, runtime, runtime->GetObjectPrototype(), "FileMode");
		DOMSetObject(&value, fmode);
		PutL("filemode", value, PROP_DONT_DELETE);

		PutNumericConstantL(*fmode, "READ", DOM_FileMode::DOM_FILEMODE_READ, runtime);
		PutNumericConstantL(*fmode, "WRITE", DOM_FileMode::DOM_FILEMODE_WRITE, runtime);
		PutNumericConstantL(*fmode, "APPEND", DOM_FileMode::DOM_FILEMODE_APPEND, runtime);
		PutNumericConstantL(*fmode, "UPDATE", DOM_FileMode::DOM_FILEMODE_UPDATE, runtime);
	}
}

#ifdef WEBSERVER_SUPPORT
OP_STATUS
DOM_IO::InstallWebserverObjects()
{

#ifdef SELFTEST
	const char *current_name;
	const char *current_module;

	if (g_webserver && g_selftest.running && g_selftest.suite->GroupInfo(-1, current_name, current_module) && current_module != NULL && !op_strcmp(current_module, "webserver") && GetFramesDocument())
	{
		if (!g_webserver->IsRunning())
		{
			RETURN_IF_ERROR(g_webserver->Start(WEBSERVER_LISTEN_LOCAL, g_pcwebserver->GetIntegerPref(PrefsCollectionWebserver::WebserverPort)));
		}

		/* we install a dummy service in this window */
		WebSubServer *new_subserver;

		OpString storagePath;
		RETURN_IF_ERROR(g_folder_manager->GetFolderPath(OPFILE_TEMP_FOLDER, storagePath));
		RETURN_IF_ERROR(storagePath.Append(UNI_L("webserver/self_test_service/")));

		RETURN_IF_ERROR(WebSubServer::Make(new_subserver, g_webserver, GetFramesDocument()->GetWindow()->Id(), storagePath, UNI_L("selftest"), UNI_L("selftest"), UNI_L("notgonnatellya!"), UNI_L("This is a test service for the SELFTEST system"), UNI_L("download url unknown"), TRUE));
		OP_STATUS status;
		if (OpStatus::IsError(status = g_webserver->AddSubServer(new_subserver))) /* takes over new_subserver*/
		{
			OP_DELETE(new_subserver);
			return status;
		}
	}
#endif

	FramesDocument *frames_doc = GetFramesDocument();

	if (g_webserver && g_webserver->IsRunning() && frames_doc)
	{
		WebSubServer *subserver =  g_webserver->WindowHasWebserverAssociation(frames_doc->GetWindow()->Id());
		if (!subserver)
			return OpStatus::OK;

		DOM_Object *window = GetEnvironment()->GetWindow();
		ES_Value webserver;
		OP_BOOLEAN got_it;

		RETURN_IF_MEMORY_ERROR(got_it = window->GetPrivate(DOM_PRIVATE_webserver, &webserver));
		if (got_it == OpBoolean::IS_FALSE)
		{
			DOM_WebServer *dom_webserver;
			dom_webserver = OP_NEW(DOM_WebServer, (subserver));

			if (!dom_webserver)
				return OpStatus::ERR_NO_MEMORY;

			RETURN_IF_LEAVE(
				DOM_WebServer::PutConstructorsL(window);
				PutObjectL("webserver", dom_webserver, "WebServer");
				);
			RETURN_IF_ERROR(window->PutPrivate(DOM_PRIVATE_webserver, *dom_webserver));
			RETURN_IF_LEAVE(dom_webserver->InitializeL(GetRuntime()));
		}
	}

	return OpStatus::OK;
}

#endif // WEBSERVER_SUPPORT

#ifdef SELFTEST
OP_STATUS
DOM_IO::GetStoragePath(OpString& storage_path)
{
	// just need some safe place to run dom file api selftests in
	return g_folder_manager->GetFolderPath(OPFILE_TEMP_FOLDER, storage_path);
}
#endif // SELFTEST

/* static */ BOOL
DOM_IO::AllowIOAPI(FramesDocument* frm_doc)
{
	BOOL allow = FALSE;

	Window* win = frm_doc->GetWindow();
	OpGadget* gadget = win->GetGadget();

	if (gadget)
	{
		OpStatus::Ignore(OpSecurityManager::CheckSecurity(
			OpSecurityManager::GADGET_DOM,
			OpSecurityContext(frm_doc->GetURL()),
			OpSecurityContext(gadget),
			allow));
	}
	else
		allow = FALSE;

#ifdef SELFTEST
	if (g_selftest.running)
		allow = TRUE;
#endif // SELFTEST

	return allow;
}

/* static */ BOOL
DOM_IO::AllowFileAPI(FramesDocument* frm_doc)
{
	BOOL allowFileAPI = DOM_IO::AllowIOAPI(frm_doc);

	Window* win = frm_doc->GetWindow();
	OpGadget* gadget = win->GetGadget();

	if (allowFileAPI && gadget)
		allowFileAPI = gadget->GetClass()->HasFileAccess();
#ifdef WEBSERVER_SUPPORT
	else if (allowFileAPI && g_webserver && g_webserver->IsRunning() && g_webserver->WindowHasWebserverAssociation(win->Id()))
		allowFileAPI = g_webserver->WindowHasWebserverAssociation(win->Id())->AllowEcmaScriptFileAccess();
#endif // WEBSERVER_SUPPORT

#ifdef SELFTEST
	if (g_selftest.running)
		allowFileAPI = TRUE;
#endif // SELFTEST

	return allowFileAPI;
}

#endif // defined(DOM_GADGET_FILE_API_SUPPORT) || defined(WEBSERVER_SUPPORT)
