/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 2003-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef OPSKINMANAGER_H
#define OPSKINMANAGER_H

#ifdef SKIN_SUPPORT

#include "modules/pi/ui/OpUiInfo.h"
#include "modules/skin/skin_listeners.h"
#include "modules/skin/OpSkinElement.h"
#include "modules/skin/OpSkin.h"
#ifdef PERSONA_SKIN_SUPPORT
#include "modules/skin/OpPersona.h"
#endif // PERSONA_SKIN_SUPPORT
#include "modules/util/adt/opvector.h"
#ifdef QUICK
# include "adjunct/desktop_util/treemodel/simpletreemodel.h"
#endif // QUICK

class VisualDevice;
class OpFile;
class OpFileDescriptor;
class OpWindow;
class OpSkin;
class OpPersona;

/**
	Handles skinning. Used by f.ex widgets, inline smileys, highlighting etc.
	Skinfile documentation can be found at: http://my.opera.com/community/customize/skins/specs/

	There can be 2 skins specified with a fallback mecanism if a skin element is not available in one of them.
	The "default skin" should be a skin that contains all needed elements. The other skin ("current skin") doesn't have to
	contain all kinds of elements because if it's missing something it will be taken from the default skin.
*/

class OpSkinManager
{
	public:
								OpSkinManager();
		virtual					~OpSkinManager();

		/** Initialize and load the skin file specified in preferences.
			Returns status of loading the skin file.
			global_skin_manager should be TRUE if this skinmanager is intended to be used as the global skinmanager used by forms and UI by default. */
		OP_STATUS				InitL(BOOL global_skin_manager = TRUE);

		/** Removed all cached bitmaps so the skin is refreshed. */
		void					Flush();

		enum SkinFileType
		{
			SkinUnknown = 0
			, SkinNormal		// our normal skin
#ifdef PERSONA_SKIN_SUPPORT
			, SkinPersona		// the persona that is on top of whatever skin we have active
#endif // PERSONA_SKIN_SUPPORT
		};

#ifdef QUICK

		void					ScanSkinFolderIfNeeded();
		void					ScanSkinFolderL();
		void					ScanFolderL(const OpStringC& searchtemplate, const OpStringC& rootfolder, BOOL is_defaults=FALSE);

		OP_STATUS				SelectSkin(INT32 index);
#endif
		OP_STATUS				SelectSkinByFile(OpFile* skinfile, BOOL rescan_skin_folder = TRUE);
		OpSkin*					CreateSkin(OpFileDescriptor* skinfile);
#ifdef PERSONA_SKIN_SUPPORT
		class OpPersona*		CreatePersona(OpFileDescriptor* personafile);
		OP_STATUS				SelectPersonaByFile(OpFile *skinfile, BOOL rescan_skin_folder = TRUE);
		void					BroadcastPersonaLoaded(OpPersona *skin);
#endif // PERSONA_SKIN_SUPPORT

#ifdef QUICK
		OP_STATUS				DeleteSkin(INT32 index);

		INT32					GetSkinCount();
		INT32					GetSelectedSkin();
		OpTreeModel*			GetTreeModel();
		void					UpdateTreeModel();
		void					RemovePosFromList(UINT32 pos);

		/** @return Whether this is a skin that was installed by the user (as opposed to coming with Opera)
		  */
		BOOL					IsUserInstalledSkin(INT32 index);

		const uni_char*			GetSkinName(INT32 index);
#endif // QUICK

#ifdef PERSONA_SKIN_SUPPORT
		/** @return Whether we have an active persona or not
		  */
		BOOL					HasPersonaSkin() { return m_persona_skin != NULL; }
#endif // PERSONA_SKIN_SUPPORT


		SkinFileType			GetSkinFileType(const OpStringC& skinfile);

