/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 2003-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#ifdef SKIN_SUPPORT

#include "modules/util/zipload.h"
#include "modules/util/hash.h"
#include "modules/skin/OpSkin.h"
#include "modules/skin/OpSkinElement.h"
#include "modules/skin/OpSkinManager.h"
#include "modules/skin/OpSkinUtils.h"

#include "modules/pi/OpBitmap.h"
#include "modules/pi/OpSystemInfo.h"
#include "modules/pi/ui/OpUiInfo.h"

#include "modules/prefs/prefsmanager/collections/pc_core.h"
#include "modules/prefsfile/prefsfile.h"

#include "modules/img/image.h"


#ifdef VEGA_SUPPORT
#include "modules/libvega/vegarenderer.h"
#include "modules/display/vis_dev.h"
#endif // VEGA_SUPPORT

/***********************************************************************************
**
**	OpSkin
**
***********************************************************************************/

OpSkin::OpSkin() :
#ifdef ASYNC_IMAGE_DECODERS
	m_current_skin_element(0),
	m_continue_loading_called(0),
#endif
	m_named_elements(&m_skinkey_hasher),
	m_inifile(NULL),
	m_zip(NULL),
	m_color_scheme_mode(COLOR_SCHEME_MODE_NONE),
	m_color_scheme_custom_type(COLOR_SCHEME_CUSTOM_NORMAL),
	m_color_scheme_color(0),
	m_current_default_color_scheme_color(0),
	m_default_color_scheme_color(0),
	m_default_color_scheme_checked(FALSE),
	m_scale(100),
	m_version(0)
{
#ifdef PERSONA_SKIN_SUPPORT
	g_skin_manager->AddSkinNotficationListener(this);
#endif // PERSONA_SKIN_SUPPORT
}

OpSkin::~OpSkin()
{
#ifdef PERSONA_SKIN_SUPPORT
	g_skin_manager->RemoveSkinNotficationListener(this);
#endif // PERSONA_SKIN_SUPPORT

	m_housekeeping_elements.DeleteAll();
	m_housekeeping_bitmaps.DeleteAll();

	OP_DELETE(m_inifile);
#ifdef SKIN_ZIP_SUPPORT
	OP_DELETE(m_zip);
#endif
}

OP_STATUS OpSkin::LoadIniFileL(OpFileDescriptor* skinfile, const uni_char *ini_file)
{
	OpFile* tmp = (OpFile*)skinfile->CreateCopy();
	LEAVE_IF_NULL(tmp);
	OpStackAutoPtr<OpFile> file(tmp);

	m_inifile = OP_NEW_L(PrefsFile, (PREFS_STD));
	m_inifile->ConstructL();

#ifdef SKIN_ZIP_SUPPORT
	if (OpZip::IsFileZipCompatible(file.get()))
	{
		ANCHORD(OpString, filename);
		filename.SetL(file->GetFullPath());

		m_zip = OP_NEW_L(OpZip, ());
		LEAVE_IF_ERROR(m_zip->Open(filename, FALSE /* read-only */));

		filename.SetL(ini_file);

		OpStackAutoPtr<OpMemFile> memoryskinfile(m_zip->CreateOpZipFile(&filename));
		if (memoryskinfile.get() == NULL)
		{
			LEAVE(OpStatus::ERR_NO_MEMORY);
		}

		m_inifile->SetFileL(memoryskinfile.get());
	}
	else
#endif // SKIN_ZIP_SUPPORT
	{
		m_inifile->SetFileL(skinfile);
	}

	RETURN_IF_ERROR(m_inifile->LoadAllL());

	return OpStatus::OK;
}

OP_STATUS OpSkin::InitL(OpFileDescriptor* skinfile)
{
	RETURN_IF_ERROR(LoadIniFileL(skinfile, UNI_L("skin.ini")));

#ifdef ASYNC_IMAGE_DECODERS
	// Preload skin. We won't get them in time for drawing otherwise, since the decoders are async.
	ContinueLoadAllSkinElements();
#endif // ASYNC_IMAGE_DECODERS

	return OpStatus::OK;
}

