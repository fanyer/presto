/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef GADGET_SUPPORT

#include "modules/gadgets/OpGadgetManager.h"
#include "modules/gadgets/OpGadgetClass.h"
#include "modules/gadgets/OpGadget.h"
#include "modules/gadgets/OpGadgetUpdate.h"
#include "modules/gadgets/gadget_utils.h"
#include "modules/doc/frm_doc.h"
#include "modules/dochand/win.h"
#include "modules/dochand/winman.h"
#include "modules/ecmascript_utils/essched.h"
#include "modules/ecmascript_utils/esthread.h"
#include "modules/ecmascript_utils/esterm.h"
#include "modules/hardcore/mh/mh.h"
#include "modules/pi/system/OpFolderLister.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"
#include "modules/prefs/prefsmanager/collections/pc_webserver.h"
#include "modules/prefsfile/prefsfile.h"
#include "modules/security_manager/include/security_manager.h"
#include "modules/util/datefun.h"
#include "modules/util/opfile/opfile.h"
#include "modules/util/opfile/opfolder.h"
#include "modules/util/opstrlst.h"
#include "modules/util/filefun.h"
#include "modules/util/zipload.h"
#include "modules/url/url_man.h"
#include "modules/url/url_rep.h"
#include "modules/windowcommander/OpWindowCommander.h"
#include "modules/windowcommander/OpWindowCommanderManager.h"
#include "modules/windowcommander/src/WindowCommander.h"
#include "modules/xmlutils/xmlfragment.h"
#include "modules/xmlutils/xmlnames.h"
#include "modules/pi/OpSystemInfo.h"
#include "modules/debug/debug.h"
#ifdef SIGNED_GADGET_SUPPORT
//# include "modules/libcrypto/src/WidgetSignatureVerifier.h"
#endif // SIGNED_GADGET_SUPPORT

#ifdef GADGETS_INSTALLER_SUPPORT
#include "modules/gadgets/OpGadgetInstallObject.h"
#endif // GADGETS_INSTALLER_SUPPORT


/***********************************************************************************
**
**	Make
**
***********************************************************************************/

/* static */ OP_STATUS
OpGadgetManager::Make(OpGadgetManager*& new_obj)
{
	new_obj = OP_NEW(OpGadgetManager, ());
	if (!new_obj)
		return OpStatus::ERR_NO_MEMORY;

	return new_obj->Construct();
}

/***********************************************************************************
**
**	OpGadgetManager
**
***********************************************************************************/

OpGadgetManager::OpGadgetManager()
	: m_prefsfile(NULL)
#ifdef EXTENSION_SUPPORT
	, m_extension_userjs_updated(FALSE)
	, m_extension_unique_supply(128)
#endif // EXTENSION_SUPPORT
	, m_write_msg_posted(FALSE)
#ifdef SELFTEST
	, m_allow_bundled_global_policy(FALSE)
#endif // SELFTEST
{
}

/***********************************************************************************
**
**	~OpGadgetManager
**
***********************************************************************************/

OpGadgetManager::~OpGadgetManager()
{
#ifdef GADGET_CLEAN_CACHE_DIR_AT_START
	g_main_message_handler->UnsetCallBack(this, MSG_GADGET_CLEAN_CACHE);
#endif // GADGET_CLEAN_CACHE_DIR_AT_START
	g_main_message_handler->UnsetCallBack(this, MSG_GADGET_DELAYED_PREFS_WRITE);

	if (m_write_msg_posted)
	{
		TRAPD(err, m_prefsfile->CommitL(TRUE, FALSE));
	}

	m_gadgets.Clear();
	m_classes.Clear();
	m_update_objects.Clear();
	m_download_objects.Clear();
	m_tokens.Clear();
	m_gadget_extensions.DeleteAll();
#ifdef EXTENSION_SUPPORT
	m_enabled_userjs.Clear();
#endif

#ifdef UTIL_ZIP_CACHE
	g_opera->util_module.m_zipcache->FlushUnused();
#endif // UTIL_ZIP_CACHE
#ifndef GADGET_CLEAN_CACHE_DIR_AT_START
	CleanCache();
#endif // !GADGET_CLEAN_CACHE_DIR_AT_START

	m_delete_on_exit.DeleteAll();

	OP_DELETE(m_prefsfile);
	m_prefsfile = NULL;
}

/***********************************************************************************
**
**	Construct
**
***********************************************************************************/

OP_STATUS
OpGadgetManager::Construct()
{
	RETURN_IF_ERROR(Initialize());

	m_prefsfile = CreatePersistentDataFile();
	if (!m_prefsfile)
		return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_ERROR(SetupBrowserLocale());

#ifndef GADGET_LOAD_SUPPORT
	if (OpStatus::IsSuccess(LoadGadgets()))
		OpStatus::Ignore(CleanWidgetsDirectory());
#endif // GADGET_LOAD_SUPPORT
#ifdef GADGET_CLEAN_CACHE_DIR_AT_START
	RETURN_IF_ERROR(g_main_message_handler->SetCallBack(this, MSG_GADGET_CLEAN_CACHE, reinterpret_cast<MH_PARAM_1>(this)));
	g_main_message_handler->PostDelayedMessage(MSG_GADGET_CLEAN_CACHE, reinterpret_cast<MH_PARAM_1>(this), 0, GADGETS_WRITE_PREFS_DELAY);
#endif // GADGET_CLEAN_CACHE_DIR_AT_START

	RETURN_IF_ERROR(g_main_message_handler->SetCallBack(this, MSG_GADGET_DELAYED_PREFS_WRITE, reinterpret_cast<MH_PARAM_1>(this)));

	return OpStatus::OK;
}

/***********************************************************************************
**
**	Initialize
**
***********************************************************************************/

OP_STATUS
OpGadgetManager::Initialize()
{
	RETURN_IF_ERROR(AddAllowedWidgetExtension(UNI_L(".wgt")));
	RETURN_IF_ERROR(AddAllowedWidgetExtension(UNI_L(".zip")));

#ifdef EXTENSION_SUPPORT
	RETURN_IF_ERROR(AddAllowedWidgetExtension(UNI_L(".oex")));
#endif // EXTENSION_SUPPORT

#ifdef WEBSERVER_SUPPORT
	RETURN_IF_ERROR(AddAllowedWidgetExtension(UNI_L(".us")));
	RETURN_IF_ERROR(AddAllowedWidgetExtension(UNI_L(".ua")));
#endif // WEBSERVER_SUPPORT

	OpStatus::Ignore(SetupGlobalGadgetsSecurity());

	return OpStatus::OK;
}

/***********************************************************************************
**
**	SetupBrowserLocale
**
***********************************************************************************/

OP_STATUS
OpGadgetManager::SetupBrowserLocale()
{
	// Get the preferred languages setting from preferences
	OpString languages;
	RETURN_IF_ERROR(languages.Set(g_pcnet->GetStringPref(PrefsCollectionNetwork::AcceptLanguage)));
	return SetupLanguagesFromLocaleList(&languages);
}

#ifdef SELFTEST
/***********************************************************************************
**
**	OverrideBrowserLocale
**
***********************************************************************************/
OP_STATUS OpGadgetManager::OverrideBrowserLocale(const char *locale)
{
	// Allow selftest code to override the browser's locale
	// setting to be able to perform locale-related tests.
	OpString languages;
	RETURN_IF_ERROR(languages.Set(locale));
	return SetupLanguagesFromLocaleList(&languages);
}
#endif

/***********************************************************************************
**
**	SetupLanguagesFromLocaleList
**
***********************************************************************************/

OP_STATUS
OpGadgetManager::SetupLanguagesFromLocaleList(OpString *languages)
{
	// Empty the list (must not be NULL)
	RETURN_IF_ERROR(m_browser_locale.Set("\0", 1));

	if (languages->IsEmpty())
	{
		return OpStatus::OK;
	}

	// Convert to lowercase
	languages->MakeLower();

	// Massage the list to form a user agent locale list as per w3c spec.
	uni_char *start = languages->CStr();
	do
	{
		// Find the end of the current token
		uni_char *end;
		for (end = start + 1; *end && *end != ',' && *end != ';'; ++ end)
		{
			/* This loop body is intentionally left empty */
		}

		uni_char *new_end = end;
		while (new_end > start)
		{
			// Separate out the current token
			uni_char token[64]; /* ARRAY OK 2010-08-24 peter */
			int len =
				uni_sprintf(token, UNI_L("%.*s,"), MIN(new_end - start, 62), start);

			// Skip token if it is a single character (should be either
			// "x" from a private use token or "i" from unlisted
			// languages, obsoleted by RFC 4646; other uses are
			// forbidden), or if it is "art" (obsolete prefix for
			// unlisted artificial languages).
			if (2 == len ||
				(4 == len && 0 == uni_strcmp(token, "art,")))
			{
				break;
			}

			// If the token isn't there already, add it to the list of
			// supported locales.
			int match = m_browser_locale.Find(token);
			if (KNotFound == match ||
				/* avoid matching substrings */
				(match > 0 && m_browser_locale[match - 1] != ','))
			{
				RETURN_IF_ERROR(m_browser_locale.Append(token));
			}

			// Shorten the token by removing the rightmost subtag.
			do
			{
				-- new_end;
			} while (new_end > start && *new_end != '-');
		}

		// If language was followed by a quality value, ignore it.
		if (';' == *end)
		{
			while (*end && *end != ',')
			{
				++ end;
			}
		}

		// Reset start pointer to next token
		start = *end ? end + 1 : NULL;
	} while (start);

	// Replace all commas with '\0'. Last char has to be comma
	// so the substitution results in "\0\0" in the end.
	OP_ASSERT(m_browser_locale.IsEmpty() || m_browser_locale.CStr()[m_browser_locale.Length() - 1] == ',');
	uni_char* cur_ptr = m_browser_locale.DataPtr();
	if (cur_ptr)
		while (*cur_ptr)
		{
			if (*cur_ptr == ',')
				*cur_ptr = 0;
			++cur_ptr;
		}

	// According to the spec, we should add "*" to the end of the list.
	// We do not do this since we don't need that to do our matching.

	return OpStatus::OK;
}

/***********************************************************************************
**
**	CloseWindowPlease
**
***********************************************************************************/

OP_STATUS
OpGadgetManager::CloseWindowPlease(Window *window)
{
	OpGadget *gadget = window->GetGadget();
	if (!gadget)
		return OpStatus::ERR;

	if (gadget->GetIsClosing() && gadget->GetWindow() == window)
		return OpStatus::OK;

#ifdef EXTENSION_SUPPORT
	const BOOL is_extension_background_window = IsExtensionBackgroundWindow(gadget, window);
#else
	const BOOL is_extension_background_window = FALSE;
#endif // EXTENSION_SUPPORT

#ifdef EXTENSION_SUPPORT
	if (is_extension_background_window)
	{
		OP_ASSERT(gadget->IsExtension());

		gadget->StopExtension();

		// Close all windows associated with this extension.
		Window *w = g_windowManager->FirstWindow(), *next_w;
		while(w)
		{
			next_w  = w->Suc();
			if (w != window && w->GetGadget() && w->GetGadget() == gadget)
				CloseWindowPlease(w);
			w = next_w;
		}
	}
#endif // EXTENSION_SUPPORT

	OP_STATUS result = OpStatus::OK;
	BOOL es_window_close = FALSE;

	if (FramesDocument *doc = window->GetCurrentDoc())
		if (ES_ThreadScheduler *scheduler = doc->GetESScheduler())
		{
			ES_TerminatingAction *action = OP_NEW(ES_WindowCloseAction, (window));
			if (!action)
				result = OpStatus::ERR_NO_MEMORY;
			else if(OpStatus::IsSuccess(result = scheduler->AddTerminatingAction(action)))
			{
				es_window_close = TRUE;
				if (gadget->GetWindow() == window)
					gadget->SetIsClosing(TRUE);
			}
		}

	if (gadget->GetWindow() == window)
		gadget->SetWindow(NULL);

	if (window->GetWindowCommander())
			window->GetWindowCommander()->SetGadget(NULL);

	/* Close associated extensions windows now if a terminating action hasn't
	 * been added. */
	if (!es_window_close && gadget->IsExtension() && !is_extension_background_window)
		window->Close();

	return result;
}

