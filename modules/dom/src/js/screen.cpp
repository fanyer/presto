/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-1999 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "modules/dom/src/js/screen.h"

#include "modules/doc/frm_doc.h"
#include "modules/display/vis_dev.h"

/* virtual */ ES_GetState
JS_Screen::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	FramesDocument *doc = GetFramesDocument();

	if (!doc)
		return GET_FAILED;

	VisualDevice *vd = doc->GetVisualDevice();

	switch (property_name)
	{
	case OP_ATOM_height:
		DOMSetNumber(value, vd->GetScreenHeight());
		return GET_SUCCESS;

	case OP_ATOM_width:
		DOMSetNumber(value, vd->GetScreenWidth());
		return GET_SUCCESS;

	case OP_ATOM_availHeight:
		DOMSetNumber(value, vd->GetScreenAvailHeight());
		return GET_SUCCESS;

	case OP_ATOM_availWidth:
		DOMSetNumber(value, vd->GetScreenAvailWidth());
		return GET_SUCCESS;

	case OP_ATOM_colorDepth:
		if (value)
		{
			int colorDepth = vd->GetScreenColorDepth();

			if (colorDepth <= 0)
				DOMSetUndefined(value);
			else
				DOMSetNumber(value, colorDepth);
		}
		return GET_SUCCESS;

	case OP_ATOM_pixelDepth:
		DOMSetNumber(value, vd->GetScreenPixelDepth());
		return GET_SUCCESS;
    }

	return GET_FAILED;
}

/* virtual */ ES_PutState
JS_Screen::PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	if (GetName(property_name, NULL, origining_runtime))
		return PUT_READ_ONLY;
	else
		return PUT_FAILED;
}