/***********************************************************************************
**
**	ContinueLoadAllSkinElements
**
***********************************************************************************/
#ifdef ASYNC_IMAGE_DECODERS
void OpSkin::ContinueLoadAllSkinElements()
{
	m_continue_loading_called++;
	if (m_continue_loading_called > 1)
		return;

#ifdef MOUSELESS
	const int NUM_SKIN_ELEMENTS = 16;
#else
	const int NUM_SKIN_ELEMENTS = 20;
#endif

	const char* s_skinelement_names[NUM_SKIN_ELEMENTS] =
	{
		"Edit Skin",
		"MultilineEdit Skin",
		"Radio Button Skin",
		"Radio Button Skin.selected",
		"Checkbox Skin",
		"Checkbox Skin.selected",
		"Push Button Skin",
		"Push Button Skin.pressed",
		"Push Button Skin.disabled",
		"Push Default Button Skin",
		"Push Default Button Skin.pressed",
		"Push Default Button Skin.disabled",
		"Scrollbar Horizontal Skin",
		"Scrollbar Horizontal Knob Skin",
		"Scrollbar Vertical Skin",
		"Scrollbar Vertical Knob Skin",
#ifndef MOUSELESS
		"Scrollbar Vertical Up Skin",
		"Scrollbar Vertical Down Skin",
		"Scrollbar Horizontal Left Skin",
		"Scrollbar Horizontal Right Skin"
#endif // MOUSELESS
	};
	if (m_current_skin_element < NUM_SKIN_ELEMENTS)
	{
		GetSkinElement(s_skinelement_names[m_current_skin_element++]);
		while (m_continue_loading_called-- > 1 && m_current_skin_element < NUM_SKIN_ELEMENTS)
			GetSkinElement(s_skinelement_names[m_current_skin_element++]);
	}
	m_continue_loading_called = 0;
}
#endif // ASYNC_IMAGE_DECODERS

/***********************************************************************************
**
**	GetOptionValue
**
***********************************************************************************/

INT32 OpSkin::GetOptionValue(const char* option, INT32 def_value)
{
	return m_inifile->ReadIntL("Options", option, def_value);
}

#ifdef SKIN_SKIN_COLOR_THEME

void OpSkin::SetColorSchemeMode(ColorSchemeMode mode)
{
	if (m_color_scheme_mode == mode)
		return;

	m_color_scheme_mode = mode;
	Flush();
}

void OpSkin::SetColorSchemeColor(INT32 color)
{
	if (m_color_scheme_color == color)
		return;

	m_color_scheme_color = color;
	Flush();
}

void OpSkin::SetColorSchemeCustomType(ColorSchemeCustomType type)
{
	if(type == m_color_scheme_custom_type)
		return;

	m_color_scheme_custom_type = type;
	Flush();
}

#endif // SKIN_SKIN_COLOR_THEME

/***********************************************************************************
**
**	Flush
**
***********************************************************************************/

void OpSkin::Flush()
{
	m_housekeeping_elements.DeleteAll();
	m_named_elements.RemoveAll();
	m_housekeeping_bitmaps.DeleteAll();
	m_named_bitmaps.RemoveAll();
	m_named_elements.RemoveAll();
#ifdef SKIN_NATIVE_ELEMENT
	OpSkinElement::FlushNativeElements();
#endif
}

/***********************************************************************************
**
**	GetSkinElement
**
***********************************************************************************/

