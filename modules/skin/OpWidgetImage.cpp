/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef SKIN_SUPPORT

#include "modules/skin/OpWidgetImage.h"
#include "modules/skin/OpSkinManager.h"
#include "modules/skin/OpSkinUtils.h"
#include "modules/display/vis_dev.h"
#include "modules/img/image.h"
#include "modules/util/gen_math.h"

/***********************************************************************************
**
**	OpWidgetImage
**
***********************************************************************************/

OpWidgetImage::OpWidgetImage(BOOL foreground)
	: m_skin_element(NULL)
{
	m_packed.use_image = USE_NONE;
	m_packed.foreground = foreground;
#ifdef ANIMATED_SKIN_SUPPORT
	m_packed.animation_started = FALSE;
#endif
	Empty();
}


OpWidgetImage::~OpWidgetImage()
{
#ifdef ANIMATED_SKIN_SUPPORT
	StartAnimation(FALSE);
#endif
	if (m_skin_element)
		m_skin_element->RemoveSkinElementListener(this);
}

/***********************************************************************************
**
**	Empty
**
***********************************************************************************/

void OpWidgetImage::Empty()
{
	m_listener = NULL;
	m_skin_manager = g_skin_manager;
	m_state = 0;
	m_hover_value = 0;
	m_packed.skin_type = SKINTYPE_DEFAULT;
	m_packed.skin_size = SKINSIZE_DEFAULT;
	m_packed.restrict_image_size = FALSE;
	m_effect = 0;
	m_effect_value = 0;
	ResetColorization();
	UnsetStretchBorder();
	m_restricting_image.Empty();
}

/***********************************************************************************
**
**	SetWidgetImage
**
***********************************************************************************/

void OpWidgetImage::SetWidgetImage(const OpWidgetImage* widget_image)
{
	if (widget_image->HasEffectBitmapImage())
	{
		Image img = widget_image->GetEffectBitmapImage();
		SetBitmapImageWithEffect(img, widget_image->GetBitmapEffect(), widget_image->GetBitmapEffectValue());
	}
	else if (widget_image->HasBitmapImage())
	{
		Image img = widget_image->GetBitmapImage();
		SetBitmapImage(img);
	}
	else
	{
		SetImage(widget_image->GetImage());
	}
}

void OpWidgetImage::ApplyColorization(COLORREF color)
{
	m_color = color; 
	m_packed.has_background_color = TRUE;
}

void OpWidgetImage::ResetColorization()
{
	m_color = OP_RGBA(0xff, 0x00, 0x00, 0xff);
	m_packed.has_background_color = FALSE;
}

/*******************************************************************************
**
**	SetColorizedBitmapImage
**
*******************************************************************************/

#ifdef SKIN_SKIN_COLOR_THEME
void OpWidgetImage::SetColorizedBitmapImage(Image &image, COLORREF color)
{
	UINT32 color_rgb = (OP_GET_R_VALUE(color) << 16)
					 | (OP_GET_G_VALUE(color) << 8)
					 |  OP_GET_B_VALUE(color);
	UINT8 alpha = OP_GET_A_VALUE(color);
	Image image_copy;

	if (image.IsEmpty())
		return;

	OpBitmap* bitmap = image.GetBitmap(null_image_listener); // bitmap locked
	if (!bitmap)
		return;
	OpBitmap* bitmap_copy = NULL;

	OpStatus::Ignore(
		OpBitmap::Create(
			&bitmap_copy, image.Width(), image.Height(), FALSE, TRUE)
	);

	if (bitmap_copy &&
		bitmap_copy->GetBpp() == 32 &&
		bitmap_copy->Supports(OpBitmap::SUPPORTS_POINTER))
	{
		// edit bitmap

		double h, s, v;
		OpSkinUtils::CopyBitmap(bitmap, bitmap_copy);
		OpSkinUtils::ConvertRGBToHSV(color_rgb, h, s, v);
		UINT32 *src = (UINT32*)(bitmap_copy->GetPointer());
		if (src)
		{
			UINT32 wh = bitmap_copy->Width() * bitmap_copy->Height();

			for(UINT32 i = 0; i < wh; i++)
			{
				*src = OpSkinUtils::ColorizePixel(*src, h, s, v);
				src++;
			}
		}
		bitmap_copy->ReleasePointer();
		image_copy = imgManager->GetImage(bitmap_copy);
	}
	image.ReleaseBitmap(); // unlock bitmap

	UINT32 effect_val = OpRound(double(alpha) / 255.0 * 100.0);
	SetBitmapImageWithEffect(image_copy, Image::EFFECT_BLEND, effect_val);
}
#endif // SKIN_SKIN_COLOR_THEME

