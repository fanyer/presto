/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright Opera Software ASA  1995-2000
 *
 * Inline functions for the ECMAScript ES_Runtime client interface
 * Lars Thomas Hansen / Karl Anders Oygard
 */

/* Functions in this file fall into two categories:
     - extremely performance sensitive (very few)
	 - inlining them will lead to smaller code on all platforms
 */

#ifndef ECMASCRIPT_INLINES_H
#define ECMASCRIPT_INLINES_H

inline BOOL
ES_Runtime::Enabled() const
{
	return enabled;
}

inline void
ES_Runtime::Disable()
{
	enabled = FALSE;
}

inline BOOL
ES_Runtime::GetErrorEnable() const
{
	return error_messages;
}

inline void
ES_Runtime::SetErrorEnable(BOOL enable)
{
	error_messages = enable;
}

inline FramesDocument*
ES_Runtime::GetFramesDocument() const
{
	return frames_doc;
}

inline void
ES_Runtime::SetDebugPrivileges()
{
	debug_privileges=TRUE;
}

inline BOOL
ES_Runtime::HasDebugPrivileges() const
{
	return debug_privileges;
}

#endif // ECMASCRIPT_INLINES_H