OpSkinElement* OpSkin::GetSkinElement(const char* name, SkinType type, SkinSize size)
{
	if (!name || !*name)
		return NULL;

	OpSkinElement* element = NULL;

	SkinKey key;

	key.m_name = name;
	key.m_type = type;
	key.m_size = size;

	{
		void *mentel = NULL;
		m_named_elements.GetData((void*) &key, &mentel);
		if (mentel)
			element = (OpSkinElement*) mentel;
	}

	if (!element)
	{
		OpStringC8 element_name(name);
		OpString8 section;

		if(OpStatus::IsError(section.Set(name)))
			return NULL;
		if(OpStatus::IsError(section.Append(g_opera->skin_module.GetSkinTypeNames()[type])))
			return NULL;

#ifdef SKIN_NATIVE_ELEMENT
		INT32 global_native = GetOptionValue("Native Skin", 0);
		INT32 native = m_inifile->ReadIntL(section, "Native", global_native);

		element = OpSkinElement::CreateElement(native ? true : false, this, element_name, type, size);
#else
		element = OpSkinElement::CreateElement(false, this, element_name, type, size);
#endif // SKIN_NATIVE_ELEMENT
		if (!element)
			return NULL;

		if (OpStatus::IsMemoryError(element->Init(name)))
		{
			OP_DELETE(element);
			return NULL;
		}

		OP_STATUS s1 = m_named_elements.Add((void*) element->GetKey(), element);
		OP_STATUS s2 = m_housekeeping_elements.Add(element);
		if (OpStatus::IsError(s1) || OpStatus::IsError(s2))
		{
			if (OpStatus::IsSuccess(s1))
			{
				void *mentel = NULL;
				m_named_elements.Remove((void*) element->GetKey(), &mentel);
			}
			if (OpStatus::IsSuccess(s2))
				m_housekeeping_elements.Remove(m_housekeeping_elements.GetCount() - 1);

			OP_DELETE(element);
			return NULL;
		}
	}

	return element->IsLoaded() ? element : NULL;
}

OP_STATUS OpSkin::RemoveSkinElement(const char* name, SkinType type, SkinSize size)
{
	if (!name || !*name)
		return OpStatus::ERR;

	SkinKey key;

	key.m_name = name;
	key.m_type = type;
	key.m_size = size;

	void *mentel = NULL;
	return m_named_elements.Remove((void*) &key, &mentel);
}

#ifdef SKIN_SKIN_COLOR_THEME

void OpSkin::UpdateCurrentDefaultColorScheme()
{
	// no native colorization when persona is loaded, it's unnecessary as the persona will colorize it
	// anyway
	if(g_skin_manager 
#ifdef PERSONA_SKIN_SUPPORT
		&& !g_skin_manager->HasPersonaSkin()
#endif // PERSONA_SKIN_SUPPORT
		)
	{
		if (g_skin_manager->GetCurrentSkin())
		{
			m_current_default_color_scheme_color = g_skin_manager->GetCurrentSkin()->GetDefaultColorScheme();
		}
		else
		{
			m_current_default_color_scheme_color = GetDefaultColorScheme();
		}
	}
}

INT32 OpSkin::GetDefaultColorScheme()
{
	if (m_default_color_scheme_checked)
		return m_default_color_scheme_color;
	m_default_color_scheme_checked = TRUE;
	OpStringC native_color_theme = m_inifile->ReadStringL("Options", "Native Color Theme");
	if (!native_color_theme.IsEmpty())
	{
		m_default_color_scheme_color = g_op_ui_info->GetSystemColor(OP_SYSTEM_COLOR_SKIN);
#ifdef SKIN_NATIVE_ELEMENT
		OpSkinElement* nelem = NULL;
		OpString8 nct;
		if (OpStatus::IsSuccess(nct.Set(native_color_theme)))
			nelem = OpSkinElement::CreateNativeElement(this, nct, SKINTYPE_DEFAULT, SKINSIZE_DEFAULT);
		if (nelem)
		{
			VisualDevice vd;
			OpBitmap* bmp = NULL;
			if (!OpStatus::IsSuccess(OpBitmap::Create(&bmp, 16, 16, FALSE, FALSE, 0, 0, TRUE)) || 
				!bmp->Supports(OpBitmap::SUPPORTS_PAINTER) || !bmp->Supports(OpBitmap::SUPPORTS_POINTER) || bmp->GetBpp() != 32)
			{
				OP_DELETE(bmp);
				OP_DELETE(nelem);
				return m_default_color_scheme_color;
			}
			bmp->SetColor(NULL, TRUE, NULL);
			OpPainter* painter = bmp->GetPainter();
			if (painter)
			{
				OpRect rect(-5,-5,26,26);
				OpRect clip(0,0,16,16);
				vd.BeginPaint(painter, clip, clip);
				nelem->Draw(&vd, rect, 0, 0, &clip);
				vd.EndPaint();
				bmp->ReleasePainter();
			}
			OP_DELETE(nelem);

			if(bmp->Supports(OpBitmap::SUPPORTS_POINTER))
			{
				UINT32* data = (UINT32*)bmp->GetPointer(OpBitmap::ACCESS_READONLY);

				UINT32 sum_alpha = 0;
				UINT32 sum_red = 0;
				UINT32 sum_green = 0;
				UINT32 sum_blue = 0;
				if (data)
				{
					for (unsigned int y = 0; y < 16; ++y)
					{
						for (unsigned int x = 0; x < 16; ++x)
						{
							sum_alpha += (data[y*16+x]>>24)&0xff;
							sum_red += (data[y*16+x]>>16)&0xff;
							sum_green += (data[y*16+x]>>8)&0xff;
							sum_blue += data[y*16+x]&0xff;
						}
					}

					bmp->ReleasePointer();
				}
				sum_alpha /= 16*16;
				sum_red /= 16*16;
				sum_green /= 16*16;
				sum_blue /= 16*16;
				if (sum_alpha > 127)
					m_default_color_scheme_color = OP_RGB(sum_red, sum_green, sum_blue);
			}
			OP_DELETE(bmp);
		}
#endif // SKIN_NATIVE_ELEMENT
	}
	return m_default_color_scheme_color;
}

