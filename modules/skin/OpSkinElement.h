/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
**
** File : OpSkinElement.h
**
**
*/
#ifndef _SKIN_OPSKINELEMENT_H_
#define _SKIN_OPSKINELEMENT_H_

#include "modules/util/adt/oplisteners.h"
#include "modules/img/image.h"
//#include "modules/skin/OpSkinAnimHandler.h"

# ifdef SKIN_SUPPORT

struct Border;
class OpSkin;
class VisualDevice;
class PrefsFile;
class OpSkinAnimationHandler;

#define SKIN_DEFAULT_MARGIN		0
/**
* changed to TWEAK_SKIN_DEFAULT_PADDING 
* #define SKIN_DEFAULT_PADDING	2
*/
#define SKIN_DEFAULT_SPACING	0

enum SkinColor
{
	SKINCOLOR_NONE = -1,
	SKINCOLOR_WINDOW = -2,
	SKINCOLOR_WINDOW_DISABLED = -3
};

enum SkinType
{
	SKINTYPE_DEFAULT,
	SKINTYPE_LEFT,
	SKINTYPE_TOP,
	SKINTYPE_RIGHT,
	SKINTYPE_BOTTOM,
	SKINTYPE_TOTAL					// must be last
};

enum SkinSize
{
	SKINSIZE_DEFAULT,
	SKINSIZE_LARGE,
	SKINSIZE_TOTAL					// must be last
};

/**
	States of the skin elements. The order of this list has some importance, since the closest skin state can be used if a skin
	doesn't specify a state that is used.
*/

enum SkinState
{
	SKINSTATE_DISABLED		= 0x001,
	SKINSTATE_ATTENTION		= 0x002,
	SKINSTATE_HOVER			= 0x004,
	SKINSTATE_FOCUSED		= 0x008,
	SKINSTATE_OPEN			= 0x010,
	SKINSTATE_SELECTED		= 0x020,
	SKINSTATE_PRESSED		= 0x040,
	SKINSTATE_MINI			= 0x080,
	SKINSTATE_INDETERMINATE = 0x100,
	SKINSTATE_RTL			= 0x200,

// Three scenarios of high resolution state
// 1. Implicit section and only low res skin bitmap
// 2. Implicit section with both high and low res skin bitmap
// 3. Explicit section, which means the skin bitmap is high res
	SKINSTATE_HIGHRES		= 0x400,

	SKINSTATE_TOTAL			= 0x800		// must be last
};

enum SkinPart
{
	SKINPART_TILE_CENTER = 0,
	SKINPART_TILE_LEFT,
	SKINPART_TILE_TOP,
	SKINPART_TILE_RIGHT,
	SKINPART_TILE_BOTTOM,
	SKINPART_CORNER_TOPLEFT,
	SKINPART_CORNER_TOPRIGHT,
	SKINPART_CORNER_BOTTOMRIGHT,
	SKINPART_CORNER_BOTTOMLEFT,
	SKINPART_TOTAL					// must be last
};

/** Specifies if some of the edges should display a arrow, and at which position it should be displayed. */

class SkinArrow
{
public:
	SkinArrow() { Reset(); }

	void Reset() { part = SKINPART_TILE_CENTER; offset = 0; }
	void SetLeft() { part = SKINPART_TILE_LEFT; }
	void SetTop() { part = SKINPART_TILE_TOP; }
	void SetRight() { part = SKINPART_TILE_RIGHT; }
	void SetBottom() { part = SKINPART_TILE_BOTTOM; }

	/** Set the offset on the edge where the arrow should be displayed.
		offset is percent from 0-100. The StretchBorder value is used as start and stop margins
		(So f.ex offset value 0 will draw a top arrow inset X pixels by the StretchBorder value. */
	void SetOffset(double offset);
public:
	SkinPart part;					///< SKINPART_TILE_CENTER means arrow is disabled.
	double offset;						///< Offset increasing to right or down (if top/bottom or left/right) from the end of the StretchBorder value.
};

struct SkinKey						// used for hash look up
{
	const char*		m_name;
	SkinType		m_type;
	SkinSize		m_size;
};

#ifdef ANIMATED_SKIN_SUPPORT
class OpSkinAnimationListener
{
public:
	virtual	~OpSkinAnimationListener() {}
	virtual void OnAnimatedImageChanged() = 0;
};
#endif // ANIMATED_SKIN_SUPPORT

class OpSkinElementListener
{
public:
	virtual ~OpSkinElementListener() {}
	virtual void OnSkinElementDeleted() = 0;
};