/***********************************************************************************
**
**	GetGadget
**
***********************************************************************************/

OpGadget *
OpGadgetManager::GetGadget(UINT index) const
{
	Link *gadget = m_gadgets.First();
	while (gadget && index--) { gadget = gadget->Suc(); }
	return static_cast<OpGadget *>(gadget);
}

/***********************************************************************************
**
**	GetGadgetByContextId
**
***********************************************************************************/

OpGadget*
OpGadgetManager::GetGadgetByContextId(URL_CONTEXT_ID context_id) const
{
	for (OpGadget *gadget = GetFirstGadget(); gadget != NULL; gadget = static_cast<OpGadget *>(gadget->Suc()))
		if (gadget->UrlContextId() == context_id)
			return gadget;
	return NULL;
}

/***********************************************************************************
**
**	GetGadgetClass
**
***********************************************************************************/

OpGadgetClass *
OpGadgetManager::GetGadgetClass(UINT index) const
{
	Link *gadget_class = m_classes.First();
	while (gadget_class && index--) { gadget_class = gadget_class->Suc(); }
	return static_cast<OpGadgetClass *>(gadget_class);
}

/***********************************************************************************
**
**	NumGadgetInstancesOf
**
***********************************************************************************/

UINT
OpGadgetManager::NumGadgetInstancesOf(OpGadgetClass* gadget_class)
{
	UINT found_instances = 0;
	OpGadget *gadget = static_cast<OpGadget *>(m_gadgets.First());
	while (gadget)
    {
		if (gadget->GetClass() == gadget_class)
			found_instances++;

		gadget = static_cast<OpGadget *>(gadget->Suc());
	}

	return found_instances;
}

/***********************************************************************************
**
**	CleanCache
**
***********************************************************************************/

OP_STATUS
OpGadgetManager::CleanCache()
{
	UINT cnt;
	OpFile directory;
	for (cnt = 0; cnt < m_delete_on_exit.GetCount(); cnt++)
	{
		OpString *dir = m_delete_on_exit.Get(cnt);
		if (dir)
		{
			if (OpStatus::IsSuccess(directory.Construct(dir->CStr(), OPFILE_GADGET_FOLDER)))
			{
#if defined(USE_ASYNC_FILEMAN) && defined(UTIL_ASYNC_FILE_OP)
				OpStatus::Ignore(directory.DeleteAsync());
#else
				OpStatus::Ignore(directory.Delete());
#endif // USE_ASYNC_FILEMAN && UTIL_ASYNC_FILE_OP
			}
			else
				return OpStatus::ERR;
		}
	}


	return OpStatus::OK;
}

/***********************************************************************************
**
**	CleanWidgetsDirectory
**
***********************************************************************************/

OP_STATUS
OpGadgetManager::CleanWidgetsDirectory()
{
	OpFolderLister *lister = OpFile::GetFolderLister(OPFILE_GADGET_FOLDER, UNI_L("*"));
	if (!lister)
		return OpStatus::OK;	// This could very well be an actual OOM, but it's more likely that it's just the directory that hasn't been created yet.
	OpAutoPtr<OpFolderLister> lister_anchor(lister);

	while (lister->Next())
	{
		const uni_char *name = lister->GetFileName();

		if (uni_str_eq(name, ".") || uni_str_eq(name, "..") || uni_str_eq(name, GADGET_PERSISTENT_DATA_FILENAME))
			continue;

		const OpStringC test(name);

		if (!FindGadget(GADGET_FIND_BY_ID, test) && !FindGadget(GADGET_FIND_BY_FILENAME, test))
		{
			// This gadget is not in use, delete it.
			OpString* item = OP_NEW(OpString, ());
			if (!item)
				return OpStatus::ERR_NO_MEMORY;
			OpAutoPtr<OpString> item_anchor(item);
			RETURN_IF_ERROR(item->Set(name));
			RETURN_IF_ERROR(m_delete_on_exit.Add(item));
			item_anchor.release();
		}
	}

	return OpStatus::OK;
}

/***********************************************************************************
**
**	OpenGadgetInternal
**
***********************************************************************************/

OP_GADGET_STATUS
OpGadgetManager::OpenGadgetInternal(const OpStringC& gadget_path, BOOL is_root_service, URLContentType type)
{
	if (!IsThisAGadgetPath(gadget_path))
		return OpStatus::ERR;

	OpGadget *gadget = NULL;
	RETURN_IF_ERROR(CreateGadget(&gadget, gadget_path, type));
	RETURN_IF_ERROR(OpenGadgetInternal(gadget, is_root_service));

	return OpStatus::OK;
}

OP_GADGET_STATUS
OpGadgetManager::OpenGadgetInternal(OpGadget *gadget, BOOL is_root_service)
{
	// If it has a window it is already open
	if (gadget->GetWindow())
		return OpStatus::OK;

#ifdef EXTENSION_SUPPORT
	if (gadget->IsExtension() && gadget->IsRunning())
		return OpStatus::ERR;
#endif // EXTENSION_SUPPORT

#ifdef WEBSERVER_SUPPORT
	gadget->SetIsRootService(is_root_service);
#else
	OP_ASSERT(!is_root_service);
#endif // WEBSERVER_SUPPORT

#ifdef SIGNED_GADGET_SUPPORT
	if (gadget->SignatureState() == GADGET_SIGNATURE_UNKNOWN || gadget->SignatureState() == GADGET_SIGNATURE_PENDING)
	{
		if (gadget->GetIsOpening() == FALSE)
		{
			gadget->SetIsOpening(TRUE);
			if (gadget->SignatureState() != GADGET_SIGNATURE_PENDING)
				RETURN_IF_ERROR(gadget->GetClass()->VerifySignature());	// NB! Without OCSP, SignatureVerified could be called synchronously.
		}
		return OpGadgetStatus::OK_SIGNATURE_VERIFICATION_IN_PROGRESS;
	}

	gadget->SetIsOpening(FALSE);
#endif // SIGNED_GADGET_SUPPORT

	if (gadget->GetClass()->OpenAllowed() != YES)
	{
		OpenGadgetFailed(gadget);
		return OpStatus::ERR;
	}

	WindowCommander *new_commander = NULL;
	new_commander = OP_NEW(WindowCommander, ());
	if (!new_commander)
	{
		OpenGadgetFailed(gadget);
		return OpStatus::ERR_NO_MEMORY;
	}

	OP_STATUS err;
	if (OpStatus::IsError(err = new_commander->Init()))
	{
		OP_DELETE(new_commander);
		OpenGadgetFailed(gadget);
		return err;
	}
	new_commander->SetGadget(gadget);

	OpUiWindowListener::CreateUiWindowArgs args;
	op_memset(&args, 0, sizeof(args));

#ifdef EXTENSION_SUPPORT
	if (gadget->IsExtension())
	{
		args.type = OpUiWindowListener::WINDOWTYPE_EXTENSION;
		args.extension.type = OpUiWindowListener::EXTENSIONTYPE_TOOLBAR;
	}
	else
#endif
	{
		args.type = OpUiWindowListener::WINDOWTYPE_WIDGET;
		args.widget.widget = gadget;
		args.widget.width = gadget->InitialWidth();
		args.widget.height = gadget->InitialHeight();
	}

	OP_STATUS create_window_status = g_windowCommanderManager->GetUiWindowListener()->CreateUiWindow(new_commander, args);

	if (OpStatus::IsError(create_window_status))
	{
		OP_DELETE(new_commander);
		OpenGadgetFailed(gadget);
	}
#ifdef EXTENSION_SUPPORT
	else
		gadget->SetIsRunning(TRUE);
#endif // EXTENSION_SUPPORT

	return create_window_status;
}

/***********************************************************************************
**
**	OpenGadgetFailed
**
***********************************************************************************/
void
OpGadgetManager::OpenGadgetFailed(OpGadgetClass* klass, OpGadget* gadget /* = NULL */)
{
	OpGadgetListener::OpGadgetStartFailedData data;
	data.gadget_class = klass;
	data.gadget = gadget;
	NotifyGadgetStartFailed(data);
}

/***********************************************************************************
**
**	CloseGadget
**
***********************************************************************************/

OP_STATUS
OpGadgetManager::CloseGadget(OpGadget *gadget, BOOL async)
{
	if (!gadget)
		return OpStatus::ERR;

	Window *window = gadget->GetWindow();
	if (!window)
		return OpStatus::OK;

	if (async)
	{
		return CloseWindowPlease(window);
	}
	else
	{
		if (gadget->GetIsClosing())
			return OpStatus::OK;

		OP_STATUS result = OpStatus::ERR;
		gadget->SetIsClosing(TRUE);
		gadget->SetWindow(NULL);
		if (window->GetWindowCommander())
#ifdef EXTENSION_SUPPORT
			if (!gadget->IsExtension())
#endif // EXTENSION_SUPPORT
				window->GetWindowCommander()->SetGadget(NULL);

		if (window->Close())
			result = OpStatus::OK;

		return result;
	}
}

/***********************************************************************************
**
**	Dump
**
***********************************************************************************/
#ifdef GADGET_DUMP_TO_FILE
OP_STATUS
OpGadgetManager::Dump(const OpStringC &filename, OpFileDescriptor *target)
{
	OP_NEW_DBG("OpGadgetManager::Dump()", "gadgets");

	TempBuffer buffer;
	OP_STATUS result = OpStatus::ERR;
	OpString error_message;

	URLContentType type = URL_GADGET_INSTALL_CONTENT;
#ifdef WEBSERVER_SUPPORT
	if (IsThisAnAlienGadgetPath(filename))
		type = URL_UNITESERVICE_INSTALL_CONTENT;
#endif
#ifdef EXTENSION_SUPPORT
	if (IsThisAnExtensionGadgetPath(filename))
		type = URL_EXTENSION_INSTALL_CONTENT;
#endif

	OpGadgetClass *klass = CreateClassWithPath(filename, type, NULL, error_message);
	if (klass)
	{
		result = klass->DumpConfiguration(&buffer);
		if (OpStatus::IsError(result))
			RETURN_IF_ERROR(error_message.Set("Dumping failed: WIDGET_ERROR_INTERNAL"));
		OP_DELETE(klass);
	}

	if (OpStatus::IsError(result))
	{
		buffer.Clear();
		RETURN_IF_ERROR(buffer.Append(error_message.CStr()));
	}

	OP_DBG((UNI_L("buffer: %s"), buffer.GetStorage()));

	RETURN_IF_ERROR(target->Open(OPFILE_OVERWRITE));
	RETURN_IF_ERROR(target->WriteUTF8Line(OpStringC(buffer.GetStorage())));
	return target->Close();
}
#endif // GADGET_DUMP_TO_FILE

/***********************************************************************************
**
**	LoadAllGadgets
**
***********************************************************************************/

OP_GADGET_STATUS
OpGadgetManager::LoadAllGadgets(unsigned int gadget_class_flags)
{
	RETURN_IF_LEAVE(m_prefsfile->LoadAllL());

	OpString_list sections;
	RETURN_IF_LEAVE(m_prefsfile->ReadAllSectionsL(sections));

	RETURN_IF_ERROR(HelpLoadAllGadgets(m_prefsfile, &sections, gadget_class_flags));

	return OpStatus::OK;
}

/***********************************************************************************
**
**	HelpLoadAllGadgets
**
***********************************************************************************/