#endif // SKIN_SKIN_COLOR_THEME

COLORREF OpSkin::ColorizeColor(COLORREF color)
{
#ifdef SKIN_SKIN_COLOR_THEME
	UpdateCurrentDefaultColorScheme();
	if (m_color_scheme_mode > 0 || m_current_default_color_scheme_color)
	{
		COLORREF colorize_color;

		if (m_color_scheme_mode == COLOR_SCHEME_MODE_SYSTEM)
		{
			colorize_color = g_op_ui_info->GetSystemColor(OP_SYSTEM_COLOR_SKIN);
		}
		else if (m_color_scheme_mode == COLOR_SCHEME_MODE_CUSTOM)
		{
			colorize_color = m_color_scheme_color;
		}
		else
		{
			colorize_color = m_current_default_color_scheme_color;
		}
		// convert colorref to ARGB pixel (but remove the alpha)
		UINT32 color = ((colorize_color & 0xff) << 16) | (colorize_color & 0xff00) | ((colorize_color >> 16) & 0xff);

		double h, s, v;
		OpSkinUtils::ConvertRGBToHSV(color, h, s, v);

		color = OpSkinUtils::ColorizePixel(color, h, s, v);

		if(m_color_scheme_custom_type == COLOR_SCHEME_CUSTOM_HUE_DARKER)
		{
			color = OpSkinUtils::ColorizePixelDarker(color, h, s, v);
		}
	}
#endif
	return color;
}

/***********************************************************************************
**
**	GetBitmap
**
***********************************************************************************/