/***********************************************************************************
**
**	SetImageAndType
**
***********************************************************************************/

void OpWidgetImage::SetImageAndType(const char* preferred_image, SkinType type, SkinSize size, const char* fallback_image)
{
	bool changed = m_preferred_image.Compare(preferred_image) ||
		(SkinType)m_packed.skin_type != type ||
		(SkinSize)m_packed.skin_size != size ||
		m_fallback_image.Compare(fallback_image);

	// Force update if m_packed.use_image == USE_NONE (Old fix for DSK-200521)
	if (changed || m_packed.use_image == USE_NONE)
	{
#ifdef ANIMATED_SKIN_SUPPORT
		StartAnimation(FALSE);
#endif
		m_bitmap_image.Empty();
		m_preferred_image.Set(preferred_image);
		m_fallback_image.Set(fallback_image);
		m_packed.skin_type = type;
		m_packed.skin_size = size;

		UpdateUseImage();

		// Addition to the fix for DSK-200521 above. We really don't wan't to call
		// OnImageChanged if nothing changed. That cause unnecessary relayout + repaint!
		if (!changed && m_packed.use_image == USE_NONE)
			return;

		if (m_listener)
		{
			m_listener->OnImageChanged(this);
#ifdef ANIMATED_SKIN_SUPPORT
			if (m_packed.use_image == USE_NONE)
				return;
			StartAnimation(TRUE);
#endif
		}
	}
}

/***********************************************************************************
**
**	SetType
**
***********************************************************************************/

void OpWidgetImage::SetType(SkinType type)
{
	if ((SkinType)m_packed.skin_type != type)
	{
		if (m_listener && m_packed.use_image != USE_BITMAP_IMAGE)
		{
#ifdef ANIMATED_SKIN_SUPPORT
			StartAnimation(FALSE);
#endif
		}
		m_packed.skin_type = type;
		UpdateUseImage();

		if (m_listener && m_packed.use_image != USE_BITMAP_IMAGE)
		{
			m_listener->OnImageChanged(this);
#ifdef ANIMATED_SKIN_SUPPORT
			StartAnimation(TRUE);
#endif
		}
	}
}

/***********************************************************************************
**
**	SetSize
**
***********************************************************************************/

void OpWidgetImage::SetSize(SkinSize size)
{
	if ((SkinSize)m_packed.skin_size != size)
	{
		if (m_packed.use_image != USE_BITMAP_IMAGE)
		{
#ifdef ANIMATED_SKIN_SUPPORT
			StartAnimation(FALSE);
#endif
		}
		m_packed.skin_size = size;
		UpdateUseImage();

		if (m_listener && m_packed.use_image != USE_BITMAP_IMAGE)
		{
			m_listener->OnImageChanged(this);
#ifdef ANIMATED_SKIN_SUPPORT
			StartAnimation(TRUE);
#endif
		}
	}
}

/***********************************************************************************
**
**	SetBitmapImage
**
***********************************************************************************/

void OpWidgetImage::SetBitmapImage(Image &image, BOOL allow_empty_bitmap)
{
	if (m_bitmap_image != image && (allow_empty_bitmap || !image.IsEmpty()))
	{
#ifdef ANIMATED_SKIN_SUPPORT
		StartAnimation(FALSE);
#endif
		m_preferred_image.Empty();
		m_fallback_image.Empty();
		m_bitmap_effect_image.Empty();
		m_bitmap_image = image;

		UpdateUseImage();

		if (m_listener)
		{
			m_listener->OnImageChanged(this);
#ifdef ANIMATED_SKIN_SUPPORT
			StartAnimation(TRUE);
#endif
		}
	}
}

/***********************************************************************************
**
**	SetBitmapImageWithEffect
**
***********************************************************************************/

void OpWidgetImage::SetBitmapImageWithEffect(Image &image, INT32 effect, INT32 effect_value)
{
	if (m_bitmap_effect_image != image || m_effect != effect || m_effect_value != effect_value)
	{
#ifdef ANIMATED_SKIN_SUPPORT
		StartAnimation(FALSE);
#endif
		m_bitmap_effect_image = image;
		m_effect = effect;
		m_effect_value = effect_value;

		UpdateUseImage();

		if (m_listener)
		{
			m_listener->OnImageChanged(this);
#ifdef ANIMATED_SKIN_SUPPORT
			StartAnimation(TRUE);
#endif
		}
	}
}

