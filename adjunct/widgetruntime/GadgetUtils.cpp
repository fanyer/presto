/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#ifdef WIDGET_RUNTIME_SUPPORT

#include "adjunct/widgetruntime/GadgetUtils.h"

#include "adjunct/quick/Application.h"
#include "adjunct/quick/managers/FavIconManager.h"
#include "adjunct/widgetruntime/InstalledGadgetList.h"
#include "adjunct/widgetruntime/pi/PlatformGadgetUtils.h"
#include "modules/pi/OpBitmap.h"
#include "modules/pi/OpSystemInfo.h"
#include "modules/gadgets/OpGadgetClass.h"
#include "modules/skin/OpSkinElement.h"
#include "modules/skin/OpSkinManager.h"
#include "modules/skin/OpSkinUtils.h"
#include "modules/util/opfile/opfile.h"
#include "modules/util/path.h"
#include "modules/util/zipload.h"


namespace
{
	OP_STATUS CopyOpBitmap(OpBitmap* src, OpBitmap* cpy)
	{
		if (!src || !cpy) return OpStatus::ERR;

		UINT32 line_count = cpy->Height();
		UINT32* data = OP_NEWA(UINT32, cpy->Width());
		RETURN_OOM_IF_NULL(data);
		
		for (UINT32 i = 0; i < line_count; i++)
		{
			if (src->GetLineData(data, i) == TRUE)
			{
				cpy->AddLine(data, i);
			}
		}

		OP_DELETEA(data);

		return OpStatus::OK;
	}
}


GadgetUtils::Validation::Validation()
	: m_result(V_OK),
	  m_msg_id(Str::NOT_A_STRING)
{
}


OP_STATUS GadgetUtils::GetGadgetName(const OpStringC& conf_xml_path,
	OpString8& widget_name)
{
	OP_ASSERT(!conf_xml_path.IsEmpty());

	OpFile conf_xml;
	RETURN_IF_ERROR(conf_xml.Construct(conf_xml_path.CStr()));
	RETURN_IF_ERROR(conf_xml.Open(OPFILE_READ));

	OpString8 buffer, line;

	while (!conf_xml.Eof())
	{
		RETURN_IF_ERROR(conf_xml.ReadLine(line));
		RETURN_IF_ERROR(buffer.Append(line));
		line.Delete(0);
	}

	int idx = 0, max = buffer.Capacity();
	OpString8 name;
	BOOL flag_end = FALSE;

	while (idx < max && !flag_end)
	{
		if (buffer[idx] == '<')
		{
			idx++;
			while (idx < max && op_isspace(buffer[idx]))
			{
				idx++;
			}

			if (strncmp(&buffer[idx], "widgetname", 10) == 0)
			{
				idx += 10;

				while (idx < max && buffer[idx] != '>') 
				{
					idx++;
				}
				idx++;

				while (idx < max && buffer[idx] != '<') 
				{
					RETURN_IF_ERROR(name.Append(&buffer[idx++], 1));
				}
				RETURN_IF_ERROR(name.Append("\0"));
				flag_end = TRUE;
			}
		}
		idx++;
	}

	if (idx < max)
	{
		RETURN_IF_ERROR(widget_name.Set(name.CStr()));
		widget_name.Strip();
	}

	conf_xml.Close();

	OP_ASSERT(idx < max);

	return (idx >= max ? OpStatus::ERR : OpStatus::OK);
}