Image OpSkin::GetBitmap(const uni_char* filename, BOOL skin_coloring, BOOL scalable, OpSkinElement::StateElement* state_element)
{
	if (!filename || !*filename)
		return Image();

	NamedBitmap		*named_bitmap = NULL;
	const uni_char	*cache_key = filename;

	// Since .mini state images are smaller than regular images they have to
	// have a different key in the named image cache so that they too are
	// retrieved
	OpString key;
	if (state_element->m_state & SKINSTATE_MINI)
	{
		key.Set(filename);
		key.Append(UNI_L(".mini"));
		cache_key = key.CStr();
	}

	// Similarly, we sometimes generate mirror images to serve as .rtl state
	// images if they were not specified explicitly.  Need a different key,
	// too.
	const bool use_mirror = (state_element->m_state & SKINSTATE_RTL) && state_element->m_mirror_rtl;
	if (use_mirror)
	{
		key.Set(cache_key);
		key.Append(UNI_L(".rtl"));
		cache_key = key.CStr();
	}

	m_named_bitmaps.GetData(cache_key, &named_bitmap);

	if (!named_bitmap)
	{
		Image image;
		ImageContentProvider* contentprovider = NULL;
		if (OpStatus::IsError(LoadBitmap(filename, image, &contentprovider, state_element)))
			// FIXME:OOM
			return Image();

#ifdef ASYNC_IMAGE_DECODERS
		named_bitmap = OP_NEW(NamedBitmap, (image, contentprovider, state_element));
		if (!named_bitmap || OpStatus::IsError(named_bitmap->Construct(cache_key)))
		{
			image.DecVisible(state_element);
			OP_DELETE(named_bitmap);
			return Image();
		}
		m_named_bitmaps.Add(named_bitmap->m_name.CStr(), named_bitmap);
		m_housekeeping_bitmaps.Add(named_bitmap);
		return named_bitmap->m_image;
#endif
		OpBitmap* bitmap = image.GetBitmap(state_element);
		if (!bitmap)
		{
			image.DecVisible(state_element);
			return Image();
		}

		INT32 scale = m_scale;

		// For state mini on a skin reduce the size by 80%
		if (state_element->m_state & SKINSTATE_MINI)
			scale = scale * 80 / 100;

		// Minimum width/height of downscaled bitmap.
		UINT32 min_dim = state_element->m_state & SKINSTATE_HIGHRES ? 32 : 16;

		if (scalable && (scale > 100 || (scale < 100 && bitmap->Width() > min_dim && bitmap->Height() > min_dim)))
		{
			UINT32 new_width = bitmap->Width() * scale / 100;
			UINT32 new_height = bitmap->Height() * scale / 100;

			if (scale < 100)
			{
				if (new_width < min_dim)
				{
					scale = 100 * min_dim / bitmap->Width();

					new_height = bitmap->Height() * scale / 100;
				}

				if (new_height < min_dim)
				{
					scale = 100 * min_dim / bitmap->Height();
				}

				new_width = bitmap->Width() * scale / 100;
				new_height = bitmap->Height() * scale / 100;
			}

			if ((new_width != bitmap->Width() || new_height != bitmap->Height()) && !image.IsAnimated())
			{
				OpBitmap* scaled_bitmap = NULL;

				OpBitmap::Create(&scaled_bitmap, new_width, new_height);

				if (scaled_bitmap)
				{
					OpSkinUtils::ScaleBitmap(bitmap, scaled_bitmap);
					image.ReleaseBitmap();
					image.ReplaceBitmap(scaled_bitmap);
					bitmap = image.GetBitmap(NULL);
				}
			}
		}

		if (use_mirror && !image.IsAnimated())
		{
			OpBitmap* mirror = OpSkinUtils::MirrorBitmap(bitmap);
			if (mirror)
			{
				image.ReleaseBitmap();
				image.ReplaceBitmap(mirror);
				bitmap = image.GetBitmap(NULL);
			}
		}

#ifdef SKIN_SKIN_COLOR_THEME
		UpdateCurrentDefaultColorScheme();
		if (skin_coloring && (m_color_scheme_mode != COLOR_SCHEME_MODE_NONE || m_current_default_color_scheme_color) && !image.IsAnimated())
		{
			// if bitmap is palette based, we must convert it to 32 bits

			if (bitmap->GetBpp() <= 8)
			{
				OpBitmap* converted_bitmap = NULL;

				OpBitmap::Create(&converted_bitmap, bitmap->Width(), bitmap->Height(), FALSE, bitmap->IsTransparent());

				if (converted_bitmap)
				{
					OpSkinUtils::CopyBitmap(bitmap, converted_bitmap);
					image.ReleaseBitmap();
					image.ReplaceBitmap(converted_bitmap);
					bitmap = image.GetBitmap(NULL);
				}
			}

			COLORREF colorize_color;

			if (m_color_scheme_mode == COLOR_SCHEME_MODE_SYSTEM)
			{
				colorize_color = g_op_ui_info->GetSystemColor(OP_SYSTEM_COLOR_SKIN);
			}
			else if (m_color_scheme_mode == COLOR_SCHEME_MODE_CUSTOM)
			{
				colorize_color = m_color_scheme_color;
			}
			else
			{
				colorize_color = m_current_default_color_scheme_color;
			}

			// convert colorref to ARGB pixel

			UINT32 color = ((colorize_color & 0xff) << 16) | (colorize_color & 0xff00) | ((colorize_color >> 16) & 0xff);

			double h, s, v;
			OpSkinUtils::ConvertRGBToHSV(color, h, s, v);

			if (bitmap->GetBpp() == 32 && bitmap->Supports(OpBitmap::SUPPORTS_POINTER))
			{
				UINT32* src = (UINT32*) bitmap->GetPointer();
				if( src )
				{
					UINT32 wh = bitmap->Width() * bitmap->Height();

					for(UINT32 i = 0; i < wh; i++)
					{
#ifdef USE_PREMULTIPLIED_ALPHA
						*src = OpSkinUtils::InvertPremultiplyAlphaPixel(*src);
#endif //USE_PREMULTIPLIED_ALPHA
						*src = OpSkinUtils::ColorizePixel(*src, h, s, v);
						if(m_color_scheme_custom_type == COLOR_SCHEME_CUSTOM_HUE_DARKER)
						{
							*src = OpSkinUtils::ColorizePixelDarker(*src, h, s, v);
						}
#ifdef USE_PREMULTIPLIED_ALPHA
						*src = OpSkinUtils::PremultiplyAlphaPixel(*src);
#endif //USE_PREMULTIPLIED_ALPHA
						src++;
					}
				}
				bitmap->ReleasePointer();
			}
			else
			{
				int width = bitmap->Width();
				int height = bitmap->Height();
				UINT32 *tmpbuf = OP_NEWA(UINT32, width);
				if (tmpbuf)
				{
					for (int y=0; y<height; y++)
					{
						bitmap->GetLineData(tmpbuf, y);
						UINT32 *data = tmpbuf;
						for (int x=0; x<width; x++)
						{
#ifdef USE_PREMULTIPLIED_ALPHA
							*data = OpSkinUtils::InvertPremultiplyAlphaPixel(*data);
#endif //USE_PREMULTIPLIED_ALPHA
							*data = OpSkinUtils::ColorizePixel(*data, h, s, v);
							if(m_color_scheme_custom_type == COLOR_SCHEME_CUSTOM_HUE_DARKER)
							{
								*data = OpSkinUtils::ColorizePixelDarker(*data, h, s, v);
							}
#ifdef USE_PREMULTIPLIED_ALPHA
							*data = OpSkinUtils::PremultiplyAlphaPixel(*data);
#endif //USE_PREMULTIPLIED_ALPHA
							data++; // after *both* uses of the address ...
						}
						/*OP_STATUS os =*/ // FIXME: ignoring return status
							bitmap->AddLine(tmpbuf, y);
					}
				}
				OP_DELETEA(tmpbuf);
			}
		}
#endif // SKIN_SKIN_COLOR_THEME

		image.ReleaseBitmap();

		named_bitmap = OP_NEW(NamedBitmap, (image, contentprovider, state_element));
		if (!named_bitmap || OpStatus::IsError(named_bitmap->Construct(cache_key)))
		{
			image.DecVisible(state_element);
			OP_DELETE(named_bitmap);
			return Image();
		}
		m_named_bitmaps.Add(named_bitmap->m_name.CStr(), named_bitmap);
		m_housekeeping_bitmaps.Add(named_bitmap);
	}

	return named_bitmap->m_image;
}