OP_GADGET_STATUS
OpGadgetManager::HelpLoadAllGadgets(PrefsFile* datafile, OpString_list* sections, unsigned int gadget_class_flags)
{
	OP_PROBE2(OP_PROBE_GADGETS_HELPLOADALLGADGETS);
	OpString identifier;
	OpString gadget_path;
	OpString download_url;

	OP_MEMORY_VAR UINT i;
	UINT nr_of_sections = sections->Count();
	for (i = 0; i < nr_of_sections; i++)
	{
		RETURN_IF_ERROR(identifier.Set(sections->Item(i)));

		OpStringC temp_path;
		OpStringC temp_download_url;
		RETURN_IF_LEAVE(temp_path = datafile->ReadStringL(identifier, UNI_L("path to widget data")));
		RETURN_IF_LEAVE(temp_download_url = datafile->ReadStringL(identifier, UNI_L("download_URL")));

		// Either no filename or this is the widget section
		if (temp_path.IsEmpty())
			continue;

		OpFile tmp;
		// HelpSaveAllGadgets() writes the serialized name
		// (OpFile::GetSerializedName()) to the attribute "path to
		// widget data", so first try to convert the serialized name
		// to a full path:
		if (OpStatus::IsSuccess(tmp.Construct(temp_path.CStr(), OPFILE_SERIALIZED_FOLDER)))
			RETURN_IF_ERROR(gadget_path.Set(tmp.GetFullPath()));
		else
			// If we cannot construct the file using option
			// OPFILE_SERIALIZED_FOLDER, the file does not exist any
			// more or "path to widget data" is the full path to the
			// gadget:
			RETURN_IF_ERROR(gadget_path.Set(temp_path.CStr()));

		int ctypeint = -1;
		URLContentType type;
		RETURN_IF_LEAVE(ctypeint = datafile->ReadIntL(identifier, UNI_L("content-type"), -1));
		switch (ctypeint)
		{
		default:
#ifdef UPGRADE_SUPPORT
			// Previous versions did not record Content-Type, so look it up
# ifdef WEBSERVER_SUPPORT
			if (IsThisAnAlienGadgetPath(gadget_path, FALSE))
			{
				type = URL_UNITESERVICE_INSTALL_CONTENT;
				break;
			}
# endif // WEBSERVER_SUPPORT
# ifdef EXTENSION_SUPPORT
			if (IsThisAnExtensionGadgetPath(gadget_path, FALSE))
			{
				type = URL_EXTENSION_INSTALL_CONTENT;
				break;
			}
# endif // EXTENSION_SUPPORT
#endif // UPGRADE_SUPPORT
			/* fall through (regular widget) */

		case 1: type = URL_GADGET_INSTALL_CONTENT; break;

#ifdef WEBSERVER_SUPPORT
		case 2: type = URL_UNITESERVICE_INSTALL_CONTENT; break;
#endif

#ifdef EXTENSION_SUPPORT
		case 3: type = URL_EXTENSION_INSTALL_CONTENT; break;
#endif
		}

		RETURN_IF_ERROR(download_url.Set(temp_download_url));

		if (gadget_path.IsEmpty() && download_url.IsEmpty())
			continue;

		if (IsContentTypeOfGadgetClass(type, gadget_class_flags))
		{
			OP_ASSERT(FindGadget(identifier) == NULL);
			// TODO: report errors while loading.
			OpStatus::Ignore(LoadGadget(datafile, identifier, gadget_path, download_url, type));
		}
	}

	return OpStatus::OK;
}

BOOL
OpGadgetManager::IsContentTypeOfGadgetClass(URLContentType type, unsigned int gadget_class_flags)
{
	return (type == URL_GADGET_INSTALL_CONTENT && gadget_class_flags & GADGET_CLASS_WIDGET)
#ifdef WEBSERVER_SUPPORT
		    || (type == URL_UNITESERVICE_INSTALL_CONTENT && gadget_class_flags & GADGET_CLASS_UNITE)
#endif // WEBSERVER_SUPPORT
#ifdef EXTENSION_SUPPORT
		    || (type == URL_EXTENSION_INSTALL_CONTENT && gadget_class_flags & GADGET_CLASS_EXTENSION)
#endif // EXTENSION_SUPPORT
			;
}


/***********************************************************************************
**
**	LoadGadget
**
***********************************************************************************/

OP_GADGET_STATUS
OpGadgetManager::LoadGadget(PrefsFile* datafile, OpString& identifier, const OpStringC& gadget_path, const OpStringC& download_url, URLContentType type)
{
	OP_ASSERT(identifier.HasContent() && "Why was this called with an empty identifier?");
	OP_ASSERT(gadget_path.HasContent() && "Without a path the widget cannot be created");
	OP_ASSERT(!FindGadget(identifier) && "Why was the same widget saved twice??");

	if (!IsThisAGadgetPath(gadget_path))
		return OpStatus::OK;

	OpGadgetClass* gadget_class = NULL;
	RETURN_IF_ERROR(CreateGadgetClass(&gadget_class, gadget_path, type, &download_url));

	OpString url;
	RETURN_IF_ERROR(gadget_class->GetGadgetUpdateUrl(url));
	if (url.Length() > 0 && gadget_class->GetUpdateInfo() == NULL)
	{
		// There is no update info on the class, load it from the prefs file
		OpStackAutoPtr<OpGadgetUpdateInfo> update_info(OP_NEW(OpGadgetUpdateInfo, (gadget_class)));
		if (update_info.get() == NULL)
			return OpStatus::ERR_NO_MEMORY;
		OP_STATUS stat = update_info->LoadFromFile(m_prefsfile, identifier, UNI_L("update "));
		if (OpStatus::IsSuccess(stat))
			gadget_class->SetUpdateInfo(update_info.release());
	}

	OpGadget* new_gadget = OP_NEW(OpGadget, (gadget_class));
	if (!new_gadget)
		return OpStatus::ERR_NO_MEMORY;
	OpStackAutoPtr<OpGadget> new_gadget_ptr(new_gadget);

	RETURN_IF_ERROR(new_gadget->Construct(identifier));
	new_gadget_ptr.release()->Into(&m_gadgets);

	TRAP_AND_RETURN(err, new_gadget->LoadFromFileL(datafile, identifier));

	// Notify listeners that a new gadget instance has been created
	OpGadgetListener::OpGadgetInstanceData data;
	data.gadget = new_gadget;
	NotifyGadgetInstanceCreated(data);

	return OpStatus::OK;
}

/***********************************************************************************
**
**	LoadGadgets
**
***********************************************************************************/

OP_GADGET_STATUS
OpGadgetManager::LoadGadgets()
{
	unsigned int gadget_class_flags = GADGET_CLASS_WIDGET | GADGET_CLASS_EXTENSION;
# ifdef WEBSERVER_SUPPORT
	gadget_class_flags |= g_pcwebserver->GetIntegerPref(PrefsCollectionWebserver::WebserverEnable) ? GADGET_CLASS_UNITE : 0;
# endif // WEBSERVER_SUPPORT
	return LoadAllGadgets(gadget_class_flags);
}

/***********************************************************************************
**
**	SaveGadgets
**
***********************************************************************************/

OP_GADGET_STATUS
OpGadgetManager::SaveGadgets()
{
	if (m_gadgets.First() == NULL)
		return OpStatus::OK;

	RETURN_IF_LEAVE(m_prefsfile->WriteIntL("widgets", "version", GADGET_PERSISTENT_DATA_FORMAT_VERSION));

	RETURN_IF_ERROR(HelpSaveAllGadgets(m_prefsfile));
	RETURN_IF_ERROR(CommitPrefs());

	return OpStatus::OK;
}

/***********************************************************************************
**
**	CommitPrefs
**
***********************************************************************************/

OP_STATUS
OpGadgetManager::CommitPrefs()
{
	if (!m_write_msg_posted)
	{
		if (!g_main_message_handler->PostDelayedMessage(MSG_GADGET_DELAYED_PREFS_WRITE, reinterpret_cast<MH_PARAM_1>(this), 0, GADGETS_WRITE_PREFS_DELAY))
			return OpStatus::ERR;

		m_write_msg_posted = TRUE;
	}

	return OpStatus::OK;
}

/***********************************************************************************
**
**	HandleCallback
**
***********************************************************************************/

void
OpGadgetManager::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	if (msg == MSG_GADGET_DELAYED_PREFS_WRITE)
	{
		m_write_msg_posted = FALSE;
		TRAPD(err, m_prefsfile->CommitL(TRUE, FALSE));
		RAISE_AND_RETURN_VOID_IF_ERROR(err);
	}
#ifdef GADGET_CLEAN_CACHE_DIR_AT_START
	if (msg == MSG_GADGET_CLEAN_CACHE)
	{
		CleanCache();
	}
#endif // GADGET_CLEAN_CACHE_DIR_AT_START
}

/***********************************************************************************
**
**	SignatureVerified
**
***********************************************************************************/

#ifdef SIGNED_GADGET_SUPPORT
void
OpGadgetManager::SignatureVerified(OpGadgetClass *klass)
{
	OP_ASSERT(klass->SignatureState() != GADGET_SIGNATURE_PENDING);

	OpGadgetListener::OpGadgetSignatureVerifiedData data;
	data.gadget_class = klass;
	NotifyGadgetSignatureVerified(data);

	OpGadget *gadget = static_cast<OpGadget*>(m_gadgets.First());
	while (gadget)
	{
		if (gadget->GetIsOpening() && gadget->GetClass() == klass)
		{
#ifdef WEBSERVER_SUPPORT
			if (gadget->IsRootService())
				RAISE_AND_RETURN_VOID_IF_ERROR(OpenRootService(gadget));
			else
#endif // WEBSERVER_SUPPORT
				RAISE_AND_RETURN_VOID_IF_ERROR(OpenGadget(gadget));
		}

		gadget = static_cast<OpGadget*>(gadget->Suc());
	}
}
#endif // SIGNED_GADGET_SUPPORT

/***********************************************************************************
**
**	HelpSaveAllGadgets
**
***********************************************************************************/

OP_GADGET_STATUS
OpGadgetManager::HelpSaveAllGadgets(PrefsFile* prefsfile)
{
	OpString download_url;
	OpString identifier;

	OpGadget * OP_MEMORY_VAR gadget = static_cast<OpGadget *>(m_gadgets.First());
	while (gadget)
	{
		RETURN_IF_ERROR(identifier.Set(gadget->GetIdentifier()));
		RETURN_IF_ERROR(gadget->GetGadgetDownloadUrl(download_url));

		OpFile tmp;
		RETURN_IF_ERROR(tmp.Construct(gadget->GetGadgetPath(), OPFILE_SERIALIZED_FOLDER));

		RETURN_IF_LEAVE(prefsfile->WriteStringL(identifier, UNI_L("path to widget data"), tmp.GetSerializedName());
						prefsfile->WriteStringL(identifier, UNI_L("download_URL"), download_url);
						gadget->SaveToFileL(prefsfile, identifier));

		// Serialize representation of MIME type, if known
		OP_MEMORY_VAR int ctypeint = 0;
		switch (gadget->GetClass()->GetContentType())
		{
		case URL_GADGET_INSTALL_CONTENT:
			ctypeint = 1;
			break;

#ifdef WEBSERVER_SUPPORT
		case URL_UNITESERVICE_INSTALL_CONTENT:
			ctypeint = 2;
			break;
#endif

#ifdef EXTENSION_SUPPORT
		case URL_EXTENSION_INSTALL_CONTENT:
			ctypeint = 3;
			break;
#endif
		}
		if (ctypeint)
			RETURN_IF_LEAVE(prefsfile->WriteIntL(identifier, UNI_L("content-type"), ctypeint));

		RETURN_IF_ERROR(gadget->GetClass()->SaveToFile(prefsfile, identifier, UNI_L("class ")));

		OpGadgetUpdateInfo *info = gadget->GetClass()->GetUpdateInfo();
		if (info)
		{
			RETURN_IF_ERROR(info->SaveToFile(m_prefsfile, identifier, UNI_L("update ")));
			RETURN_IF_ERROR(CommitPrefs());
		}

		gadget = static_cast<OpGadget *>(gadget->Suc());
	}

	return OpStatus::OK;
}

/***********************************************************************************
**
**	RemoveGadget
**
***********************************************************************************/