/** Text shadow data for skin state element */
class OpSkinTextShadow
{
public:
	OpSkinTextShadow() : ofs_x(0), ofs_y(0), radius(0), color(OP_RGB(0, 0, 0)) {}
public:
	int ofs_x, ofs_y, radius;
	UINT32 color;
};


/**
 * OpSkinElement is responsible for drawing specified skin element. Platforms
 * implement drawing of native UI elements by overriding methods in this class.
 */
class OpSkinElement
#ifdef ANIMATED_SKIN_SUPPORT
		: public OpSkinAnimationListener
#endif // ANIMATED_SKIN_SUPPORT
{
#ifdef ANIMATED_SKIN_SUPPORT
	friend class OpSkinAnimationHandler;
#endif // ANIMATED_SKIN_SUPPORT
#ifdef PERSONA_SKIN_SUPPORT
	friend class OpPersona;
#endif // PERSONA_SKIN_SUPPORT

	public:

							OpSkinElement(OpSkin* skin, const OpStringC8& name, SkinType type, SkinSize size);

		virtual				~OpSkinElement();

		/** Initialize the skin element. Must be called before the object is being used. May be overridden, but always call base implementation first. */
		virtual OP_STATUS	Init(const char *name);

#ifdef SKIN_NATIVE_ELEMENT
		/** Create a skin element that's native to the current platform
		 * Implement this on your platform and override various OpSkinElement
		 * functions to allow for native skinning
		 */
		static OpSkinElement* CreateNativeElement(OpSkin* skin, const OpStringC8& name, SkinType type, SkinSize size);

		/** Flush any global caches or other data kept about native skin elements
		 * so that the skin is refreshed.
		 * Empty implementation is allowed here, if the implementation of OpSkinElement
		 * on your platform does not save data outside of the OpSkinElement structure.
		 */
		static void			FlushNativeElements();

#endif // SKIN_NATIVE_ELEMENT

		/** Wrapper around creating a element, native or normal.
		 */
		static OpSkinElement* CreateElement(bool native, OpSkin* skin, const OpStringC8& name, SkinType type, SkinSize size);

		struct DrawArguments
		{
			DrawArguments()
				: hover_value(0)
				, clip_rect(NULL)
				, arrow(NULL)
				, forced_effect(0)
				, forced_effect_value(0)
				, colorize_background(FALSE)
				, background_color(OP_RGB(0xff,0x00,0x00))
			{}

			INT32		hover_value;
			const		OpRect *clip_rect;
			const		SkinArrow *arrow;
			INT32		forced_effect;
			INT32		forced_effect_value;
			BOOL		colorize_background;
			COLORREF	background_color;
		};

		OP_STATUS			Draw(VisualDevice* vd, const OpPoint& point, INT32 state, INT32 hover_value, const OpRect* clip_rect);

		/**
		 * @deprecated use Draw(VisualDevice*, OpRect) or Draw(VisualDevice*, OpRect, DrawArguments)
		 */
		DEPRECATED( virtual OP_STATUS Draw(VisualDevice* vd, OpRect rect, INT32 state, INT32 hover_value, const OpRect* clip_rect) );

		virtual OP_STATUS	Draw(VisualDevice* vd, OpRect rect, INT32 state, DrawArguments args);
		OP_STATUS			Draw(VisualDevice* vd, OpRect rect, INT32 state);

		virtual OP_STATUS	DrawOverlay(VisualDevice* vd, OpRect rect, INT32 state, INT32 hover_value, const OpRect* clip_rect);
		virtual OP_STATUS	DrawWithEdgeArrow(VisualDevice* vd, OpRect rect, INT32 state, DrawArguments args);

#ifdef SKIN_OUTLINE_SUPPORT
		/** Draw a specific skin part. Tile-parts will be tiled in rect and other parts will just be blitted at rect.x, rect.y */
		OP_STATUS			DrawSkinPart(VisualDevice* vd, OpRect rect, SkinPart part);
		virtual OP_STATUS	GetBorderWidth(INT32* left, INT32* top, INT32* right, INT32* bottom, INT32 state);
#endif

		virtual OP_STATUS	GetSize(INT32* width, INT32* height, INT32 state);
		virtual OP_STATUS	GetMinSize(INT32* minwidth, INT32* minheight, INT32 state);
		virtual OP_STATUS	GetMargin(INT32* left, INT32* top, INT32* right, INT32* bottom, INT32 state);
		virtual OP_STATUS	GetPadding(INT32* left, INT32* top, INT32* right, INT32* bottom, INT32 state);
		virtual OP_STATUS	GetStretchBorder(UINT8 *left, UINT8* top, UINT8* right, UINT8* bottom, INT32 state);
		virtual OP_STATUS	GetArrowSize(const SkinArrow &arrow, INT32* width, INT32* height, INT32 state);
		virtual OP_STATUS	GetSpacing(INT32* spacing, INT32 state);
		virtual OP_STATUS	GetBackgroundColor(UINT32* color, INT32 state);
		virtual OP_STATUS	GetTextColor(UINT32* color, INT32 state);
		virtual OP_STATUS	GetLinkColor(UINT32* color, INT32 state);
		virtual OP_STATUS	GetTextShadow(const OpSkinTextShadow*& shadow, INT32 state);
		OP_STATUS			GetTextBold(BOOL* bold, INT32 state);
		OP_STATUS			GetTextUnderline(BOOL* underline, INT32 state);
		OP_STATUS			GetTextZoom(BOOL* zoom, INT32 state);
		OP_STATUS			GetButtonTextPadding(INT32 *padding, INT32 state);
		OP_STATUS			GetRadius(UINT8 *values, INT32 state);
		Image				GetImage(INT32 state, SkinPart part = SKINPART_TILE_CENTER);

		/** Creates and return a new OpBitmap. The ownership of the bitmap is passed to caller.
			This is not a standard function, and is only available if you have a special skin element with support for it. */
		virtual OP_STATUS	GetBitmap(OpBitmap*& bitmap, INT32 state, int width, int height) {return OpStatus::ERR_NOT_SUPPORTED;}

		SkinKey*			GetKey() {return &m_key;}
		SkinType			GetType() {return m_key.m_type;}
		SkinSize			GetSize() {return m_key.m_size;}

		virtual BOOL		IsLoaded() { return m_state_elements.GetCount() && m_state_elements.Get(0) != NULL; }
		BOOL				HasBoldState() {return m_has_bold_state;}

#ifdef SKINNABLE_AREA_ELEMENT
	    OP_STATUS            GetOutlineColor(UINT32* color, INT32 state);
	    OP_STATUS            GetOutlineWidth(UINT32* color, INT32 state);
#endif //SKINNABLE_AREA_ELEMENT

#ifdef USE_GRADIENT_SKIN
		OP_STATUS            GetGradientColors(UINT32* min_color, UINT32* color, UINT32* min_border_color, UINT32* border_color, UINT32* bottom_border_color, INT32 state);
#endif // USE_GRADIENT_SKIN

#ifdef ANIMATED_SKIN_SUPPORT
		// OpSkinAnimationListener implementation

		/** Called on animated skin images when a new frame is available */
		void OnAnimatedImageChanged();

		void SetAnimationListener(OpSkinAnimationListener *listener);
		void RemoveAnimationListener(OpSkinAnimationListener *listener);
		void StartAnimation(BOOL start, VisualDevice *vd, BOOL animate_from_beginning);
#endif // ANIMATED_SKIN_SUPPORT

		OP_STATUS AddSkinElementListener(OpSkinElementListener *listener) { return m_element_listeners.Add(listener); }
		OP_STATUS RemoveSkinElementListener(OpSkinElementListener *listener) { return m_element_listeners.Remove(listener); }

#ifdef SKIN_ELEMENT_FALLBACK_DISABLE
		// fall back to this element if the version of the skin is >= the given version here
		UINT32 GetFallbackVersion() { return m_fallback_version; }
#endif // SKIN_ELEMENT_FALLBACK_DISABLE

		BOOL IsNative() const { return m_is_native; }

		virtual BOOL HasTransparentState();

// FIXME: ADS compiler does not allow the protected class StateElement
// being used as a type for a member attribute in StateElementRef.
// For now it is just made public to make it compile. markuso
	public:
		/** StateElement holds all info for the element in a certain state (Any combination of SkinState) */
		class StateElement : public ImageListener
		{
			OpAutoVector<OpString8> m_parentlist;

			public:

				enum Type
				{
					TypeImage,					///< TileCenter image will be centered
					TypeBox,					///< All tiles will be used (gaps between them if area to paint is larger)
					TypeBoxTile,				///< All tiles will be used, and tiled individually across the area to paint.
					TypeBoxTileVertical,		///< All tiles will be used, and tiled vertically across the area to paint.
					TypeBoxTileHorizontal,		///< All tiles will be used, and tiled horizontally across the area to paint.
					TypeBoxStretch,				///< TileCenter will be sliced in 9 parts automatically and individually stretched across the area to paint (Using the StretchBorder property as border thickness so border is stretched only along the edge).
					TypeBestFit					///< TileCenter will be used according to the "best fit aka crop aka proportional strech" rules.
				};
#ifdef ANIMATED_SKIN_SUPPORT
				~StateElement();
#endif // ANIMATED_SKIN_SUPPORT

				static StateElement* Create(OpSkin* skin, const char* name, INT32 state, StateElement* first_element, OpAutoVector<OpString8>* parentlist, OpSkinElement* skinElement, BOOL mirror_rtl, BOOL high_res);

				void				LoadByKeyL(OpSkin* skin, const char* name, OpSkinElement* skinElement, BOOL hires_suffix = FALSE);
				void				LoadBySectionL(OpSkin* skin, const char* name, OpSkinElement* skinElement = NULL, BOOL hires_suffix = FALSE);

				void				Draw(VisualDevice* vd, OpRect& rect, INT32 effect, INT32 effect_value, const OpRect* clip_rect, BOOL drop_if_larger = FALSE);
				void				DrawOverlay(VisualDevice* vd, OpRect& rect, INT32 effect, INT32 effect_value, const OpRect* clip_rect, BOOL drop_if_larger = FALSE);
				BOOL				DrawSkinArrow(VisualDevice* vd, const OpRect& rect, INT32 effect, INT32 effect_value, const SkinArrow &arrow, OpRect &used_rect);
#ifdef SKIN_OUTLINE_SUPPORT
				void				DrawSkinPart(VisualDevice* vd, OpRect& rect, SkinPart part);
#endif
#ifdef SKIN_SKIN_COLOR_THEME
				void				DrawColorizedBackground(VisualDevice* vd, OpRect& rect, INT32 effect, INT32 effect_value, const OpRect* clip_rect, COLORREF color);
#endif
				void				SetRadiusOnBorder(Border &border);

				void				OnPortionDecoded();
				void				OnError(OP_STATUS status);
				Image				GetImage(SkinPart part) { return m_images[part]; }
#ifdef ANIMATED_SKIN_SUPPORT
				void				StartAnimation(BOOL start, VisualDevice *vd, BOOL animate_from_beginning);
				Image				GetAnimatedImage(SkinPart part, VisualDevice *vd);
#endif // ANIMATED_SKIN_SUPPORT

				Type				m_type;
				Image				m_images[SKINPART_TOTAL];
				Image				m_scaled_image_cache;
				INT32				m_width;
				INT32				m_height;
				INT32				m_minwidth;
				INT32				m_minheight;

				INT32				m_margin_left;
				INT32				m_margin_top;
				INT32				m_margin_right;
				INT32				m_margin_bottom;
				INT32				m_padding_left;
				INT32				m_padding_top;
				INT32				m_padding_right;
				INT32				m_padding_bottom;
				INT32				m_spacing;
				INT32				m_color;
				INT32				m_text_color;
				INT32				m_text_link_color;
				INT32				m_text_bold;
				INT32				m_text_underline;
				INT32				m_text_zoom;
				INT32				m_text_button_padding;		// overrides the global setting
				INT32				m_blend;
				BOOL				m_clear_background;
				UINT8				m_border[4];
				INT32				m_border_color;
				short				m_border_style;
				UINT8				m_stretch_border[4];
				UINT8				m_radius[4];
				UINT8				m_colorized_background_padding[4];
				UINT8				m_colorized_background_radius[4];
				INT32				m_state;					// Holds it own state, for special settings for different states
				UINT8				m_child_index;				// If this is a Child0 or Overlay0 element it will be 0 (and so on for Child1 etc.). It's -1 if it's not a child element.
				BOOL				m_colorize;					// Colorize the skin or not
				OpAutoVector<ImageListener> m_child_elements;
				OpAutoVector<ImageListener> m_overlay_elements;
				OpSkinElement*		m_parent_skinelement;

#ifdef SKINNABLE_AREA_ELEMENT
			    UINT32              m_outline_color;
			    UINT32              m_outline_width;
#endif //SKINNABLE_AREA_ELEMENT

#ifdef USE_GRADIENT_SKIN
				INT32				m_min_color;
				INT32				m_min_border_color;
				INT32				m_bottom_border_color;
#endif // USE_GRADIENT_SKIN

				OpSkinTextShadow	m_text_shadow;

				/// The animation handler for animated images
#ifdef ANIMATED_SKIN_SUPPORT
				OpSkinAnimationHandler* m_animation_handlers[SKINPART_TOTAL];
#endif // ANIMATED_SKIN_SUPPORT

#ifdef SKIN_ELEMENT_FALLBACK_DISABLE
				// used to set the parent skin element with this value
				UINT32				m_fallback_version;
#endif // SKIN_ELEMENT_FALLBACK_DISABLE

				BOOL				m_mirror_rtl;

				BOOL				m_drop_if_larger;

#ifdef PIXEL_SCALE_RENDERING_SUPPORT
				// these are used to calculate the dest rect for rendering
				INT32				m_tilewidth[SKINPART_TOTAL];
				INT32				m_tileheight[SKINPART_TOTAL];
#endif // PIXEL_SCALE_RENDERING_SUPPORT

		private:
			void Blit(VisualDevice* vd, Image image, INT32 x, INT32 y,
					  int dst_width, int dst_height, int effect, int effect_value);
			void BlitClip(VisualDevice* vd, Image image, const OpRect &src_rect, const OpRect &dst_rect, int effect, int effect_value);
			void BlitCentered(VisualDevice* vd, Image image, INT32 x, INT32 y,
							  BOOL center_x, BOOL center_y, int dst_width, int dst_height,
							  int effect, int effect_value, const OpRect* clip_rect);
			void BlitStretch(VisualDevice* vd, Image image, const OpRect &rect, int effect, int effect_value, const OpRect* clip_rect);
			void BlitStretch(VisualDevice* vd, Image image, const OpRect &dest, const OpRect &source, int effect, int effect_value);
			void BlitTiled(VisualDevice* vd, Image image, OpRect rect, int ofs_x, int ofs_y,
						   int effect, int effect_value, const OpRect* clip_rect,
						   int tilewidth, int tileheight);
		};
	protected:

		BOOL				m_is_native;

// FIXME: ADS compiler does not allow encapsulated class StateElement to access private method OverrideDefaults()
//        For now it is just made public to make it compile. steink
	public:
		virtual void		OverrideDefaults(OpSkinElement::StateElement* se) {}

#ifdef PERSONA_SKIN_SUPPORT
		/** If we have a persona, we need to override certain states */
		virtual void		OverridePersonaDefaults(OpSkinElement::StateElement* se);
#endif // PERSONA_SKIN_SUPPORT

	private:
#ifdef ANIMATED_SKIN_SUPPORT
		class StateElementRef : public Link
		{
		public:
			StateElementRef(StateElement* elm, VisualDevice *vd, SkinPart image_id)
				:	m_elm(elm), m_image_id(image_id), m_vd(vd) {}
			StateElement* m_elm;
			SkinPart m_image_id;
			VisualDevice *m_vd;
		};
#endif // ANIMATED_SKIN_SUPPORT

		/** Load a StateElement for the given state and put it into the list of available state elements. */
		OP_STATUS			LoadStateElement(INT32 state, OpString8& state_element_name, BOOL mirror_for_rtl = FALSE, BOOL high_res = FALSE);

		/** Get a state element.
		 *  @param[in] fallback_to_closest If TRUE and there is no StateElement for that state,
			it will return the closest state element. (Based on the
		order of the states, in the SkinState enum) 
		 *  @param[in] load_if_not_exist Try to load missing states if TRUE */
		StateElement*		GetStateElement(INT32 state, BOOL fallback_to_closest = TRUE, BOOL load_if_not_exist = TRUE);

		/** Insert the state element into m_state_elements in order */
		OP_STATUS			InsertStateElement(StateElement* state_elm);

		SkinKey				m_key;
		OpString8			m_name;
		OpSkin*				m_skin;
		/** We load the state element for mini when used, instead of when OpSkinElement is initialized so we don't
			load and scale bitmaps that are never used. */
		BOOL				m_mini_state_known;
		BOOL				m_has_bold_state;
		BOOL				m_has_hover_state;
		/** List of state elements. The state elements are ordered after the state value (lowest first). */
		OpVector<StateElement> m_state_elements;

#ifdef ANIMATED_SKIN_SUPPORT
		OpListeners<OpSkinAnimationListener> m_animation_listeners;
#endif // ANIMATED_SKIN_SUPPORT
		OpListeners<OpSkinElementListener> m_element_listeners;
#ifdef SKIN_ELEMENT_FALLBACK_DISABLE
		// fall back to this element if the version of the skin is >= the given version here
		UINT32				m_fallback_version;
#endif // SKIN_ELEMENT_FALLBACK_DISABLE
		BOOL				m_mirror_rtl;
};

# endif // SKIN_SUPPORT

#endif // !_SKIN_OPSKINELEMENT_H_
