/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * File : OpSkinElement.cpp
 */
#include "core/pch.h"

#ifdef SKIN_SUPPORT

#include "modules/skin/OpSkinElement.h"
#include "modules/skin/OpSkin.h"
#ifdef PERSONA_SKIN_SUPPORT
#include "modules/skin/OpPersona.h"
#endif // PERSONA_SKIN_SUPPORT
#include "modules/skin/OpSkinAnimHandler.h"
#include "modules/skin/OpSkinManager.h"
#include "modules/skin/OpSkinUtils.h"
#include "modules/skin/skin_module.h"

#include "modules/prefsfile/prefsfile.h"
#include "modules/prefs/prefsmanager/collections/pc_core.h"

#include "modules/display/vis_dev.h"
#include "modules/util/gen_math.h"
#include "modules/util/OpLineParser.h"

#include "modules/pi/ui/OpUiInfo.h"
#include "modules/pi/OpSystemInfo.h"

#include "modules/img/image.h"
#include "modules/dochand/winman.h"

#include "modules/layout/layout.h"

#define s_skinsize_names g_opera->skin_module.GetSkinSizeNames()
#define s_skinsize_sizes g_opera->skin_module.GetSkinSizeSizes()
#define s_skintype_names g_opera->skin_module.GetSkinTypeNames()
#define s_skintype_sizes g_opera->skin_module.GetSkinTypeSizes()
#define s_skinpart_names g_opera->skin_module.GetSkinPartNames()

// helpers

UINT32 GetColor(INT32 color);

void SkinArrow::SetOffset(double offset)
{
	this->offset = offset;
	this->offset = MIN(MAX(this->offset,0),100);
}

/***********************************************************************************
**
**	OpSkinElement
**
***********************************************************************************/

OpSkinElement::OpSkinElement(OpSkin* skin, const OpStringC8& name, SkinType type, SkinSize size)
	: m_is_native(FALSE)
	, m_skin(skin)
	, m_mini_state_known(FALSE)
	, m_has_bold_state(FALSE)
	, m_has_hover_state(FALSE)
#ifdef SKIN_ELEMENT_FALLBACK_DISABLE
	, m_fallback_version(0)
#endif // SKIN_ELEMENT_FALLBACK_DISABLE
	, m_mirror_rtl(FALSE)
{
	m_key.m_name		= NULL;
	m_key.m_type		= type;
	m_key.m_size		= size;
	m_state_elements.SetAllocationStepSize(2);
}

OP_STATUS OpSkinElement::Init(const char *name)
{
	RETURN_IF_ERROR(m_name.Set(name));
	m_key.m_name		= m_name.CStr();

	OpString8 state_element_name;

	// Reserve some memory here so the string Appends in LoadStateElement will get cheaper.
	if (!state_element_name.Reserve(128))
		return OpStatus::ERR_NO_MEMORY;

	for (INT32 i = 0; i < SKINSTATE_TOTAL; i++)
	{
		// We skip MINI state until it's really asked for, since we don't want to load and
		// scale extra bitmaps that are never going to be used.
		if (i & SKINSTATE_MINI)
			continue;

		OP_STATUS status = LoadStateElement(i, state_element_name);
		if (OpStatus::IsMemoryError(status))
			return status;
		if (OpStatus::IsError(status) && i == 0)
			return status;
	}
	return OpStatus::OK;
}

OpSkinElement::~OpSkinElement()
{
	for (OpListenersIterator iterator(m_element_listeners); m_element_listeners.HasNext(iterator);)
		m_element_listeners.GetNext(iterator)->OnSkinElementDeleted();

	for (UINT32 i = 0; i < m_state_elements.GetCount(); i++)
	{
		StateElement *state_elm = m_state_elements.Get(i);
		OP_DELETE(state_elm);
	}

	if (m_key.m_name)
		m_skin->RemoveSkinElement(m_key.m_name, m_key.m_type, m_key.m_size);
}

/* static */
OpSkinElement* OpSkinElement::CreateElement(bool native, OpSkin* skin, const OpStringC8& name, SkinType type, SkinSize size)
{
	OpSkinElement *element = NULL;
#ifdef SKIN_NATIVE_ELEMENT
#ifdef PERSONA_SKIN_SUPPORT
	// no native element if we're clearing the background on the element anyway
	if(g_skin_manager->GetPersona() && g_skin_manager->GetPersona()->IsInClearElementsList(name))
		native = false;
#endif // PERSONA_SKIN_SUPPORT
	if (native)
		element = OpSkinElement::CreateNativeElement(skin, name, type, size);
#endif // SKIN_NATIVE_ELEMENT
	if (!element)
		element = OP_NEW(OpSkinElement, (skin, name, type, size));

	return element;
}

/***********************************************************************************
**
**	Draw
**
***********************************************************************************/

OP_STATUS OpSkinElement::Draw(VisualDevice* vd, const OpPoint& point, INT32 state, INT32 hover_value, const OpRect* clip_rect)
{
	StateElement* state_element = GetStateElement(state);

	if (!state_element)
		return OpStatus::ERR;

	return Draw(vd, OpRect(point.x, point.y, state_element->m_width, state_element->m_height), state, hover_value, clip_rect);
}

/***********************************************************************************
**
**	Draw
**
***********************************************************************************/

OP_STATUS OpSkinElement::Draw(VisualDevice* vd, OpRect rect, INT32 state, INT32 hover_value, const OpRect* clip_rect)
{
	DrawArguments args;
   	args.hover_value = hover_value;
	args.clip_rect = clip_rect;
	return Draw(vd, rect, state, args);
}

OP_STATUS OpSkinElement::Draw(VisualDevice* vd, OpRect rect, INT32 state, DrawArguments args)
{
	INT32 hover_value = args.hover_value;
	const OpRect* clip_rect = args.clip_rect;

	INT32 effect = 0;
	INT32 effect_value = 0;

#ifdef SKIN_NO_HOVER
	state &= ~SKINSTATE_HOVER;
	hover_value = 0;
#endif

#ifdef PIXEL_SCALE_RENDERING_SUPPORT
	// find high-res state element if current pixel scale is greater
	// than 100. since we dont support multi-level high-res state
	// element, the non-normal state(pixelscale!=100) will be treated
	// as high-res state.
	if (vd->GetVPScale().GetScale() > 100)
		state |= SKINSTATE_HIGHRES;
#endif // PIXEL_SCALE_RENDERING_SUPPORT

	BOOL hover_blend = (hover_value || state & SKINSTATE_HOVER) && m_has_hover_state;

	StateElement* state_element = GetStateElement(hover_blend ? (state | SKINSTATE_HOVER) : state);

	if (!state_element)
		return OpStatus::ERR;

	if (state_element->m_clear_background)
	{
		// FIXME: is this UINT32 equivalent to COLORREF?
		UINT32 bgc = g_skin_manager->GetTransparentBackgroundColor();
		vd->SetColor((bgc>>16)&0xff, (bgc>>8)&0xff, bgc&0xff, (bgc>>24)&0xff);
		vd->ClearRect(rect);

		OpRect r = rect;

		// Sometimes 'Transparent background clearing' testcase
		// fails because r rect being broken by one of following calls.
		// Unfortuantely, I can't reproduce it with 100% probability :(
		// [pobara 2011-01-13]

		r.x += vd->GetTranslationX() - vd->GetRenderingViewX();
		r.y += vd->GetTranslationY() - vd->GetRenderingViewY();
		r.IntersectWith(vd->GetClipping());
		vd->ScaleToScreen(r);

		// you might not always have a OpView (eg. you don't for windows menus which can also be skinned)
		if(vd->GetOpView())
		{
			if(OpWindow* topwin = vd->GetOpView()->GetRootWindow())
			{
				INT32 winx, winy;
				topwin->GetInnerPos(&winx, &winy);

				OpPoint screenp = vd->GetOpView()->ConvertToScreen(OpPoint(r.x, r.y));
				r.x = screenp.x-winx;
				r.y = screenp.y-winy;
			}
		}
		g_skin_manager->TrigBackgroundCleared(vd->painter, r);
	}
	if (!g_pccore->GetIntegerPref(PrefsCollectionCore::SpecialEffects))
	{
		hover_value =  state & SKINSTATE_HOVER ? 100 : 0;
	}

	if (hover_blend)
	{
		args.hover_value = 0;
		Draw(vd, rect, state & ~SKINSTATE_HOVER, args);
		effect = Image::EFFECT_BLEND;
		effect_value = hover_value * state_element->m_blend / 100;
	}
	else if (args.forced_effect > 0)
	{
		effect = args.forced_effect;
		effect_value = args.forced_effect_value;
	}
	else
	{
		effect = 0;
		effect_value = 0;
		if (state & SKINSTATE_DISABLED)
		{
			if (g_skin_manager->GetDimDisabledBackgrounds() || state_element->m_type == StateElement::TypeImage)
				effect = Image::EFFECT_DISABLED;
		}
		else if (hover_value && g_skin_manager->GetGlowOnHover())
		{
			effect = Image::EFFECT_GLOW;
			effect_value = hover_value;
		}
	}

#ifdef SKIN_SKIN_COLOR_THEME
	if (args.colorize_background)
	{
		state_element->DrawColorizedBackground(vd, rect, 0, 0, clip_rect, args.background_color);
	}
#endif // SKIN_SKIN_COLOR_THEME
	state_element->Draw(vd, rect, effect, effect_value, clip_rect);

	return OpStatus::OK;
}

OP_STATUS OpSkinElement::DrawOverlay(VisualDevice* vd, OpRect rect, INT32 state, INT32 hover_value, const OpRect* clip_rect)
{
	INT32 effect = 0;
	INT32 effect_value = 0;

#ifdef SKIN_NO_HOVER
	state &= ~SKINSTATE_HOVER;
	hover_value = 0;
#endif

#ifdef PIXEL_SCALE_RENDERING_SUPPORT
	// find high-res state element, see comments in OpSkinElement::Draw
	if (vd->GetVPScale().GetScale() > 100)
		state |= SKINSTATE_HIGHRES;
#endif // PIXEL_SCALE_RENDERING_SUPPORT

	BOOL hover_blend = (hover_value || state & SKINSTATE_HOVER) && m_has_hover_state;

	StateElement* state_element = GetStateElement(hover_blend ? (state | SKINSTATE_HOVER) : state);

	if (!state_element)
		return OpStatus::ERR;

	state_element->DrawOverlay(vd, rect, effect, effect_value, clip_rect);

	return OpStatus::OK;
}

OP_STATUS OpSkinElement::DrawWithEdgeArrow(VisualDevice* vd, OpRect rect, INT32 state, DrawArguments args)
{
	OP_ASSERT(args.arrow);

	if (args.arrow->part != SKINPART_TILE_CENTER)
	{
		OpRect used_rect;
		const INT32 arrow_state = (state & ~SKINSTATE_RTL);

#ifdef PIXEL_SCALE_RENDERING_SUPPORT
		// find high-res state element, see comments in OpSkinElement::Draw
		// note: do not add SKINSTATE_HIGHRES to arrow_state, which
		// will be used as effect later in DrawSkinArrow.
		if (vd->GetVPScale().GetScale() > 100)
			state |= SKINSTATE_HIGHRES;
#endif // PIXEL_SCALE_RENDERING_SUPPORT

		StateElement* state_element = GetStateElement(state);

		if (state_element && state_element->DrawSkinArrow(vd, rect, arrow_state, args.hover_value, *args.arrow, used_rect))
		{
			// If we painted a arrow, we will paint the rest of the skin multiple times
			// with clipping to exclude the part where the arrow was painted.
			// This is needed to avoid shadow and transparency problems with the arrow image.
			BgRegion rgn;
			RETURN_IF_ERROR(rgn.Set(rect));
			RETURN_IF_ERROR(rgn.ExcludeRect(used_rect, FALSE));
			for(int i = 0; i < rgn.num_rects; i++)
			{
				vd->BeginClipping(rgn.rects[i]);
				RETURN_IF_ERROR(Draw(vd, rect, state, args));
				vd->EndClipping();
			}
			return OpStatus::OK;
		}
	}
	return Draw(vd, rect, state, args);
}

OP_STATUS OpSkinElement::Draw(VisualDevice* vd, OpRect rect, INT32 state)
{
	DrawArguments args;
	return Draw(vd, rect, state, args);
}

#ifdef SKIN_OUTLINE_SUPPORT

/***********************************************************************************
**
**	DrawSkinPart
**
***********************************************************************************/