void OpWidgetImage::SetStretchBorder(UINT8 top, UINT8 right, UINT8 bottom, UINT8 left)
{
	m_stretch_border_top = top;
	m_stretch_border_right = right;
	m_stretch_border_bottom = bottom;
	m_stretch_border_left = left;
	m_packed.has_stretch_border = TRUE;
}

void OpWidgetImage::UnsetStretchBorder()
{
	m_stretch_border_top = 0;
	m_stretch_border_right = 0;
	m_stretch_border_bottom = 0;
	m_stretch_border_left = 0;
	m_packed.has_stretch_border = FALSE;
}

inline Border OpWidgetImage::GetStretchBorder() const
{
	Border stretch_border;
	stretch_border.Reset();
	stretch_border.top.width = m_stretch_border_top;
	stretch_border.right.width = m_stretch_border_right;
	stretch_border.bottom.width = m_stretch_border_bottom;
	stretch_border.left.width = m_stretch_border_left;
	return stretch_border;
}

inline BOOL OpWidgetImage::HasStretchBorder() const
{
	return m_packed.has_stretch_border && HasBitmapImage();
}

/***********************************************************************************
**
**	SetState
**
***********************************************************************************/

void OpWidgetImage::SetState(INT32 state, INT32 mask, INT32 hover_value)
{
	state = (m_state & ~mask) | (state & mask);

	if (m_state != state || m_hover_value != hover_value)
	{
#ifdef ANIMATED_SKIN_SUPPORT
		StartAnimation(FALSE);
#endif
		m_state = state;
		m_hover_value = hover_value;

		if (m_listener)
		{
			m_listener->OnImageChanged(this);
#ifdef ANIMATED_SKIN_SUPPORT
			StartAnimation(TRUE);
#endif
		}
	}
}

/***********************************************************************************
**
**	GetImage
**
***********************************************************************************/

const char* OpWidgetImage::GetImage() const
{
	switch (m_packed.use_image)
	{
		case USE_PREFERRED_IMAGE:
			return m_preferred_image.CStr();
		case USE_FALLBACK_IMAGE:
			return m_fallback_image.CStr();
		default:
			return NULL;
	}
}

/***********************************************************************************
**
**	HasContent
**
***********************************************************************************/

BOOL OpWidgetImage::HasContent()
{
	return m_packed.use_image != USE_NONE;
}

/***********************************************************************************
**
**	GetSize
**
***********************************************************************************/

void OpWidgetImage::GetSize(INT32* width, INT32* height)
{
	*width = 0;
	*height = 0;

	switch (m_packed.use_image)
	{
		case USE_PREFERRED_IMAGE:
		case USE_FALLBACK_IMAGE:
			if (HasSkinElement())
				m_skin_element->GetSize(width, height, m_state);
			break;
		case USE_BITMAP_IMAGE:
			*width = m_bitmap_image.Width();
			*height = m_bitmap_image.Height();
			break;
		case USE_EFFECT_BITMAP_IMAGE:
			*width = m_bitmap_effect_image.Width();
			*height = m_bitmap_effect_image.Height();
			break;
	}
}

/***********************************************************************************
**
**	GetButtonTextPadding - returns -1 if not set
**
***********************************************************************************/
void OpWidgetImage::GetButtonTextPadding(INT32 *padding)
{
	*padding = -1;

	if (HasSkinElement())
	{
		m_skin_element->GetButtonTextPadding(padding, m_state);
	}
}

/***********************************************************************************
**
**	GetMargin
**
***********************************************************************************/

void OpWidgetImage::GetMargin(INT32* left, INT32* top, INT32* right, INT32* bottom)
{
	*left = *top = *right = *bottom = SKIN_DEFAULT_MARGIN;

	if (HasSkinElement())
	{
		m_skin_element->GetMargin(left, top, right, bottom, m_state);
	}
}

/***********************************************************************************
**
**	GetPadding
**
***********************************************************************************/

void OpWidgetImage::GetPadding(INT32* left, INT32* top, INT32* right, INT32* bottom)
{
	*left = *top = *right = *bottom = SKIN_DEFAULT_PADDING;

	if (HasSkinElement())
	{
		m_skin_element->GetPadding(left, top, right, bottom, m_state);
	}
}

