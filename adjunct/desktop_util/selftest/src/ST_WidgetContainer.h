/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * spoon / Arjan van Leeuwen (arjanl)
 */

#ifndef ST_WIDGET_CONTAINER_H
#define ST_WIDGET_CONTAINER_H

#ifdef SELFTEST

#include "modules/widgets/WidgetContainer.h"

/** @brief A WidgetContainer that can be used in isolation
  */
class ST_WidgetContainer
{
public:
	ST_WidgetContainer() : m_container(0), m_op_window(0) {}
	~ST_WidgetContainer() { OP_DELETE(m_container); OP_DELETE(m_op_window); }

	OP_STATUS Init(const OpRect& rect)
	{
		RETURN_IF_ERROR(OpWindow::Create(&m_op_window));
		RETURN_IF_ERROR(m_op_window->Init());
		m_container = OP_NEW(WidgetContainer, ());
		return m_container->Init(rect, m_op_window);
	}

	void AddWidget(OpWidget* widget) { m_container->GetRoot()->AddChild(widget); }

private:
	WidgetContainer* m_container;
	OpWindow* m_op_window;
};

#endif // SELFTEST

#endif // ST_WIDGET_CONTAINER_H
