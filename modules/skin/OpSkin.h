/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 2003-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef OPSKIN_H
#define OPSKIN_H

#include "modules/skin/skin_capabilities.h"
#include "modules/skin/skin_listeners.h"
#include "modules/skin/OpSkinElement.h"
#include "modules/util/adt/opvector.h"
#include "modules/util/OpHashTable.h"
#include "modules/util/opfile/opfile.h"
#include "modules/img/image.h"

class OpFileDescriptor;
class VisualDevice;
class PrefsFile;
class OpZip;
class ImageDecoder;

/***********************************************************************************
**
**	SkinImageContentProvider
**
***********************************************************************************/

class SkinImageContentProvider : public ImageContentProvider
{
public:
	SkinImageContentProvider(char* data, UINT32 numbytes)
		: data(data)
		, numbytes(numbytes)
		, content_type(0)
		, loaded(FALSE) { }
	~SkinImageContentProvider() { }

	OP_STATUS GetData(const char*& data, INT32& data_len, BOOL& more) { data = this->data; data_len = numbytes; more = FALSE; return OpStatus::OK; }
	OP_STATUS Grow() { return OpStatus::ERR; }
	void  ConsumeData(INT32 len) { loaded = TRUE; }

	void Reset() {}
	void Rewind() {}
	BOOL IsLoaded() { return TRUE; }
	INT32 ContentType() { return content_type; }
	void SetContentType(INT32 type) { content_type = type; }
	INT32 ContentSize() { return numbytes; }
	BOOL IsEqual(ImageContentProvider* content_provider) { return FALSE; } // FIXME:IMG-EMIL
private:
	const char* data;
	UINT32 numbytes;
	INT32 content_type;
	BOOL loaded;
};

/**
	OpSkin represents a skin file.
	It is managed by OpSkinManager and should not be created directly.
	It creates and manages OpSkinElements, applies color theme and scale.
*/

class OpSkin
#ifdef PERSONA_SKIN_SUPPORT
	: public SkinNotificationListener