/***********************************************************************************
**
**	GetSpacing
**
***********************************************************************************/

void OpWidgetImage::GetSpacing(INT32* spacing)
{
	*spacing = SKIN_DEFAULT_SPACING;

	if (HasSkinElement())
	{
		m_skin_element->GetSpacing(spacing, m_state);
	}
}

/***********************************************************************************
**
**	Draw
**
***********************************************************************************/

void OpWidgetImage::Draw(VisualDevice* vd, OpPoint pos, const OpRect* clip_rect)
{
	INT32 width;
	INT32 height;

	GetSize(&width, &height);

	Draw(vd, OpRect(pos.x, pos.y, width, height), clip_rect);
}

/***********************************************************************************
**
**	Draw
**
***********************************************************************************/

void OpWidgetImage::Draw(VisualDevice* vd, OpRect dest, const OpRect* clip_rect, INT32 state, BOOL overlay)
{
	switch (m_packed.use_image)
	{
		case USE_PREFERRED_IMAGE:
		case USE_FALLBACK_IMAGE:
		{
			INT32 drawstate = m_state;
#if defined(USE_SKINSTATE_PARAMETER)
			// Mac: For focus ring, Unix: Hovered dropdowns (DSK-293790)
			drawstate |= state;
#endif
			if (HasSkinElement())
			{
				if (overlay)
					m_skin_element->DrawOverlay(vd, dest, drawstate, m_hover_value, clip_rect);
				else
				{
					OpSkinElement::DrawArguments args;
					args.hover_value = m_hover_value;
					args.clip_rect = clip_rect;
					args.arrow = &m_arrow;
					args.colorize_background = m_packed.has_background_color;
					args.background_color = m_color;

					INT32 alpha = m_packed.has_background_color ? 
						OP_GET_A_VALUE(m_color) : 0xff;
					if (alpha < 0xff)
					{
						args.forced_effect = Image::EFFECT_BLEND;
						args.forced_effect_value = OpRound(double(alpha) / 255. * 100.);
					}
					m_skin_element->DrawWithEdgeArrow(vd, dest, drawstate, args);
				}
			}

			break;
		}
		case USE_BITMAP_IMAGE:
		{
			if (overlay)
				break;

			INT32 effect = m_state & SKINSTATE_DISABLED ? Image::EFFECT_DISABLED : m_hover_value ? Image::EFFECT_GLOW : 0;
			INT32 effect_value = effect & Image::EFFECT_GLOW ? m_hover_value : 0;

			OpRect src(0, 0, m_bitmap_image.Width(), m_bitmap_image.Height());

			if (clip_rect)
				vd->BeginClipping(*clip_rect);

			if (m_packed.has_stretch_border && m_bitmap_image.ImageDecoded())
			{
				BorderImage border_image;
				border_image.Reset();
				border_image.cut[0] = m_stretch_border_top;
				border_image.cut[1] = m_stretch_border_right;
				border_image.cut[2] = m_stretch_border_bottom;
				border_image.cut[3] = m_stretch_border_left;

				Border stretch_border = GetStretchBorder();
				
				vd->PaintBorderImage(m_bitmap_image, null_image_listener, dest,
						&stretch_border, &border_image, NULL);
			}

			if (!m_packed.has_stretch_border)
			{
				vd->ImageOutEffect(m_bitmap_image, src, dest, effect, effect_value);
			}

			if (clip_rect)
				vd->EndClipping();

			break;
		}
		case USE_EFFECT_BITMAP_IMAGE:
		{
			if (overlay)
				break;

			OpRect src(0, 0, m_bitmap_effect_image.Width(), m_bitmap_effect_image.Height());

			if (clip_rect)
				vd->BeginClipping(*clip_rect);

			if (m_packed.has_stretch_border && m_bitmap_effect_image.ImageDecoded())
			{
				BorderImage border_image;
				border_image.Reset();
				border_image.cut[0] = m_stretch_border_top;
				border_image.cut[1] = m_stretch_border_right;
				border_image.cut[2] = m_stretch_border_bottom;
				border_image.cut[3] = m_stretch_border_left;

				Border stretch_border = GetStretchBorder();

				VisualDevice::ImageEffect img_effect;
				img_effect.effect = m_effect;
				img_effect.effect_value = m_effect_value;
				vd->PaintBorderImage(m_bitmap_effect_image, null_image_listener, dest,
						&stretch_border, &border_image, &img_effect);
			}

			if (!m_packed.has_stretch_border)
			{
				vd->ImageOutEffect(m_bitmap_effect_image, src, dest, m_effect, m_effect_value);
			}

			if (clip_rect)
				vd->EndClipping();

			break;
		}
	}
}