OP_STATUS
OpGadgetManager::RemoveGadget(const OpStringC& gadget_path)
{
	// Check if it is a gadget path
	if (!IsThisAGadgetPath(gadget_path))
		return OpStatus::OK;

	// Destroy gadget objects constructed from gadget_path
	while (OpGadget* gadget = FindGadget(GADGET_FIND_BY_PATH, gadget_path))
	{
		// Notify listeners that the gadget instance is about to be removed
		OpGadgetListener::OpGadgetInstanceData data;
		data.gadget = gadget;
		NotifyGadgetInstanceRemoved(data);
		DestroyGadget(gadget);
	}

	// Delegate gadget class associated to gadget_path
	OpGadgetClass *gadget_class = NULL;
	RETURN_IF_ERROR(FindGadgetClass(gadget_class, gadget_path));

	if (gadget_class)
	{
		// Notify listeners that the gadget is about to be removed
		OpGadgetListener::OpGadgetInstallRemoveData data;
		data.gadget_class = gadget_class;
		NotifyGadgetRemoved(data);

		// Remove the gadget
		DeleteGadgetClass(gadget_class);
	}

#ifdef GADGET_CLEAN_CACHE_DIR_AT_START
	// Try removing the files associated with the gadget
	g_main_message_handler->PostDelayedMessage(MSG_GADGET_CLEAN_CACHE, reinterpret_cast<MH_PARAM_1>(this), 0, GADGETS_WRITE_PREFS_DELAY);
#endif // GADGET_CLEAN_CACHE_DIR_AT_START

	return OpStatus::OK;
}

/***********************************************************************************
**
**	FindGadgetClass
**
***********************************************************************************/

OP_STATUS
OpGadgetManager::FindGadgetClass(OpGadgetClass*& gadget_class, const OpStringC& gadget_path)
{
	gadget_class = NULL;

	OpGadgetClass *klass = static_cast<OpGadgetClass *>(m_classes.First());
	while (klass)
	{
		OpStringC klass_gadget_path(klass->GetGadgetPath());

		if (gadget_path.Compare(klass_gadget_path) == 0)
		{
			gadget_class = klass;
			break;
		}

		klass = static_cast<OpGadgetClass *>(klass->Suc());
	}

	return OpStatus::OK;
}

/***********************************************************************************
**
**	DeleteGadgetClass -- remove data the widget has been using, such as the .zip file, cache etc.
**
***********************************************************************************/

OP_STATUS
OpGadgetManager::DeleteGadgetClass(OpGadgetClass* gadget_class)
{
	if (!gadget_class || NumGadgetInstancesOf(gadget_class) > 0)
		return OpStatus::ERR;

	OpString directory;
	OpFile fp;

	RETURN_IF_ERROR(directory.Set(gadget_class->GetGadgetPath()));

	OP_DELETE(gadget_class);

	if (OpStatus::IsSuccess(fp.Construct(directory.CStr())))
	{
#if defined(USE_ASYNC_FILEMAN) && defined(UTIL_ASYNC_FILE_OP)
		OpStatus::Ignore(fp.DeleteAsync());
#else
		OpStatus::Ignore(fp.Delete());
#endif // USE_ASYNC_FILEMAN && UTIL_ASYNC_FILE_OP
	}

	// Try to destroy on destruction as well.
	OpString* item = OP_NEW(OpString, ());
	if (!item)
		return OpStatus::ERR_NO_MEMORY;
	OpAutoPtr<OpString> item_anchor(item);
	RETURN_IF_ERROR(item->Set(directory));
	RETURN_IF_ERROR(m_delete_on_exit.Add(item_anchor.release()));

	return OpStatus::OK;
}

/***********************************************************************************
**
**	DestroyGadget
**
***********************************************************************************/

OP_STATUS
OpGadgetManager::DestroyGadget(OpGadget* gadget)
{
	if (!gadget)
		return OpStatus::OK;

	RETURN_IF_LEAVE(m_prefsfile->DeleteSectionL(gadget->GetIdentifier()));
	RETURN_IF_LEAVE(CommitPrefs());

	OpString *dir = OP_NEW(OpString, ());
	if (!dir)
		return OpStatus::ERR_NO_MEMORY;
	OpStackAutoPtr<OpString> dir_ptr(dir);

	RETURN_IF_ERROR(dir->Set(gadget->GetIdentifier()));
	RETURN_IF_ERROR(m_delete_on_exit.Add(dir_ptr.release()));

	OP_DELETE(gadget);

#ifdef UTIL_ZIP_CACHE
	g_opera->util_module.m_zipcache->FlushUnused();
#endif // UTIL_ZIP_CACHE

	return OpStatus::OK;
}

/***********************************************************************************
**
**  FindInactiveGadget
**
***********************************************************************************/

OP_STATUS
OpGadgetManager::FindInactiveGadget(OpGadget** found_gadget, OpGadgetClass* gadget_class)
{
	OP_ASSERT(found_gadget);
	OP_ASSERT(gadget_class);

	// Loop through gadgets list. A match happens when:
	// - Gadget has the same class as gadget_class
	// - Gadget is not active, i.e., not in use.

	*found_gadget = NULL;

	OpGadget *gadget = static_cast<OpGadget *>(m_gadgets.First());
	while (gadget)
	{
		if (!gadget->GetIsActive() &&
			gadget->GetClass() == gadget_class)
		{
			*found_gadget = gadget;
			break;
		}

		gadget = static_cast<OpGadget *>(gadget->Suc());
	}

	return OpStatus::OK;
}

/***********************************************************************************
**
**	CreateGadget
**
***********************************************************************************/

OP_GADGET_STATUS
OpGadgetManager::CreateGadget(OpGadget** gadget, OpGadgetClass *klass)
{
	OP_PROBE2(OP_PROBE_GADGETS_CREATEGADGET);
	*gadget = NULL;

	if (!klass)
		return OpStatus::ERR;

	OpGadget* new_gadget = NULL;
	OpAutoPtr<OpGadget> new_gadget_ptr;

	// Try to get a gadget object that is inactive
	RETURN_IF_ERROR(FindInactiveGadget(&new_gadget, klass));

	if (new_gadget)
		*gadget = new_gadget;
	else
	{
		new_gadget = OP_NEW(OpGadget, (klass));
		if (!new_gadget)
			return OpStatus::ERR_NO_MEMORY;
		new_gadget_ptr = new_gadget;

		OpString id;
		RETURN_IF_ERROR(new_gadget->Construct(id));

		OpString name;
		RETURN_IF_ERROR(klass->GetGadgetName(name));
		RETURN_IF_ERROR(new_gadget->SetUIData(UNI_L("name"), name.CStr()));

		*gadget = new_gadget;
		new_gadget_ptr.release()->Into(&m_gadgets);

		RETURN_IF_ERROR(SaveGadgets());
	}

	new_gadget->SetIsActive(TRUE);

	// Notify listeners that a new gadget instance has been created
	OpGadgetListener::OpGadgetInstanceData data;
	data.gadget = *gadget;
	NotifyGadgetInstanceCreated(data);

	return OpStatus::OK;
}

OP_GADGET_STATUS
OpGadgetManager::CreateGadget(OpGadget** gadget, const OpStringC& gadget_path_arg, URLContentType type)
{
	*gadget = NULL;

	OpGadgetClass* gadget_class = NULL;
	RETURN_IF_ERROR(FindOrCreateClassWithPath(&gadget_class, gadget_path_arg, type));

	OP_ASSERT(gadget_class);
	if (!gadget_class)
		return OpStatus::ERR;

	return CreateGadget(gadget, gadget_class);
}

OP_GADGET_STATUS
OpGadgetManager::UpgradeGadget(OpGadget* gadget, const OpStringC& gadget_path, URLContentType type)
{
	// Error if it is not a gadget folder.
	if (!IsThisAGadgetPath(gadget_path))
		return OpStatus::ERR;

	OpGadgetClass* gadget_class = NULL;

	OpString url;
	RETURN_IF_ERROR(gadget->GetGadgetDownloadUrl(url));
	RETURN_IF_ERROR(FindOrCreateClassWithPath(&gadget_class, gadget_path, type, &url));

	OP_ASSERT(gadget_class);
	if (!gadget_class)
		return OpStatus::ERR;

	gadget->Reload();

	RETURN_IF_ERROR(gadget->Upgrade(gadget_class));

	return OpStatus::OK;
}

OP_GADGET_STATUS
OpGadgetManager::UpgradeGadget(const uni_char* id, const OpStringC& gadget_path, URLContentType type)
{
	OpGadget *gadget;
	OpStringC find_param(id);
	gadget = FindGadget(GADGET_FIND_BY_GADGET_ID, find_param);

	if (gadget)
		return UpgradeGadget(gadget, gadget_path, type);

	return OpStatus::ERR;
}

/***********************************************************************************
**
**	FindGadget
**
***********************************************************************************/

OpGadget*
OpGadgetManager::FindGadget(const OpStringC& unique_identifier)
{
	return FindGadget(GADGET_FIND_BY_ID, unique_identifier);
}

/** \deprecated */
OpGadget*
OpGadgetManager::FindGadget(const OpStringC8& unique_identifier)
{
	OpString tmp;
	RETURN_VALUE_IF_ERROR(tmp.Set(unique_identifier), NULL);
	return FindGadget(GADGET_FIND_BY_ID, tmp);
}

/***********************************************************************************
**
**	FindGadget
**
***********************************************************************************/

OpGadget*
OpGadgetManager::FindGadget(const GADGETFindType find_type, const OpStringC& find_param, const GADGETFindState state)
{
	OpString param;

	if (find_param.IsEmpty())
		return NULL;

	// Ignore protocol token if find_param contains any.
	const uni_char *sub_param = uni_strchr(find_param.CStr(), ':');
	if (sub_param)
		sub_param++;

	OpGadget *gadget = static_cast<OpGadget *>(m_gadgets.First());
	while (gadget)
	{
		switch(find_type)
		{
		case(GADGET_FIND_BY_NAME):
			{
				if (OpStatus::IsError(gadget->GetGadgetName(param)))
					continue;
			}
			break;
		case(GADGET_FIND_BY_PATH):
			{
				const uni_char *tmp_path = gadget->GetGadgetPath();
				if (!tmp_path || OpStatus::IsError(param.Set(tmp_path)))
					continue;
			}
			break;
		case(GADGET_FIND_BY_URL):
			{
				if (OpStatus::IsError(gadget->GetGadgetUrl(param)))
					continue;
			}
			break;
		case(GADGET_FIND_BY_URL_WITHOUT_INDEX):
			{
				if (OpStatus::IsError(gadget->GetGadgetUrl(param, FALSE, TRUE)))
					continue;
			}
			break;
		case(GADGET_FIND_BY_DOWNLOAD_URL):
			{
				if (OpStatus::IsError(gadget->GetGadgetDownloadUrl(param)))
					continue;
			}
			break;

		case(GADGET_FIND_BY_ID):
			{
				if (OpStatus::IsError(param.Set(gadget->GetIdentifier())))
					continue;
			}
			break;
		case(GADGET_FIND_BY_FILENAME):
			{
				if (OpStatus::IsError(gadget->GetGadgetFileName(param)))
					continue;
			}
			break;
		case(GADGET_FIND_BY_GADGET_ID):
			{
				if(OpStatus::IsError(gadget->GetGadgetId(param)))
					continue;
			}
			break;
#ifdef WEBSERVER_SUPPORT
		case(GADGET_FIND_BY_SERVICE_NAME):
			{
				if(OpStatus::IsError(param.Set(gadget->GetUIData(UNI_L("serviceName")))))
					continue;
			}
			break;
#endif
		default:
			return NULL;
		}

		if (param.HasContent() &&
			((uni_stricmp(param.CStr(), find_param.CStr()) == 0) || (sub_param && (uni_stricmp(param.CStr(), sub_param) == 0))))
		{
			if (( gadget->GetWindow() && (state & GADGET_FIND_ACTIVE  )) ||
				(!gadget->GetWindow() && (state & GADGET_FIND_INACTIVE)))
			{
				return gadget;
			}
		}

		gadget = static_cast<OpGadget *>(gadget->Suc());
	}

	return NULL;
}


/***********************************************************************************
**
**	CreateGadgetClass
**
***********************************************************************************/

OP_GADGET_STATUS
OpGadgetManager::CreateGadgetClass(const OpStringC& gadget_path, URLContentType type)
{
	OpGadgetClass *klass;
	return CreateGadgetClass(&klass, gadget_path, type);
}

