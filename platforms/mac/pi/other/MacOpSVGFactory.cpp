/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef SVG_SUPPORT

#include "modules/svg/src/SVGFactory.h"
#include "modules/svg/src/SVGCanvasQuartz.h"
#include "modules/svg/src/SVGCanvasVega.h"
#include "platforms/mac/util/systemcapabilities.h"

class MacOpSVGFactory : public SVGFactory
{
	OP_STATUS CreateSVGCanvas(SVGCanvas** new_canvas)
	{
//		*new_canvas = new SVGCanvasQuartz();
		*new_canvas = new SVGCanvasVega();
		return *new_canvas ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
	}

	OP_STATUS CreateSVGFont(SVGFont** new_font, OpFont* font)
	{
#if 0
		*new_font = new MacOpSVGFont(font);
		return *new_font ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
#else
		return OpStatus::ERR;
#endif
	}
};

/** Static function to create the OpFactory with. */
SVGFactory *SVGFactory::Create()
{
	return new MacOpSVGFactory;
}

#endif // SVG_SUPPORT