OP_STATUS OpSkinElement::DrawSkinPart(VisualDevice* vd, OpRect rect, SkinPart part)
{
	INT32 state = 0;

#ifdef PIXEL_SCALE_RENDERING_SUPPORT
	// find high-res state element, see comments in OpSkinElement::Draw
	if (vd->GetVPScale().GetScale() > 100)
		state |= SKINSTATE_HIGHRES;
#endif // PIXEL_SCALE_RENDERING_SUPPORT

	StateElement* state_element = GetStateElement(state);

	if (!state_element)
		return OpStatus::ERR;

	state_element->DrawSkinPart(vd, rect, part);

	return OpStatus::OK;
}

OP_STATUS OpSkinElement::GetBorderWidth(INT32* left, INT32* top, INT32* right, INT32* bottom, INT32 state)
{
	StateElement* state_element = GetStateElement(state);

	if (!state_element)
		return OpStatus::ERR;

	*left = state_element->GetImage(SKINPART_TILE_LEFT).Width();
	*top = state_element->GetImage(SKINPART_TILE_TOP).Height();
	*right = state_element->GetImage(SKINPART_TILE_RIGHT).Width();
	*bottom = state_element->GetImage(SKINPART_TILE_BOTTOM).Height();

	return OpStatus::OK;
}

#endif // SKIN_OUTLINE_SUPPORT

/***********************************************************************************
**
**	GetSize
**
***********************************************************************************/

OP_STATUS OpSkinElement::GetSize(INT32* width, INT32* height, INT32 state)
{
	StateElement* state_element = GetStateElement(state);

	if (!state_element)
		return OpStatus::ERR;

	*width = state_element->m_width;
	*height = state_element->m_height;

	return OpStatus::OK;
}

/***********************************************************************************
**
**	GetMinSize
**
***********************************************************************************/

OP_STATUS OpSkinElement::GetMinSize(INT32* minwidth, INT32* minheight, INT32 state)
{
	StateElement* state_element = GetStateElement(state);

	if (!state_element)
		return OpStatus::ERR;

	*minwidth = state_element->m_minwidth;
	*minheight = state_element->m_minheight;

	return OpStatus::OK;
}

/***********************************************************************************
**
**	GetMargin
**
***********************************************************************************/

OP_STATUS OpSkinElement::GetMargin(INT32* left, INT32* top, INT32* right, INT32* bottom, INT32 state)
{
	StateElement* state_element = GetStateElement(state);

	if (!state_element)
		return OpStatus::ERR;

	*left = state_element->m_margin_left;
	*top = state_element->m_margin_top;
	*right = state_element->m_margin_right;
	*bottom = state_element->m_margin_bottom;

	return OpStatus::OK;
}

/***********************************************************************************
**
**	GetButtonTextPadding - Get additional padding only used for text buttons, overrides to the global setting
**
***********************************************************************************/
OP_STATUS OpSkinElement::GetButtonTextPadding(INT32 *padding, INT32 state)
{
	StateElement* state_element = GetStateElement(state);

	if (!state_element || state_element->m_text_button_padding == -1)
		return OpStatus::ERR;

	*padding = state_element->m_text_button_padding;

	return OpStatus::OK;
}

/**********************************************************************************/

OP_STATUS OpSkinElement::GetRadius(UINT8 *values, INT32 state)
{
	StateElement* state_element = GetStateElement(state);

	if (!state_element)
		return OpStatus::ERR;

	for(int i = 0; i < 4; i++)
		values[i] = state_element->m_radius[i];

	return OpStatus::OK;
}

/***********************************************************************************
**
**	GetPadding
**
***********************************************************************************/

OP_STATUS OpSkinElement::GetPadding(INT32* left, INT32* top, INT32* right, INT32* bottom, INT32 state)
{
	StateElement* state_element = GetStateElement(state);

	if (!state_element)
		return OpStatus::ERR;

	*left = state_element->m_padding_left;
	*top = state_element->m_padding_top;
	*right = state_element->m_padding_right;
	*bottom = state_element->m_padding_bottom;

	return OpStatus::OK;
}

/***********************************************************************************
 **
 **	GetStretchBorder
 **
 ***********************************************************************************/

OP_STATUS OpSkinElement::GetStretchBorder(UINT8 *left, UINT8* top, UINT8* right, UINT8* bottom, INT32 state)
{
	StateElement* state_element = GetStateElement(state);
	
	if (!state_element)
		return OpStatus::ERR;
	
	*left = state_element->m_stretch_border[0];
	*top = state_element->m_stretch_border[1];
	*right = state_element->m_stretch_border[2];
	*bottom = state_element->m_stretch_border[3];
	
	return OpStatus::OK;
}

OP_STATUS OpSkinElement::GetArrowSize(const SkinArrow &arrow, INT32 *width, INT32 *height, INT32 state)
{
	StateElement* state_element = GetStateElement(state);
	
	if (!state_element)
		return OpStatus::ERR;

	if (arrow.part == SKINPART_TILE_CENTER)
		*width = *height = 0;
	else
	{
		*width = INT32(state_element->m_images[arrow.part].Width());
		*height = INT32(state_element->m_images[arrow.part].Height());
	}
	return OpStatus::OK;
}

/***********************************************************************************
**
**	GetSpacing
**
***********************************************************************************/

OP_STATUS OpSkinElement::GetSpacing(INT32* spacing, INT32 state)
{
	StateElement* state_element = GetStateElement(state);

	if (!state_element)
		return OpStatus::ERR;

	*spacing = state_element->m_spacing;

	return OpStatus::OK;
}

/***********************************************************************************
**
**	GetBackgroundColor
**
***********************************************************************************/

OP_STATUS OpSkinElement::GetBackgroundColor(UINT32* color, INT32 state)
{
	StateElement* state_element = GetStateElement(state, FALSE);

	if (!state_element || state_element->m_color == SKINCOLOR_NONE)
		return OpStatus::ERR;

	*color = state_element->m_color;

	return OpStatus::OK;
}

/***********************************************************************************
 **
 **	GetLinkColor
 **
 ***********************************************************************************/

OP_STATUS OpSkinElement::GetLinkColor(UINT32* color, INT32 state)
{
	StateElement* state_element;

	if (GetStateElement(state, FALSE))
	{
		state_element = GetStateElement(state, FALSE);
	}
	else if (state & SKINSTATE_DISABLED && GetStateElement(SKINSTATE_DISABLED, FALSE))
	{
		state_element = GetStateElement(SKINSTATE_DISABLED, FALSE);
	}
	else
	{
		state_element = GetStateElement(state);
	}

	if (!state_element || state_element->m_text_link_color == SKINCOLOR_NONE)
		return OpStatus::ERR;

	*color = GetColor(state_element->m_text_link_color);
	*color |= (*color & OP_RGBA(0, 0, 0, 255));		// reset alpha

	return OpStatus::OK;
}

/***********************************************************************************
**
**	GetTextColor
**
***********************************************************************************/

OP_STATUS OpSkinElement::GetTextColor(UINT32* color, INT32 state)
{
	// Note, a special case for text color:
	// Since we can always draw disabled images by blending them,
	// the SKINSTATE_DISABLED flag is defined to be the lowest bit in the state
	// flags, and will therefore be the first to be discarded
	// when searching for the closest matching state element.
	//
	// However, this doesn't work very well for text color, since we do not support blending
	// the text like that, and in that case it is not desired that SKINSTATE_DISABLED
	// is discarded first, because that would mean that we would need
	// to define disabled text color for all pressed and selected states etc in the skin
	// file. So we add a special case for SKINSTATE_DISABLED

	StateElement* state_element;

	if (GetStateElement(state, FALSE))
	{
		state_element = GetStateElement(state, FALSE);
	}
	else if (state & SKINSTATE_DISABLED && GetStateElement(SKINSTATE_DISABLED, FALSE))
	{
		state_element = GetStateElement(SKINSTATE_DISABLED, FALSE);
	}
	else
	{
		state_element = GetStateElement(state);
	}

	if (!state_element || state_element->m_text_color == SKINCOLOR_NONE)
		return OpStatus::ERR;

	*color = GetColor(state_element->m_text_color);
	*color |= (*color & OP_RGBA(0, 0, 0, 255));		// reset alpha

	return OpStatus::OK;
}

OP_STATUS OpSkinElement::GetTextShadow(const OpSkinTextShadow*& shadow, INT32 state)
{
	StateElement* state_element = GetStateElement(state, FALSE);
	if (!state_element)
		state_element = GetStateElement(state);

	if (!state_element)
		return OpStatus::ERR;

	shadow = &state_element->m_text_shadow;

	return OpStatus::OK;
}

/***********************************************************************************
**
**	GetTextBold
**
***********************************************************************************/

OP_STATUS OpSkinElement::GetTextBold(BOOL* bold, INT32 state)
{
	StateElement* state_element = GetStateElement(state);

	if (!state_element || state_element->m_text_bold == -1)
		return OpStatus::ERR;

	*bold = state_element->m_text_bold != 0;

	return OpStatus::OK;
}

/***********************************************************************************
**
**	GetTextUnderline
**
***********************************************************************************/

OP_STATUS OpSkinElement::GetTextUnderline(BOOL* underline, INT32 state)
{
	StateElement* state_element = GetStateElement(state);

	if (!state_element || state_element->m_text_underline == -1)
		return OpStatus::ERR;

	*underline = state_element->m_text_underline != 0;

	return OpStatus::OK;
}

/***********************************************************************************
**
**	GetTextZoom
**
***********************************************************************************/

OP_STATUS OpSkinElement::GetTextZoom(BOOL* zoom, INT32 state)
{
	StateElement* state_element = GetStateElement(state);

	if (!state_element || state_element->m_text_zoom == -1)
		return OpStatus::ERR;

	*zoom = state_element->m_text_zoom != 0;

	return OpStatus::OK;
}

/***********************************************************************************
**
**	GetImage
**
***********************************************************************************/

Image OpSkinElement::GetImage(INT32 state, SkinPart part/* = SKINPART_TILE_CENTER*/)
{
	StateElement* state_element = GetStateElement(state);

	if (!state_element /*|| state_element->m_type != StateElement::TypeImage*/)
		return Image();

#ifdef ANIMATED_SKIN_SUPPORT
	return state_element->GetAnimatedImage(part, NULL);
#else
	return state_element->GetImage(part);
#endif // ANIMATED_SKIN_SUPPORT

}

#ifdef SKINNABLE_AREA_ELEMENT
OP_STATUS OpSkinElement::GetOutlineColor(UINT32* color, INT32 state)
{
	StateElement* state_element = GetStateElement(state);

	if (!state_element)
		return OpStatus::ERR;

	*color = state_element->m_outline_color;
	*color |= (*color & OP_RGBA(0, 0, 0, 255));		// reset alpha

	return OpStatus::OK;
}
#endif //SKINNABLE_AREA_ELEMENT

#ifdef SKINNABLE_AREA_ELEMENT
OP_STATUS OpSkinElement::GetOutlineWidth(UINT32* width, INT32 state)
{
	StateElement* state_element = GetStateElement(state);

	if (!state_element)
		return OpStatus::ERR;

	*width = state_element->m_outline_width;

	return OpStatus::OK;
}
#endif //SKINNABLE_AREA_ELEMENT

////////////////////////////////////////////////////////////////////////////////////

#ifdef USE_GRADIENT_SKIN
OP_STATUS OpSkinElement::GetGradientColors(UINT32* min_color, UINT32* color, UINT32* min_border_color, UINT32* border_color, UINT32* bottom_border_color, INT32 state)
{
	StateElement* state_element = GetStateElement(state, FALSE);

	// You must have all the gradient colours set otherwise the graident can't be used
	if (!state_element || state_element->m_min_color == SKINCOLOR_NONE || state_element->m_color == SKINCOLOR_NONE ||
		state_element->m_min_border_color == SKINCOLOR_NONE || state_element->m_border_color == SKINCOLOR_NONE || 
		state_element->m_bottom_border_color == SKINCOLOR_NONE)
		return OpStatus::ERR;

	*min_color				= state_element->m_min_color;
	*color					= state_element->m_color;
	*min_border_color		= state_element->m_min_border_color;
	*border_color			= state_element->m_border_color;
	*bottom_border_color	= state_element->m_bottom_border_color;

	return OpStatus::OK;
}
#endif // USE_GRADIENT_SKIN

#ifdef PERSONA_SKIN_SUPPORT
void OpSkinElement::OverridePersonaDefaults(OpSkinElement::StateElement* se)
{
	if(g_skin_manager->HasPersonaSkin())
	{
		if(g_skin_manager->GetPersona()->IsInClearElementsList(m_name))
		{
			se->m_color = SKINCOLOR_NONE;

			for (int i = 0; i < SKINPART_TOTAL; i++)
			{
				se->m_images[i].Empty();
			}
			se->m_type = StateElement::TypeImage;
			se->m_clear_background = TRUE;
		}
	}
}
#endif // PERSONA_SKIN_SUPPORT

