/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2011-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef MAC_OPENGL_CONTEXT_H__
#define MAC_OPENGL_CONTEXT_H__

class OpenGLContextWrapper
{
public:
	static void* GetSharedContext();
	static void* InitSharedContext();
	static void SetSharedContextAsCurrent();
	static void* GetCurrentContext();
	static void SetContextAsCurrent(void* ctx);
	static void Uninitialize();
};

#endif /* !MAC_OPENGL_CONTEXT_H__ */
