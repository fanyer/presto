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

#include "adjunct/quick_toolkit/creators/QuickGridRowCreator.h"

#include "adjunct/desktop_util/adt/typedobjectcollection.h"
#include "adjunct/quick_toolkit/creators/QuickWidgetCreator.h"
#include "adjunct/quick_toolkit/widgets/QuickGrid/QuickGrid.h"
#include "adjunct/ui_parser/ParserIterators.h"

OP_STATUS QuickGridRowCreator::Instantiate(QuickGrid& grid, TypedObjectCollection& collection)
{
	RETURN_IF_ERROR(grid.AddRow());

	for (ParserSequenceIterator it(m_sequence); it; ++it)
	{
		ParserNodeMapping child_node;
		m_sequence.GetChildNodeByID(it.Get(), child_node);

		QuickWidgetCreator widget_creator(collection, m_log, m_has_quick_window);
		RETURN_IF_ERROR(widget_creator.Init(&child_node));

		OpAutoPtr<QuickWidget> widget;
		RETURN_IF_ERROR(widget_creator.CreateWidget(widget));
		RETURN_IF_ERROR(grid.InsertWidget(widget.release(), 1));
	}

	return OpStatus::OK;
}
