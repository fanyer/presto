/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#if defined(GADGET_SUPPORT) && defined(SELFTEST)
#include "modules/gadgets/gadget_selftest_utils.h"

#include "modules/gadgets/OpGadget.h"
#include "modules/gadgets/OpGadgetManager.h"
#include "modules/doc/frm_doc.h"
#include "modules/dochand/win.h"
#include "modules/url/url_api.h"
#include "modules/url/url2.h"
#include "modules/windowcommander/OpWindowCommander.h"
#include "modules/windowcommander/src/WindowCommander.h"
#include "modules/selftest/testutils.h"


class SelftestOpenGadgetListener : public OpGadgetListener
{
public:
	SelftestOpenGadgetListener(OpListenable<OpGadgetListener>* listenable, OpGadget* gadget, GadgetSelftestUtils::GadgetOpenCallback* callback, Window* window = NULL)
		:m_gadget(gadget), m_window(window), m_callback(callback), m_listenable(listenable)
	{
	}

	virtual void OnGadgetSignatureVerified(const OpGadgetSignatureVerifiedData& data)
	{
		if (data.gadget_class == m_gadget->GetClass())
		{
			OP_GADGET_STATUS open_status;

			if (m_window)
				open_status = GadgetSelftestUtils::OpenGadgetInWindow(m_gadget, m_window);
			else
				open_status = g_gadget_manager->OpenGadget(m_gadget);

			m_callback->OnGadgetOpen(open_status);
			OP_DELETE(this);
		}
	}

	~SelftestOpenGadgetListener() {
		OpStatus::Ignore(m_listenable->DetachListener(this));
	}

	virtual void OnGadgetUpdateReady(const OpGadgetUpdateData& data) {}
	virtual void OnGadgetUpdateError(const OpGadgetErrorData& data) {}
	virtual void OnGadgetDownloadPermissionNeeded(const OpGadgetDownloadData& data, GadgetDownloadPermissionCallback *callback) {}
	virtual void OnGadgetDownloaded(const OpGadgetDownloadData& data) {}
	virtual void OnGadgetInstalled(const OpGadgetInstallRemoveData& data) {}
	virtual void OnGadgetRemoved(const OpGadgetInstallRemoveData& data) {}
	virtual void OnGadgetUpgraded(const OpGadgetStartStopUpgradeData& data) {}
	virtual void OnRequestRunGadget(const OpGadgetDownloadData& data) {}
	virtual void OnGadgetStarted(const OpGadgetStartStopUpgradeData& data) {}
	virtual void OnGadgetStartFailed(const OpGadgetStartFailedData& data) {}
	virtual void OnGadgetStopped(const OpGadgetStartStopUpgradeData& data) {}
	virtual void OnGadgetInstanceCreated(const OpGadgetInstanceData& data) {}
	virtual void OnGadgetInstanceRemoved(const OpGadgetInstanceData& data) {}
	virtual void OnGadgetDownloadError(const OpGadgetErrorData& data) {}

private:
	OpGadget* m_gadget;
	Window* m_window;
	GadgetSelftestUtils::GadgetOpenCallback* m_callback;
	OpListenable<OpGadgetListener>* m_listenable;
};

OP_GADGET_STATUS GadgetSelftestUtils::LoadGadgetForSelftest(const char* path, URLContentType type, OpGadget** new_gadget, GadgetSelftestUtils::GadgetOpenCallback* callback)
{
	OpString widget_path;
	RETURN_IF_ERROR(widget_path.SetFromUTF8(path));
	return LoadGadgetForSelftest(widget_path.CStr(), type, new_gadget, callback);
}

OP_GADGET_STATUS GadgetSelftestUtils::LoadGadgetForSelftest(const uni_char* path, URLContentType type, OpGadget** new_gadget, GadgetSelftestUtils::GadgetOpenCallback* callback)
{
	if (!g_gadget_manager->IsThisAGadgetPath(path))
		return OpStatus::ERR_NOT_SUPPORTED;

	if (!callback)
		callback = &m_default_open_callback;

	RETURN_IF_ERROR(g_gadget_manager->CreateGadget(&m_gadget, path, type));
	OP_ASSERT(m_gadget);

	if (new_gadget)
		*new_gadget = m_gadget;

	Window* window = g_selftest.utils->GetWindow();

	m_close_window_on_unload = FALSE;

	if (m_gadget->SignatureState() == GADGET_SIGNATURE_PENDING)
	{
		OpAutoPtr<SelftestOpenGadgetListener> ap_listener(OP_NEW(SelftestOpenGadgetListener, (g_gadget_manager, m_gadget, callback, window)));
		RETURN_OOM_IF_NULL(ap_listener.get());
		RETURN_IF_ERROR(g_gadget_manager->AttachListener(ap_listener.get()));
		ap_listener.release();	// The listener deletes itself
		return OpGadgetStatus::OK_SIGNATURE_VERIFICATION_IN_PROGRESS;
	}
	else if (m_gadget->SignatureState() != GADGET_SIGNATURE_UNKNOWN)
		return GadgetSelftestUtils::OpenGadgetInWindow(m_gadget, window);
	else
	{
		OP_ASSERT(!"Gadget's signature state should never be GADGET_SIGNATURE_UNKNOWN here");
		return OpStatus::ERR;
	}
}

