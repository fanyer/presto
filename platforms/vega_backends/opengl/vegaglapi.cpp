/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch_system_includes.h"

#ifdef VEGA_BACKEND_OPENGL

#include "modules/debug/debug.h"

#include "vegaglapi.h"
#include "vegagldevice.h"
#ifdef VEGA_BACKENDS_OPENGL_REGISTRY
# include "vegaglapi_c.inc"
#endif // VEGA_BACKENDS_OPENGL_REGISTRY

OP_STATUS CheckGLError()
{
	GLenum ret;
	bool oom = false, error = false;
	while ((ret = glGetError()) != GL_NO_ERROR)
	{
		error = true;
		OP_ASSERT(ret == GL_OUT_OF_MEMORY);
		if (ret == GL_OUT_OF_MEMORY)
		{
			oom = true;
		}
	}

	return error ? (oom ? OpStatus::ERR_NO_MEMORY : OpStatus::ERR) : OpStatus::OK;
}

#endif // VEGA_BACKEND_OPENGL