/***********************************************************************************
**
**	LoadBitmap
**
***********************************************************************************/

OP_STATUS OpSkin::LoadBitmap(const uni_char* filename, Image& image, ImageContentProvider** contentprovider, ImageListener* imagelistener)
{
	if (filename == NULL)
	{
		return OpStatus::ERR;
	}

	UINT8* data = NULL;
	long length = 0;

#ifdef SKIN_ZIP_SUPPORT
	if (m_zip)
	{
		OpString filname;
		RETURN_IF_ERROR(filname.Set(filename));
		length = m_zip->GetFile(&filname, data);
		if (length == -1)
			return OpStatus::ERR;
	}
	else
#endif // SKIN_ZIP_SUPPORT
	{
		// Build full path with filename
		OpString fullfilename;
		OpFile* tmp = (OpFile*) m_inifile->GetFile()->CreateCopy();
		if (!tmp)
			return OpStatus::ERR;

		OP_STATUS status = fullfilename.Set(tmp->GetFullPath(), uni_strlen(tmp->GetFullPath()) - uni_strlen(tmp->GetName()));

		OP_DELETE(tmp);

		RETURN_IF_ERROR(status);
		RETURN_IF_ERROR(fullfilename.Append(filename));

		// Load data
		OpFile file;

		RETURN_IF_ERROR(file.Construct(fullfilename));
		RETURN_IF_ERROR(file.Open(OPFILE_READ));

		OpFileLength file_len;
		RETURN_IF_ERROR(file.GetFileLength(file_len));
		length = (unsigned int)file_len;

		data = OP_NEWA(UINT8, length);
		if (!data)
			return OpStatus::ERR_NO_MEMORY;

		if (OpStatus::IsError(file.Read(data, length, &file_len)) || file_len != static_cast<OpFileLength>(length))
		{
			OP_DELETEA(data);
			return OpStatus::ERR;
		}
		file.Close();
	}

	if(!data)
	{
		return OpStatus::ERR;
	}

	*contentprovider = OP_NEW(SkinImageContentProvider, ((char *)data, length));
	if (*contentprovider == NULL)
	{
		OP_DELETEA(data);
		return OpStatus::ERR_NO_MEMORY;
	}

	image = imgManager->GetImage(*contentprovider);

	// If this assert trig, it meas that imgManager gave us an existing image from our
	// brand new content provider. That can only happen if we had live Image objects using
	// it when it was removed, and that we happened to get the same address again, finding
	// that zombie Image.
	OP_ASSERT(!image.ImageDecoded());

	if (imagelistener == NULL)
		imagelistener = null_image_listener;

	image.IncVisible(imagelistener); // IMG-EMIL Must be balanced and don't use the provider in DecVisible
#ifndef ASYNC_IMAGE_DECODERS_EMULATION
	image.OnLoadAll(*contentprovider); // Need to disable images in gui when emulating asynchronous image decoders.
#endif // !ASYNC_IMAGE_DECODERS_EMULATION

	OP_DELETEA(data);

	OP_STATUS status;
#ifdef ASYNC_IMAGE_DECODERS
	status = !image.IsEmpty() ? OpStatus::OK : OpStatus::ERR;
#else
	status = image.ImageDecoded() ? OpStatus::OK : OpStatus::ERR;
#endif
	if (OpStatus::IsError(status))
		image.DecVisible(imagelistener);
	return status;
}

