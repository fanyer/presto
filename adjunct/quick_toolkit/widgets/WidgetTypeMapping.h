/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#ifndef WIDGET_TYPE_MAPPING_H
#define WIDGET_TYPE_MAPPING_H

#include "modules/util/OpTypedObject.h"

namespace WidgetTypeMapping
{
	/** Maps a string to a widget type
	  * @param string String to map to widget type
	  * @param length If given, consider only first length characters of string
	  * @return Type that matches string, or OpTypedObject::UNKNOWN_TYPE
	  *
	  * @example OpTypedObject::Type type = WidgetTypeMapping::FromString("Button");
	  */
	OpTypedObject::Type FromString(const OpStringC8& string);
	OpTypedObject::Type FromString(const OpStringC8& string, unsigned length);
};

#endif // WIDGET_TYPE_MAPPING_H