/***********************************************************************************
**
**	GetStateElement
**
***********************************************************************************/

OpSkinElement::StateElement* OpSkinElement::GetStateElement(INT32 state, BOOL fallback_to_closest, BOOL load_if_not_exist)
{
	if ((state & SKINSTATE_MINI) && !m_mini_state_known && load_if_not_exist)
	{
		OpString8 state_element_name;
		// Reserve some memory here so the string Appends in LoadStateElement will get cheaper.
		if (!state_element_name.Reserve(128))
			return NULL;

		for (INT32 i = SKINSTATE_MINI; i < SKINSTATE_TOTAL; i++)
		{
			OP_STATUS status = LoadStateElement(i, state_element_name);
			if (OpStatus::IsMemoryError(status))
				return NULL;
		}

		m_mini_state_known = TRUE;
	}

	// Find the state element
	INT32 i;
	for (i = 0; i < (INT32)m_state_elements.GetCount(); i++)
	{
		StateElement *state_elm = m_state_elements.Get(i);

		if (state_elm->m_state == state)
			return state_elm;
		else if (state_elm->m_state > state)
			break; // States are sorted, so there is no match further on.
	}

	// No find. Find the closest element

	if (!fallback_to_closest)
		return NULL;

	for (i = (INT32)m_state_elements.GetCount() - 1; i > 0; i--)
	{
		StateElement *state_elm = m_state_elements.Get(i);
		if (state_elm->m_state > state)
			continue;
		if ((state_elm->m_state | state) == state)
			return state_elm;
	}

	return m_state_elements.GetCount() ? m_state_elements.Get(0) : NULL;
}

/***********************************************************************************
**
**	LoadStateElement
**
***********************************************************************************/

OP_STATUS OpSkinElement::LoadStateElement(INT32 state, OpString8& state_element_name, BOOL mirror_rtl, BOOL high_res)
{
	RETURN_IF_ERROR(state_element_name.Set(m_key.m_name));

	if (m_key.m_size)
		RETURN_IF_ERROR(state_element_name.Append(s_skinsize_names[m_key.m_size], s_skinsize_sizes[m_key.m_size]));

	if (m_key.m_type)
		RETURN_IF_ERROR(state_element_name.Append(s_skintype_names[m_key.m_type], s_skintype_sizes[m_key.m_type]));

	if (state & SKINSTATE_PRESSED)
		RETURN_IF_ERROR(state_element_name.Append(".pressed", 8));

	if (state & SKINSTATE_SELECTED)
		RETURN_IF_ERROR(state_element_name.Append(".selected", 9));

	if (state & SKINSTATE_OPEN)
		RETURN_IF_ERROR(state_element_name.Append(".open", 5));

	if (state & SKINSTATE_HOVER)
		RETURN_IF_ERROR(state_element_name.Append(".hover", 6));

	if (state & SKINSTATE_ATTENTION)
		RETURN_IF_ERROR(state_element_name.Append(".attention", 10));

	if (state & SKINSTATE_DISABLED)
		RETURN_IF_ERROR(state_element_name.Append(".disabled", 9));

	if (state & SKINSTATE_FOCUSED)
		RETURN_IF_ERROR(state_element_name.Append(".focused", 8));

	if (state & SKINSTATE_MINI)
		RETURN_IF_ERROR(state_element_name.Append(".mini", 5));

	if (state & SKINSTATE_INDETERMINATE)
		RETURN_IF_ERROR(state_element_name.Append(".indeterminate", 14));

	if ((state & SKINSTATE_RTL) && !mirror_rtl)
		RETURN_IF_ERROR(state_element_name.Append(".rtl", 4));

#ifdef PIXEL_SCALE_RENDERING_SUPPORT
	if ((state & SKINSTATE_HIGHRES) && !high_res)
		RETURN_IF_ERROR(state_element_name.Append(".hires", 6));
#endif // PIXEL_SCALE_RENDERING_SUPPORT

	StateElement* first_elm = m_state_elements.GetCount() ? m_state_elements.Get(0) : NULL;
	StateElement* state_elm = StateElement::Create(m_skin, state_element_name.CStr(), state, first_elm, NULL, this, mirror_rtl, high_res);

#ifdef PIXEL_SCALE_RENDERING_SUPPORT
	// like the rtl state(see below), automatically create high-res-state
	// stateelement if not explicitly defined in the ini file.
	if (!high_res && !state_elm && (state & SKINSTATE_HIGHRES))
		return LoadStateElement(state, state_element_name, mirror_rtl, TRUE);
#endif // PIXEL_SCALE_RENDERING_SUPPORT

	if (!mirror_rtl && !state_elm && (state & SKINSTATE_RTL) && m_mirror_rtl)
		// This state combination contains RTL and there's no explicit
		// definition for it.  But we were told to mirror the image for RTL
		// (m_mirror_rtl == TRUE), so load the closest matching state and
		// mirror it.
		return LoadStateElement(state, state_element_name, TRUE, high_res);

	if (!state_elm)
		return OpStatus::ERR;

	if (state_elm->m_text_bold != -1 && state_elm->m_text_bold != 0)
	{
		m_has_bold_state = TRUE;
	}

	if (state & SKINSTATE_HOVER)
	{
		m_has_hover_state = TRUE;
	}
	state_elm->m_parent_skinelement = this;

	if (state == 0)
	{
#ifdef SKIN_ELEMENT_FALLBACK_DISABLE
		// only get the fallback version from the default state
		m_fallback_version = state_elm->m_fallback_version;
#endif // SKIN_ELEMENT_FALLBACK_DISABLE
		m_mirror_rtl = state_elm->m_mirror_rtl;
	}

	OP_STATUS status = InsertStateElement(state_elm);
	if (OpStatus::IsError(status))
	{
		OP_DELETE(state_elm);
		return status;
	}

	return OpStatus::OK;
}

OP_STATUS OpSkinElement::InsertStateElement(StateElement* state_elm)
{
	// Insert the state element into m_state_elements in order
	UINT32 i;
	for (i = 0; i < m_state_elements.GetCount(); i++)
	{
		StateElement* elm = m_state_elements.Get(i);
		if (state_elm->m_state < elm->m_state)
			break;
	}
	return m_state_elements.Insert(i, state_elm);
}

/***********************************************************************************
**
**	OnAnimatedImageChanged() - called on animated skin images when a new frame is available
**
***********************************************************************************/
#ifdef ANIMATED_SKIN_SUPPORT

void OpSkinElement::OnAnimatedImageChanged()
{
	for (OpListenersIterator iterator(m_animation_listeners); m_animation_listeners.HasNext(iterator);)
	{
		m_animation_listeners.GetNext(iterator)->OnAnimatedImageChanged();
	}
}

void OpSkinElement::SetAnimationListener(OpSkinAnimationListener *listener)
{
	OpStatus::Ignore(m_animation_listeners.Add(listener));
}

void OpSkinElement::RemoveAnimationListener(OpSkinAnimationListener *listener)
{
	OpStatus::Ignore(m_animation_listeners.Remove(listener));
}

#endif // ANIMATED_SKIN_SUPPORT

/***********************************************************************************
**
**	StateElement::Create
**
***********************************************************************************/

/*static*/ OpSkinElement::StateElement*	OpSkinElement::StateElement::Create(OpSkin* skin, const char* name, INT32 state, StateElement* first_element, OpAutoVector<OpString8>* parentlist, OpSkinElement* skinElement, BOOL mirror_rtl, BOOL high_res)
{
	if (!name)
		return NULL;

	//first check if the element is linked to itself
	if(parentlist)
	{
		for(unsigned int i=0; i < parentlist->GetCount(); i++)
		{
			if(op_strcmp(name, parentlist->Get(i)->CStr()) == 0)
				return NULL;
		}
	}

	PrefsFile* ini_file = skin->GetPrefsFile();
	// IsSection and IsKey are slow functions that does linear searches
	BOOL is_section = ini_file->IsSection(name);
	BOOL is_image = !is_section && ini_file->IsKey("Images", name);
	BOOL is_box = !is_section && !is_image &&
		ini_file->IsKey("Boxes", name);

	if (!is_section && !is_image && !is_box)
		return NULL;

	StateElement* OP_MEMORY_VAR state_element = OP_NEW(StateElement, ());
	if (!state_element)
		return NULL;

	const size_t name_len = op_strlen(name);
	if(name_len > 18 &&
	   op_strncmp(name, "Scrollbar", 9) == 0 &&
	   op_strcmp(name + name_len - 9, "Knob Skin") == 0)
		state_element->m_drop_if_larger = TRUE;
	else
		state_element->m_drop_if_larger = FALSE;

	//copy the vector from the parent element
	if(parentlist)
	{
		for(unsigned int i=0; i < parentlist->GetCount(); i++)
		{
			OpString8* newstring = OP_NEW(OpString8, ());
			if(!newstring)
				break;

			newstring->Set(parentlist->Get(i)->CStr());
			state_element->m_parentlist.Add(newstring);
		}
	}

	BOOL is_v_scrollbar = !op_strcmp(name, "Scrollbar Vertical Skin");
	BOOL is_h_scrollbar = !is_v_scrollbar && !op_strcmp(name, "Scrollbar Horizontal Skin");
	BOOL is_slider_track = !op_strcmp(name, "Slider Vertical Track") ||
							!op_strcmp(name, "Slider Horizontal Track");

	state_element->m_type				= is_box ? StateElement::TypeBox : StateElement::TypeImage;
	if (is_slider_track)
	{
		// Give the slider a different default size. We need this to be backwards compatible with
		// skins that doesn't specify it.
		state_element->m_width				= 4;
		state_element->m_height				= 4;
	}
	else
	{
		state_element->m_width				= is_v_scrollbar ? g_op_ui_info->GetVerticalScrollbarWidth() : 16;
		state_element->m_height				= is_h_scrollbar ? g_op_ui_info->GetHorizontalScrollbarHeight() : 16;
	}
	state_element->m_minwidth			= 8;
	state_element->m_minheight			= 8;
	state_element->m_margin_left		= first_element ? first_element->m_margin_left : SKIN_DEFAULT_MARGIN;
	state_element->m_margin_top			= first_element ? first_element->m_margin_top : SKIN_DEFAULT_MARGIN;
	state_element->m_margin_right		= first_element ? first_element->m_margin_right : SKIN_DEFAULT_MARGIN;
	state_element->m_margin_bottom		= first_element ? first_element->m_margin_bottom : SKIN_DEFAULT_MARGIN;
	state_element->m_padding_left		= first_element ? first_element->m_padding_left : SKIN_DEFAULT_PADDING;
	state_element->m_padding_top		= first_element ? first_element->m_padding_top : SKIN_DEFAULT_PADDING;
	state_element->m_padding_right		= first_element ? first_element->m_padding_right : SKIN_DEFAULT_PADDING;
	state_element->m_padding_bottom		= first_element ? first_element->m_padding_bottom : SKIN_DEFAULT_PADDING;
	state_element->m_spacing			= first_element ? first_element->m_spacing : SKIN_DEFAULT_SPACING;
	state_element->m_color				= SKINCOLOR_NONE;
	state_element->m_text_color			= SKINCOLOR_NONE;
	state_element->m_text_link_color	= SKINCOLOR_NONE;
	state_element->m_text_bold			= -1;
	state_element->m_text_underline		= -1;
	state_element->m_text_zoom			= -1;
	state_element->m_text_button_padding = -1;
	state_element->m_blend				= 100;
	state_element->m_clear_background	= FALSE;
	state_element->m_colorize			= FALSE;
	for(int i = 0; i < 4; i++)
		state_element->m_border[i]				= 0;
	state_element->m_border_color		= OP_RGB(0, 0, 0);
	state_element->m_border_style		= CSS_VALUE_solid;
	for(int cut = 0; cut < 4; cut++)
		state_element->m_stretch_border[cut]		= first_element ? first_element->m_stretch_border[cut] : 2;
	for(int r = 0; r < 4; r++)
		state_element->m_radius[r]		= first_element ? first_element->m_radius[r] : 0;
	for(int i = 0; i < 4; i++)
	{
		state_element->m_colorized_background_padding[i] = 0;
		state_element->m_colorized_background_radius[i] = 0;
	}
	state_element->m_parent_skinelement = NULL;
	state_element->m_state				= state;
	state_element->m_child_index		= static_cast<UINT8>(-1);
#ifdef SKIN_ELEMENT_FALLBACK_DISABLE
	state_element->m_fallback_version	= 0;
#endif // SKIN_ELEMENT_FALLBACK_DISABLE
	state_element->m_mirror_rtl			= mirror_rtl;
#ifdef PIXEL_SCALE_RENDERING_SUPPORT
	op_memset(state_element->m_tilewidth, 0, sizeof(state_element->m_tilewidth));
	op_memset(state_element->m_tileheight, 0, sizeof(state_element->m_tileheight));
#endif // PIXEL_SCALE_RENDERING_SUPPORT

#ifdef SKINNABLE_AREA_ELEMENT
	state_element->m_outline_color	    = g_op_ui_info->GetSystemColor(OP_SYSTEM_COLOR_BACKGROUND_SELECTED);
	state_element->m_outline_width	    = 2;
#endif

#ifdef USE_GRADIENT_SKIN
	state_element->m_min_color				= SKINCOLOR_NONE;
	state_element->m_min_border_color		= SKINCOLOR_NONE;
	state_element->m_bottom_border_color	= SKINCOLOR_NONE;
#endif // USE_GRADIENT_SKIN

#ifdef ANIMATED_SKIN_SUPPORT
	for(INT32 i = 0; i < SKINPART_TOTAL; i++)
	{
		state_element->m_animation_handlers[i] = NULL;
	}
#endif

//	op_memset(state_element->m_images, 0, SKINPART_TOTAL * sizeof(Image));
	if (skinElement)
		skinElement->OverrideDefaults(state_element);

	if (is_section)
	{
		TRAPD(rc, state_element->LoadBySectionL(skin, name, skinElement, high_res));
		OP_ASSERT(OpStatus::IsSuccess(rc));
		if(OpStatus::IsError(rc))
		{
			OP_DELETE(state_element);
			state_element = NULL;
		}
	}
	else
	{
		TRAPD(rc, state_element->LoadByKeyL(skin, name, skinElement, high_res));
		OP_ASSERT(OpStatus::IsSuccess(rc));
		if(OpStatus::IsError(rc))
		{
			OP_DELETE(state_element);
			state_element = NULL;
		}
	}
#ifdef PERSONA_SKIN_SUPPORT
	if (skinElement && state_element)
		skinElement->OverridePersonaDefaults(state_element);
#endif // PERSONA_SKIN_SUPPORT

	return state_element;
}