/***********************************************************************************
**
**	Hash functions
**
***********************************************************************************/

UINT32 OpSkin::SkinKeyHasher::Hash(const void* key)
{
	SkinKey* skin_key = (SkinKey*) key;
	return djb2hash(skin_key->m_name) + skin_key->m_type + skin_key->m_size;
}

BOOL OpSkin::SkinKeyHasher::KeysAreEqual(const void* key1, const void* key2)
{
	SkinKey* skin_key1 = (SkinKey*) key1;
	SkinKey* skin_key2 = (SkinKey*) key2;

	return skin_key1->m_type == skin_key2->m_type && skin_key1->m_size == skin_key2->m_size && !op_strcmp(skin_key1->m_name, skin_key2->m_name);
}


/***********************************************************************************
**
**	ReadColorL
**
***********************************************************************************/

INT32 OpSkin::ReadColorL(const char* name, const char* key, INT32 default_color, BOOL colorize)
{
	INT32 a, r, g, b;

	OpStringC color = m_inifile->ReadStringL(name, key);

	if (7 == color.Length() && 3 == uni_sscanf(color.CStr(), UNI_L("#%2x%2x%2x"), &r, &g, &b))
	{
		return colorize ? ColorizeColor(OP_RGB(r, g, b)) : OP_RGB(r, g, b);
	}
	else if (9 == color.Length() && 4 == uni_sscanf(color.CStr(), UNI_L("#%2x%2x%2x%2x"), &a,  &r, &g, &b))
	{
		return OP_RGBA(r, g, b, a);
	}
	else
	{
		if (color && uni_stricmp(color, UNI_L("Window")) == 0)
		{
			return SKINCOLOR_WINDOW;
		}
		if (color && uni_stricmp(color, UNI_L("Window Disabled")) == 0)
		{
			return SKINCOLOR_WINDOW_DISABLED;
		}
	}

	return default_color;
}

