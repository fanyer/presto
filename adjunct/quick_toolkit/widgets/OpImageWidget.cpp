/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "adjunct/quick/WindowCommanderProxy.h"
#include "modules/dochand/win.h"
#include "modules/dochand/winman.h"
#include "modules/locale/oplanguagemanager.h"
//#include "modules/dochand/winman_constants.h"

#include "OpImageWidget.h"
#include "modules/url/url_man.h"

DEFINE_CONSTRUCT(OpWebImageWidget);

/***********************************************************************************
**
**	Init
**
***********************************************************************************/

OP_STATUS OpWebImageWidget::Init()
{
	RETURN_IF_ERROR(OpBrowserView::Init());

	WindowCommanderProxy::SetType(this, WIN_TYPE_INFO);
	if (GetWindowCommander())
		GetWindowCommander()->SetShowScrollbars(FALSE);

	SetTabStop(FALSE);

	GetBorderSkin()->SetImage("Web Image Browser Skin", "Browser Skin");

	return OpStatus::OK;
}

/***********************************************************************************
**
**	SetImage
**
***********************************************************************************/

void OpWebImageWidget::SetImage(const uni_char* path, BOOL scale_width, BOOL scale_height)
{
	URL url = BeginWrite();

	if (path && *path)
	{
#ifdef USE_ABOUT_FRAMEWORK
		OpWebImageWidgetHTMLView html(url, path, scale_width, scale_height);
		html.GenerateData();
#else
		OpString img_style;
		img_style.AppendFormat(UNI_L("width: %s; height: %s;"),
								scale_width ? UNI_L("100%") : UNI_L("auto"),
								scale_height ? UNI_L("100%") : UNI_L("auto"));

		OpString top;
		top.AppendFormat(UNI_L("<html><style> body {margin: 0px; padding: 0px;} img { %s } </style><body><img src=\""), img_style.CStr());
		const OpStringC bottom(UNI_L("\"></body></html>"));
		
		url.WriteDocumentData(top);
		url.WriteDocumentData(path);
		url.WriteDocumentData(bottom);
#endif
	}
	
	EndWrite(url);
}

#ifdef USE_ABOUT_FRAMEWORK
OP_STATUS OpWebImageWidget::OpWebImageWidgetHTMLView::GenerateData()
{
	OpenDocument();

	OpString img_style;
	img_style.AppendFormat(UNI_L("width: %s; height: %s;"),
						   scale_width ? UNI_L("100%") : UNI_L("auto"),
						   scale_height ? UNI_L("100%") : UNI_L("auto"));

	m_url.WriteDocumentDataUniSprintf(UNI_L("<style> body {margin: 0px; padding: 0px;} img { %s } </style>"), img_style.CStr());
	OpenBody(Str::NOT_A_STRING);

	m_url.WriteDocumentDataUniSprintf(UNI_L("<img src=\"%s\">"), path);
	return CloseDocument();
}

#endif
