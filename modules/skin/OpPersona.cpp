/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * File : OpPersona.cpp
 *
 * Description : Personas that are applied on top of the loaded skin, modifying the skin
 */
#include "core/pch.h"

#ifdef PERSONA_SKIN_SUPPORT

#include "modules/skin/OpSkin.h"
#include "modules/skin/OpPersona.h"
#include "modules/skin/OpSkinElement.h"
#include "modules/skin/OpSkinManager.h"
#include "modules/skin/OpSkinUtils.h"

#include "modules/pi/OpBitmap.h"
#include "modules/pi/OpSystemInfo.h"
#include "modules/pi/ui/OpUiInfo.h"

#include "modules/prefs/prefsmanager/collections/pc_core.h"
#include "modules/prefsfile/prefsfile.h"
#include "modules/prefsfile/prefsentry.h"
#include "modules/prefsfile/prefssection.h"

#include "modules/img/image.h"

/***********************************************************************************
**
**	OpPersona
**
***********************************************************************************/

OpPersona::OpPersona()
{
}

OpPersona::~OpPersona()
{
	FreeClearElementsList();
}

OP_STATUS OpPersona::InitL(OpFileDescriptor* skinfile)
{
	OP_STATUS s = OpSkin::LoadIniFileL(skinfile, UNI_L("persona.ini"));
	if(OpStatus::IsSuccess(s))
	{
		Image image = GetBackgroundImage();
		if(!image.IsEmpty())
		{
			OpBitmap *bitmap = image.GetBitmap(NULL);
			if(bitmap)
			{
				INT32 color_scheme_color = OpSkinUtils::GetAverageColorOfBitmap(bitmap);
				if(color_scheme_color)
				{
					color_scheme_color = ReadColorL("Options", "Tint Color", ReadColorL("Options", "Colorize Color", color_scheme_color, FALSE), FALSE);

					SetColorSchemeColor(color_scheme_color);
					SetColorSchemeMode(COLOR_SCHEME_MODE_CUSTOM);
					SetColorSchemeCustomType(COLOR_SCHEME_CUSTOM_NORMAL);
				}
				image.ReleaseBitmap();
			}
		}
		GenerateClearElementsList();
	}
	return s;
}

Image OpPersona::GetBackgroundImage()
{
	OpSkinElement *elm = GetBackgroundImageSkinElement();
	if(elm)
	{
		return elm->GetImage(0, SKINPART_TILE_CENTER);
	}
	return Image();
}

OpSkinElement* OpPersona::GetBackgroundImageSkinElement()
{
	return GetSkinElement("Window Image", SKINTYPE_DEFAULT, SKINSIZE_DEFAULT);
}

void OpPersona::OnSkinLoaded(OpSkin *loaded_skin)
{
	if(GetColorSchemeColor())
	{
		loaded_skin->SetColorSchemeMode(GetColorSchemeMode());
		loaded_skin->SetColorSchemeColor(GetColorSchemeColor());
		loaded_skin->SetColorSchemeCustomType(GetColorSchemeCustomType());
	}
	GenerateClearElementsList();
}

bool OpPersona::IsInClearElementsList(const OpStringC8& name)
{
	for(UINT32 x = 0; x < m_clear_elements.GetCount(); x++)
	{
		char* element = m_clear_elements.Get(x);
		if(name.CompareI(element) == 0)
			return true;
	}
	return false;
}

OP_STATUS OpPersona::GenerateClearElementsList()
{
	FreeClearElementsList();

	PrefsFile *file = GetPrefsFile();
	
	PrefsSection *clear = file->ReadSectionL(UNI_L("clear elements"));
	OpStackAutoPtr<PrefsSection> section_ap(clear);

	const PrefsEntry *entry = clear ? clear->Entries() : NULL;
	while (entry)
	{
		const uni_char *key = entry->Key();
		const uni_char *data = entry->Value();

		if(data && uni_atoi(data) == 1)
		{
			OpString8 tmp;
			RETURN_IF_ERROR(tmp.Set(key));

			OpAutoArray<char> name(op_strdup(tmp.CStr()));
			RETURN_OOM_IF_NULL(name.get());

			RETURN_IF_ERROR(m_clear_elements.Add(name.get()));
			name.release();
		}
		entry = (const PrefsEntry *) entry->Suc();
	}
	return OpStatus::OK;
}

void OpPersona::FreeClearElementsList()
{
	for(UINT32 x = 0; x < m_clear_elements.GetCount(); x++)
	{
		op_free(m_clear_elements.Get(x));
	}
	m_clear_elements.Clear();
}

#endif // PERSONA_SKIN_SUPPORT
