/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "modules/style/src/css_viewport_rule.h"
#include "modules/style/src/css_parser.h"

/* virtual */ OP_STATUS
CSS_ViewportRule::GetCssText(CSS* stylesheet, TempBuffer* buf, unsigned int indent_level)
{
	RETURN_IF_ERROR(buf->Append("@-o-viewport"));
	return CSS_DeclarationBlockRule::GetCssText(stylesheet, buf, indent_level);
}

/* virtual */ CSS_PARSE_STATUS
CSS_ViewportRule::SetCssText(HLDocProfile* hld_prof, CSS* stylesheet, const uni_char* text, int len)
{
	return stylesheet->ParseAndInsertRule(hld_prof, this, NULL, NULL, TRUE, CSS_TOK_DOM_VIEWPORT_RULE, text, len);
}

#ifdef CSS_VIEWPORT_SUPPORT
/* virtual */ void
CSS_ViewportRule::AddViewportProperties(CSS_Viewport* viewport)
{
	CSS_property_list* props = GetPropertyList();
	CSS_decl* decl = props->GetFirstDecl();

	while (decl)
	{
		BOOL user = decl->GetUserDefined();
		BOOL important = decl->GetImportant();

		switch (decl->GetProperty())
		{
		case CSS_PROPERTY_zoom:
			viewport->SetZoom(GetZoomFactorFromDecl(decl), user, important);
			break;

		case CSS_PROPERTY_min_zoom:
			viewport->SetMinZoom(GetZoomFactorFromDecl(decl), user, important);
			break;

		case CSS_PROPERTY_max_zoom:
			viewport->SetMaxZoom(GetZoomFactorFromDecl(decl), user, important);
			break;

		case CSS_PROPERTY_user_zoom:
			OP_ASSERT(decl->GetDeclType() == CSS_DECL_TYPE);
			viewport->SetUserZoom(decl->TypeValue() == CSS_VALUE_zoom, user, important);
			break;

		case CSS_PROPERTY_min_width:
			viewport->SetMinWidth(GetViewportLengthFromDecl(decl), user, important);
			break;

		case CSS_PROPERTY_max_width:
			viewport->SetMaxWidth(GetViewportLengthFromDecl(decl), user, important);
			break;

		case CSS_PROPERTY_min_height:
			viewport->SetMinHeight(GetViewportLengthFromDecl(decl), user, important);
			break;

		case CSS_PROPERTY_max_height:
			viewport->SetMaxHeight(GetViewportLengthFromDecl(decl), user, important);
			break;

		case CSS_PROPERTY_orientation:
			{
				OP_ASSERT(decl->GetDeclType() == CSS_DECL_TYPE);
				CSS_Viewport::Orientation orientation(CSS_Viewport::Auto);
				if (decl->TypeValue() == CSS_VALUE_portrait)
					orientation = CSS_Viewport::Portrait;
				else if (decl->TypeValue() == CSS_VALUE_landscape)
					orientation = CSS_Viewport::Landscape;
				viewport->SetOrientation(orientation, user, important);
			}
			break;
		}

		decl = decl->Suc();
	}
}

CSS_ViewportLength
CSS_ViewportRule::GetViewportLengthFromDecl(CSS_decl* decl)
{
	if (decl->GetDeclType() == CSS_DECL_TYPE)
	{
		CSS_ViewportLength::Type len_type = CSS_ViewportLength::AUTO;

		switch (decl->TypeValue())
		{
		case CSS_VALUE_device_width:
			len_type = CSS_ViewportLength::DEVICE_WIDTH;
			break;
		case CSS_VALUE_device_height:
			len_type = CSS_ViewportLength::DEVICE_HEIGHT;
			break;
		default:
			OP_ASSERT(!"Unhandled length keyword");
		case CSS_VALUE_auto:
			break;
		}

		return CSS_ViewportLength(len_type);
	}
	else if (decl->GetDeclType() == CSS_DECL_NUMBER)
	{
		CSS_ViewportLength::Type unit = CSS_ViewportLength::PX;

		switch (decl->GetValueType(0))
		{
		case CSS_PERCENTAGE:
			unit = CSS_ViewportLength::PERCENT;
			break;
		case CSS_EM:
			unit = CSS_ViewportLength::EM;
			break;
		case CSS_REM:
			unit = CSS_ViewportLength::REM;
			break;
		case CSS_EX:
			unit = CSS_ViewportLength::EX;
			break;
		case CSS_CM:
			unit = CSS_ViewportLength::CM;
			break;
		case CSS_MM:
			unit = CSS_ViewportLength::MM;
			break;
		case CSS_IN:
			unit = CSS_ViewportLength::INCH;
			break;
		case CSS_PT:
			unit = CSS_ViewportLength::PT;
			break;
		case CSS_PC:
			unit = CSS_ViewportLength::PC;
			break;
		default:
			OP_ASSERT(!"Unhandled length unit");
		case CSS_PX:
			break;
		}

		return CSS_ViewportLength(decl->GetNumberValue(0), unit);
	}
	else
	{
		OP_ASSERT(!"Unrecognized <viewport length> type");
		return CSS_ViewportLength();
	}
}

double
CSS_ViewportRule::GetZoomFactorFromDecl(CSS_decl* decl)
{
	if (decl->GetDeclType() == CSS_DECL_NUMBER)
	{
		double zoom = decl->GetNumberValue(0);
		short type = decl->GetValueType(0);

		OP_ASSERT(type == CSS_NUMBER || type == CSS_PERCENTAGE);

		if (type == CSS_PERCENTAGE)
			zoom /= 100;

		return zoom;
	}
	else
	{
		OP_ASSERT(decl->GetDeclType() == CSS_DECL_TYPE && decl->TypeValue() == CSS_VALUE_auto);
		return CSS_VIEWPORT_ZOOM_AUTO;
	}
}

#endif // CSS_VIEWPORT_SUPPORT