/***********************************************************************************
**
**	StateElement::LoadByKey
**
***********************************************************************************/
#ifdef PIXEL_SCALE_RENDERING_SUPPORT
static void GetFilenameWithSuffixL(OpString& filename, const uni_char* src)
{
	filename.Empty();

	const uni_char* suffix = HIGH_RESOLUTION_IMAGE_SUFFIX;
	int suffix_len = uni_strlen(suffix);
	if (!suffix_len)
		return;

	// append the suffix to the filename
	UINT extlen = 0;
	const uni_char* extpos = FindFileExtension(src, &extlen);

	if (extpos)
	{
		filename.SetL(src, extpos - src - 1);
		filename.AppendL(suffix);
		filename.AppendL(extpos - 1);
	}
	else
	{
		filename.SetL(src);
		filename.AppendL(suffix);
	}
}
#endif // PIXEL_SCALE_RENDERING_SUPPORT

void OpSkinElement::StateElement::LoadByKeyL(OpSkin* skin, const char* name, OpSkinElement* skinElement, BOOL hires_suffix)
{
	PrefsFile* ini_file = skin->GetPrefsFile();

	OpStringC value(ini_file->ReadStringL("Boxes", name));

	if (!value)
		value = ini_file->ReadStringL("Images", name);
	if (!value)
		return;

	OpLineParser line(value);

	OpString filename;

	// GetNextValue takes an INT32& so these must be INT32, even though we use them as BOOLs:
	INT32 scalable = TRUE;
	INT32 colorize = m_colorize;

	line.GetNextToken(filename);
	if (filename == UNI_L("mirror"))
	{
		m_mirror_rtl = TRUE;
		OpString8 source;
		line.GetNextToken8(source);
		LoadByKeyL(skin, source.CStr(), skinElement);
		return;
	}

	line.GetNextValue(colorize);
	m_colorize = !!colorize;
	line.GetNextValue(m_margin_left);
	line.GetNextValue(m_margin_top);
	line.GetNextValue(m_margin_right);
	line.GetNextValue(m_margin_bottom);
	line.GetNextValue(m_padding_left);
	line.GetNextValue(m_padding_top);
	line.GetNextValue(m_padding_right);
	line.GetNextValue(m_padding_bottom);
	line.GetNextValue(scalable);
	m_minwidth = ini_file->ReadIntL(name, "MinWidth", m_minwidth);
	m_minheight = ini_file->ReadIntL(name, "MinHeight", m_minheight);

#ifdef PIXEL_SCALE_RENDERING_SUPPORT
	// try to load the bitmap with regular name
	int step = 1;
	OpString fname;
	fname.SetL(filename);

	// append the suffix to the filename
	// hires_suffix==TRUE means the high-res state is not explicitly
	// presents in the ini file, try to find and load high-res bitmap
	// with the specified suffix.
	OpString strfilename;
	if (hires_suffix)
	{
		OpString img_filename;
		GetFilenameWithSuffixL(img_filename, fname.CStr());
		filename.SetL(img_filename);

		// try filename with suffix first, and fallback to regular one if fail
		++step;
	}

	while (step-- > 0) {
#endif // PIXEL_SCALE_RENDERING_SUPPORT

	m_images[0] = skin->GetBitmap(filename.CStr(), m_colorize, !!scalable, this);

#ifdef ASYNC_IMAGE_DECODERS
	if (!m_images[0].IsEmpty())
#else
	if (m_images[0].ImageDecoded())
#endif
	{
		m_width			= m_images[0].Width();
		m_height		= m_images[0].Height();

		OnPortionDecoded();

#ifdef PIXEL_SCALE_RENDERING_SUPPORT
		// use the image dimension as tile size
		m_tilewidth[0] = m_images[0].Width();
		m_tileheight[0] = m_images[0].Height();
		break;
	}
	else
	{
		filename.SetL(fname);
	}
#endif // PIXEL_SCALE_RENDERING_SUPPORT
	}

#ifdef PIXEL_SCALE_RENDERING_SUPPORT
	if (m_state & SKINSTATE_HIGHRES)
	{
		if (!hires_suffix)
		{
			// This fit the scenario 3 of high-res state(see comments for SKINSTATE_HIGHRES)
			// High res state without hires_suffix means this is a explicit
			// high resolution bitmap, but without further information, we
			// could only assume the device pixel scale is 200.
			m_tilewidth[0] = m_tilewidth[0] / 2;
			m_tileheight[0] = m_tileheight[0] / 2;
		}
		else if (step && skinElement)
		{
			// high res skin bitmap loaded, fit scenario 2 of high-res
			// state, see comments for SKINSTATE_HIGHRES. try to find
			// the regular state element, and take its bitmap
			// dimension as the tile size.
			INT32 state = m_state & ~SKINSTATE_HIGHRES;
			// Do not load non-existing state element, as LoadStateElement does not support re-entry.
			OpSkinElement::StateElement* regular_state = skinElement->GetStateElement(state, TRUE, FALSE);
			if (!regular_state)
				regular_state = skinElement->m_state_elements.GetCount() ?
									skinElement->m_state_elements.Get(0) : NULL;

			if (regular_state)
			{
				Image imgNormal = regular_state->m_images[0];
#ifdef ASYNC_IMAGE_DECODERS
				if (!imgNormal.IsEmpty())
#else
				if (imgNormal.ImageDecoded())
#endif
				{
					m_tilewidth[0] = imgNormal.Width();
					m_tileheight[0] = imgNormal.Height();
				}
			}
		}
	}
#endif // PIXEL_SCALE_RENDERING_SUPPORT
}

/***********************************************************************************
**
**	StateElement::LoadBySection
**
***********************************************************************************/

