/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef OPWIDGETIMAGE_H
#define OPWIDGETIMAGE_H

#include "modules/skin/OpSkinElement.h"
#include "modules/layout/layout.h"

class VisualDevice;
class OpWidgetImage;
class OpSkinManager;

/** Listener for changes that might happen to a OpWidgetImage. */

class OpWidgetImageListener
{
	public:

		virtual			~OpWidgetImageListener() {}

		/** Called when something has changed with the image so it has to be repainted, and new size checked. */
		virtual void	OnImageChanged(OpWidgetImage* widget_image) = 0;

#ifdef ANIMATED_SKIN_SUPPORT
		/** Called on animated skin images when a new frame is to be drawn. */
		virtual void	OnAnimatedImageChanged(OpWidgetImage* widget_image){}
		/** Called to check if the listener really is visible. If not, the animation don't have to run. */
		virtual BOOL	IsAnimationVisible() { return FALSE; }
#endif
};

/** OpWidgetImage is used to parametrize drawing of single OpWidget
 *
 * This class can hold a specified OpSkinElement or Image (bitmap), and
 * parameters that are required to correctly draw parts of specific
 * OpWidget.
 *
 * This class is used heavily, it's important to keep memory usage of
 * single object as low as possible. Use m_packed struct to compress as
 * much members as possible.
 */
class OpWidgetImage : public OpSkinElementListener
#ifdef ANIMATED_SKIN_SUPPORT
				, public OpSkinAnimationListener
#endif // ANIMATED_SKIN_SUPPORT