OP_GADGET_STATUS GadgetSelftestUtils::LoadAndOpenGadget(const char* path, URLContentType type, OpGadget** new_gadget, GadgetSelftestUtils::GadgetOpenCallback* callback)
{
	OpString widget_path;
	RETURN_IF_ERROR(widget_path.SetFromUTF8(path));
	return LoadAndOpenGadget(widget_path.CStr(), type, new_gadget, callback);
}

OP_GADGET_STATUS GadgetSelftestUtils::LoadAndOpenGadget(const uni_char* path, URLContentType type, OpGadget** new_gadget, GadgetSelftestUtils::GadgetOpenCallback* callback)
{
	if (!g_gadget_manager->IsThisAGadgetPath(path))
		return OpStatus::ERR_FILE_NOT_FOUND;

	if (!callback)
		callback = &m_default_open_callback;

	OpGadgetClass* dummy_class;
	// Note: Creating the gadget class explicitly is required to have the gogi
	// layer notice it and not crash when the widget is started.
	// Unfurtunately gogi implicitly creates a widget so with the CreateGadget
	// below the widget will be loaded twice and unloaded only once by UnloadGadget.
	RETURN_IF_ERROR(g_gadget_manager->CreateGadgetClass(&dummy_class, path, type, NULL));
	RETURN_IF_ERROR(g_gadget_manager->CreateGadget(&m_gadget, path, type));
	OP_ASSERT(m_gadget);

	if (new_gadget)
		*new_gadget = m_gadget;

	m_close_window_on_unload = TRUE;

	if (m_gadget->SignatureState() == GADGET_SIGNATURE_PENDING)
	{
		OpAutoPtr<SelftestOpenGadgetListener> ap_listener(OP_NEW(SelftestOpenGadgetListener, (g_gadget_manager, m_gadget, callback)));
		RETURN_OOM_IF_NULL(ap_listener.get());
		RETURN_IF_ERROR(g_gadget_manager->AttachListener(ap_listener.get()));
		ap_listener.release();	// The listener deletes itself
		return OpGadgetStatus::OK_SIGNATURE_VERIFICATION_IN_PROGRESS;
	}
	else if (m_gadget->SignatureState() != GADGET_SIGNATURE_UNKNOWN)
		return g_gadget_manager->OpenGadget(m_gadget);
	else
	{
		OP_ASSERT(!"Gadget's signature state should never be GADGET_SIGNATURE_UNKNOWN here");
		return OpStatus::ERR;
	}
}

void GadgetSelftestUtils::UnloadGadget()
{
	if (m_gadget)
	{
		Window* win = m_gadget->GetWindow();

		if (!win)
			return;

		if (m_close_window_on_unload)
		{
			g_gadget_manager->CloseWindowPlease(win);
			win->Close();
		}
		else
		{
			// don't know if these are really needed...
			win->SetType(WIN_TYPE_NORMAL);
			win->SetGadget(NULL);
			win->GetWindowCommander()->OpenURL(UNI_L("opera:blank"), FALSE);
			m_gadget->SetIsClosing(FALSE);
		}

		g_gadget_manager->DestroyGadget(m_gadget);

		m_gadget = NULL;
	}
}

/* static */
OP_STATUS GadgetSelftestUtils::OpenGadgetInWindow(OpGadget* gadget, Window* window)
{
	OP_ASSERT(gadget);
	OP_ASSERT(window);

	window->SetType(WIN_TYPE_GADGET);
	window->SetGadget(gadget);
	window->SetShowScrollbars(FALSE);
	gadget->SetWindow(window);

	OpString url;
	RETURN_IF_ERROR(gadget->GetGadgetUrl(url));
	OpWindowCommander* wc = (OpWindowCommander *) g_selftest.utils->GetWindow()->GetWindowCommander();

	OpWindowCommander::OpenURLOptions options;
	options.entered_by_user = TRUE;
	if (!wc->OpenURL(url.CStr(), options))
		return OpStatus::ERR;

	return OpStatus::OK;
}

void GadgetSelftestUtils::SelftestGadgetOpenCallback::OnGadgetOpen(OP_GADGET_STATUS status)
{
	if (OpStatus::IsSuccess(status))
		ST_passed();
	else
		ST_failed("Opening gadget failed, status: %d", status);
}

#endif //defined(GADGET_SUPPORT) && defined(SELFTEST)