void OpSkinElement::StateElement::LoadBySectionL(OpSkin* skin, const char* name, OpSkinElement* skinElement, BOOL hires_suffix)
{
	PrefsFile* ini_file = skin->GetPrefsFile();

	// Enable colorization for section by default
	m_colorize = TRUE;

	const uni_char* clone = ini_file->ReadStringL(name, "Clone");

	if (clone && *clone && !uni_str_eq(clone,name))
	{
		OpString8 clone8;
		clone8.Set(clone);
		LoadBySectionL(skin, clone8, skinElement, hires_suffix);
	}

	if (m_state == 0)
		m_mirror_rtl = ini_file->ReadIntL(name, "Mirror RTL",  m_mirror_rtl) != 0;

	const uni_char* type = ini_file->ReadStringL(name, "Type");

	if (type && uni_stricmp(type, "Box") == 0)
	{
		m_type = TypeBox;
	}
	else if (type && uni_stricmp(type, "BoxTile") == 0)
	{
		m_type = TypeBoxTile;
	}
	else if (type && uni_stricmp(type, "BoxTileVertical") == 0)
	{
		m_type = TypeBoxTileVertical;
	}
	else if (type && uni_stricmp(type, "BoxTileHorizontal") == 0)
	{
		m_type = TypeBoxTileHorizontal;
	}
	else if (type && uni_stricmp(type, "BoxStretch") == 0)
	{
		m_type = TypeBoxStretch;
	}
	else if (type && uni_stricmp(type, "BestFit") == 0)
	{
		m_type = TypeBestFit;
	}
	else if (type)
	{
		m_type = TypeImage;
	}

	UINT8 shorthand[4];
	if (skin->ReadTopRightBottomLeftL(name, "Margin", shorthand))
	{
		m_margin_top = shorthand[0];
		m_margin_right = shorthand[1];
		m_margin_bottom = shorthand[2];
		m_margin_left = shorthand[3];
	}
	if (skin->ReadTopRightBottomLeftL(name, "Padding", shorthand))
	{
		m_padding_top = shorthand[0];
		m_padding_right = shorthand[1];
		m_padding_bottom = shorthand[2];
		m_padding_left = shorthand[3];
	}

	m_colorize			= ini_file->ReadBoolL(name, "Colorize",	m_colorize);
	m_width				= ini_file->ReadIntL(name, "Width",			m_width);
	m_height			= ini_file->ReadIntL(name, "Height",		m_height);
	m_minwidth			= ini_file->ReadIntL(name, "MinWidth",		m_minwidth);
	m_minheight			= ini_file->ReadIntL(name, "MinHeight",		m_minheight);
	m_margin_left		= ini_file->ReadIntL(name, "Margin Left",	m_margin_left);
	m_margin_top		= ini_file->ReadIntL(name, "Margin Top",	m_margin_top);
	m_margin_right		= ini_file->ReadIntL(name, "Margin Right",	m_margin_right);
	m_margin_bottom		= ini_file->ReadIntL(name, "Margin Bottom",	m_margin_bottom);
	m_padding_left		= ini_file->ReadIntL(name, "Padding Left",	m_padding_left);
	m_padding_top		= ini_file->ReadIntL(name, "Padding Top",	m_padding_top);
	m_padding_right		= ini_file->ReadIntL(name, "Padding Right",	m_padding_right);
	m_padding_bottom	= ini_file->ReadIntL(name, "Padding Bottom",m_padding_bottom);
	m_spacing			= ini_file->ReadIntL(name, "Spacing",		m_spacing);
	m_text_bold			= ini_file->ReadIntL(name, "Text Bold",		m_text_bold);
	m_text_underline	= ini_file->ReadIntL(name, "Text Underline",m_text_underline);
	m_text_zoom			= ini_file->ReadIntL(name, "Text Zoom",		m_text_zoom);
	m_blend				= ini_file->ReadIntL(name, "Blend",			m_blend);
	m_clear_background	= ini_file->ReadBoolL(name, "ClearBackground",	m_clear_background);
	m_color				= skin->ReadColorL(name, "Color",				m_color, m_colorize);
	m_text_color		= skin->ReadColorL(name, "Text Color",		m_text_color, m_colorize);
	m_text_link_color	= skin->ReadColorL(name, "Link Color",		m_text_link_color, m_colorize);
	skin->ReadTopRightBottomLeftL(name, "Border",	m_border);
	m_border_color		= skin->ReadColorL(name, "Border Color",		m_border_color, m_colorize);
	m_border_style		= skin->ReadBorderStyleL(name, "Border Style",m_border_style);
	skin->ReadTopRightBottomLeftL(name, "StretchBorder",	m_stretch_border);
	skin->ReadTopRightBottomLeftL(name, "Radius",	m_radius);
	skin->ReadTopRightBottomLeftL(name, "ColorBackgroundPadding",	m_colorized_background_padding);
	skin->ReadTopRightBottomLeftL(name, "ColorBackgroundRadius",	m_colorized_background_radius);
	if ((m_state & SKINSTATE_RTL) && m_mirror_rtl)
	{
		op_swap(m_stretch_border[1], m_stretch_border[3]);
		op_swap(m_radius[0], m_radius[1]);
		op_swap(m_radius[2], m_radius[3]);
		op_swap(m_colorized_background_padding[1], m_colorized_background_padding[3]);
		op_swap(m_colorized_background_radius[1], m_colorized_background_radius[3]);
	}

	skin->ReadTextShadowL(name, "Text Shadow", &m_text_shadow);
	m_text_shadow.color = skin->ReadColorL(name, "Text Shadow Color",m_text_shadow.color, m_colorize);

#ifdef SKINNABLE_AREA_ELEMENT
	m_outline_color		= skin->ReadColorL(name, "Outline Color",   m_outline_color, m_colorize);
	m_outline_width		= ini_file->ReadIntL(name, "Outline Width",   m_outline_width);
#endif

#ifdef USE_GRADIENT_SKIN
	m_min_color				= skin->ReadColorL(name, "Min Color",			m_min_color, m_colorize);
	m_min_border_color		= skin->ReadColorL(name, "Min Border Color",		m_min_border_color, m_colorize);
	m_bottom_border_color	= skin->ReadColorL(name, "Bottom Border Color",	m_bottom_border_color, m_colorize);
#endif // USE_GRADIENT_SKIN

#ifdef SKIN_ELEMENT_FALLBACK_DISABLE
	m_fallback_version	= ini_file->ReadIntL(name, "Fallback Version",  0);
#endif // SKIN_ELEMENT_FALLBACK_DISABLE

	// get all bitmaps

	BOOL scalable = ini_file->ReadBoolL(name, "Scalable");

#ifdef ASYNC_IMAGE_DECODERS
	BOOL call_onportiondecoded = FALSE;
#endif

	INT32 i;
	for (i = 0; i < SKINPART_TOTAL; i++)
	{
		const uni_char* filename = ini_file->ReadStringL(name, s_skinpart_names[i]);
		if (i == SKINPART_TILE_CENTER && !filename)
		{
			// special alias for "Tile Center" is "Background" to simplify it a bit
			filename = ini_file->ReadStringL(name, "Background");
		}

#ifdef PIXEL_SCALE_RENDERING_SUPPORT
		// append the suffix to the filename, see comments in LoadByKeyL
		int step = 1;
		OpString strfilename;
		const uni_char* fname = filename;
		if (hires_suffix)
		{
			const uni_char* fname = filename;
			GetFilenameWithSuffixL(strfilename, fname);
			filename = strfilename.CStr();
			++step;
		}

		while (step-- > 0) {
#endif // PIXEL_SCALE_RENDERING_SUPPORT

		Image image = skin->GetBitmap(filename, m_colorize, scalable, this);

#ifdef ASYNC_IMAGE_DECODERS
		if (!image.IsEmpty())
#else
		if (image.ImageDecoded())
#endif
		{
			m_images[i] = image;
#ifdef ASYNC_IMAGE_DECODERS
			if (image.ImageDecoded())
				call_onportiondecoded = TRUE;
			else
				call_onportiondecoded = FALSE;
#endif
#ifdef PIXEL_SCALE_RENDERING_SUPPORT
			m_tilewidth[i] = image.Width();
			m_tileheight[i] = image.Height();
			break;
		}
		else
		{
			filename = fname;
		}
#endif // PIXEL_SCALE_RENDERING_SUPPORT
		}

#ifdef PIXEL_SCALE_RENDERING_SUPPORT
		// see LoadByKeyL for detail comments
		if (m_state & SKINSTATE_HIGHRES)
		{
			if (!hires_suffix)
			{
				m_tilewidth[i] = m_tilewidth[i] / 2;
				m_tileheight[i] = m_tileheight[i] / 2;
			}
			else if (step && skinElement)
			{
				INT32 state = m_state & ~SKINSTATE_HIGHRES;

				// Do not load non-existing state element, as LoadStateElement does not support re-entry.
				OpSkinElement::StateElement* regular_state = skinElement->GetStateElement(state, TRUE, FALSE);
				if (!regular_state)
					regular_state = skinElement->m_state_elements.GetCount() ?
										skinElement->m_state_elements.Get(0) : NULL;

				if (regular_state)
				{
					Image imgNormal = regular_state->m_images[i];
#ifdef ASYNC_IMAGE_DECODERS
					if (!imgNormal.IsEmpty())
#else
					if (imgNormal.ImageDecoded())
#endif
					{
						m_tilewidth[i] = imgNormal.Width();
						m_tileheight[i] = imgNormal.Height();
					}
				}
			}
		}
#endif // PIXEL_SCALE_RENDERING_SUPPORT
	}

#ifdef ASYNC_IMAGE_DECODERS
	if (call_onportiondecoded)
		OnPortionDecoded();
#endif

	//add to vector that is used to prevent recursive linkage

	OpString8* thisname = OP_NEW(OpString8, ());
	if (!thisname)
		return;

	thisname->Set(name);
	m_parentlist.Add(thisname);

	// load any children

	for (i = 0;;i++)
	{
		char child_name[32]; // ARRAY OK 2009-04-24 wonko

		op_snprintf(child_name, 31, "Child%i", i);

		const uni_char* child_skin = ini_file->ReadStringL(name, child_name);

		if (!child_skin)
			break;

		// We may have got it from the Clone and then it's already in the list.
		// We should override that, so remove any child element with the same index in the list.
		for(UINT32 c = 0; c < m_child_elements.GetCount(); c++)
		{
			StateElement* child = static_cast<StateElement*>(m_child_elements.Get(c));
			if (child->m_child_index == i)
			{
				m_child_elements.Delete(c, 1);
				break;
			}
		}

		OpString8 child_skin8;
		child_skin8.Set(child_skin);

		StateElement* child_element = StateElement::Create(skin, child_skin8, 0, NULL, &m_parentlist,
														   skinElement, m_mirror_rtl, !!(m_state & SKINSTATE_HIGHRES));
		if (!child_element)
			break;

		child_element->m_child_index = i;
		m_child_elements.Add(child_element);
	}

	// load any overlays

	for (i = 0;;i++)
	{
		char overlay_name[32]; // ARRAY OK 2010-05-28 emil

		op_snprintf(overlay_name, 31, "Overlay%i", i);

		const uni_char* overlay_skin = ini_file->ReadStringL(name, overlay_name);

		if (!overlay_skin)
			break;

		// We may have got it from the Clone and then it's already in the list.
		// We should override that, so remove any overlay element with the same index in the list.
		for(UINT32 c = 0; c < m_overlay_elements.GetCount(); c++)
		{
			StateElement* overlay = static_cast<StateElement*>(m_overlay_elements.Get(c));
			if (overlay->m_child_index == i)
			{
				m_overlay_elements.Delete(c, 1);
				break;
			}
		}

		OpString8 overlay_skin8;
		overlay_skin8.Set(overlay_skin); // FIXME:OOM

		StateElement* overlay_element = StateElement::Create(skin, overlay_skin8, 0, NULL, &m_parentlist,
															 skinElement, m_mirror_rtl, !!(m_state & SKINSTATE_HIGHRES));

		if (!overlay_element)
			break;

		overlay_element->m_child_index = i;
		m_overlay_elements.Add(overlay_element);
	}

	m_parentlist.Delete(thisname);
}

#ifdef ANIMATED_SKIN_SUPPORT

Image OpSkinElement::StateElement::GetAnimatedImage(SkinPart part, VisualDevice *vd)
{
	Image return_image = m_images[part];

	if(return_image.IsAnimated())
	{
		UINT32 width = return_image.Width();
		UINT32 height = return_image.Height();

		OpSkinAnimationHandler *animation_handler = OpSkinAnimationHandler::GetImageAnimationHandler(this, vd, (SkinPart)part);

		// we need to restart the animation now
		if (animation_handler)
		{
			animation_handler->OnPortionDecoded(this);
			animation_handler->DecRef(this, vd, part);
		}
		OpBitmap* src_bitmap = m_images[part].GetBitmap(this);

		if (src_bitmap)
		{
			OpBitmap* scaled_bitmap = NULL;

			OpBitmap::Create(&scaled_bitmap, width, height);

			if (scaled_bitmap)
			{
				OpSkinUtils::CopyBitmap(src_bitmap, scaled_bitmap);
				return_image = imgManager->GetImage(scaled_bitmap);
				if (return_image.IsEmpty())	// OOM
					OP_DELETE(scaled_bitmap);
			}
			m_images[part].ReleaseBitmap();
		}
	}
	return return_image;
}
#endif // ANIMATED_SKIN_SUPPORT

#if defined(ANIMATED_SKIN_SUPPORT)
#define LOCAL_GET_PART_IMAGE(part) GetAnimatedImage(part, vd)
#else
#define LOCAL_GET_PART_IMAGE(part) m_images[part]
#endif // ANIMATED_SKIN_SUPPORT

#ifdef SKIN_OUTLINE_SUPPORT

/***********************************************************************************
**
**	StateElement::DrawSkinPart
**
***********************************************************************************/

void OpSkinElement::StateElement::DrawSkinPart(VisualDevice* vd, OpRect& rect, SkinPart part)
{
	INT32 effect = 0, effect_value = 0;
	Image img = LOCAL_GET_PART_IMAGE(part);

#ifdef PIXEL_SCALE_RENDERING_SUPPORT
	int tilewidth = m_tilewidth[part];
	int tileheight = m_tileheight[part];
#else
	int tilewidth = img.Width();
	int tileheight = img.Height();
#endif // PIXEL_SCALE_RENDERING_SUPPORT

	if (part >= SKINPART_CORNER_TOPLEFT && part <= SKINPART_CORNER_BOTTOMLEFT)
	{
		Blit(vd, img, rect.x, rect.y, tilewidth, tileheight, effect, effect_value);
	}
	else
	{
		BlitTiled(vd, img, rect, 0, 0, effect, effect_value, NULL, tilewidth, tileheight);
	}
}

#endif // SKIN_OUTLINE_SUPPORT

/***********************************************************************************
**
**	StateElement::Draw
**
***********************************************************************************/