/***********************************************************************************
**
**	CreateGadgetClass
**
***********************************************************************************/

OP_GADGET_STATUS
OpGadgetManager::CreateGadgetClass(OpGadgetClass** gadget_class, const OpStringC& gadget_path, URLContentType type, const OpStringC* download_url)
{
	return FindOrCreateClassWithPath(gadget_class, gadget_path, type, download_url);
}

/***********************************************************************************
**
**	FindOrCreateClassWithPath
**
***********************************************************************************/

OP_GADGET_STATUS
OpGadgetManager::FindOrCreateClassWithPath(OpGadgetClass** gadget_class, const OpStringC& gadget_path_arg, URLContentType type, const OpStringC* download_url)
{
	OP_PROBE2(OP_PROBE_GADGETS_FINDORCREATECLASSWITHPATH);
	if (gadget_path_arg.IsEmpty())
		return OpStatus::ERR;

	OpGadget *gadget = NULL;
	OpString gadget_path;
	RETURN_IF_ERROR(NormalizeGadgetPath(gadget_path, gadget_path_arg, &gadget));
	if (type == URL_XML_CONTENT && gadget_path.Length() != gadget_path_arg.Length())
	{
		// We were passed a config.xml document, and NormalizeGadgetPath gave
		// us the containing folder. We need to look at the extension of the
		// containing folder to find a MIME type.
#if defined WEBSERVER_SUPPORT || defined EXTENSION_SUPPORT
		URLContentType detected_type = URL_UNKNOWN_CONTENT;
		Viewer *v = g_viewers->FindViewerByFilename(gadget_path.CStr());
		if (v)
			detected_type = v->GetContentType();

		switch (detected_type)
		{
#ifdef WEBSERVER_SUPPORT
		case URL_UNITESERVICE_INSTALL_CONTENT:
#endif

#ifdef EXTENSION_SUPPORT
		case URL_EXTENSION_INSTALL_CONTENT:
#endif // EXTENSION_SUPPORT
		case URL_GADGET_INSTALL_CONTENT:
			// We know this.
			type = detected_type;
			break;

		default:
			// Fall back to a standard gadget, since the structure is
			// consistent with this being one.
#endif
			type = URL_GADGET_INSTALL_CONTENT;
#if defined WEBSERVER_SUPPORT || defined EXTENSION_SUPPORT
		}
#endif
	}

	if (gadget)
	{
		*gadget_class = gadget->GetClass();
		return OpStatus::OK;
	}

	OpGadgetClass* klass = static_cast<OpGadgetClass *>(m_classes.First());
	while (klass)
	{
		OpStringC klass_gadget_path(klass->GetGadgetPath());

		if (gadget_path.Compare(klass_gadget_path) == 0 && klass->GetContentType() == type)
		{
			*gadget_class = klass;
			return OpStatus::OK;
		}

		klass = static_cast<OpGadgetClass *>(klass->Suc());
	}

	//  If we got here, then the class isn't loaded, so we'll try to load it now.
	OpString error_message;
	OpGadgetClass *gadget_class_ptr = CreateClassWithPath(gadget_path, type, download_url ? download_url->CStr(): NULL, error_message);
	if (!gadget_class_ptr)
	{
		GADGETS_REPORT_ERROR_TO_CONSOLE((NULL, UNI_L("%s"), error_message.CStr()));
		return OpStatus::ERR_NO_MEMORY;
	}

	*gadget_class = gadget_class_ptr;
	(*gadget_class)->Into(&m_classes);
	OpGadgetListener::OpGadgetInstallRemoveData data;
	data.gadget_class = *gadget_class;
	NotifyGadgetInstalled(data);

	return OpStatus::OK;
}

/***********************************************************************************
**
**	CreateClassWithPath
**
***********************************************************************************/

OpGadgetClass*
OpGadgetManager::CreateClassWithPath(const OpStringC& gadget_path, URLContentType type, const uni_char* download_url, OpString& error_message)
{
	OP_PROBE2(OP_PROBE_GADGETS_CREATECLASSWITHPATH);

	OpString load_error_message;
	OpAutoPtr<OpGadgetClass> gadget_class(OP_NEW(OpGadgetClass, ()));
	OP_STATUS error = OpStatus::OK;
	do
	{
		if (!gadget_class.get())
		{
			error = OpStatus::ERR_NO_MEMORY;
			break;
		}

		if (download_url && *download_url)
			error = gadget_class->SetGadgetDownloadUrl(download_url);
		if (OpStatus::IsError(error))
			break;
		error = gadget_class->Construct(gadget_path, type);
		if (OpStatus::IsError(error))
			break;
		error = gadget_class->LoadInfoFromInfoFile(load_error_message);
	} while (FALSE);

	if (OpStatus::IsError(error))
	{
		OpGadgetClass::GetGadgetErrorMessage(error, load_error_message.CStr(), error_message);
		return NULL;
	}

	return gadget_class.release();
}

/***********************************************************************************
**
**	GetIDs
**
***********************************************************************************/

OP_STATUS
OpGadgetManager::GetIDs(OpVector<OpString>* ids)
{
	OpGadget *gadget = static_cast<OpGadget *>(m_gadgets.First());
	while (gadget)
	{
		OpString* id = OP_NEW(OpString, ());
		if (!id)
			return OpStatus::ERR_NO_MEMORY;
		OpStackAutoPtr<OpString> id_ptr(id);

		RETURN_IF_ERROR(id->Set(gadget->GetIdentifier()));
		RETURN_IF_ERROR(ids->Add(id_ptr.release()));

		gadget = static_cast<OpGadget *>(gadget->Suc());
	}

	return OpStatus::OK;
}

/** \deprecated */
OP_STATUS
OpGadgetManager::GetIDs(OpVector<OpString8>* ids)
{
	OpGadget *gadget = static_cast<OpGadget *>(m_gadgets.First());
	while (gadget)
	{
		OpString id;
		OpString8* id8 = OP_NEW(OpString8, ());
		if (!id8)
			return OpStatus::ERR_NO_MEMORY;
		OpStackAutoPtr<OpString8> id8_ptr(id8);

		RETURN_IF_ERROR(id.Set(gadget->GetIdentifier()));
		RETURN_IF_ERROR(id8->Set(id.CStr()));
		RETURN_IF_ERROR(ids->Add(id8_ptr.release()));

		gadget = static_cast<OpGadget *>(gadget->Suc());
	}

	return OpStatus::OK;
}

/***********************************************************************************
**
**	GetIDForURL
**
***********************************************************************************/

OP_STATUS
OpGadgetManager::GetIDForURL(OpString& id, const OpStringC& url)
{
	OpGadget *gadget = static_cast<OpGadget *>(m_gadgets.First());
	while (gadget)
	{
		const uni_char* dl_url = gadget->GetUIData(UNI_L("original_widget_download_URL"));

		if (url.Compare(dl_url) == 0)
			return id.Set(gadget->GetIdentifier());

		gadget = static_cast<OpGadget *>(gadget->Suc());
	}

	//  If we get here, no gadget came from that url.
	id.Empty();
	return OpStatus::OK;
}

PrefsFile*
OpGadgetManager::CreatePersistentDataFile()
{
	OpFile file;

	RETURN_VALUE_IF_ERROR(file.Construct(GADGET_PERSISTENT_DATA_FILENAME, OPFILE_GADGET_FOLDER), NULL);

	PrefsFile *prefsfile = OP_NEW(PrefsFile, (PREFS_XML));
	if (!prefsfile)
		return NULL;
	OpStackAutoPtr<PrefsFile> prefsfile_ptr(prefsfile);

	TRAPD(err, prefsfile_ptr->ConstructL(); prefsfile_ptr->SetFileL(&file));
	RETURN_VALUE_IF_ERROR(err, NULL);

	return prefsfile_ptr.release();;
}

OP_STATUS
OpGadgetManager::SetGlobalData(const uni_char *key, const uni_char *data)
{
	RETURN_IF_LEAVE(m_prefsfile->WriteStringL(UNI_L("global"), key, data));
	RETURN_IF_ERROR(CommitPrefs());
	return OpStatus::OK;
}

/***********************************************************************************
**
**	GetNamespaceUrl
**
***********************************************************************************/

OP_STATUS OpGadgetManager::GetNamespaceUrl(GadgetNamespace gadgetNamespace, OpString &url)
{
	switch (gadgetNamespace)
	{
	case GADGETNS_UNKNOWN: return url.Set("");
	case GADGETNS_OPERA_2006: return url.Set("http://xmlns.opera.com/2006/widget");
	case GADGETNS_W3C_1_0: return url.Set("http://www.w3.org/ns/widgets");
	case GADGETNS_OPERA: return url.Set("");
	case GADGETNS_JIL_1_0: return url.Set("http://www.jil.org/ns/widgets");
	default:
		OP_ASSERT(!"Unknown namespace enum, cannot get URL");
		return OpStatus::ERR;
	}
}

const uni_char*
OpGadgetManager::GetGlobalData(const uni_char *key)
{
	OP_STATUS err;
	OpStringC data;
	TRAP_AND_RETURN_VALUE_IF_ERROR(err, data = m_prefsfile->ReadStringL(UNI_L("global"), key), NULL);
	return data.CStr();
}

OP_STATUS
OpGadgetManager::FindGadgetResource(MessageHandler* msg_handler, URL* url, OpString& full_path)
{
	OP_ASSERT(msg_handler);
	OP_ASSERT(url);

	if (!msg_handler || !url || url->GetAttribute(URL::KType) != URL_WIDGET)
		return OpStatus::ERR;

	// Get server name (widget id).
	const ServerName *sn = reinterpret_cast<const ServerName *>(url->GetAttribute(URL::KServerName, NULL));
	const uni_char *server_name = sn ? sn->UniName() : NULL;

	if (!server_name)
		return OpStatus::ERR;

	OpGadget* gadget;
	URL_CONTEXT_ID id;
	BOOL widget_manager_access = FALSE;
	OpSecurityState state;

	OP_STATUS error = OpStatus::ERR;
	if (msg_handler->GetDocumentManager())
		error = OpSecurityManager::CheckSecurity(OpSecurityManager::GADGET_MANAGER_ACCESS, OpSecurityContext(msg_handler->GetDocumentManager()->GetCurrentURL()), OpSecurityContext(*url), widget_manager_access);
	if (OpStatus::IsSuccess(error) && widget_manager_access)
	{
		// If we have widget manager access then we try to access the file as if the gadet tried to acces its own files.
		gadget = g_gadget_manager->FindGadget(GADGET_FIND_BY_ID, server_name, FALSE);
		id = gadget->UrlContextId();
	}
	else // Else just use current gadget.
	{
		gadget = msg_handler->GetWindow() ? msg_handler->GetWindow()->GetGadget() : NULL;
		id = url->GetContextId();
	}

	if (!gadget || !id)
		return OpStatus::ERR;

	OP_ASSERT(gadget->GetIdentifier());
	if ((gadget->UrlContextId() == id) && uni_stri_eq(server_name, gadget->GetIdentifier()))
	{
		// Yup, this is a gadget. http://i.imgur.com/XYoWg.jpg

		OpString temp;
		RETURN_IF_ERROR(url->GetAttribute(URL::KUniPath_Escaped, temp));
		if (!temp.CStr())
			RETURN_IF_ERROR(temp.Set(""));

		// Remove URL escapes
		UriUnescape::ReplaceChars(temp.CStr(), UriUnescape::LocalfileUrlUtf8);

#if PATHSEPCHAR != '/'
		// Convert to local path separators. Cannot use the
		// UriUnescape::LocalfileUtf8 flag above because it does not converts
		// both "/" and "%5C" to "\". The call below converts "\" to "/".
		OpGadgetUtils::ChangeToLocalPathSeparators(temp.DataPtr());
#endif

		uni_char * file_name = temp.DataPtr();
		while (*file_name == PATHSEPCHAR)
			++file_name; // remove leading slash from "path" part of url.

		// Need to figure out the file-system path (might be localized).
		// This will also check that the file name does not contain any reserved
		// characters.
		BOOL found = FALSE;
		Findi18nPath(gadget->GetClass(), file_name, gadget->GetClass()->HasLocalesFolder(), full_path, found, NULL);

		// If this is the root document, we set the URL's MIME type
		// and character encoding here, if any were set.
		if (found && 0 == uni_strcmp(file_name, gadget->GetClass()->GetGadgetFile()))
		{
			// MIME type.
			OpString8 mime_type;
			if (OpStatus::IsSuccess(mime_type.Set(gadget->GetAttribute(WIDGET_CONTENT_ATTR_TYPE))) &&
				!mime_type.IsEmpty())
			{
				url->SetAttribute(URL::KMIME_ForceContentType, mime_type);
			}

			// Character encoding.
			OpString8 encoding;
			if (OpStatus::IsSuccess(encoding.Set(gadget->GetAttribute(WIDGET_CONTENT_ATTR_CHARSET))))
			{
				if (!encoding.IsEmpty())
				{
					url->SetAttribute(URL::KMIME_CharSet, encoding);
				}
				else if (gadget->GetClass()->SupportsNamespace(GADGETNS_W3C_1_0))
				{
					// The W3C widget mandate a default of UTF-8.
					url->SetAttribute(URL::KMIME_CharSet, "utf-8");
				}
			}
		}
		return found ? OpStatus::OK : OpStatus::ERR_FILE_NOT_FOUND;
	}

	return OpStatus::ERR;
}