/***********************************************************************************
**
**	GetRestrictedSize
**
***********************************************************************************/

void OpWidgetImage::GetRestrictedSize(INT32* width, INT32* height)
{
	GetSize(width, height);

	if(m_packed.restrict_image_size)
	{
		INT32 restricted_width = 16;
		INT32 restricted_height = 16;

		m_skin_manager->GetSize(
			(m_restricting_image.IsEmpty() ? 
					"Window Document Icon"
			: m_restricting_image.CStr()),
			&restricted_width, &restricted_height, m_state,
			(SkinType) m_packed.skin_type, (SkinSize) m_packed.skin_size,
			m_packed.foreground);

		/*
		*width = MIN(*width, restricted_width);
		*height = MIN(*height, restricted_height);

		if (*width < 16)
			*width = 16;

		if (*height < 16)
			*height = 16;
		*/
		*width = restricted_width;
		*height = restricted_height;
	}
}

/***********************************************************************************
**
**	SetRestrictingImage
**
***********************************************************************************/

void OpWidgetImage::SetRestrictingImage(const char* restricting_image)
{
	if (restricting_image)
		OpStatus::Ignore(m_restricting_image.Set(restricting_image));
	else
		m_restricting_image.Empty();
}

/***********************************************************************************
**
**	CalculateScaledRect
**
***********************************************************************************/

OpRect OpWidgetImage::CalculateScaledRect(const OpRect& available_rect, BOOL center_x, BOOL center_y)
{
	INT32 width;
	INT32 height;

	GetRestrictedSize(&width, &height);

	if (width && width > available_rect.width)
	{
		height = height * available_rect.width / width;
		width = available_rect.width;
	}

	if (height && height > available_rect.height)
	{
		width = width * available_rect.height / height;
		height = available_rect.height;
	}

	if (width < 0 || height < 0)
	{
		width = 0;
		height = 0;
	}

	OpRect rect;

	rect.x = center_x ? available_rect.x + (available_rect.width - width) / 2 : available_rect.x;
	rect.y = center_y ? available_rect.y + (available_rect.height - height) / 2 : available_rect.y;
	rect.width = width;
	rect.height = height;

	return rect;
}

/***********************************************************************************
**
**	AddMargin
**
***********************************************************************************/

void OpWidgetImage::AddMargin(OpRect& rect)
{
	INT32 left, top, right, bottom;

	GetMargin(&left, &top, &right, &bottom);

	rect.x += left;
	rect.y += top;
	rect.width -= left + right;
	rect.height -= top + bottom;
}

/***********************************************************************************
**
**	AddPadding
**
***********************************************************************************/

void OpWidgetImage::AddPadding(OpRect& rect)
{
	INT32 left, top, right, bottom;

	GetPadding(&left, &top, &right, &bottom);

	rect.x += left;
	rect.y += top;
	rect.width -= left + right;
	rect.height -= top + bottom;
}

/***********************************************************************************
**
**	UpdateUseImage
**
***********************************************************************************/

void OpWidgetImage::UpdateUseImage()
{
	if (m_skin_element)
		m_skin_element->RemoveSkinElementListener(this);
	m_skin_element = NULL;

	if (HasEffectBitmapImage())
		m_packed.use_image = USE_EFFECT_BITMAP_IMAGE;
	else if (HasBitmapImage())
	{
		m_packed.use_image = USE_BITMAP_IMAGE;
	}
	else if (m_preferred_image.HasContent() &&
			(m_skin_element = m_skin_manager->GetSkinElement(m_preferred_image.CStr(), (SkinType) m_packed.skin_type, (SkinSize) m_packed.skin_size, m_packed.foreground)))
	{
		m_packed.use_image = USE_PREFERRED_IMAGE;
		m_skin_element->AddSkinElementListener(this);
	}
	else if (m_fallback_image.HasContent() &&
			(m_skin_element = m_skin_manager->GetSkinElement(m_fallback_image.CStr(), (SkinType) m_packed.skin_type, (SkinSize) m_packed.skin_size, m_packed.foreground)))
	{
		m_packed.use_image = USE_FALLBACK_IMAGE;
		m_skin_element->AddSkinElementListener(this);
	}
	else
		m_packed.use_image = USE_NONE;
}