void OpSkinElement::StateElement::Draw(VisualDevice* vd, OpRect& rect, INT32 effect, INT32 effect_value, const OpRect* clip_rect, BOOL drop_if_larger)
{
	if (rect.IsEmpty())
		return;

#ifdef PIXEL_SCALE_RENDERING_SUPPORT
	PixelScaler scaler((m_state & SKINSTATE_HIGHRES) ? vd->GetVPScale() : 100);
#endif // PIXEL_SCALE_RENDERING_SUPPORT

	if (clip_rect)
		vd->BeginClipping(*clip_rect);

#ifdef VEGA_OPPAINTER_SUPPORT
	if (m_color != SKINCOLOR_NONE)
	{
		Border border;
		SetRadiusOnBorder(border);

		// reset the alpha (which is incorrect due to INT32 vs UINT32)
		UINT32 fill_color = (m_color & OP_RGBA(0, 0, 0, 255)) | GetColor(m_color);

		vd->SetBgColor(fill_color);
		vd->DrawBgColorWithRadius(rect, &border);
	}
#endif // VEGA_OPPAINTER_SUPPORT

	if (m_type == StateElement::TypeBoxStretch)
	{
		Image image = LOCAL_GET_PART_IMAGE(SKINPART_TILE_CENTER);

#ifdef VEGA_OPPAINTER_SUPPORT
		VisualDevice::ImageEffect vdeffect;
		vdeffect.effect = effect;
		vdeffect.effect_value = effect_value;
		vdeffect.image_listener = this;
		Border border;
		border.Reset();
		border.top.width = m_stretch_border[0];
		border.right.width = m_stretch_border[1];
		border.bottom.width = m_stretch_border[2];
		border.left.width = m_stretch_border[3];
		BorderImage border_image;
		border_image.Reset();
		border_image.cut[0] = m_stretch_border[0];
		border_image.cut[1] = m_stretch_border[1];
		border_image.cut[2] = m_stretch_border[2];
		border_image.cut[3] = m_stretch_border[3];

		if (image.ImageDecoded())
		{
#ifdef PIXEL_SCALE_RENDERING_SUPPORT
			// border_image.cut should be in device pixels, since they
			// are always positive integers(UINT8 m_stretch_border[4]).
			int tilewidth = m_tilewidth[SKINPART_TILE_CENTER];
			int tileheight = m_tileheight[SKINPART_TILE_CENTER];
			border_image.cut[0] = (border_image.cut[0] * image.Height() + tileheight - 1) / tileheight;
			border_image.cut[1] = (border_image.cut[1] * image.Width() + tilewidth - 1) / tilewidth;
			border_image.cut[2] = (border_image.cut[2] * image.Height() + tileheight - 1) / tileheight;
			border_image.cut[3] = (border_image.cut[3] * image.Width() + tilewidth - 1) / tilewidth;
#endif // PIXEL_SCALE_RENDERING_SUPPORT

			vd->PaintBorderImage(image, this, rect, &border, &border_image, &vdeffect);
		}
#else
		int vw = rect.width;
		int vh = rect.height;
		int bw = image.Width();
		int bh = image.Height();
		int top = m_stretch_border[0];
		int right = m_stretch_border[1];
		int bottom = m_stretch_border[2];
		int left = m_stretch_border[3];

#ifdef PIXEL_SCALE_RENDERING_SUPPORT
		top = (top * image.Height() + tileheight - 1) / tileheight;
		right = (right * image.Width() + tilewidth - 1) / tilewidth;
		bottom = (bottom * image.Height() + tileheight - 1) / tileheight;
		left = (left * image.Width() + tilewidth - 1) / tilewidth;
#endif // PIXEL_SCALE_RENDERING_SUPPORT

		// Corners - always drawn
		{
			// top-left corner
			BlitClip(vd, image,
					 OpRect(0, 0, left, top),
					 OpRect(rect.x,
							rect.y+0,
							m_stretch_border[3],
							m_stretch_border[0]),
					 effect, effect_value);
			// top-right corner
			BlitClip(vd, image,
					 OpRect(bw-right, 0, right, top),
					 OpRect(rect.x+vw-m_stretch_border[1],
							rect.y,
							m_stretch_border[1],
							m_stretch_border[0]),
					 effect, effect_value);
			// bottom-left corner
			BlitClip(vd, image,
					 OpRect(0, bh-bottom, left, bottom),
					 OpRect(rect.x+0,
							rect.y+vh-m_stretch_border[2],
							m_stretch_border[3],
							m_stretch_border[2]),
					 effect, effect_value);
			// bottom-right corner
			BlitClip(vd, image,
					 OpRect(bw-right, bh-bottom, right, bottom),
					 OpRect(rect.x+vw-m_stretch_border[1],
							rect.y+vh-m_stretch_border[2],
							m_stretch_border[1],
							m_stretch_border[2]),
					 effect, effect_value);
		}

		// edges - only drawn if width/height of element is greater than sum of sides

		// Left & right edge
		if (vh > m_stretch_border[0] + m_stretch_border[2])
		{
			// left edge
			BlitStretch(vd, image,
						OpRect(rect.x,
							   rect.y+m_stretch_border[0],
							   m_stretch_border[3],
							   vh - m_stretch_border[0] - m_stretch_border[2]),
						OpRect(0, top, left, bh-top-bottom),
						effect, effect_value);
			// right edge
			BlitStretch(vd, image,
						OpRect(rect.x+vw-m_stretch_border[1],
							   rect.y+m_stretch_border[0],
							   m_stretch_border[1],
							   vh - m_stretch_border[0] - m_stretch_border[2]),
						OpRect(bw-right, top, right, bh-top-bottom),
						effect, effect_value);
		}

		// Top & bottom edge
		if (vw > m_stretch_border[1] + m_stretch_border[3])
		{
			// top edge
			BlitStretch(vd, image,
						OpRect(rect.x+m_stretch_border[3],
							   rect.y,
							   vw - m_stretch_border[1] - m_stretch_border[3],
							   m_stretch_border[0]),
						OpRect(left, 0, bw-left-right, top),
						effect, effect_value);
			// bottom edge
			BlitStretch(vd, image,
						OpRect(rect.x+m_stretch_border[3],
							   rect.y+vh-m_stretch_border[2],
							   vw - m_stretch_border[1] - m_stretch_border[3],
							   m_stretch_border[2]),
						OpRect(left, bh-bottom, bw-left-right, bottom),
						effect, effect_value);
		}

		// Center - only drawn if both width and height are bigger than sum of corresponding sides
		if (vw > m_stretch_border[1] + m_stretch_border[3] && vh > m_stretch_border[0] + m_stretch_border[2])
			BlitStretch(vd, image,
						OpRect(rect.x+m_stretch_border[3],
							   rect.y+m_stretch_border[0],
							   vw - m_stretch_border[1] - m_stretch_border[3],
							   vh - m_stretch_border[0] - m_stretch_border[2]),
						OpRect(left, top, bw-left-right, bh-top-bottom),
						effect, effect_value);
#endif
	}
	else if (m_type == StateElement::TypeBestFit)
	{
		Image image = LOCAL_GET_PART_IMAGE(SKINPART_TILE_CENTER);
		if (image.ImageDecoded())
		{
			UINT32 img_w = image.Width();
			UINT32 img_h = image.Height();

			//  I have assumed the following values are non-negative; otherwise,
			//  the following calculations could be totally screwy, and they
			//  should be rewritten to account for negative values.
			OP_ASSERT(/* img_w >= 0 && img_h >= 0 && */
					  rect.width >= 0 && rect.height >= 0);

			OpRect src_rect(0, 0, img_w, img_h);

			if (img_w * rect.height > img_h * rect.width)
			{
				//  image is too wide, so we crop left & right
				src_rect.width = (img_h * rect.width) / rect.height; // this is fine, because rect.height CANNOT be zero
				src_rect.x = (img_w - src_rect.width) / 2;
			}
			else if (img_w * rect.height < img_h * rect.width)
			{
				//  image is too tall, so we crop top * bottom
				src_rect.height = (img_w * rect.height) / rect.width; // this is fine, because rect.width CANNOT be zero
				src_rect.y = (img_h - src_rect.height) / 2;
			}

			if (src_rect.width > 0 && src_rect.height > 0)
				vd->ImageOutEffect(image, src_rect, rect, effect, effect_value, this);
		}
	}
	else if (m_type != StateElement::TypeImage)
	{
#ifdef PIXEL_SCALE_RENDERING_SUPPORT
		// BoxTilexxx handling strategy
		// 1. transform the dest rect to device pixels, calculate the left/top/right/bottom
		//    based on the bitmap width and height(which is also in device pixels). hopefully
		//    there will be no need to shrink the left/top/right/bottom, since pixel scale is
		//    mostly greater than 100, if the button rect is big enough, and skin is well designed.
		// 2. transform the dest rects of all 9 parts back to its origin coordinates, and paint.
		//
		// the benefit is all corners will be drawn without scaling, even no high resolution skin
		// bitmap is provided.
		OpRect rect_origin = rect;
		rect = TO_DEVICE_PIXEL(scaler, rect);
#endif // PIXEL_SCALE_RENDERING_SUPPORT

		INT32 left		= m_images[SKINPART_TILE_LEFT].ImageDecoded() ? m_images[SKINPART_TILE_LEFT].Width() : 0;
		INT32 top		= m_images[SKINPART_TILE_TOP].ImageDecoded() ? m_images[SKINPART_TILE_TOP].Height() : 0;
		INT32 right		= m_images[SKINPART_TILE_RIGHT].ImageDecoded() ? m_images[SKINPART_TILE_RIGHT].Width() : 0;
		INT32 bottom	= m_images[SKINPART_TILE_BOTTOM].ImageDecoded() ? m_images[SKINPART_TILE_BOTTOM].Height() : 0;

		if (!left && m_images[SKINPART_CORNER_TOPLEFT].ImageDecoded())
		{
			left = m_images[SKINPART_CORNER_TOPLEFT].Width();
		}

		if (!left && m_images[SKINPART_CORNER_BOTTOMLEFT].ImageDecoded())
		{
			left = m_images[SKINPART_CORNER_BOTTOMLEFT].Width();
		}

		if (!top && m_images[SKINPART_CORNER_TOPLEFT].ImageDecoded())
		{
			top = m_images[SKINPART_CORNER_TOPLEFT].Height();
		}

		if (!top && m_images[SKINPART_CORNER_TOPRIGHT].ImageDecoded())
		{
			top = m_images[SKINPART_CORNER_TOPRIGHT].Height();
		}

		if (!right && m_images[SKINPART_CORNER_TOPRIGHT].ImageDecoded())
		{
			right = m_images[SKINPART_CORNER_TOPRIGHT].Width();
		}

		if (!right && m_images[SKINPART_CORNER_BOTTOMRIGHT].ImageDecoded())
		{
			right = m_images[SKINPART_CORNER_BOTTOMRIGHT].Width();
		}

		if (!bottom && m_images[SKINPART_CORNER_BOTTOMLEFT].ImageDecoded())
		{
			bottom = m_images[SKINPART_CORNER_BOTTOMLEFT].Height();
		}

		if (!bottom && m_images[SKINPART_CORNER_BOTTOMRIGHT].ImageDecoded())
		{
			bottom = m_images[SKINPART_CORNER_BOTTOMRIGHT].Height();
		}

		// Clip so we don't draw outside rect.

		int bitmap_right = right;
		int bitmap_bottom = bottom;

		int overflow = -(rect.width - left - right);
		if (overflow > 0 && (left - right))
		{
			int ratio_left = 1000 * left / (left + right);
			int ratio_right = 1000 * right / (left + right);
			left = MAX(0, left - (overflow * ratio_left) / 1000);
			right = MAX(0, right - (overflow * ratio_right) / 1000);
		}
		else if (overflow > 0)
		{
			left = rect.width / 2;
			right = rect.width - left;
		}
		overflow = -(rect.height - top - bottom);
		if (overflow > 0 && (top + bottom))
		{
			int ratio_top = 1000 * top / (top + bottom);
			int ratio_bottom = 1000 * bottom / (top + bottom);
			top = MAX(0, top - (overflow * ratio_top) / 1000);
			bottom = MAX(0, bottom - (overflow * ratio_bottom) / 1000);
		}
		else if (overflow > 0)
		{
			top = rect.height / 2;
			bottom = rect.height - top;
		}

#ifdef PIXEL_SCALE_RENDERING_SUPPORT
		// transform from device pixels back to origin
		rect = rect_origin;
		left = FROM_DEVICE_PIXEL(scaler, left);
		right = FROM_DEVICE_PIXEL(scaler, right);
		top = FROM_DEVICE_PIXEL(scaler, top);
		bottom = FROM_DEVICE_PIXEL(scaler, bottom);
		bitmap_right = FROM_DEVICE_PIXEL(scaler, bitmap_right);
		bitmap_bottom = FROM_DEVICE_PIXEL(scaler, bitmap_bottom);
#endif // PIXEL_SCALE_RENDERING_SUPPORT

		OpRect center(rect.x + left, rect.y + top, rect.width - left - right, rect.height - top - bottom);

		// Draw edges
		if (left > 0 && center.height > 0)
		{
			Image img = LOCAL_GET_PART_IMAGE(SKINPART_TILE_LEFT);
			int tilewidth = left;
			int tileheight = img.Height();

			if (m_type == StateElement::TypeBoxTile || m_type == StateElement::TypeBoxTileVertical)
			{
#ifdef PIXEL_SCALE_RENDERING_SUPPORT
				// when tiling performed, the only thing matters is
				// the dest rect, no need to scale the source
				// bitmap. if low-res bitmap is available, the tile
				// count will be twice.
				tileheight = FROM_DEVICE_PIXEL(scaler, tileheight);
#endif // PIXEL_SCALE_RENDERING_SUPPORT
				OpRect dst(rect.x, center.y, left, center.height);
				BlitTiled(vd, img, dst, 0, 0, effect, effect_value,
						  clip_rect, tilewidth, tileheight);
			}
			else
			{
#ifdef PIXEL_SCALE_RENDERING_SUPPORT
				// if no tiling performed, the tileheight/width must
				// the save as the regular state(as it is used to
				// calculate the dest rect later). if only low-res
				// bitmap is available, it will be scaled.
				tileheight = m_tileheight[SKINPART_TILE_LEFT];
#endif // PIXEL_SCALE_RENDERING_SUPPORT
				BlitCentered(vd, img, rect.x, center.y + center.height / 2, FALSE, TRUE,
							 tilewidth, tileheight, effect, effect_value, clip_rect);
			}
		}
		if (top > 0 && center.width > 0)
		{
			Image img = LOCAL_GET_PART_IMAGE(SKINPART_TILE_TOP);
			int tilewidth = img.Width();
			int tileheight = top;

			if (m_type == StateElement::TypeBoxTile || m_type == StateElement::TypeBoxTileHorizontal)
			{
#ifdef PIXEL_SCALE_RENDERING_SUPPORT
				tilewidth = FROM_DEVICE_PIXEL(scaler, tilewidth);
#endif // PIXEL_SCALE_RENDERING_SUPPORT
				OpRect dst(center.x, rect.y, center.width, top);
				BlitTiled(vd, img, dst, 0, 0, effect, effect_value,
						  clip_rect, tilewidth, tileheight);
			}
			else
			{
#ifdef PIXEL_SCALE_RENDERING_SUPPORT
				tilewidth = m_tilewidth[SKINPART_TILE_LEFT];
#endif // PIXEL_SCALE_RENDERING_SUPPORT
				BlitCentered(vd, img, center.x + center.width / 2, rect.y, TRUE, FALSE,
							 tilewidth, tileheight, effect, effect_value, clip_rect);
			}
		}
		if (right > 0 && center.height > 0)
		{
			Image img = LOCAL_GET_PART_IMAGE(SKINPART_TILE_RIGHT);
			int tilewidth = right;
			int tileheight = img.Height();

			if (m_type == StateElement::TypeBoxTile || m_type == StateElement::TypeBoxTileVertical)
			{
#ifdef PIXEL_SCALE_RENDERING_SUPPORT
				tileheight = FROM_DEVICE_PIXEL(scaler, tileheight);
#endif // PIXEL_SCALE_RENDERING_SUPPORT
				OpRect dst(rect.x + rect.width - right, center.y, right, center.height);
				BlitTiled(vd, img, dst, 0, 0, effect, effect_value,
						  clip_rect, tilewidth, tileheight);
			}
			else
			{
#ifdef PIXEL_SCALE_RENDERING_SUPPORT
				tileheight = m_tileheight[SKINPART_TILE_RIGHT];
#endif // PIXEL_SCALE_RENDERING_SUPPORT
				BlitCentered(vd, img, rect.x + rect.width - right, center.y + center.height / 2,
							 FALSE, TRUE, tilewidth, tileheight, effect, effect_value, clip_rect);
			}
		}
		if (bottom > 0 && center.width > 0)
		{
			Image img = LOCAL_GET_PART_IMAGE(SKINPART_TILE_BOTTOM);
			int tilewidth = img.Width();
			int tileheight = bottom;

			if (m_type == StateElement::TypeBoxTile || m_type == StateElement::TypeBoxTileHorizontal)
			{
#ifdef PIXEL_SCALE_RENDERING_SUPPORT
				tilewidth = FROM_DEVICE_PIXEL(scaler, tilewidth);
#endif // PIXEL_SCALE_RENDERING_SUPPORT
				OpRect dst(center.x, rect.y + rect.height - bottom, center.width, bottom);
				BlitTiled(vd, img, dst, 0, bitmap_bottom - bottom, effect, effect_value,
						  clip_rect, tilewidth, tileheight);
			}
			else
			{
#ifdef PIXEL_SCALE_RENDERING_SUPPORT
				tilewidth = m_tilewidth[SKINPART_TILE_BOTTOM];
#endif // PIXEL_SCALE_RENDERING_SUPPORT
				BlitCentered(vd, img, center.x + center.width / 2, rect.y + rect.height - bottom,
							 TRUE, FALSE, tilewidth, tileheight, effect, effect_value, clip_rect);
			}
		}
		// Draw center
		if (center.width > 0 && center.height > 0)
		{
			Image img = LOCAL_GET_PART_IMAGE(SKINPART_TILE_CENTER);
			int tilewidth = img.Width();
			int tileheight = img.Height();

			if (m_type == StateElement::TypeBoxTile)
			{
#ifdef PIXEL_SCALE_RENDERING_SUPPORT
				// tiling performed in both direction, dont scale the
				// source bitmap. see detail comments above(draw left edge)
				tilewidth = FROM_DEVICE_PIXEL(scaler, tilewidth);
				tileheight = FROM_DEVICE_PIXEL(scaler, tileheight);
#endif // PIXEL_SCALE_RENDERING_SUPPORT
				BlitTiled(vd, img, center, 0, 0, effect, effect_value,
						  clip_rect, tilewidth, tileheight);
			}
			else if (m_type == StateElement::TypeBoxTileVertical)
			{
#ifdef PIXEL_SCALE_RENDERING_SUPPORT
				// tilewith might be scaled if only low-res bitmap
				// available, since the tiling is performed vertically
				tilewidth = m_tilewidth[SKINPART_TILE_CENTER];
				tileheight = FROM_DEVICE_PIXEL(scaler, tileheight);
#endif // PIXEL_SCALE_RENDERING_SUPPORT
				if (tilewidth < center.width)
				{
					center.x += (center.width - tilewidth) / 2;
					center.width = tilewidth;
				}

				BlitTiled(vd, img, center, 0, 0, effect, effect_value,
						  clip_rect, tilewidth, tileheight);
			}
			else if (m_type == StateElement::TypeBoxTileHorizontal)
			{
#ifdef PIXEL_SCALE_RENDERING_SUPPORT
				// tileheight might be scaled if only low-res bitmap
				// available, since the tiling is performed horizontally
				tilewidth = FROM_DEVICE_PIXEL(scaler, tilewidth);
				tileheight = m_tileheight[SKINPART_TILE_CENTER];
#endif // PIXEL_SCALE_RENDERING_SUPPORT
				if (tileheight < center.height)
				{
					center.y += (center.height - tileheight) / 2;
					center.height = tileheight;
				}

				BlitTiled(vd, img, center, 0, 0, effect, effect_value,
						  clip_rect, tilewidth, tileheight);
			}
			else
			{
#ifdef PIXEL_SCALE_RENDERING_SUPPORT
				// scale to fit the dest rect if only low-res bitmap
				tilewidth = m_tilewidth[SKINPART_TILE_CENTER];
				tileheight = m_tileheight[SKINPART_TILE_CENTER];
#endif // PIXEL_SCALE_RENDERING_SUPPORT

				if (!drop_if_larger || (tilewidth <= center.width && tileheight <= center.height))
				{
					BlitCentered(vd, img, center.x + center.width / 2, center.y + center.height / 2,
								 TRUE, TRUE, tilewidth, tileheight, effect, effect_value, clip_rect);
				}
			}
		}

		// Draw corners
		if (left > 0 && top > 0)
		{
			Image img = LOCAL_GET_PART_IMAGE(SKINPART_CORNER_TOPLEFT);
			OpRect dst_rect(rect.x, rect.y, left, top);
			OpRect src_rect(0, 0, left, top);
#ifdef PIXEL_SCALE_RENDERING_SUPPORT
			src_rect = TO_DEVICE_PIXEL(scaler, src_rect);
			src_rect.width = MIN(src_rect.width, (INT32)img.Width());
			src_rect.height = MIN(src_rect.height, (INT32)img.Height());
#endif // PIXEL_SCALE_RENDERING_SUPPORT
			
			BlitClip(vd, img, src_rect, dst_rect, effect, effect_value);
		}
		if (right > 0 && top > 0)
		{
			Image img = LOCAL_GET_PART_IMAGE(SKINPART_CORNER_TOPRIGHT);
			OpRect dst_rect(rect.x + rect.width - right, rect.y, right, top);
			OpRect src_rect(bitmap_right - right, 0, right, top);
#ifdef PIXEL_SCALE_RENDERING_SUPPORT
			src_rect = TO_DEVICE_PIXEL(scaler, src_rect);
			src_rect.width = MIN(src_rect.width, (INT32)img.Width());
			src_rect.height = MIN(src_rect.height, (INT32)img.Height());
#endif // PIXEL_SCALE_RENDERING_SUPPORT

			BlitClip(vd, img, src_rect, dst_rect, effect, effect_value);
		}
		if (right > 0 && bottom > 0)
		{
			Image img = LOCAL_GET_PART_IMAGE(SKINPART_CORNER_BOTTOMRIGHT);
			OpRect dst_rect(rect.x + rect.width - right, rect.y + rect.height - bottom, right, bottom);
			OpRect src_rect(bitmap_right - right, bitmap_bottom - bottom, right, bottom);
#ifdef PIXEL_SCALE_RENDERING_SUPPORT
			src_rect = TO_DEVICE_PIXEL(scaler, src_rect);
			src_rect.width = MIN(src_rect.width, (INT32)img.Width());
			src_rect.height = MIN(src_rect.height, (INT32)img.Height());
#endif // PIXEL_SCALE_RENDERING_SUPPORT

			BlitClip(vd, img, src_rect, dst_rect, effect, effect_value);
		}
		if (left > 0 && bottom > 0)
		{
			Image img = LOCAL_GET_PART_IMAGE(SKINPART_CORNER_BOTTOMLEFT);
			OpRect dst_rect(rect.x, rect.y + rect.height - bottom, left, bottom);
			OpRect src_rect(0, bitmap_bottom - bottom, left, bottom);
#ifdef PIXEL_SCALE_RENDERING_SUPPORT
			src_rect = TO_DEVICE_PIXEL(scaler, src_rect);
			src_rect.width = MIN(src_rect.width, (INT32)img.Width());
			src_rect.height = MIN(src_rect.height, (INT32)img.Height());
#endif // PIXEL_SCALE_RENDERING_SUPPORT

			BlitClip(vd, img, src_rect, dst_rect, effect, effect_value);
		}
	}
	else
	{
		Image image = LOCAL_GET_PART_IMAGE(SKINPART_TILE_CENTER);
#ifdef ANIMATED_SKIN_SUPPORT
		BOOL animated = m_images[SKINPART_TILE_CENTER].IsAnimated();
#endif // ANIMATED_SKIN_SUPPORT

		// maybe use scaled cache
#ifdef SKIN_SMOOTH_SCALE_IMAGE_DRAW
		OpRect rect_pixel = rect;

#ifdef PIXEL_SCALE_RENDERING_SUPPORT
		// image scale must be performed in device pixels
		rect_pixel = TO_DEVICE_PIXEL(scaler, rect_pixel);
#endif // PIXEL_SCALE_RENDERING_SUPPORT

		if (rect_pixel.width != (INT32)image.Width() || rect_pixel.height != (INT32)image.Height())
		{
			if (rect_pixel.width != (INT32)m_scaled_image_cache.Width()
				|| rect_pixel.height != (INT32)m_scaled_image_cache.Height()
#ifdef ANIMATED_SKIN_SUPPORT
				|| animated
#endif // ANIMATED_SKIN_SUPPORT
				)
			{
				OpBitmap* src_bitmap = image.GetBitmap(this);

				if (src_bitmap)
				{
					OpBitmap* scaled_bitmap = NULL;

					OpBitmap::Create(&scaled_bitmap, rect_pixel.width, rect_pixel.height);

					if (scaled_bitmap)
					{
						OpSkinUtils::ScaleBitmap(src_bitmap, scaled_bitmap);
						m_scaled_image_cache = imgManager->GetImage(scaled_bitmap);
						if (m_scaled_image_cache.IsEmpty())	// OOM
							OP_DELETE(scaled_bitmap);
					}
					image.ReleaseBitmap();
				}
			}
			image = m_scaled_image_cache;
		}
#ifdef ANIMATED_SKIN_SUPPORT
		else if(animated)
		{
			m_scaled_image_cache = image;
		}
#endif // ANIMATED_SKIN_SUPPORT
#endif // SKIN_SMOOTH_SCALE_IMAGE_DRAW

		BlitStretch(vd, image, rect, effect, effect_value, clip_rect);
	}

#ifdef VEGA_OPPAINTER_SUPPORT
	if (m_border[0] || m_border[1] || m_border[2] || m_border[3])
	{
		// reset the alpha (which is incorrect due to INT32 vs UINT32)
		UINT32 fill_color = (m_border_color & OP_RGBA(0, 0, 0, 255)) | GetColor(m_border_color);

		Border border;
		SetRadiusOnBorder(border);
		border.left.color = border.top.color = border.right.color = border.bottom.color = fill_color;
		border.left.style = border.top.style = border.right.style = border.bottom.style = m_border_style;
		border.top.width = m_border[0];
		border.right.width = m_border[1];
		border.bottom.width = m_border[2];
		border.left.width = m_border[3];
		vd->Translate(rect.x, rect.y);
		vd->DrawBorderWithRadius(rect.width, rect.height, &border);
		vd->Translate(-rect.x, -rect.y);
	}
#endif // VEGA_OPPAINTER_SUPPORT

	rect.x += m_padding_left;
	rect.y += m_padding_top;
	rect.width -= m_padding_left + m_padding_right;
	rect.height -= m_padding_top + m_padding_bottom;

	for (UINT32 i = 0; i < m_child_elements.GetCount(); i++)
	{
		StateElement* child = static_cast<StateElement*>(m_child_elements.Get(i));

		OpRect child_rect = rect;

		child_rect.x += child->m_margin_left;
		child_rect.y += child->m_margin_top;
		child_rect.width -= child->m_margin_left + child->m_margin_right;
		child_rect.height -= child->m_margin_top + child->m_margin_bottom;

		child->Draw(vd, child_rect, effect, effect_value, clip_rect, m_drop_if_larger);
	}

	if (clip_rect)
		vd->EndClipping();
}

