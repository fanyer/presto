/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef DOM_CLONEHANDLER_H
#define DOM_CLONEHANDLER_H

#include "modules/ecmascript/ecmascript.h"

class DOM_CloneHandler
	: public ES_HostObjectCloneHandler
{
public:
	virtual OP_STATUS Clone(EcmaScript_Object *source_object, ES_Runtime *target_runtime, EcmaScript_Object *&target_object);

#ifdef ES_PERSISTENT_SUPPORT
	virtual OP_STATUS Clone(EcmaScript_Object *source_object, ES_PersistentItem *&target_item);
	virtual OP_STATUS Clone(ES_PersistentItem *&source_item, ES_Runtime *target_runtime, EcmaScript_Object *&target_object);
#endif // ES_PERSISTENT_SUPPORT
};

#endif // DOM_CLONEHANDLER_H