		void 					SetTransparentBackgroundColor(UINT32 col){ m_transparent_background_color = col; }
		UINT32 					GetTransparentBackgroundColor(){ return m_transparent_background_color; }
		OP_STATUS				AddTransparentBackgroundListener(TransparentBackgroundListener* tl) { return m_transparent_background_listeners.Add(tl); }
		void 					RemoveTransparentBackgroundListener(TransparentBackgroundListener* tl) { m_transparent_background_listeners.RemoveByItem(tl); }
		void 					TrigBackgroundCleared(OpPainter* painter, const OpRect& r);
		void					BroadcastSkinLoaded(OpSkin *skin);
		void 					AddSkinNotficationListener(SkinNotificationListener* listener){ OpStatus::Ignore(m_skin_notification_listeners.Add(listener)); }
		void 					RemoveSkinNotficationListener(SkinNotificationListener* listener){ OpStatus::Ignore(m_skin_notification_listeners.Remove(listener)); }

		INT32					GetOptionValue(const char* option, INT32 def_value = 0);

#ifdef SKIN_SKIN_COLOR_THEME
		OpSkin::ColorSchemeMode	GetColorSchemeMode() {return m_color_scheme_mode;}
		INT32					GetColorSchemeColor() {return m_color_scheme_color;}
		void					SetColorSchemeMode(OpSkin::ColorSchemeMode mode);
		void					SetColorSchemeColor(INT32 color);
		OpSkin::ColorSchemeCustomType	GetColorSchemeCustomType() { return m_color_scheme_custom_type; }
		void					SetColorSchemeCustomType(OpSkin::ColorSchemeCustomType type);
#endif

		/** Set the scale of the skins. If not 100, the skin images will be scaled using the given scale. */
		void					SetScale(INT32 scale);
		INT32					GetScale() {return m_scale;}

		// get skin element

		/** Get a OpSkinElement that match the given name, type and size.
			The element will be taken from the current skin. If it's a foreground-skin
			and not available in the current skin, it will be taken from the default skin. */
		OpSkinElement*			GetSkinElement(const char* name, SkinType type = SKINTYPE_DEFAULT, SkinSize size = SKINSIZE_DEFAULT, BOOL foreground = FALSE);

		OP_STATUS				DrawElement(VisualDevice* vd, const char* name, const OpPoint& point, INT32 state = 0, INT32 state_value = 0, const OpRect* clip_rect = NULL, SkinType type = SKINTYPE_DEFAULT, SkinSize size = SKINSIZE_DEFAULT, BOOL foreground = TRUE);
		OP_STATUS				DrawElement(VisualDevice* vd, const char* name, const OpRect& rect, INT32 state = 0, INT32 state_value = 0, const OpRect* clip_rect = NULL, SkinType type = SKINTYPE_DEFAULT, SkinSize size = SKINSIZE_DEFAULT, BOOL foreground = TRUE);
		OP_STATUS				GetSize(const char* name, INT32* width, INT32* height, INT32 state = 0, SkinType type = SKINTYPE_DEFAULT, SkinSize size = SKINSIZE_DEFAULT, BOOL foreground = TRUE);

		OP_STATUS				GetMargin(const char* name, INT32* left, INT32* top, INT32* right, INT32* bottom, INT32 state = 0, SkinType type = SKINTYPE_DEFAULT, SkinSize size = SKINSIZE_DEFAULT, BOOL foreground = FALSE);
		OP_STATUS				GetPadding(const char* name, INT32* left, INT32* top, INT32* right, INT32* bottom, INT32 state = 0, SkinType type = SKINTYPE_DEFAULT, SkinSize size = SKINSIZE_DEFAULT, BOOL foreground = FALSE);
		OP_STATUS				GetSpacing(const char* name, INT32* spacing, INT32 state = 0, SkinType type = SKINTYPE_DEFAULT, SkinSize size = SKINSIZE_DEFAULT, BOOL foreground = FALSE);
		OP_STATUS				GetBackgroundColor(const char* name, UINT32* color, INT32 state = 0, SkinType type = SKINTYPE_DEFAULT, SkinSize size = SKINSIZE_DEFAULT, BOOL foreground = FALSE);
		OP_STATUS				GetTextColor(const char* name, UINT32* color, INT32 state = 0, SkinType type = SKINTYPE_DEFAULT, SkinSize size = SKINSIZE_DEFAULT, BOOL foreground = FALSE);
		OP_STATUS				GetTextBold(const char* name, BOOL* bold, INT32 state, SkinType type, SkinSize size = SKINSIZE_DEFAULT, BOOL foreground = FALSE);
		OP_STATUS				GetTextUnderline(const char* name, BOOL* underline, INT32 state, SkinType type, SkinSize size = SKINSIZE_DEFAULT, BOOL foreground = FALSE);
		OP_STATUS				GetTextZoom(const char* name, BOOL* zoom, INT32 state, SkinType type, SkinSize size = SKINSIZE_DEFAULT, BOOL foreground = FALSE);