BOOL
OpGadgetManager::IsThisAGadgetResource(const uni_char* resource_path)
{
	OpGadget *gadget = static_cast<OpGadget *>(m_gadgets.First());
	while (gadget)
	{
		if (gadget->GetGadgetPath() && uni_strstr(resource_path, gadget->GetGadgetPath()))
			return TRUE;

		gadget = static_cast<OpGadget *>(gadget->Suc());
	}

	return FALSE;
}

/***********************************************************************************
**
**	IsThisAValidWidgetURL
**
***********************************************************************************/

BOOL
OpGadgetManager::IsThisAValidWidgetURL(const OpStringC& widget_url)
{
	if (widget_url.Find(UNI_L("widget://")) != 0)
		return FALSE;

	OpString gadget_path;
	RETURN_VALUE_IF_ERROR(NormalizeGadgetPath(gadget_path, widget_url), FALSE);

	OpFile file;
	RETURN_VALUE_IF_ERROR(file.Construct(gadget_path.CStr(), OPFILE_ABSOLUTE_FOLDER, OPFILE_FLAGS_ASSUME_ZIP | OPFILE_FLAGS_CASE_SENSITIVE_ZIP), FALSE);

	BOOL valid = FALSE;
	RETURN_VALUE_IF_ERROR(file.Exists(valid), FALSE);
	return valid;
}

/***********************************************************************************
**
**	IsThisAGadgetPath
**
***********************************************************************************/

BOOL
OpGadgetManager::IsThisAGadgetPath(const OpStringC& gadget_path)
{
	OP_PROBE2(OP_PROBE_GADGETS_ISTHISAGADGETPATH);
	if (!gadget_path.HasContent())
		return FALSE;

	// See if it is a proper widget URL
	if (IsThisAValidWidgetURL(gadget_path))
		return TRUE;

	// See if it correspond to a known gadget class
	OpGadgetClass *klass = static_cast<OpGadgetClass *>(m_classes.First());
	while (klass)
	{
		const uni_char *klass_path = klass->GetGadgetPath();
		if (klass_path && uni_stricmp(klass_path, gadget_path.CStr()) == 0)
			return TRUE;

		klass = static_cast<OpGadgetClass *>(klass->Suc());
	}

	// See if it corresponds to a widget archive
	BOOL exists = FALSE;
	OpFileInfo::Mode mode = OpFileInfo::FILE;
	OpFile file;
	RETURN_VALUE_IF_ERROR(file.Construct(gadget_path.CStr(), OPFILE_ABSOLUTE_FOLDER, OPFILE_FLAGS_ASSUME_ZIP | OPFILE_FLAGS_CASE_SENSITIVE_ZIP), FALSE);
	RETURN_VALUE_IF_ERROR(file.Exists(exists), FALSE);

	if (!exists)
		return FALSE;

	// If this was the config.xml, it is a widget
	int config_file_name_index = gadget_path.Find(GADGET_CONFIGURATION_FILE);
	if (config_file_name_index != KNotFound &&
		config_file_name_index > 1 &&
#if PATHSEPCHAR != '/'
		(PATHSEPCHAR == gadget_path[config_file_name_index - 1] ||
		 '/' == gadget_path[config_file_name_index - 1]) &&
#else
		'/' == gadget_path[config_file_name_index - 1] &&
#endif
	    config_file_name_index + GADGET_CONFIGURATION_FILE_LEN == gadget_path.Length())
		return TRUE;

	// Check if it's a zip file
	RETURN_VALUE_IF_ERROR(file.GetMode(mode), FALSE);
	if (mode == OpFileInfo::FILE && !OpZip::IsFileZipCompatible(&file))
		return FALSE;

	// Now see if we can find a config.xml in the root directory
	OpString config_xml_path;
	OpFile config_xml_file;
	RETURN_VALUE_IF_ERROR(config_xml_path.AppendFormat(UNI_L("%s%s%s"), gadget_path.CStr(), UNI_L(PATHSEP), GADGET_CONFIGURATION_FILE), FALSE);
	RETURN_VALUE_IF_ERROR(config_xml_file.Construct(config_xml_path.CStr(), OPFILE_ABSOLUTE_FOLDER, OPFILE_FLAGS_ASSUME_ZIP | OPFILE_FLAGS_CASE_SENSITIVE_ZIP), FALSE);
	RETURN_VALUE_IF_ERROR(config_xml_file.Exists(exists), FALSE);

	if (exists)
	{
		RETURN_VALUE_IF_ERROR(config_xml_file.GetMode(mode), FALSE);
		return mode == OpFileInfo::FILE;
	}

	// Legacy widgets may contain a sub-directory with the same name as
	// the widget, and store the config.xml file there. Allow that.
	OpString legacy_root_path;
	RETURN_VALUE_IF_ERROR(FindLegacyRootPath(gadget_path, legacy_root_path), FALSE);
	return !legacy_root_path.IsEmpty();
}

#if defined WEBSERVER_SUPPORT || defined EXTENSION_SUPPORT
/***********************************************************************************
**
**	IsThisA
**
***********************************************************************************/

BOOL OpGadgetManager::IsThisA(const OpStringC& gadget_path, URLContentType wanted_type, BOOL allow_noext)
{
	OP_PROBE2_PARAM(OP_PROBE_GADGETS_ISTHISA, wanted_type);
	// Fetch MIME type from file name extension.
	URLContentType type = allow_noext ? wanted_type : URL_UNKNOWN_CONTENT;
	Viewer *v = g_viewers->FindViewerByFilename(gadget_path.CStr());
	if (v)
		type = v->GetContentType();

	// Fail check if the file name extension is wrong (or missing if it was
	// required).
	if (type != wanted_type)
		return FALSE;

	// Both Unite apps and Extensions must pass the regular widget check.
	if (!IsThisAGadgetPath(gadget_path))
		return FALSE;

	OpGadgetClass *klass = OP_NEW(OpGadgetClass, ());
	if (!klass)
	{
		g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);
		return FALSE;
	}

	// Now try parsing the configuration file and see if we get the
	// expected result back.
	OpString error_reason;
	BOOL result = FALSE;
	if (OpStatus::IsSuccess(klass->Construct(gadget_path, type)) &&
		OpStatus::IsSuccess(klass->LoadInfoFromInfoFile(error_reason)))
	{
		switch (wanted_type)
		{
#ifdef WEBSERVER_SUPPORT
		case URL_UNITESERVICE_INSTALL_CONTENT:
			result = klass->IsSubserver()
					&& GADGET_WEBSERVER_TYPE_SERVICE == klass->GetWebserverType();
			break;
#endif

#ifdef EXTENSION_SUPPORT
		case URL_EXTENSION_INSTALL_CONTENT:
			result = klass->IsExtension();
			break;
#endif
		}
	}

	OP_DELETE(klass);
	return result;
}
#endif

#ifdef WEBSERVER_SUPPORT
OP_GADGET_STATUS OpGadgetManager::OpenUniteServices()
{
	OpGadget *gadget = static_cast<OpGadget *>(m_gadgets.First());
	while (gadget)
	{
		OpGadgetClass *klass = gadget->GetClass();
		if (klass->IsSubserver()
			&& GADGET_WEBSERVER_TYPE_SERVICE == klass->GetWebserverType()
			&& !gadget->IsRootService())
		{
			OpenGadget(gadget);
		}

		gadget = static_cast<OpGadget *>(gadget->Suc());
	}

	return OpStatus::OK;
}

OP_GADGET_STATUS
OpGadgetManager::CloseUniteServices()
{
	OpGadget *gadget = static_cast<OpGadget *>(m_gadgets.First());

	while (gadget)
	{
		if (gadget->IsSubserver())
			RETURN_IF_MEMORY_ERROR(CloseGadget(gadget));

		gadget = static_cast<OpGadget *>(gadget->Suc());
	}

	return OpStatus::OK;
}

OP_STATUS
OpGadgetManager::GetUniqueSubServerPath(OpGadget* gadget, const OpStringC &service_path, OpString &unique_service_path)
{
	OP_ASSERT(gadget);

	const uni_char *service_name = gadget->GetUIData(UNI_L("serviceName"));

	if (service_name != NULL)
		return unique_service_path.Set(service_name);

	OpString service_path_candidate;
	RETURN_IF_ERROR(gadget->GetGadgetSubserverUri(service_path_candidate));

	int count = 1;
	BOOL not_yet_unique = TRUE;

	while (not_yet_unique)
	{
		not_yet_unique = FALSE;
		OpGadget *a_gadget = static_cast<OpGadget *>(m_gadgets.First());
		while (a_gadget)
		{
			const uni_char *serviceName = a_gadget->GetUIData(UNI_L("serviceName"));

			if (serviceName && uni_str_eq(serviceName, service_path_candidate.CStr()))
			{
				not_yet_unique = TRUE;
				RETURN_IF_ERROR(service_path_candidate.Set(service_path));
				RETURN_IF_ERROR(service_path_candidate.AppendFormat(UNI_L("_%d"), count));
				count++;
				break;
			}

			a_gadget = static_cast<OpGadget *>(a_gadget->Suc());
		}
	}

	RETURN_IF_ERROR(gadget->SetUIData(UNI_L("serviceName"), service_path_candidate.CStr()));
	RETURN_IF_ERROR(unique_service_path.Set(service_path_candidate));

	return OpStatus::OK;
}
#endif // WEBSERVER_SUPPORT