{
	public:

							OpWidgetImage(BOOL foreground = TRUE);
							~OpWidgetImage();

		/** Set this image to contain the same image as widget_image */
		void				SetWidgetImage(const OpWidgetImage* widget_image);

		void				SetForeground(BOOL foreground) {m_packed.foreground = foreground;}
		BOOL				IsForeground() const {return m_packed.foreground;}

		void				SetSkinManager(OpSkinManager* skin_manager) {m_skin_manager = skin_manager;}

		/** Get the SkinArrow on which the edge arrow properties can be set. */
		SkinArrow*			GetArrow() { return &m_arrow; }

		void				PointArrow(const OpRect& widget_image, const OpRect& arrow_target);

		void				SetImageAndType(const char* preferred_image, SkinType type, SkinSize size, const char* fallback_image = NULL);
		void				SetImage(const char* preferred_image, const char* fallback_image = NULL) { SetImageAndType(preferred_image, (SkinType) m_packed.skin_type, (SkinSize) m_packed.skin_size, fallback_image); }
		void				SetPreferredImage(const char* preferred_image);

		void				SetType(SkinType type);
		void				SetSize(SkinSize size);

		const char*			GetImage() const;
		SkinType			GetType() const { return (SkinType) m_packed.skin_type; }
		SkinSize			GetSize() const { return (SkinSize) m_packed.skin_size; }

		INT32				GetHoverValue() const { return m_hover_value; }

		/** Turns on colorization for this object.
		 *
		 * Use this, if you want to colorize certain widget, and this widget
		 * uses skin.
		 *
		 * To use this, object has to have skin element defined; colorized
		 * area is defined through ColorBackgroundPadding and
		 * ColorBackgroundRadius in skin element.
		 *
		 * Alpha channel of color will affect skin element.
		 *
		 * @param color
		 */
		void				ApplyColorization(COLORREF color);
		void				ResetColorization();

		/** Use bitmap for this WidgetImage instead of SkinElement.
		 *
		 *  By default, bitmap identified by bitmap_image will be automatically
		 *  stretched to desired widget size. You can change default stretch
		 *  method to StretchBorder with @ref SetStretchBorder method.
		 *
		 *  @param bitmap_image
		 *  @param allow_empty_bitmap
		 */
		void				SetBitmapImage(Image &bitmap_image, BOOL allow_empty_bitmap = TRUE);
		Image				GetBitmapImage() const { return m_bitmap_image; }
		BOOL				HasBitmapImage() const { return !m_bitmap_image.IsEmpty(); }

		void				SetBitmapImageWithEffect(Image &bitmap_image, INT32 effect, INT32 effect_value);
		Image				GetEffectBitmapImage() const { return m_bitmap_effect_image; }
		BOOL				HasEffectBitmapImage() const { return !m_bitmap_effect_image.IsEmpty(); }
		INT32				GetBitmapEffect() const { return m_effect; }
		INT32				GetBitmapEffectValue() const { return m_effect_value; }

#ifdef SKIN_SKIN_COLOR_THEME
		/** Set image and apply colorization.
		 * 
		 * Colorization is applied to bitmap before it is used in this object.
		 *
		 * Use this, if you want to colorize certain widget, and this widget
		 * uses bitmap.
		 *
		 * Adjust alpha channel of color parameter if you want to change alpha
		 * channel of colorized bitmap.
		 *
		 * @param image
		 * @param color
		 */
		void				SetColorizedBitmapImage(Image &bitmap_image, COLORREF color);
#endif // SKIN_SKIN_COLOR_THEME

		void				SetStretchBorder(UINT8 top, UINT8 right, UINT8 bottom, UINT8 left);
		Border				GetStretchBorder() const;

		void				UnsetStretchBorder();
		BOOL				HasStretchBorder() const;

		void				SetState(INT32 state, INT32 mask = -1, INT32 hover_value = 0);
		INT32				GetState() const {return m_state;}

		BOOL				HasContent();

		BOOL				HasImage() const {return m_packed.use_image == USE_PREFERRED_IMAGE || m_packed.use_image == USE_FALLBACK_IMAGE;}

		void				GetSize(INT32* width, INT32* height);
		void				GetMargin(INT32* left, INT32* top, INT32* right, INT32* bottom);
		void				GetPadding(INT32* left, INT32* top, INT32* right, INT32* bottom);
		void				GetSpacing(INT32* spacing);

		void				GetButtonTextPadding(INT32 *padding);	// overrides the global setting

		void				AddMargin(OpRect& rect);
		void				AddPadding(OpRect& rect);

	
		/** Draw the content on the given pos with the size of its content. */
		void				Draw(VisualDevice* vd, OpPoint pos, const OpRect* clip_rect = NULL);

		/** Draw the content on the given rect (stretched).
			@param overlay If TRUE, only the skin element overlays will be painted (if any). If FALSE no overlays will be painted. */
		void				Draw(VisualDevice* vd, OpRect rect, const OpRect* clip_rect = NULL, INT32 state = 0, BOOL overlay = FALSE);

		/** Get the size of the image, or the restricted size if it is set to be restricted with SetRestrictImageSize. */
		void				GetRestrictedSize(INT32* width, INT32* height);

		/** Calculate a rect proportionally correct inside the available_rect for this image.
			center_x and center_y specify if it should be centered horizontally and vertically in available_rect or
			just at the first available position. */
		OpRect				CalculateScaledRect(const OpRect& available_rect, BOOL center_x, BOOL center_y);

		/** Set the listener of this image. */
		void				SetListener(OpWidgetImageListener* listener) {m_listener = listener;}

		/** Restrict the image to 16x16 or the size specified in skin element
		 * named through SetRestrictingImage. If no element was named, "Window Document Icon"
		 * will be used (in the call to GetRestrictedSize).
		 */
		void				SetRestrictImageSize(BOOL b) { m_packed.restrict_image_size = b; }

		void				SetRestrictingImage(const char* restricting_image);

		/** @return TRUE if the image is restricted in size (in the call to GetRestrictedSize) */
		BOOL				GetRestrictImageSize() const { return m_packed.restrict_image_size; }

		/** Reset some of the members, but not all. That is the best explanation i have for now. If you need something
			like this, tell the module owner what you need so this can be properly defined or recreated as new a function. */
		void				Empty();

		// From OpSkinElementListener
		void				OnSkinElementDeleted();

#ifdef ANIMATED_SKIN_SUPPORT
		// From OpSkinAnimationListener
		virtual void		OnAnimatedImageChanged();

		void				StartAnimation(BOOL start, BOOL animate_from_beginning = FALSE);
#endif
		BOOL				HasSkinElement();

		BOOL				HasFallbackElement() { return m_fallback_image.HasContent(); }
		const char*			GetFallbackElement() { return m_fallback_image.CStr(); }

		OpSkinElement*		GetSkinElement();
	private:
		void				UpdateUseImage();

		enum ImageType
		{
			  USE_NONE = 0
			, USE_PREFERRED_IMAGE
			, USE_FALLBACK_IMAGE
			, USE_BITMAP_IMAGE
			, USE_EFFECT_BITMAP_IMAGE
			// If more image types is added, be sure that m_packed.use_image has enough bits!
		};

		OpString8				m_preferred_image;
		OpString8				m_fallback_image;
		OpSkinElement*			m_skin_element;
		Image					m_bitmap_image;
		Image					m_bitmap_effect_image;
		OpWidgetImageListener*	m_listener;
		OpSkinManager*			m_skin_manager;
		INT32					m_state;
		INT32					m_hover_value;
		UINT8					m_stretch_border_top;
		UINT8					m_stretch_border_right;
		UINT8					m_stretch_border_bottom;
		UINT8					m_stretch_border_left;
		COLORREF				m_color;
		INT32					m_effect;
		INT32					m_effect_value;
		union
		{
			struct
			{
				unsigned int	foreground:1;
				unsigned int	restrict_image_size:1;

				// Use 'unsigned int' instead of the corresponding enum types
				// to gain an extra usable bit on Visual Studio, which
				// otherwise treats the types as signed. This also works around
				// a compilation problem on Brew (see
				// https://wiki.oslo.osa/developerwiki/ADS#Privacy_issues_within_a_class.27s_member_classes)

				unsigned int	use_image:5; ///< Must be able to hold a ImageType!
				unsigned int	skin_type:6; ///< Must be able to hold a SkinType!
				unsigned int	skin_size:3; ///< Must be able to hold a SkinSize!
#ifdef ANIMATED_SKIN_SUPPORT
				unsigned int	animation_started:1;
#endif
				bool			has_stretch_border:1;
				bool			has_background_color:1;
			} m_packed;
			unsigned int m_packed_init;
		};
		SkinArrow m_arrow;
		OpString8				m_restricting_image;
};

#endif // OPWIDGETIMAGE_H