OP_STATUS GadgetUtils::GetGadgetIcon(Image& img,
		const OpGadgetClass* gadget_class,
		UINT32 desired_width,
		UINT32 desired_height,
		INT32 effect_mask,
		INT32 effect_value,
		BOOL can_scale_down,
		BOOL can_scale_up,
		FallbackIconProvider& fallback_provider)
{
	INT32 dummy_width = static_cast<INT32>(desired_width);
	INT32 dummy_height = static_cast<INT32>(desired_height);

	OP_ASSERT(dummy_width >= 0);
	OP_ASSERT(dummy_height >= 0);

	OpBitmap* icon = NULL;
	if (gadget_class &&
		OpStatus::IsSuccess(const_cast<OpGadgetClass*>(gadget_class)->GetGadgetIcon(&icon, dummy_width, dummy_height, FALSE, TRUE)))
	{
		img = imgManager->GetImage(icon);
		if (img.IsEmpty())
		{
			return OpStatus::ERR_NO_MEMORY;
		}
	}
	else
	{
		RETURN_IF_ERROR(fallback_provider.GetIcon(img));
	}

	// Needs scaling?
	BOOL is_bigger = img.Width() > desired_width || img.Height() > desired_height;
	BOOL is_smaller = img.Width() < desired_width || img.Height() < desired_height;
	if ((is_bigger && can_scale_down) || (is_smaller && can_scale_up))
	{
		OpBitmap* bmp = NULL;
		OpBitmap* bmp_cpy = NULL;

		bmp = img.GetBitmap(null_image_listener);
		RETURN_OOM_IF_NULL(bmp);

		OP_STATUS status = OpBitmap::Create(&bmp_cpy, desired_width, desired_height);
		if (OpStatus::IsSuccess(status))
		{
			OP_ASSERT(bmp_cpy != NULL);
			OpSkinUtils::ScaleBitmap(bmp, bmp_cpy);
		}
		img.ReleaseBitmap();
		if (OpStatus::IsSuccess(status))
		{
			img = imgManager->GetImage(bmp_cpy);
			if (img.IsEmpty())
			{
				status = OpStatus::ERR_NO_MEMORY;
			}
		}
		if (OpStatus::IsError(status))
		{
			OP_DELETE(bmp_cpy);
			return status;
		}
	}

	// Needs effects?

	if (effect_mask)
	{
		OpBitmap* bmp = NULL;
		OpBitmap* bmp_cpy = NULL;

		bmp = img.GetEffectBitmap(effect_mask, effect_value,
				null_image_listener);
		RETURN_OOM_IF_NULL(bmp);

		OP_STATUS status = OpBitmap::Create(&bmp_cpy, desired_width, desired_height);
		if (OpStatus::IsSuccess(status))
		{
			OP_ASSERT(bmp_cpy != NULL);
			status = CopyOpBitmap(bmp, bmp_cpy);
		}
		img.ReleaseEffectBitmap();
		if (OpStatus::IsSuccess(status))
		{
			img = imgManager->GetImage(bmp_cpy);
			if (img.IsEmpty())
			{
				status = OpStatus::ERR_NO_MEMORY;
			}
		}
		if (OpStatus::IsError(status))
		{
			OP_DELETE(bmp_cpy);
			return status;
		}
	}

	OP_ASSERT(FALSE == img.IsEmpty());

	return OpStatus::OK;
}

OP_STATUS GadgetUtils::GetGenericGadgetIcon(Image& icon, unsigned int size)
{
	OpSkinElement* element = NULL;

	if (size <= 16)
	{
		element = g_skin_manager->GetSkinElement("Widget");
	}
	else
	{
		OpString8 el_name;
		RETURN_IF_ERROR(el_name.AppendFormat("Widget %d", size));
		element = g_skin_manager->GetSkinElement(el_name.CStr());
	}

	if (element)
	{
		icon = element->GetImage(SKINSTATE_FOCUSED);
		return !icon.IsEmpty() ? OpStatus::OK : OpStatus::ERR;
	}
	else
	{
		return OpStatus::ERR;
	}
}

int GadgetUtils::GetInstallationCount()
{
	InstalledGadgetList installations;
	RETURN_VALUE_IF_ERROR(installations.Init(), -1);
	int number = installations.GetCount();
	return MAX(-1, number);
}

#endif // WIDGET_RUNTIME_SUPPORT