OP_STATUS
OpGadgetManager::NormalizeGadgetPath(OpString& gadget_path, const OpStringC& gadget_url, OpGadget** gadget_arg)
{
	if (gadget_url.Find(UNI_L("widget://")) == 0)
	{
		// Open widget via widget protocol

		const uni_char* first = gadget_url.CStr() + 9;
		const uni_char* last = uni_strchr(first, '/');
		int len = last ? last - first : int(KAll);

		OpString identifier;
		RETURN_IF_ERROR(identifier.Set(gadget_url.CStr() + 9, len));

		OpGadget* gadget = FindGadget(identifier);

		if (gadget_arg)
			*gadget_arg = gadget;

		if (!gadget)
			return OpStatus::ERR;

		RETURN_IF_ERROR(gadget->GetGadgetPath(gadget_path));

		if (last)
			RETURN_IF_ERROR(gadget_path.Append(last));
	}
	else
	{
		// Open widget via regular path
		// If this points to a file that is not a widget archive, assume it's config.xml
		// and default to its parent

		OpFile file;
		if (OpStatus::IsSuccess(file.Construct(gadget_url.CStr(), OPFILE_ABSOLUTE_FOLDER, OPFILE_FLAGS_ASSUME_ZIP | OPFILE_FLAGS_CASE_SENSITIVE_ZIP)))
		{
			BOOL exists;
			RETURN_IF_ERROR(file.Exists(exists));

			if (!exists)
				return OpStatus::ERR_FILE_NOT_FOUND;

			OpFileInfo::Mode mode;
			RETURN_IF_ERROR(file.GetMode(mode));

			if (mode == OpFileInfo::FILE)
			{
				OpZip zipfile;
				OpString temp_str;
				RETURN_IF_ERROR(temp_str.Set(gadget_url));
#ifdef UTIL_ZIP_CACHE
				if(g_zipcache->IsZipFile(gadget_url))
#else // !UTIL_ZIP_CACHE
				OpZip zip_file;
				// Doing it using zip_file.Open() instead of zip_file.IsZipFile()
				// because the first variant contains the second plus additional checks.
				if (OpStatus::IsError(zip_file.Open(gadget_url, FALSE)))
#endif // !UTIL_ZIP_CACHE
					RETURN_IF_ERROR(gadget_path.Set(gadget_url));
				else
				{
					RETURN_IF_ERROR(file.GetDirectory(gadget_path));
					if (gadget_path.HasContent() && gadget_path[gadget_path.Length() - 1] == PATHSEPCHAR)
						gadget_path[gadget_path.Length() - 1] = '\0';
				}
			}
			else if (mode == OpFileInfo::DIRECTORY)
			{
				RETURN_IF_ERROR(gadget_path.Set(gadget_url));
			}
		}
	}

	uni_char *purl = gadget_path.CStr();
	for (int i = 0; i < gadget_path.Length(); i++)
	{
		if(purl[i] == '/' || purl[i] == '\\')
			purl[i] = PATHSEPCHAR;
	}

	return OpStatus::OK;
}

OP_STATUS
OpGadgetManager::Findi18nPath(OpGadgetClass* klass, const OpStringC& path, BOOL traverse, OpString& target, BOOL &found, OpString *locale)
{
	OP_ASSERT(klass);

	// Make sure that the file name does not contain any forbidden characters.
#if PATHSEPCHAR == '/'
	if (path.IsEmpty() || path.CStr()[uni_strcspn(path.CStr(), UNI_L("<>:\"|?*^`{}!\\"))])
#else
	if (path.IsEmpty() || path.CStr()[uni_strcspn(path.CStr(), UNI_L("<>:\"|?*^`{}!/"))])
#endif
	{
		found = FALSE;
		return OpStatus::ERR_FILE_NOT_FOUND;
	}

	// If path starts with a separator then do not add another one
	// or file will be not found
	bool starts_with_path_separator = path[0] == PATHSEPCHAR;

	target.Empty();
	const OpStringC gadget_path(klass->GetGadgetRootPath());
	if (traverse)
	{
		BOOL default_locale_set = FALSE;
		const uni_char *default_locale = klass->GetAttribute(WIDGET_DEFAULT_LOCALE, &default_locale_set);
		OpString locale_top;
		RETURN_IF_ERROR(locale_top.SetConcat(gadget_path, UNI_L(PATHSEP),
			UNI_L("locales"), UNI_L(PATHSEP)));
		OpString locale_path;
		OpGadgetManager::GadgetLocaleIterator locale_iterator = GetBrowserLocaleIterator();
		const uni_char* locale_str;
		while((locale_str = locale_iterator.GetNextLocale()) || (locale_str = default_locale))
		{
			RETURN_IF_ERROR(locale_path.SetConcat(locale_top, locale_str, starts_with_path_separator ? UNI_L("") : UNI_L(PATHSEP), path));

			OpFile file;
			RETURN_IF_ERROR(file.Construct(locale_path.CStr(), OPFILE_ABSOLUTE_FOLDER, OPFILE_FLAGS_ASSUME_ZIP | OPFILE_FLAGS_CASE_SENSITIVE_ZIP));
			RETURN_IF_ERROR(file.Exists(found));

			if (found)
			{
				RETURN_IF_ERROR(target.TakeOver(locale_path));
				if (locale)
					RETURN_IF_ERROR(locale->Set(locale_str));
				break;
			}
			if (locale_str == default_locale)
				default_locale = 0;
		}

	}

	if (target.IsEmpty())
	{
		// Assume no locale is an English locale
		if (locale)
			RETURN_IF_ERROR(locale->Set("en"));

		// Look for the file at the root of the widget
		RETURN_IF_ERROR(target.SetConcat(gadget_path, starts_with_path_separator ? UNI_L("") : UNI_L(PATHSEP), path));

		// See if it is there
		OpFile file;
		RETURN_IF_ERROR(file.Construct(target.CStr(), OPFILE_ABSOLUTE_FOLDER, OPFILE_FLAGS_ASSUME_ZIP | OPFILE_FLAGS_CASE_SENSITIVE_ZIP));
		RETURN_IF_ERROR(file.Exists(found));
	}

	return OpStatus::OK;
}

OP_STATUS
OpGadgetManager::FindLegacyRootPath(const OpStringC &gadget_path, OpString &gadget_root_path)
{
	// Try locating the root path inside the legacy widget by looking
	// for the config.xml in a sub-directory of the widget archive.
	// Only a first-level sub-directory is allowed.

	OpFile gadget_archive;
	BOOL found;
	OpFileInfo::Mode mode;
	RETURN_IF_ERROR(gadget_archive.Construct(gadget_path.CStr(), OPFILE_ABSOLUTE_FOLDER, OPFILE_FLAGS_ASSUME_ZIP | OPFILE_FLAGS_CASE_SENSITIVE_ZIP));
	RETURN_IF_ERROR(gadget_archive.Exists(found));
	if (!found)
		return OpStatus::ERR_FILE_NOT_FOUND;
	RETURN_IF_ERROR(gadget_archive.GetMode(mode));

	OpFolderLister *lister = NULL;
	if (mode == OpFileInfo::DIRECTORY)
	{
		RETURN_IF_ERROR(OpFolderLister::Create(&lister));
	}
	else
	{
		RETURN_IF_ERROR(OpZipFolderLister::Create(&lister));
	}
	OpAutoPtr<OpFolderLister> lister_anchor(lister);
	if (OpStatus::IsError(lister->Construct(gadget_path.CStr(), UNI_L("*"))))
		return OpStatus::OK;

	UINT32 found_paths = 0;
	OpString tmp_path;
	while (lister->Next())
	{
		// Try all first-level folders, except "." and ".."
		const uni_char *name = lister->GetFileName();
		if (uni_str_eq(name, ".") || uni_str_eq(name, ".."))
			continue;

		const uni_char* fullpath = lister->GetFullPath();
		size_t fullpath_len = uni_strlen(fullpath);
		RETURN_IF_ERROR(tmp_path.SetConcat(fullpath, UNI_L(PATHSEP), GADGET_CONFIGURATION_FILE));

		OpFile file;
		BOOL found = FALSE;
		RETURN_IF_ERROR(file.Construct(tmp_path.CStr(), OPFILE_ABSOLUTE_FOLDER, OPFILE_FLAGS_ASSUME_ZIP | OPFILE_FLAGS_CASE_SENSITIVE_ZIP));
		RETURN_IF_ERROR(file.Exists(found));
		if (found)
		{
			OpFileInfo::Mode file_mode = OpFileInfo::FILE;
			RETURN_IF_ERROR(file.GetMode(file_mode));
			if (OpFileInfo::FILE == file_mode)
			{
				found_paths++;
				if (found_paths > 1)
					return OpStatus::ERR;
				gadget_root_path.Set(fullpath, fullpath_len);
			}
		}
	}

	return OpStatus::OK;
}


int
OpGadgetManager::GetLanguagePriority(const uni_char *xmllang, const uni_char* default_locale)
{
	// Sanity check
	if (!xmllang || !*xmllang)
		return 0;

	// Iterate over the language list, trying to find a match.
	int langnum = 1;
	GadgetLocaleIterator iterator = GetBrowserLocaleIterator();
	const uni_char* locale_str;
	while ((locale_str = iterator.GetNextLocale()) != NULL)
	{
		if (uni_stri_eq(locale_str, xmllang))
			return langnum;
		++langnum;
	}

	// try the default_locale
	if (default_locale && uni_stri_eq(default_locale, xmllang))
		return langnum;

	// No match found
	return 0;
}

/***********************************************************************************
**
**	SetupGlobalGadgetsSecurity
**
**  Parse widgets.xml and set up
**    - per-widget security overrides
**    - the global blacklist and global security overrides
**
**  The file widgets.xml is located in the user prefs folder (with opera.ini);
**  at this time there is only the user one, no "fixed" or "global" settings
**  are applied.
**
**  If any errors at all (including OOM errors) except file-not-found are
**  encountered during the opening or parsing of the ini file, then a call is
**  made to the security manager to turn off all privileges for all widgets.
**  This is somewhat draconian, but always safe.
**
**  If widgets.xml is not found, then the default security model is used.
**
***********************************************************************************/

OP_STATUS
OpGadgetManager::SetupGlobalGadgetsSecurity()
{
	BOOL security_failure = FALSE;
	RETURN_IF_ERROR(ParseGlobalGadgetsSecurity("widgets.xml", FALSE, security_failure));
	if (security_failure)
	{
		// security_failure
		GADGETS_REPORT_ERROR_TO_CONSOLE((NULL, UNI_L("widgets.xml could not be parsed, restricted security settings will apply.")));
		OpSecurityManager::SetRestrictedGadgetPolicy();
		return OpStatus::OK;
	}

	return OpStatus::OK;
}

OP_STATUS
OpGadgetManager::ParseGlobalGadgetsSecurity(const char *ini_name, BOOL signed_security, BOOL &security_failure)
{
	OpString widgets_ini;
	OP_STATUS err;
	BOOL exists = FALSE;
	OpString8 l;
	XMLFragment f;
	OpFile inf;

	security_failure = FALSE;
	if (OpStatus::IsSuccess(err = g_folder_manager->GetFolderPath(OPFILE_USERPREFS_FOLDER, widgets_ini)) &&
		OpStatus::IsSuccess(err = widgets_ini.Append(ini_name)) &&
		OpStatus::IsSuccess(err = inf.Construct(widgets_ini.CStr())) &&
		OpStatus::IsSuccess(err = inf.Exists(exists)))
	{
		if (!exists)
			return OpStatus::OK;

		if (OpStatus::IsSuccess(err = inf.Open(OPFILE_READ)))
		{
			BOOL has_widgets_elm = FALSE;
			BOOL has_security_elm = FALSE;

			if (OpStatus::IsSuccess(err = f.Parse(static_cast<OpFileDescriptor*>(&inf))))
			{
				// The following code always leaves the fragment pointing after the "widgets"
				// element, if there was one, or at the current element otherwise.


				if (f.EnterElement(UNI_L("widgets")))
				{
					has_widgets_elm = TRUE;

					while (OpStatus::IsSuccess(err) && f.HasMoreElements())
					{
						if (f.EnterElement(UNI_L("widget")))
						{
							err = OpStatus::ERR;	// obsolete element
							f.EnterAnyElement();	// skip
							f.LeaveElement();		//   this element
						}
						else if (!has_security_elm)
						{
							// These are security settings for all widgets (global blacklists and
							// global security overrides)

							if (f.EnterElement(UNI_L("security")))
							{
								if (OpStatus::IsSuccess(err = OpSecurityManager::SetGlobalGadgetPolicy(&f)))	// Does both EnterElement() and LeaveElement()
									has_security_elm = TRUE;
								f.LeaveElement();		//   this element
							}
							else
							{
								f.EnterAnyElement();	// skip
								f.LeaveElement();		//   this element
							}
						}
						else
						{
							f.EnterAnyElement();	// skip
							f.LeaveElement();		//   this element
						}
					}
					f.LeaveElement();	// widgets
				}
				else
				{
					f.EnterAnyElement();	// skip
					f.LeaveElement();		//   this element
				}
			}

			OpStatus::Ignore(inf.Close());

			if (OpStatus::IsSuccess(err) && has_widgets_elm && has_security_elm)
				return OpStatus::OK;
		}
	}

	// security_failure
	security_failure = TRUE;

	GADGETS_REPORT_ERROR_TO_CONSOLE((NULL, UNI_L("widgets.xml could not be parsed, restricted security settings will apply.\n")));
	OpSecurityManager::SetRestrictedGadgetPolicy();

	// We're in error, but running with restricted security.
	return OpStatus::OK;
}

