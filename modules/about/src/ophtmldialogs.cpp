/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2008 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

/* HTML dialogs for core */

#include "core/pch.h"

#ifdef ABOUT_HTML_DIALOGS

#include "modules/about/ophtmldialogs.h"

#include "modules/windowcommander/src/WindowCommanderManager.h"
#include "modules/display/vis_dev.h"
#include "modules/doc/frm_doc.h"
#include "modules/dochand/win.h"
#include "modules/dochand/docman.h"
#include "modules/security_manager/include/security_manager.h"

/**
 * Implementation of the HTMLDialogData
 */

/*virtual*/
HTMLDialogData::~HTMLDialogData()
{
}

/*virtual*/
OP_STATUS HTMLDialogData::DeepCopy(HTMLDialogData *src)
{
	// Numeric values, just set and forget
	identifier = src->identifier;
	modal = src->modal;
	width = src->width;
	height = src->height;

	html_template = src->html_template;

	return OpStatus::OK;
}


/**
 * Implementation of the HTMLDialogManager
 */

HTMLDialogManager::~HTMLDialogManager()
{
	CancelDialogs();
}

void 
HTMLDialogManager::Add(HTML_Dialog *dlg)
{
    dlg->Into(&m_dialoglist);
}

void 
HTMLDialogManager::Remove(HTML_Dialog *dlg)
{
	Window *w = dlg->GetOpener();
	if (w && w->VisualDev())
		w->VisualDev()->RestoreFocus(FOCUS_REASON_OTHER);
	
	delete dlg;
}

HTML_Dialog* 
HTMLDialogManager::FindDialog(Window *win)
{
    HTML_Dialog *dlg;
    for (dlg = (HTML_Dialog*)m_dialoglist.First() ; dlg ; dlg = (HTML_Dialog*)dlg->Suc())
    {
        if (dlg->GetWindow() == win)
            return dlg;
    }
    return NULL;
}

void 
HTMLDialogManager::ActivateDialogs(Window *win)
{
	HTML_Dialog *dlg;
    for (dlg = (HTML_Dialog*)m_dialoglist.First() ; dlg ; dlg = (HTML_Dialog*)dlg->Suc())
    {
        // Opener = NULL means the dialog isn't connected and must be kept on top at all times.
		if (!dlg->GetOpener() || dlg->GetOpener() == win)
			dlg->GetWindow()->Raise();
		else
			dlg->GetWindow()->Lower();
    }
}

void 
HTMLDialogManager::CancelDialogs(Window *win/*=NULL*/)
{
	HTML_Dialog *dlg = (HTML_Dialog*)m_dialoglist.First();
	while (dlg)
	{
		if (win == NULL || dlg->GetOpener() == win)
		{
			HTML_Dialog *tmp = dlg;
			dlg = (HTML_Dialog*)dlg->Suc();

			OpStatus::Ignore(tmp->CloseDialog());
		}
		else
			dlg = (HTML_Dialog*)dlg->Suc();
	}
}

OP_STATUS
HTMLDialogManager::OpenAsyncDialog(Window *opener, AsyncHTMLDialogData *data)
{
/*
	OpString file;
	RETURN_IF_ERROR(g_folder_manager->GetFolderPath(OPFILE_RESOURCES_FOLDER, file));
	RETURN_IF_ERROR(file.Append(data->html_template));
*/
	HTML_AsyncDialog *dlg = OP_NEW(HTML_AsyncDialog, ());

	if (dlg && OpStatus::IsSuccess(dlg->Construct(data->html_template, opener, data)))
	{
		Add(dlg);
		dlg->GetWindow()->Raise();
		return OpStatus::OK;
	}

	delete dlg;
	return OpStatus::ERR;
}


/**
 * Implementation of the HTML_Dialog
 */

HTML_Dialog::~HTML_Dialog()
{
	Out();
}

OP_STATUS HTML_Dialog::Construct(URL &template_url, Window *opener, HTMLDialogData *data)
{
	RETURN_IF_ERROR(m_data.DeepCopy(data));

	OpWindowCommander *new_wc;
	RETURN_IF_MEMORY_ERROR(g_windowCommanderManager->GetWindowCommander(&new_wc));

	OpUiWindowListener *ui_listener = g_windowCommanderManager->GetUiWindowListener();
#ifdef WIC_CREATEDIALOGWINDOW
	OP_STATUS oom = ui_listener->CreateDialogWindow(new_wc, opener?opener->GetWindowCommander():NULL,
		data->width, data->height, data->modal);
#else // WIC_CREATEDIALOGWINDOW
# error "You do need OpUiWindowListener::CreateDialogWindow() from alchemilla_7_alpha_2"
#endif // WIC_CREATEDIALOGWINDOW

	if (OpStatus::IsError(oom))
	{
		ui_listener->CloseUiWindow(new_wc);
		g_windowCommanderManager->ReleaseWindowCommander(new_wc);
		return oom;
	}

	m_opener = opener;
	m_win = new_wc->GetWindow();

#ifdef HISTORY_SUPPORT
	m_win->DisableGlobalHistory();
#endif

	OpInputContext *current_root = m_win->VisualDev();
	OpInputContext *app = m_opener ? m_opener->VisualDev() : NULL;

	current_root->SetParentInputContext(app);

	// Make window hidden by default, the dialog gets activated later
	if (m_opener)
		m_win->Lower();

	m_win->SetType(WIN_TYPE_DIALOG);

	m_win->DocManager()->OpenURL(template_url, 
		opener ? opener->GetCurrentDoc()->GetURL() : URL(),
		TRUE, FALSE, FALSE, FALSE, NotEnteredByUser, FALSE,
		FALSE, FALSE, TRUE);

	return OpStatus::OK;
}

OP_STATUS HTML_Dialog::SetResult(const uni_char *result)
{
	OP_DELETE(m_result);
	m_result = UniSetNewStr(result);
	return (!m_result && result) ? OpStatus::ERR_NO_MEMORY : OpStatus::OK;
}


/**
 * Implementation of the AsyncHTMLDialogData
 */

/*virtual*/
OP_STATUS AsyncHTMLDialogData::DeepCopy(AsyncHTMLDialogData *src)
{
	// We will probably want to use the same id and callback, but I'm not really sure
	callback = src->callback;

	return HTMLDialogData::DeepCopy(src);
}


/**
 * Implementation of the HTML_AsyncDialog
 */

OP_STATUS HTML_AsyncDialog::Construct(URL &template_url, Window *opener, AsyncHTMLDialogData *data)
{
	m_callback = data->callback;

	return HTML_Dialog::Construct(template_url, opener, data);
}

/*virtual*/
OP_STATUS HTML_AsyncDialog::CloseDialog()
{
	OP_STATUS status = OpStatus::OK;
	if (m_callback)
		status = m_callback->OnClose(m_result);
	delete this;
	return status;
}

#endif // ABOUT_HTML_DIALOGS
