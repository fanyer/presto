/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 */

#include "core/pch.h"

#include "adjunct/desktop_util/skin/SkinUtils.h"
#include "modules/skin/OpSkinManager.h"

/***********************************************************************************
**
**
**
***********************************************************************************/
int SkinUtils::GetTextColor(const OpStringC8& elem_name)
{
	OpSkinElement* element = g_skin_manager->GetSkinElement(elem_name.CStr());

	if(!element)
		return -1;

	UINT32 color = 0;
	RETURN_VALUE_IF_ERROR(element->GetTextColor(&color, SKINSTATE_ATTENTION), -1);

	return color;
}

/***********************************************************************************
**
** If this is a recognized theme (skin or persona), open it and notify the callee
**
***********************************************************************************/
BOOL SkinUtils::OpenIfThemeFile(OpStringC address)
{
	OpFile skinfile;
	RETURN_VALUE_IF_ERROR(skinfile.Construct(address.CStr()), FALSE);

	OpSkinManager::SkinFileType skin_type = g_skin_manager->GetSkinFileType(address);
	if(skin_type == OpSkinManager::SkinNormal)
	{
		OpFile dummy_persona;
		if (OpStatus::IsError(g_skin_manager->SelectSkinByFile(&skinfile)))
		{
			skin_type = OpSkinManager::SkinUnknown;
		}
		else 
		{
#ifdef PERSONA_SKIN_SUPPORT
			// attempt to install an empty persona as personas should not apply 
			// when installing third party skins
			OpStatus::Ignore(g_skin_manager->SelectPersonaByFile(&dummy_persona));
#endif // PERSONA_SKIN_SUPPORT
		}
	}
#ifdef PERSONA_SKIN_SUPPORT
	else if(skin_type == OpSkinManager::SkinPersona)
	{
		if (OpStatus::IsError(g_skin_manager->SelectPersonaByFile(&skinfile)))
		{
			skin_type = OpSkinManager::SkinUnknown;
		}
	}
#endif // PERSONA_SKIN_SUPPORT
	return skin_type != OpSkinManager::SkinUnknown;
}

