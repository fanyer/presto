/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Tomasz Gólczyñski
 */

#ifndef QUICK_COMPOSE_EDIT_H
#define QUICK_COMPOSE_EDIT_H

#include "adjunct/quick_toolkit/widgets/QuickTextWidget.h"
#include "adjunct/quick/widgets/OpComposeEdit.h"

class QuickComposeEdit: public QuickEditableTextWidgetWrapper<OpComposeEdit>
{
	IMPLEMENT_TYPEDOBJECT(QuickEditableTextWidgetWrapper<OpComposeEdit>);
};
#endif//QUICK_COMPOSE_EDIT_H