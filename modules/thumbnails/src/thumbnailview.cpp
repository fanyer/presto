/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2008-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#ifdef CORE_THUMBNAIL_SUPPORT

#include "modules/dochand/win.h"
#include "modules/thumbnails/src/thumbnailview.h"
#include "modules/widgets/WidgetContainer.h"
#include "modules/windowcommander/OpWindowCommander.h"
#include "modules/windowcommander/OpWindowCommanderManager.h"


OpThumbnailView::OpThumbnailView()
  :	m_window_commander(NULL),
	m_op_window(NULL)
{
}

OpThumbnailView::~OpThumbnailView()
{
}

/* static */ OP_STATUS
OpThumbnailView::Construct(OpThumbnailView** obj)
{
	*obj = OP_NEW(OpThumbnailView, ());
	if (!*obj)
		return OpStatus::ERR_NO_MEMORY;

	if (OpStatus::IsError((*obj)->init_status))
	{
		OP_DELETE(*obj);
		return OpStatus::ERR_NO_MEMORY;
	}
	return OpStatus::OK;
}

OP_STATUS
OpThumbnailView::Init()
{
	RETURN_IF_ERROR(g_windowCommanderManager->GetWindowCommander(&m_window_commander));
	RETURN_IF_ERROR(OpWindow::Create(&m_op_window));
	RETURN_IF_ERROR(m_op_window->Init(OpWindow::STYLE_CHILD, OpTypedObject::WINDOW_TYPE_UNKNOWN, NULL, GetWidgetContainer()->GetOpView()));
	RETURN_IF_ERROR(m_window_commander->OnUiWindowCreated(m_op_window));

#if defined _SSL_SUPPORT_ && defined _NATIVE_SSL_SUPPORT_ || defined WIC_USE_SSL_LISTENER
	m_window_commander->SetSSLListener(g_windowCommanderManager->GetSSLListener());
#endif // defined _SSL_SUPPORT_ && defined _NATIVE_SSL_SUPPORT_ || defined WIC_USE_SSL_LISTENER
#ifdef _ASK_COOKIE
	m_window_commander->SetCookieListener(g_cookie_API->GetCookieListener());
#endif // _ASK_COOKIE
	m_window_commander->SetDocumentListener(this);

#ifdef HISTORY_SUPPORT
	m_window_commander->DisableGlobalHistory();
#endif

	return OpStatus::OK;
}

Window*
OpThumbnailView::GetWindow() const
{
	return m_window_commander->ViolateCoreEncapsulationAndGetWindow();
}

void
OpThumbnailView::OnAdded()
{
	if (m_op_window)
	{
		m_op_window->SetParent(NULL, GetWidgetContainer()->GetOpView());
	}
	else
	{
		init_status = Init();
	}
}

void
OpThumbnailView::OnDeleted()
{
	m_window_commander->OnUiWindowClosing();
	g_windowCommanderManager->ReleaseWindowCommander(m_window_commander);

	OP_DELETE(m_op_window);
}

#endif // CORE_THUMBNAIL_SUPPORT