OP_GADGET_STATUS
OpGadgetManager::Update(OpGadgetClass *klass, const uni_char* additional_params)
{
	const uni_char* update_url_str = klass->GetGadgetUpdateUrl();

	if (klass->IsUpdating())
		return OpStatus::OK;

	if (!update_url_str || !*update_url_str)
	{
		GADGETS_REPORT_ERROR_TO_CONSOLE((klass->GetGadgetPath(), UNI_L("Widget update: no update URL")));
		return OpStatus::ERR;
	}

	OpGadgetUpdateObject *update_obj;
	OpGadgetClassProxy proxy(klass);
	RETURN_IF_ERROR(OpGadgetUpdateObject::Make(
						update_obj,
						g_main_message_handler,
						proxy,
						additional_params));

	update_obj->Into(&m_update_objects);

	return OpStatus::OK;
}

OP_GADGET_STATUS
OpGadgetManager::Update(const uni_char* update_description_document_url,
						const void* user_data,
						const uni_char* additional_params)
{

	if (!update_description_document_url || !*update_description_document_url)
	{
		GADGETS_REPORT_ERROR_TO_CONSOLE((NULL, UNI_L("Widget update: no update description document URL")));
		return OpStatus::ERR;
	}

	OpGadgetUpdateObject *update_obj;
	OpGadgetClassProxy proxy(update_description_document_url, user_data);
	RETURN_IF_ERROR(OpGadgetUpdateObject::Make(
						update_obj,
						g_main_message_handler,
						proxy,
						additional_params));

	update_obj->Into(&m_update_objects);

	return OpStatus::OK;
}

void
OpGadgetManager::CancelUpdate(OpGadgetClass* klass)
{
	OpGadgetUpdateObject* updateObject =  static_cast<OpGadgetUpdateObject*>(m_update_objects.First());
	while (updateObject)
	{
		OpGadgetUpdateObject* tmp = updateObject;
		updateObject = static_cast<OpGadgetUpdateObject*>(updateObject->Suc());

		if (tmp->GetClass() == klass)
			OP_DELETE(tmp);
	}
}

OP_STATUS
OpGadgetManager::Disable(OpGadgetClass *klass)
{
	// TODO
	/* Dear future implementer: klass may be NULL, in which case there's no
	  point in trying to disable it. It means it's not an update but a
	  fresh installation.
	 */
	return OpStatus::OK;
}

OP_STATUS
OpGadgetManager::AddAllowedWidgetExtension(const uni_char *ext)
{
	if (!ext || !*ext)
		return OpStatus::OK;

	OpString *s = OP_NEW(OpString, ());
	if (!s)
		return OpStatus::ERR_NO_MEMORY;
	OpAutoPtr<OpString> s_ptr(s);

	RETURN_IF_ERROR(s->Set(ext));
	RETURN_IF_ERROR(m_gadget_extensions.Add(s));
	s_ptr.release();

	return OpStatus::OK;
}

BOOL
OpGadgetManager::IsAllowedExtension(const uni_char *filename)
{
	if (!filename)
		return FALSE;

	UINT32 i;
	for (i = 0; i < m_gadget_extensions.GetCount(); i++)
	{
		const uni_char *ext = m_gadget_extensions.Get(i)->CStr();
		const uni_char *start = filename + uni_strlen(filename) - uni_strlen(ext);

		if (uni_stricmp(start, ext) == 0)
			return TRUE;
	}

	return FALSE;
}

OP_GADGET_STATUS
OpGadgetManager::DownloadAndInstall(const uni_char *url, const uni_char *caption, void* userdata)
{
	if (!url || !*url)
		return OpStatus::ERR;

	// Ask OS if it's OK to download this widget.
	// Notify platform about available update
	OpGadgetListener *listener = g_windowCommanderManager->GetGadgetListener();
	if (listener)
	{
		OpGadgetDownloadCallbackToken *token;
		RETURN_IF_ERROR(OpGadgetDownloadCallbackToken::Make(token, url, caption, userdata));
		token->Into(&m_tokens);

		OpGadgetListener::OpGadgetDownloadData data;
		data.caption = caption;
		data.url = url;
		data.path = NULL;
		data.identifier = NULL;
		listener->OnGadgetDownloadPermissionNeeded(data, token);
	}

	return OpStatus::OK;
}

OP_GADGET_STATUS
OpGadgetManager::DownloadAndInstall_stage2(BOOL allow, const uni_char *url, const uni_char *caption, void *userdata)
{
	BOOL confirmed = allow;

	// Download confirmed/denied, notify BMW
	OpGadgetInstallListener::GadgetInstallEvent event;
	event.event = confirmed ? OpGadgetInstallListener::GADGET_INSTALL_USER_CONFIRMED : OpGadgetInstallListener::GADGET_INSTALL_USER_CANCELLED;
	event.userdata = userdata;
	event.download_object = NULL;
	NotifyGadgetInstallEvent(event);

	if (confirmed)
	{
		OpGadgetDownloadObject *download_obj;
		RETURN_IF_ERROR(OpGadgetDownloadObject::Make(download_obj, g_main_message_handler, url, caption, userdata));
		download_obj->Into(&m_download_objects);
	}
	else
	{
		event.event = OpGadgetInstallListener::GADGET_INSTALL_PROCESS_FINISHED;
		NotifyGadgetInstallEvent(event);
	}

	return OpStatus::OK;
}

OP_STATUS
OpGadgetManager::DownloadFailed(OpGadgetDownloadObject *obj)
{
	// Download failed, notify BMW
	OpGadgetInstallListener::GadgetInstallEvent event;
	event.event = OpGadgetInstallListener::GADGET_INSTALL_DOWNLOAD_FAILED;
	event.userdata = obj->UserData();
	event.download_object = obj;
	NotifyGadgetInstallEvent(event);

	event.event = OpGadgetInstallListener::GADGET_INSTALL_PROCESS_FINISHED;
	NotifyGadgetInstallEvent(event);

	return OpStatus::OK;
}

OP_STATUS
OpGadgetManager::DownloadSuccessful(OpGadgetDownloadObject *obj)
{
	// Download successful, notify BMW
	OpGadgetInstallListener::GadgetInstallEvent event;
	event.event = OpGadgetInstallListener::GADGET_INSTALL_DOWNLOADED;
	event.userdata = obj->UserData();
	event.download_object = obj;
	NotifyGadgetInstallEvent(event);

	OP_STATUS res = Install(obj);
	if (OpStatus::IsError(res))
	{
		if (res == OpStatus::ERR_NO_DISK)
			event.event = OpGadgetInstallListener::GADGET_INSTALL_NOT_ENOUGH_SPACE;
		else
			event.event = OpGadgetInstallListener::GADGET_INSTALL_INSTALLATION_FAILED;
	}
	else
	{
		event.event = OpGadgetInstallListener::GADGET_INSTALL_PROCESS_FINISHED;
	}

	NotifyGadgetInstallEvent(event);

	return res;
}

OP_GADGET_STATUS
OpGadgetManager::Install(OpGadgetDownloadObject *obj)
{
	URL url = obj->DownloadUrl();
	OpString filename;
	TRAP_AND_RETURN(err, url.GetAttributeL(URL::KSuggestedFileName_L, filename));

	OpString widget_path;
	RETURN_IF_ERROR(g_folder_manager->GetFolderPath(OPFILE_GADGET_FOLDER, widget_path));
	RETURN_IF_ERROR(widget_path.Append(filename));

	RETURN_IF_ERROR(CreateUniqueFilename(widget_path));

	if (url.SaveAsFile(widget_path) != 0)
		return OpStatus::ERR;

	// Notify OS (and other listeners) that the new widget has been downloaded and is ready to be instantiated.
	OpGadgetListener::OpGadgetDownloadData data;
	data.caption = obj->Caption();
	data.url = obj->DownloadUrlStr();;
	data.path = widget_path.CStr();
	data.identifier = NULL;
	NotifyGadgetDownloaded(data);

	// Create gadget
	OpGadget *gadget;
	RETURN_IF_ERROR(CreateGadget(&gadget, widget_path, static_cast<URLContentType>(url.GetAttribute(URL::KContentType))));

	data.identifier = gadget->GetIdentifier();
	NotifyRequestRunGadget(data);

	// Installation successful, notify BMW
	OpGadgetInstallListener::GadgetInstallEvent event;
	event.event = OpGadgetInstallListener::GADGET_INSTALL_INSTALLED;
	event.userdata = obj->UserData();
	event.download_object = NULL;
	NotifyGadgetInstallEvent(event);

	return OpStatus::OK;
}

#ifdef GADGETS_INSTALLER_SUPPORT
OP_STATUS
OpGadgetManager::Install(OpGadgetInstallObject *obj)
{
	BOOL file_exists = TRUE;
	OpFile file;
	RETURN_IF_ERROR(file.Construct(obj->GetFullPath()));
	RETURN_IF_ERROR(file.Exists(file_exists));
	if (!file_exists)
		return OpStatus::ERR_NO_SUCH_RESOURCE;

	// Create gadget
	URLContentType content_type;
	RETURN_IF_ERROR(obj->GetUrlType(content_type));
	OpGadget* gadget;
	RETURN_IF_ERROR(CreateGadget(&gadget, file.GetFullPath(), content_type));

	obj->SetIsInstalled(TRUE);
	obj->SetGadgetClass(gadget->GetClass());

	// Installation successful, notify BMW
	OpGadgetInstallListener::GadgetInstallEvent event;
	event.event = OpGadgetInstallListener::GADGET_INSTALL_INSTALLED;
	event.userdata = NULL;
	event.download_object = NULL;
	NotifyGadgetInstallEvent(event);
	return OpStatus::OK;
}

OP_STATUS
OpGadgetManager::Upgrade(OpGadgetInstallObject *obj)
{
	OpGadget *gadget = obj->GetGadget();
	OP_ASSERT(gadget);

	BOOL file_exists = TRUE;
	OpFile file;
	RETURN_IF_ERROR(file.Construct(obj->GetFullPath()));
	RETURN_IF_ERROR(file.Exists(file_exists));
	if (!file_exists)
		return OpStatus::ERR_NO_SUCH_RESOURCE;

	// Upgrade gadget
	OpGadgetClass *old_klass = gadget->GetClass();
	URLContentType content_type;
	RETURN_IF_ERROR(obj->GetUrlType(content_type));
	RETURN_IF_ERROR(UpgradeGadget(gadget, file.GetFullPath(), content_type));

	obj->SetIsInstalled(TRUE);
	obj->SetGadgetClass(gadget->GetClass());

	// Remove old gadget class
	OpString old_path;
	RETURN_IF_ERROR(old_klass->GetGadgetPath(old_path));
	RETURN_IF_ERROR(RemoveGadget(old_path));

	return OpStatus::OK;
}
#endif // GADGETS_INSTALLER_SUPPORT

#ifdef EXTENSION_SUPPORT
void OpGadgetManager::DeleteUserJavaScripts(OpGadget *owner)
{
	// Remove all scripts with this owner
	OpGadgetExtensionUserJS *extension_userjs = GetFirstActiveExtensionUserJS();
	while (extension_userjs)
	{
		OpGadgetExtensionUserJS *next = extension_userjs->Suc();

		if (extension_userjs->owner == owner)
		{
			m_extension_userjs_updated = TRUE;
			extension_userjs->Out();
			OP_DELETE(extension_userjs);
		}

		extension_userjs = next;
	}
}

OP_STATUS OpGadgetManager::OnExtensionStorageDataClear(URL_CONTEXT_ID context_id, OpGadget::InitPrefsReason reason)
{
	if (OpGadget *gadget = GetGadgetByContextId(context_id))
		return gadget->InitializePreferences(reason);
	return OpStatus::OK;
}
#endif

#endif // GADGET_SUPPORT
