/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef SKIN_SUPPORT

#include "modules/skin/skin_module.h"
#include "modules/skin/OpSkinManager.h"
#include "modules/skin/IndpWidgetPainter.h"
#include "modules/widgets/OpWidgetPainterManager.h"
#include "modules/widgets/OpWidget.h"


void SkinModule::InitL(const OperaInitInfo& info)
{
	m_skintype_names[0] = "";
	m_skintype_names[1] = ".left";
	m_skintype_names[2] = ".top";
	m_skintype_names[3] = ".right";
	m_skintype_names[4] = ".bottom";

	m_skintype_sizes[0] = 0;
	m_skintype_sizes[1] = 5;
	m_skintype_sizes[2] = 4;
	m_skintype_sizes[3] = 6;
	m_skintype_sizes[4] = 7;

	m_skinsize_names[0] = "";
	m_skinsize_names[1] = ".large";

	m_skinsize_sizes[0] = 0;
	m_skinsize_sizes[1] = 6;

	m_skinpart_names[0] = "Tile Center";
	m_skinpart_names[1] = "Tile Left";
	m_skinpart_names[2] = "Tile Top";
	m_skinpart_names[3] = "Tile Right";
	m_skinpart_names[4] = "Tile Bottom";
	m_skinpart_names[5] = "Corner Topleft";
	m_skinpart_names[6] = "Corner Topright";
	m_skinpart_names[7] = "Corner Bottomright";
	m_skinpart_names[8] = "Corner Bottomleft";
#ifdef ANIMATED_SKIN_SUPPORT
	m_image_animation_handlers = OP_NEW_L(Head, ());
#endif // ANIMATED_SKIN_SUPPORT

	skin_manager = OP_NEW_L(OpSkinManager, ());
	if (OpStatus::IsError(skin_manager->InitL()) && g_widgetpaintermanager)
	{
		g_widgetpaintermanager->SetPrimaryWidgetPainter(NULL);
	}

#ifdef SKIN_PRELOAD_COMMON_BITMAPS
	g_skin_manager->GetSkinElement("Scrollbar Horizontal Skin");
	g_skin_manager->GetSkinElement("Scrollbar Horizontal Knob Skin");
	g_skin_manager->GetSkinElement("Scrollbar Horizontal Left Skin");
	g_skin_manager->GetSkinElement("Scrollbar Horizontal Right Skin");
	g_skin_manager->GetSkinElement("Scrollbar Vertical Skin");
	g_skin_manager->GetSkinElement("Scrollbar Vertical Knob Skin");
	g_skin_manager->GetSkinElement("Scrollbar Vertical Down Skin");
	g_skin_manager->GetSkinElement("Scrollbar Vertical Up Skin");

	g_skin_manager->GetSkinElement("Push Button Skin");
	g_skin_manager->GetSkinElement("Push Default Button Skin");

	g_skin_manager->GetSkinElement("Radio Button Skin");
	g_skin_manager->GetSkinElement("Checkbox Skin");

	g_skin_manager->GetSkinElement("Edit Skin");
	g_skin_manager->GetSkinElement("MultilineEdit Skin");

	g_skin_manager->GetSkinElement("Dropdown Skin");
	g_skin_manager->GetSkinElement("Dropdown Button Skin");
	g_skin_manager->GetSkinElement("Listbox Skin");
#endif // SKIN_PRELOAD_COMMON_BITMAPS
}

void SkinModule::Destroy()
{
	OP_DELETE(g_IndpWidgetInfo);
	g_IndpWidgetInfo = NULL;

	OP_DELETE(skin_manager);
	skin_manager = NULL;

#ifdef ANIMATED_SKIN_SUPPORT
#ifdef _DEBUG
	OP_ASSERT(m_image_animation_handlers->Cardinal() == 0);
#endif // _DEBUG

	OP_DELETE(m_image_animation_handlers);
	m_image_animation_handlers = NULL;
#endif // ANIMATED_SKIN_SUPPORT
}

#endif // SKIN_SUPPORT