void OpSkinElement::StateElement::DrawOverlay(VisualDevice* vd, OpRect& rect, INT32 effect, INT32 effect_value, const OpRect* clip_rect, BOOL drop_if_larger)
{
	if (rect.IsEmpty())
		return;

	for (UINT32 i = 0; i < m_overlay_elements.GetCount(); i++)
	{
		StateElement* overlay = static_cast<StateElement*>(m_overlay_elements.Get(i));

		OpRect overlay_rect = rect;

		overlay_rect.x += overlay->m_margin_left;
		overlay_rect.y += overlay->m_margin_top;
		overlay_rect.width -= overlay->m_margin_left + overlay->m_margin_right;
		overlay_rect.height -= overlay->m_margin_top + overlay->m_margin_bottom;

		overlay->Draw(vd, overlay_rect, effect, effect_value, clip_rect, m_drop_if_larger);
	}
}

#ifdef SKIN_SKIN_COLOR_THEME
void OpSkinElement::StateElement::DrawColorizedBackground(VisualDevice* vd, OpRect& rect, INT32 effect, INT32 effect_value, const OpRect* clip_rect, COLORREF color)
{
	if (rect.IsEmpty())
		return;

	if (clip_rect)
		vd->BeginClipping(*clip_rect);

	vd->SetBgColor(color);

	UINT8 padding_top		= m_colorized_background_padding[0];
	UINT8 padding_right		= m_colorized_background_padding[1];
	UINT8 padding_bottom	= m_colorized_background_padding[2];
	UINT8 padding_left		= m_colorized_background_padding[3];

	OpRect dest = rect;
	dest.x		+= padding_left;
	dest.y		+= padding_top;
	dest.width	-= padding_left+padding_right;
	dest.height -= padding_top+padding_bottom;

	Border radius;
	UINT8 radius_top	= m_colorized_background_radius[0];
	UINT8 radius_right	= m_colorized_background_radius[1];
	UINT8 radius_bottom	= m_colorized_background_radius[2];
	UINT8 radius_left	= m_colorized_background_radius[3];

	radius.Reset();
	radius.top.SetRadius(radius_top);
	radius.right.SetRadius(radius_right);
	radius.bottom.SetRadius(radius_bottom);
	radius.left.SetRadius(radius_left);

	vd->DrawBgColorWithRadius(dest, &radius);

	if (clip_rect)
		vd->EndClipping();
}
#endif // SKIN_SKIN_COLOR_THEME

