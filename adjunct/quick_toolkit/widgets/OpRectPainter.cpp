/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#include "core/pch.h"

#include "adjunct/quick_toolkit/widgets/OpRectPainter.h"

#include "adjunct/quick_toolkit/widgets/WidgetSizes.h"
#include "modules/display/vis_dev.h"

OpRectPainter::OpRectPainter(QuickWidget* layout)
  : m_layout(layout)
  , m_show_text(TRUE)
{
	SetIgnoresMouse(TRUE);
}

void OpRectPainter::OnPaint(OpWidgetPainter* widget_painter, const OpRect &paint_rect)
{
	VisualDevice* vd = GetVisualDevice();
	if (!vd)
		return;

	vd->SetColor(255, 0, 0);

	for (unsigned i = 0; i < m_elems.GetCount(); ++i)
	{
		Element& elem = m_elems[i];
		vd->DrawRect(elem.rect);

		if (!m_show_text)
			continue;

		vd->SetFontSize(8);

		OpString width_desc;
		GetSizeDescription(elem.min_width, elem.nom_width, elem.pref_width, width_desc);

		vd->TxtOut(elem.rect.x + 10, elem.rect.y, width_desc.CStr(), width_desc.Length());

		OpString height_desc;
		GetSizeDescription(elem.min_height, elem.nom_height, elem.pref_height, height_desc);

		vd->TxtOut(elem.rect.x, elem.rect.y + 10, height_desc.CStr(), height_desc.Length());
	}
}

void OpRectPainter::GetSizeDescription(unsigned min, unsigned nom, unsigned pref, OpString& description)
{
	OpStatus::Ignore(description.AppendFormat(UNI_L("%u/%u/"), min, nom));

	switch (pref)
	{
	case WidgetSizes::Infinity:
		OpStatus::Ignore(description.Append("Inf")); break;
	case WidgetSizes::Fill:
		OpStatus::Ignore(description.Append("Fill")); break;
	default:
		OpStatus::Ignore(description.AppendFormat(UNI_L("%u"), pref));
	}
}
