/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#ifdef WIDGET_RUNTIME_SUPPORT

#include "adjunct/desktop_util/string/stringutils.h"
#include "adjunct/quick_toolkit/widgets/OpToolTip.h"
#include "adjunct/quick_toolkit/widgets/OpToolTipListener.h"
#include "adjunct/widgetruntime/GadgetTooltipHandler.h"
#include "modules/locale/locale-enum.h"
#include "modules/locale/oplanguagemanager.h"

class GadgetTooltipHandler::TooltipListener : public OpToolTipListener
{
public:
	virtual BOOL HasToolTipText(OpToolTip* tooltip)
	{
		return m_link.HasContent() || m_title.HasContent();
	}

	virtual void GetToolTipText(OpToolTip* tooltip, OpInfoText& text)
	{
		OpString title_descr;
		g_languageManager->GetString(Str::SI_IDSTR_HL_TREE_TITLE, title_descr);
		text.AddTooltipText(title_descr.CStr(), m_title.CStr());

		OpString link_descr;
		g_languageManager->GetString(Str::SI_LOCATION_TEXT, link_descr);
		text.AddTooltipText(link_descr.CStr(), m_link.CStr());
	}

	OP_STATUS SetToolTipText(const OpStringC& link, const OpStringC& title)
	{
		RETURN_IF_ERROR(m_link.Set(link));
		RETURN_IF_ERROR(m_title.Set(title));
		return OpStatus::OK;
	}

private:
	OpString m_link;
	OpString m_title;
};


GadgetTooltipHandler::GadgetTooltipHandler()
	: m_tooltip(NULL), m_listener(NULL)
{
}

GadgetTooltipHandler::~GadgetTooltipHandler()
{
	CleanUp();
}

OP_STATUS GadgetTooltipHandler::Init()
{
	CleanUp();

	m_listener = OP_NEW(TooltipListener, ());
	RETURN_OOM_IF_NULL(m_listener);

	m_tooltip = OP_NEW(OpToolTip, ());
	RETURN_OOM_IF_NULL(m_tooltip);
	RETURN_IF_ERROR(m_tooltip->Init());

	return OpStatus::OK;
}

void GadgetTooltipHandler::CleanUp()
{
	if (NULL != m_tooltip)
	{
		m_tooltip->SetListener(NULL);
	}

	OP_DELETE(m_listener);
	m_listener = NULL;

	OP_DELETE(m_tooltip);
	m_tooltip = NULL;
}

OP_STATUS GadgetTooltipHandler::ShowTooltip(const OpStringC& link,
		const OpStringC& title)
{
	// Ignore widget-internal links.
	if (StringUtils::StartsWith(link, UNI_L("widget://"), TRUE))
	{
		RETURN_IF_ERROR(m_listener->SetToolTipText(OpStringC(), title));
	}
	else
	{
		RETURN_IF_ERROR(m_listener->SetToolTipText(link, title));
	}
	// OpToolTip has no Show().  You just set the listener.
	m_tooltip->SetListener(m_listener);
	return OpStatus::OK;
}

OP_STATUS GadgetTooltipHandler::HideTooltip()
{
	m_tooltip->SetListener(NULL);
	return OpStatus::OK;
}

#ifdef SELFTEST
OpToolTipListener* GadgetTooltipHandler::GetTooltipListener() const
{
	return m_tooltip->GetListener();
}
#endif // SELFTEST

#endif // WIDGET_RUNTIME_SUPPORT