void OpSkinElement::StateElement::SetRadiusOnBorder(Border &border)
{
	border.Reset();
	border.top.radius_start = border.left.radius_start = m_radius[0];
	border.top.radius_end = border.right.radius_start = m_radius[1];
	border.right.radius_end = border.bottom.radius_end = m_radius[2];
	border.left.radius_end = border.bottom.radius_start = m_radius[3];
}

BOOL OpSkinElement::StateElement::DrawSkinArrow(VisualDevice* vd, const OpRect& rect, INT32 effect, INT32 effect_value, const SkinArrow &arrow, OpRect &used_rect)
{
	if (rect.IsEmpty() || m_type != StateElement::TypeBoxStretch)
		return FALSE;

	Image img = LOCAL_GET_PART_IMAGE(arrow.part);
#ifdef PIXEL_SCALE_RENDERING_SUPPORT
	int tilewidth = m_tilewidth[arrow.part];
	int tileheight = m_tileheight[arrow.part];
#else
	int tilewidth = img.Width();
	int tileheight = img.Height();
#endif // PIXEL_SCALE_RENDERING_SUPPORT

	int offset;
	if (arrow.part == SKINPART_TILE_LEFT || arrow.part == SKINPART_TILE_RIGHT)
		offset = OpRound(((rect.height - tileheight - (int)m_stretch_border[0] - (int)m_stretch_border[2]) * arrow.offset) / 100 + (int)m_stretch_border[0]);
	else
		offset = OpRound(((rect.width - tilewidth - (int)m_stretch_border[1] - (int)m_stretch_border[3]) * arrow.offset) / 100 + (int)m_stretch_border[3]);

	used_rect.Set(0, 0, tilewidth, tileheight);

	if (arrow.part == SKINPART_TILE_LEFT)
		used_rect.OffsetBy(rect.x, rect.y + offset);
	else if (arrow.part == SKINPART_TILE_TOP)
		used_rect.OffsetBy(rect.x + offset, rect.y);
	else if (arrow.part == SKINPART_TILE_RIGHT)
		used_rect.OffsetBy(rect.x + rect.width - tilewidth, rect.y + offset);
	else if (arrow.part == SKINPART_TILE_BOTTOM)
		used_rect.OffsetBy(rect.x + offset, rect.y + rect.height - tileheight);
	else
		return FALSE;

	Blit(vd, img, used_rect.x, used_rect.y, tilewidth, tileheight, effect, effect_value);

	return TRUE;
}

/***********************************************************************************
**
**	OnPortionDecoded
**
***********************************************************************************/

void OpSkinElement::StateElement::OnPortionDecoded()
{
#ifdef ASYNC_IMAGE_DECODERS
	INT32 i;
	BOOL has_some_image = FALSE;

	for (i = 0; i < SKINPART_TOTAL; i++)
	{
		BOOL empty = m_images[i].IsEmpty();
		BOOL decoded = m_images[i].ImageDecoded();

		if (empty)
			continue;

		has_some_image = TRUE;

		if (!decoded)
			return;
	}

	if (!has_some_image)
		return;

	m_skin->ContinueLoadAllSkinElements();

	// Notify someone that every skin element has finished loading and decoding
	Window* window = g_windowManager->FirstWindow();

	while (window)
	{
		window->VisualDev()->UpdateAll();
		window->VisualDev()->InvalidateScrollbars();
		window = window->Suc();
	}
#endif // ASYNC_IMAGE_DECODERS
}

/***********************************************************************************
**
**	OnError
**
***********************************************************************************/

void OpSkinElement::StateElement::OnError(OP_STATUS status)
{
	OpStatus::Ignore(status);
}

/***********************************************************************************
**
**	StartAnimation - Start or stops an skin part animation, if any is running
**
***********************************************************************************/

#ifdef ANIMATED_SKIN_SUPPORT

void OpSkinElement::StartAnimation(BOOL start, VisualDevice *vd, BOOL animate_from_beginning)
{
	for (UINT32 i = 0; i < m_state_elements.GetCount(); i++)
	{
		StateElement* state_elm = m_state_elements.Get(i);
		if (state_elm)
		{
			state_elm->StartAnimation(start, vd, animate_from_beginning);
		}
	}
}

void OpSkinElement::StateElement::StartAnimation(BOOL start, VisualDevice *vd, BOOL animate_from_beginning)
{
	INT32 i;
	for(i = 0; i < SKINPART_TOTAL; i++)
	{
		if(m_images[i].IsAnimated())
		{
			if (start)
			{
				if (animate_from_beginning)
				{
					this->GetImage((SkinPart)i).ResetAnimation(this);
				}

				if (m_animation_handlers[i])
				{
					m_animation_handlers[i]->IncRef();
				}
				else
				{
					m_animation_handlers[i] = OpSkinAnimationHandler::GetImageAnimationHandler(this, vd, (SkinPart)i);
				}
			}
			else if (m_animation_handlers[i])
			{
				INT32 ref_count = m_animation_handlers[i]->GetRefCount();
				m_animation_handlers[i]->DecRef(this, vd, (SkinPart)i);
				if(ref_count == 1)
				{
					m_animation_handlers[i] = NULL;
				}
			}
		}
	}
}

OpSkinElement::StateElement::~StateElement()
{
}

#endif // ANIMATED_SKIN_SUPPORT

BOOL OpSkinElement::HasTransparentState()
{
	for (UINT32 i = 0; i < m_state_elements.GetCount(); ++i)
	{
		if (m_state_elements.Get(i)->m_clear_background)
			return TRUE;
	}
	return FALSE;
}


void OpSkinElement::StateElement::Blit(VisualDevice* vd, Image image, INT32 x, INT32 y,
									   int dst_width, int dst_height, int effect, int effect_value)
{
	if (!image.ImageDecoded())
		return;

	vd->ImageOutEffect(image, OpRect(0, 0, image.Width(), image.Height()), OpRect(x, y, dst_width, dst_height), effect, effect_value, this);
}

void OpSkinElement::StateElement::BlitClip(VisualDevice* vd, Image image, const OpRect &src_rect, const OpRect &dst_rect, int effect, int effect_value)
{
	if (!image.ImageDecoded())
		return;

	vd->ImageOutEffect(image, src_rect, dst_rect, effect, effect_value, this);
}

void OpSkinElement::StateElement::BlitCentered(VisualDevice* vd, Image image, INT32 x, INT32 y,
											   BOOL center_x, BOOL center_y, int dst_width, int dst_height,
											   int effect, int effect_value, const OpRect* clip_rect)
{
	if (!image.ImageDecoded())
		return;

	OpRect rect(center_x ? x - dst_width / 2 : x , center_y ? y - dst_height / 2 : y, dst_width, dst_height);

	if (!clip_rect || clip_rect->Intersecting(rect))
	{
		vd->ImageOutEffect(image, OpRect(0, 0, image.Width(), image.Height()), rect, effect, effect_value, this);
	}
}

void OpSkinElement::StateElement::BlitStretch(VisualDevice* vd, Image image, const OpRect &rect, int effect, int effect_value, const OpRect* clip_rect)
{
	if (!image.ImageDecoded())
		return;

	if (!clip_rect || clip_rect->Intersecting(rect))
	{
		vd->ImageOutEffect(image, OpRect(0, 0, image.Width(), image.Height()), rect, effect, effect_value, this);
	}
}

void OpSkinElement::StateElement::BlitStretch(VisualDevice* vd, Image image, const OpRect &dest, const OpRect &source, int effect, int effect_value)
{
	if (!image.ImageDecoded())
		return;

	vd->ImageOutEffect(image, source, dest, effect, effect_value, this);
}

void OpSkinElement::StateElement::BlitTiled(VisualDevice* vd, Image image, OpRect rect, int ofs_x, int ofs_y,
											int effect, int effect_value, const OpRect* clip_rect,
											int tilewidth, int tileheight)
{
	if (!image.ImageDecoded())
		return;

	if (!clip_rect || clip_rect->Intersecting(rect))
	{
		OpPoint offset;
		if (clip_rect)
		{
			if (clip_rect->x > rect.x)
				offset.x = clip_rect->x - rect.x;
			if (clip_rect->y > rect.y)
				offset.y = clip_rect->y - rect.y;
			rect.IntersectWith(*clip_rect);
		}

		offset.x += ofs_x;
		offset.y += ofs_y;

		int imgscale_x = tilewidth * 100 / image.Width();
		int imgscale_y = tileheight * 100 / image.Height();
		if (effect)
		{
			vd->ImageOutTiledEffect(image, rect, offset, effect, effect_value, imgscale_x, imgscale_y);
		}
		else
		{
			vd->ImageOutTiled(image, rect, offset, this, imgscale_x, imgscale_y, 0, 0);
		}
	}
}

/***********************************************************************************
**
**	GetColor
**
***********************************************************************************/

UINT32 GetColor(INT32 color)
{
	if (color >= 0)
		return color;

	switch (color)
	{
		case SKINCOLOR_WINDOW:
			return g_op_ui_info->GetSystemColor(OP_SYSTEM_COLOR_BACKGROUND);

		case SKINCOLOR_WINDOW_DISABLED:
			return g_op_ui_info->GetSystemColor(OP_SYSTEM_COLOR_BACKGROUND_DISABLED);
	}

	return 0;
}

#endif // SKIN_SUPPORT