		/** Return the current skin. */
		OpSkin*					GetCurrentSkin() { return m_skin ? m_skin : m_defaultskin; }
#ifdef PERSONA_SKIN_SUPPORT
		/** Returns the active persona or NULL if no persona is active */
		OpPersona*				GetPersona() { return m_persona_skin; }
#endif // PERSONA_SKIN_SUPPORT

		/** Skin can override the system color. If the skin has no color, the true system color will be returned. */
		UINT32					GetSystemColor(OP_SYSTEM_COLOR color);
		UINT32					GetFieldsetBorderColor() { return m_fieldset_border_color; }
		BOOL					GetDrawFocusRect() { return m_draw_focus_rect; }
		UINT32					GetLinePadding() { return m_line_padding; }
		BOOL					GetDimDisabledBackgrounds() { return m_dim_disabled_backgrounds; }
		BOOL					GetGlowOnHover() { return m_glow_on_hover; }

#ifdef ANIMATED_SKIN_SUPPORT
		/** SetAnimationListener - sets the listener to skin animation elements, if any */
		void					SetAnimationListener(const char *name, OpSkinAnimationListener *listener);

		/** RemoveAnimationListener - removes the listener to skin animation elements, if any */
		void					RemoveAnimationListener(const char *name, OpSkinAnimationListener *listener);

		/** StartAnimation - Starts or stops the named skin element animation, if any. */
		void					StartAnimation(BOOL start, const char *name, VisualDevice *vd, BOOL animate_from_beginning);
#endif // ANIMATED_SKIN_SUPPORT

	private:

		OpSkinElement*			GetSkinElement(OpSkin* skin, const char* name, SkinType type = SKINTYPE_DEFAULT, SkinSize size = SKINSIZE_DEFAULT, BOOL foreground = FALSE);

		OpSkin*					m_skin;
		OpSkin*					m_defaultskin;
#ifdef QUICK
		SimpleTreeModel			m_skin_list;				//< Filenames
#endif // QUICK
		INT32					m_selected_from_list;
		BOOL					m_is_global_skin_manager;
		UINT32					m_selected_text_color;
		UINT32					m_selected_text_bgcolor;
		UINT32					m_selected_text_color_nofocus;
		UINT32					m_selected_text_bgcolor_nofocus;
		UINT32					m_fieldset_border_color;
		BOOL					m_draw_focus_rect;
		UINT32					m_line_padding;
		OpSkin::ColorSchemeMode	m_color_scheme_mode;
		INT32					m_color_scheme_color;
		OpSkin::ColorSchemeCustomType m_color_scheme_custom_type;
		INT32					m_scale;
		BOOL					m_has_scanned_skin_folder;
		BOOL					m_dim_disabled_backgrounds;
		BOOL					m_glow_on_hover;
		UINT32					m_transparent_background_color;
		OpVector<TransparentBackgroundListener> m_transparent_background_listeners;
		OpListeners<SkinNotificationListener>		m_skin_notification_listeners;
#ifdef PERSONA_SKIN_SUPPORT
		OpPersona*				m_persona_skin;
#endif // PERSONA_SKIN_SUPPORT
};

#endif // SKIN_SUPPORT
#endif // !OPSKINMANAGER_H
