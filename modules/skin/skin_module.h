/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef MODULES_SKIN_SKIN_MODULE_H
#define MODULES_SKIN_SKIN_MODULE_H

#ifdef SKIN_SUPPORT

#include "modules/hardcore/opera/module.h"

class OpSkinManager;
class OpWidgetInfo;

class SkinModule : public OperaModule
{
public:

	SkinModule() : skin_manager(NULL), indpWidgetInfo(NULL)
#ifdef ANIMATED_SKIN_SUPPORT
		, m_image_animation_handlers(NULL)
#endif
	{}

	void InitL(const OperaInitInfo& info);

	void Destroy();

	OpSkinManager* skin_manager;
	OpWidgetInfo* indpWidgetInfo;
#ifdef ANIMATED_SKIN_SUPPORT
	Head *m_image_animation_handlers;	// list of all animation handlers currently in use
#endif // ANIMATED_SKIN_SUPPORT
	/**
	 * Internal method
	 */
	inline const char** GetSkinTypeNames() { return m_skintype_names; }
	/**
	 * Internal method
	 */
	inline const char** GetSkinSizeNames() { return m_skinsize_names; }
	/**
	 * Internal method
	 */
	inline const char** GetSkinPartNames() { return m_skinpart_names; }
	/**
	 * Internal method
	 */
	inline UINT32*			GetSkinTypeSizes() { return m_skintype_sizes; }
	/**
	 * Internal method
	 */
	inline UINT32*			GetSkinSizeSizes() { return m_skinsize_sizes; }

private:
	const char* m_skintype_names[5];
	const char* m_skinsize_names[2];
	const char* m_skinpart_names[9];
	UINT32			m_skintype_sizes[5];
	UINT32			m_skinsize_sizes[2];
};

#define g_skin_manager g_opera->skin_module.skin_manager
#define g_IndpWidgetInfo g_opera->skin_module.indpWidgetInfo

#define SKIN_MODULE_REQUIRED

#endif // SKIN_SUPPORT

#endif // !MODULES_SKIN_SKIN_MODULE_H
