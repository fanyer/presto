/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2011-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "platforms/mac/util/OpenGLContextWrapper.h"
#include "platforms/mac/model/SharedOpenGLContext.h"

void OpenGLContextWrapper::Uninitialize()
{
	[SharedOpenGLContext clearSharedContext];
}

void* OpenGLContextWrapper::GetSharedContext()
{
	return [SharedOpenGLContext sharedContext];
}

void* OpenGLContextWrapper::InitSharedContext()
{
	return [SharedOpenGLContext sharedContext];
}

void OpenGLContextWrapper::SetSharedContextAsCurrent()
{
	SetContextAsCurrent(GetSharedContext());
}

void* OpenGLContextWrapper::GetCurrentContext()
{
	return [NSOpenGLContext currentContext];
}

void OpenGLContextWrapper::SetContextAsCurrent(void* ctx)
{
	[static_cast<NSOpenGLContext*>(ctx) makeCurrentContext];
}