OpSkinElement* OpWidgetImage::GetSkinElement()
{
	if (!HasSkinElement())
		return NULL;
	return m_skin_element;
}

/***********************************************************************************
**
**	HasSkinElement
**
***********************************************************************************/

BOOL OpWidgetImage::HasSkinElement()
{
	if (m_skin_element)
		return TRUE;

	switch (m_packed.use_image)
	{
		case USE_PREFERRED_IMAGE:
		case USE_FALLBACK_IMAGE:
			UpdateUseImage();
			return m_skin_element != NULL;

		case USE_NONE:
			if (m_preferred_image.HasContent() || m_fallback_image.HasContent())
			{
				UpdateUseImage();
				return m_skin_element != NULL;
			}
			/* fall through */
		default:
			return FALSE;
	}
}

#ifdef ANIMATED_SKIN_SUPPORT
void OpWidgetImage::OnAnimatedImageChanged()
{
	if(m_listener)
	{
		m_listener->OnAnimatedImageChanged(this);
	}
}

void OpWidgetImage::StartAnimation(BOOL start, BOOL animate_from_beginning /*= FALSE*/)
{
	if (start && (!m_listener || !m_listener->IsAnimationVisible()))
		// Don't start the animation if the listener isn't visible
		return;

	if(static_cast<bool>(m_packed.animation_started) != !!start)
	{
		m_packed.animation_started = !!start;

		if(HasSkinElement())
		{
			const char* image = GetImage();
			if(start)
			{
				g_skin_manager->SetAnimationListener(image, this);
			}
			else
			{
				g_skin_manager->RemoveAnimationListener(image, this);
			}
			g_skin_manager->StartAnimation(start, image, NULL, animate_from_beginning);
		}
	}
}

#endif // ANIMATED_SKIN_SUPPORT

void OpWidgetImage::OnSkinElementDeleted()
{
#ifdef ANIMATED_SKIN_SUPPORT
	StartAnimation(FALSE);
#endif
	m_skin_element = NULL;
}

void OpWidgetImage::PointArrow(const OpRect& widget_image, const OpRect& arrow_target)
{
	if (!HasSkinElement())
		return;

	UINT8 stretch_border[4];
	RETURN_VOID_IF_ERROR(m_skin_element->GetStretchBorder(
				&stretch_border[3], // left
				&stretch_border[0], // top
				&stretch_border[1], // right
				&stretch_border[2], // bottom
				0));
	INT32 arrow_width, arrow_height;
	double offset = 0;
 
	m_arrow.Reset();
	OpPoint target(arrow_target.Center());
	if (target.y <= widget_image.y)
	{
		m_arrow.SetTop();
		m_skin_element->GetArrowSize(m_arrow, &arrow_width, &arrow_height, 0);
		offset = target.x - (widget_image.x + stretch_border[3] + double(arrow_width)/2.);
		m_arrow.SetOffset(offset * 100. /
				(widget_image.width - stretch_border[3] - stretch_border[1] - arrow_width));
	}
	else if (target.y >= widget_image.y + widget_image.height)
	{
		m_arrow.SetBottom();
		m_skin_element->GetArrowSize(m_arrow, &arrow_width, &arrow_height, 0);
		offset = target.x - (widget_image.x + stretch_border[3] + double(arrow_width)/2.);
		m_arrow.SetOffset(offset * 100. /
				(widget_image.width - stretch_border[3] - stretch_border[1] - arrow_width));
	}
	else if (target.x <= widget_image.x)
	{
		m_arrow.SetLeft();
		m_skin_element->GetArrowSize(m_arrow, &arrow_width, &arrow_height, 0);
		offset = target.y - (widget_image.y + stretch_border[0] + double(arrow_height)/2.);
		m_arrow.SetOffset(offset * 100.
				/ (widget_image.height - stretch_border[0] - stretch_border[2] - arrow_height));
	}
	else if (target.x >= widget_image.x + widget_image.width)
	{
		m_arrow.SetRight();
		m_skin_element->GetArrowSize(m_arrow, &arrow_width, &arrow_height, 0);
		offset = target.y - (widget_image.y + stretch_border[0] + double(arrow_height)/2.);
		m_arrow.SetOffset(offset * 100. /
				(widget_image.height - stretch_border[0] - stretch_border[2] - arrow_height));
	}
}

#endif // SKIN_SUPPORT
