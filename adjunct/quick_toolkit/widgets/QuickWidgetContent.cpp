/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#include "core/pch.h"

#include "adjunct/quick_toolkit/widgets/QuickWidgetContent.h"
#include "adjunct/quick_toolkit/widgets/NullWidget.h"

NullWidget QuickWidgetContent::s_null_widget;

QuickWidgetContent::QuickWidgetContent()
  : m_contents(&s_null_widget)
{
}

void QuickWidgetContent::SetContent(QuickWidget* contents)
{
	CleanContents();

	if (contents)
		m_contents = contents;
	else
		m_contents = &s_null_widget;
}

void QuickWidgetContent::CleanContents()
{
	if (m_contents != &s_null_widget)
		OP_DELETE(m_contents);
}