short OpSkin::ReadBorderStyleL(const char* name, const char* key, short default_style)
{
	OpStringC style = m_inifile->ReadStringL(name, key);
	if (!style.IsEmpty())
	{
		if (uni_stricmp(style, UNI_L("solid")) == 0)
			default_style = CSS_VALUE_solid;
		else if (uni_stricmp(style, UNI_L("outset")) == 0)
			default_style = CSS_VALUE_outset;
		else if (uni_stricmp(style, UNI_L("inset")) == 0)
			default_style = CSS_VALUE_inset;
		else if (uni_stricmp(style, UNI_L("double")) == 0)
			default_style = CSS_VALUE_double;
		else if (uni_stricmp(style, UNI_L("groove")) == 0)
			default_style = CSS_VALUE_groove;
		else if (uni_stricmp(style, UNI_L("ridge")) == 0)
			default_style = CSS_VALUE_ridge;
		else if (uni_stricmp(style, UNI_L("dotted")) == 0)
			default_style = CSS_VALUE_dotted;
		else if (uni_stricmp(style, UNI_L("dashed")) == 0)
			default_style = CSS_VALUE_dashed;
	}
	return default_style;
}

BOOL OpSkin::ReadTopRightBottomLeftL(const char* name, const char* key, UINT8 *four_values)
{
	OpStringC str = m_inifile->ReadStringL(name, key);

	int a, b, c, d;
	if (str && 4 == uni_sscanf(str.CStr(), UNI_L("%d %d %d %d"), &a, &b, &c, &d))
	{
		// top, right, bottom, left
		four_values[0] = a;
		four_values[1] = b;
		four_values[2] = c;
		four_values[3] = d;
	}
	else if (str && 3 == uni_sscanf(str.CStr(), UNI_L("%d %d %d"), &a, &b, &c))
	{
		// top, right/left, bottom
		four_values[0] = a;
		four_values[1] = b;
		four_values[2] = c;
		four_values[3] = b;
	}
	else if (str && 2 == uni_sscanf(str.CStr(), UNI_L("%d %d"), &a, &b))
	{
		// top/bottom, right/left
		four_values[0] = a;
		four_values[1] = b;
		four_values[2] = a;
		four_values[3] = b;
	}
	else if (str && 1 == uni_sscanf(str.CStr(), UNI_L("%d"), &a))
	{
		// top/right/bottom/left
		for(int i = 0; i < 4; i++)
			four_values[i] = a;
	}
	else
		return FALSE;
	return TRUE;
}

void OpSkin::ReadTextShadowL(const char* name, const char* key, OpSkinTextShadow *text_shadow)
{
	OpStringC str = m_inifile->ReadStringL(name, key);

	int ofs_x = 0, ofs_y = 0;
	/* Leave this out since radius is CPU expensive to draw and we probably don't need it (or want anyone to create slow skins).
	int radius = 0;
	if (str && 3 == uni_sscanf(str.CStr(), UNI_L("%d %d %d"), &ofs_x, &ofs_y, &radius))
	{
		text_shadow->ofs_x = ofs_x;
		text_shadow->ofs_y = ofs_y;
		text_shadow->radius = radius;
	}
	else*/
	if (str && 2 == uni_sscanf(str.CStr(), UNI_L("%d %d"), &ofs_x, &ofs_y))
	{
		text_shadow->ofs_x = ofs_x;
		text_shadow->ofs_y = ofs_y;
	}
}

const OpStringC OpSkin::GetSkinName()
{
	OpStringC name;

	TRAPD(err, name = GetPrefsFile()->ReadStringL("Info", "Name"));

	return name;
}

const OpStringC OpSkin::GetSkinAuthor()
{
	OpStringC name;

	TRAPD(err, name = GetPrefsFile()->ReadStringL("Info", "Author"));

	return name;
}
#ifdef PERSONA_SKIN_SUPPORT

void OpSkin::OnPersonaLoaded(OpPersona *loaded_persona)
{
	if(loaded_persona)
	{
		// update the skin with the persona color scheme
		SetColorSchemeMode(loaded_persona->GetColorSchemeMode());
		SetColorSchemeColor(loaded_persona->GetColorSchemeColor());
		SetColorSchemeCustomType(loaded_persona->GetColorSchemeCustomType());
	}
	else
	{
		m_default_color_scheme_checked = FALSE;
		UpdateCurrentDefaultColorScheme();
		SetColorSchemeMode(COLOR_SCHEME_MODE_NONE);
		SetColorSchemeCustomType(COLOR_SCHEME_CUSTOM_NORMAL);
	}
}

#endif // PERSONA_SKIN_SUPPORT
#endif // SKIN_SUPPORT
