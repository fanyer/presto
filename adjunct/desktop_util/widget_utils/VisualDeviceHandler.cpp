/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Arjan van Leeuwen
 */

#include "core/pch.h"

#include "adjunct/desktop_util/widget_utils/VisualDeviceHandler.h"

#include "modules/widgets/OpWidget.h"
#include "modules/display/vis_dev.h"

VisualDeviceHandler::VisualDeviceHandler(OpWidget* widget)
  : m_widget(widget)
  , m_vd(0)
{
	if (!widget->GetVisualDevice())
	{
		m_vd = OP_NEW(VisualDevice, ());
		m_widget->SetVisualDevice(m_vd);
	}
}
		
VisualDeviceHandler::~VisualDeviceHandler()
{
	if (m_vd)
	{
		m_widget->SetVisualDevice(0);
		OP_DELETE(m_vd);
	}
}