#endif // PERSONA_SKIN_SUPPORT
{
	public:
		enum ColorSchemeMode
		{
			COLOR_SCHEME_MODE_NONE,
			COLOR_SCHEME_MODE_SYSTEM,
			COLOR_SCHEME_MODE_CUSTOM
		};
		enum ColorSchemeCustomType
		{
			COLOR_SCHEME_CUSTOM_NORMAL,			// the normal colorization 
			COLOR_SCHEME_CUSTOM_HUE_DARKER		// use a hue + darker double colorization
		};
								OpSkin();
		virtual					~OpSkin();

		/** Init the skin from the given file. Return OpStatus::OK if successfully loaded. */
		virtual OP_STATUS		InitL(OpFileDescriptor* skinfile);

		/** Removed all cached bitmaps so the skin is refreshed. */
		void					Flush();

		INT32					GetOptionValue(const char* option, INT32 def_value = 0);

#ifdef SKIN_SKIN_COLOR_THEME
		ColorSchemeMode			GetColorSchemeMode() {return m_color_scheme_mode;}
		INT32					GetColorSchemeColor() {return m_color_scheme_color;}
		void					SetColorSchemeMode(ColorSchemeMode mode);
		void					SetColorSchemeColor(INT32 color);
		ColorSchemeCustomType	GetColorSchemeCustomType() { return m_color_scheme_custom_type; }
		void					SetColorSchemeCustomType(ColorSchemeCustomType type);
#endif

		/** Set the scale of the skins. If not 100, the skin images will be scaled using the given scale. */
		void					SetScale(INT32 scale) {m_scale = scale; Flush();}
		INT32					GetScale() {return m_scale;}

		/** Apply the current color scheme on the given color. */
		COLORREF				ColorizeColor(COLORREF color);

		INT32					ReadColorL(const char* name, const char* key, INT32 default_color, BOOL colorize = TRUE);

		short					ReadBorderStyleL(const char* name, const char* key, short default_style);

		/** Read a combination of 1-4 integers into the array four_values which should be at least 4 values long.
			It will accept shorthands using 1, 2, 3 values, as well as the full 4 values.
			Returns TRUE on success (and all four values are set) or FALSE if it read nothing and four_values is unchanged. */
		BOOL					ReadTopRightBottomLeftL(const char* name, const char* key, UINT8 *four_values);

		void					ReadTextShadowL(const char* name, const char* key, OpSkinTextShadow *text_shadow);

		// Get a skin element

		OpSkinElement*			GetSkinElement(const char* name, SkinType type = SKINTYPE_DEFAULT, SkinSize size = SKINSIZE_DEFAULT);

		// Remove a skin element

		OP_STATUS				RemoveSkinElement(const char* name, SkinType type = SKINTYPE_DEFAULT, SkinSize size = SKINSIZE_DEFAULT);

		// functions used by OpSkinElement

		Image					GetBitmap(const uni_char* filename, BOOL skin_coloring, BOOL scalable, OpSkinElement::StateElement* state_element);
		PrefsFile*				GetPrefsFile() {return m_inifile;}

#ifdef ASYNC_IMAGE_DECODERS
		void					ContinueLoadAllSkinElements();
#endif

		UINT32					GetSkinVersion() { return m_version; }
		void					SetSkinVersion(UINT32 version) { m_version = version; }

		/** Get some basic skin information */
		const OpStringC			GetSkinAuthor();
		const OpStringC			GetSkinName();

#ifdef SKIN_SKIN_COLOR_THEME
		virtual INT32			GetDefaultColorScheme();
#endif // SKIN_SKIN_COLOR_THEME

	protected:
		OP_STATUS				LoadIniFileL(OpFileDescriptor* skinfile, const uni_char *ini_file);
#ifdef SKIN_SKIN_COLOR_THEME
		virtual void 			UpdateCurrentDefaultColorScheme();
#endif // SKIN_SKIN_COLOR_THEME

#ifdef PERSONA_SKIN_SUPPORT
		virtual void			OnSkinLoaded(OpSkin *loaded_skin) {}
		virtual void			OnPersonaLoaded(OpPersona *loaded_persona);
#endif // PERSONA_SKIN_SUPPORT

	private:

#ifdef ASYNC_IMAGE_DECODERS
		INT32					m_current_skin_element;
		INT32					m_continue_loading_called;
#endif

		class SkinKeyHasher : public OpHashFunctions
		{
			virtual UINT32					Hash(const void* key);
			virtual BOOL					KeysAreEqual(const void* key1, const void* key2);
		};

		class NamedBitmap
		{
			public:

				NamedBitmap(Image& image, ImageContentProvider* contentprovider, ImageListener* imagelistener)
				{
					m_image = image;
					m_contentprovider = contentprovider;
					m_imageListener = imagelistener;
				}
				virtual ~NamedBitmap()
				{
					m_image.DecVisible(m_imageListener);
					m_image.Empty();
					imgManager->ResetImage(m_contentprovider);
					OP_DELETE(m_contentprovider);
				}

				OP_STATUS Construct(const uni_char* name)
				{
					return m_name.Set(name);
				}

				OpString	m_name;
				Image		m_image;
				ImageContentProvider* m_contentprovider;
				ImageListener* m_imageListener;
		};

		OP_STATUS						LoadBitmap(const uni_char* filename, Image& image, ImageContentProvider** contentprovider, ImageListener* listener);

		OpHashTable						m_named_elements;
		OpStringHashTable<NamedBitmap>	m_named_bitmaps;

		SkinKeyHasher					m_skinkey_hasher;

		PrefsFile*						m_inifile;
		OpZip*							m_zip;
		ColorSchemeMode					m_color_scheme_mode;
		ColorSchemeCustomType			m_color_scheme_custom_type;
		INT32							m_color_scheme_color;
		INT32							m_current_default_color_scheme_color;
		INT32							m_default_color_scheme_color;
		BOOL							m_default_color_scheme_checked;
		INT32							m_scale;
		UINT32							m_version;

		//just for housekeeping

		OpVector<NamedBitmap>		m_housekeeping_bitmaps;
		OpVector<OpSkinElement>		m_housekeeping_elements;
};

#endif // !OPSKIN_H
