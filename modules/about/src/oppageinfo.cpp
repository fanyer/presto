/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2006-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Peter Krefting
*/

#include "core/pch.h"

#ifdef DOC_HAS_PAGE_INFO
#include "modules/about/oppageinfo.h"
#include "modules/prefs/prefsmanager/collections/pc_files.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/img/image.h"
#include "modules/logdoc/htm_ldoc.h"
#include "modules/logdoc/logdoc.h"
#include "modules/logdoc/urlimgcontprov.h"
#include "modules/util/htmlify.h"
#include "modules/url/url2.h"
#include "modules/doc/frm_doc.h"
#include "modules/dochand/fdelm.h"
#include "modules/pi/OpSystemInfo.h"
#include "modules/pi/OpLocale.h"
#include "modules/dochand/win.h"
#ifdef M2_SUPPORT
# include "adjunct/m2/src/engine/engine.h"
# include "adjunct/m2/src/engine/message.h"
#endif
#ifdef TRUST_RATING
# include "modules/dochand/fraud_check.h"
#endif

#if defined AB_USE_IMAGE_METADATA && defined IMAGE_METADATA_SUPPORT
// EXIF strings - make sure this array always matches the ImageMetaData enum in img/image.h !!
static const int exif_locale_id[] = {
	static_cast<int>(Str::D_IMAGE_PROPERTIES_METADATA_ORIENTATION),
	static_cast<int>(Str::D_IMAGE_PROPERTIES_METADATA_DATETIME),
	static_cast<int>(Str::D_IMAGE_PROPERTIES_METADATA_DESCRIPTION),
	static_cast<int>(Str::D_IMAGE_PROPERTIES_METADATA_MAKE),
	static_cast<int>(Str::D_IMAGE_PROPERTIES_METADATA_MODEL),
	static_cast<int>(Str::D_IMAGE_PROPERTIES_METADATA_SOFTWARE),
	static_cast<int>(Str::D_IMAGE_PROPERTIES_METADATA_ARTIST),
	static_cast<int>(Str::D_IMAGE_PROPERTIES_METADATA_COPYRIGHT),
	static_cast<int>(Str::D_IMAGE_PROPERTIES_METADATA_EXPOSURETIME),
	static_cast<int>(Str::D_IMAGE_PROPERTIES_METADATA_FNUMBER),
	static_cast<int>(Str::D_IMAGE_PROPERTIES_METADATA_EXPOSUREPROGRAM),
	static_cast<int>(Str::D_IMAGE_PROPERTIES_METADATA_SPECTRALSENSITIVITY),
	static_cast<int>(Str::D_IMAGE_PROPERTIES_METADATA_ISOSPEEDRATING),
	static_cast<int>(Str::D_IMAGE_PROPERTIES_METADATA_SHUTTERSPEED),
	static_cast<int>(Str::D_IMAGE_PROPERTIES_METADATA_APERTURE),
	static_cast<int>(Str::D_IMAGE_PROPERTIES_METADATA_BRIGHTNESS),
	static_cast<int>(Str::D_IMAGE_PROPERTIES_METADATA_EXPOSUREBIAS),
	static_cast<int>(Str::D_IMAGE_PROPERTIES_METADATA_MAXAPERTURE),
	static_cast<int>(Str::D_IMAGE_PROPERTIES_METADATA_SUBJECTDISTANCE),
	static_cast<int>(Str::D_IMAGE_PROPERTIES_METADATA_METERINGMODE),
	static_cast<int>(Str::D_IMAGE_PROPERTIES_METADATA_LIGHTSOURCE),
	static_cast<int>(Str::D_IMAGE_PROPERTIES_METADATA_FLASH),
	static_cast<int>(Str::D_IMAGE_PROPERTIES_METADATA_FOCALLENGTH),
	static_cast<int>(Str::D_IMAGE_PROPERTIES_METADATA_SUBJECTAREA),
	static_cast<int>(Str::D_IMAGE_PROPERTIES_METADATA_FLASHENERGY),
	static_cast<int>(Str::D_IMAGE_PROPERTIES_METADATA_FOCALPLANE_X_RESOLUTION),
	static_cast<int>(Str::D_IMAGE_PROPERTIES_METADATA_FOCALPLANE_Y_RESOLUTION),
	static_cast<int>(Str::D_IMAGE_PROPERTIES_METADATA_FOCALPLANERESOLUTIONUNIT),
	static_cast<int>(Str::D_IMAGE_PROPERTIES_METADATA_SUBJECTLOCATION),
	static_cast<int>(Str::D_IMAGE_PROPERTIES_METADATA_EXPOSUREINDEX),
	static_cast<int>(Str::D_IMAGE_PROPERTIES_METADATA_SENSINGMETHOD),
	static_cast<int>(Str::D_IMAGE_PROPERTIES_METADATA_CUSTOMRENDERED),
	static_cast<int>(Str::D_IMAGE_PROPERTIES_METADATA_EXPOSUREMODE),
	static_cast<int>(Str::D_IMAGE_PROPERTIES_METADATA_WHITEBALANCE),
	static_cast<int>(Str::D_IMAGE_PROPERTIES_METADATA_DIGITALZOOMRATIO),
	static_cast<int>(Str::D_IMAGE_PROPERTIES_METADATA_FOCALLENGTHIN35MMFILM),
	static_cast<int>(Str::D_IMAGE_PROPERTIES_METADATA_SCENECAPTURETYPE),
	static_cast<int>(Str::D_IMAGE_PROPERTIES_METADATA_GAINCONTROL),
	static_cast<int>(Str::D_IMAGE_PROPERTIES_METADATA_CONTRAST),
	static_cast<int>(Str::D_IMAGE_PROPERTIES_METADATA_SATURATION),
	static_cast<int>(Str::D_IMAGE_PROPERTIES_METADATA_SHARPNESS),
	static_cast<int>(Str::D_IMAGE_PROPERTIES_METADATA_SUBJECTDISTANCERANGE),
};
#endif // AB_USE_IMAGE_METADATA && IMAGE_METADATA_SUPPORT

#ifdef DISPLAY_WRITINGSYSTEM_HEURISTIC
Str::LocaleString OpPageInfo::ScriptToLocaleString(WritingSystem::Script script)
{
    switch (script)
    {
	case WritingSystem::LatinWestern: return Str::S_WRITINGSYSTEM_LATINWESTERN;
	case WritingSystem::LatinEastern: return Str::S_WRITINGSYSTEM_LATINEASTERN;
	case WritingSystem::LatinUnknown: return Str::S_WRITINGSYSTEM_LATINUNKNOWN;
	case WritingSystem::Cyrillic: return Str::SI_IDPREFS_BLOCK_CYRILLIC;
	case WritingSystem::Baltic: return Str::S_WRITINGSYSTEM_BALTIC;
	case WritingSystem::Turkish: return Str::S_WRITINGSYSTEM_TURKISH;
	case WritingSystem::Greek: return Str::SI_IDPREFS_BLOCK_GREEK;
	case WritingSystem::Hebrew: return Str::SI_IDPREFS_BLOCK_HEBREW;
	case WritingSystem::Arabic: return Str::SI_IDPREFS_BLOCK_ARABIC;
	case WritingSystem::Persian: return Str::S_WRITINGSYSTEM_PERSIAN;
	case WritingSystem::IndicDevanagari: return Str::SI_IDPREFS_BLOCK_DEVANGARI;
	case WritingSystem::IndicBengali: return Str::SI_IDPREFS_BLOCK_BENGALI;
	case WritingSystem::IndicGurmukhi: return Str::SI_IDPREFS_BLOCK_GURMUKHI;
	case WritingSystem::IndicGujarati: return Str::SI_IDPREFS_BLOCK_GUJARATI;
	case WritingSystem::IndicOriya: return Str::SI_IDPREFS_BLOCK_ORIYA;
	case WritingSystem::IndicTamil: return Str::SI_IDPREFS_BLOCK_TAMIL;
	case WritingSystem::IndicTelugu: return Str::SI_IDPREFS_BLOCK_TELUGU;
	case WritingSystem::IndicKannada: return Str::SI_IDPREFS_BLOCK_KANNADA;
	case WritingSystem::IndicMalayalam: return Str::SI_IDPREFS_BLOCK_MALAYALAM;
	case WritingSystem::IndicSinhala: return Str::SI_IDPREFS_BLOCK_SINHALA;
	case WritingSystem::IndicLao: return Str::SI_IDPREFS_BLOCK_LAO;
	case WritingSystem::IndicTibetan: return Str::SI_IDPREFS_BLOCK_TIBETAN;
	case WritingSystem::IndicMyanmar: return Str::SI_IDPREFS_BLOCK_MYANMAR;
	case WritingSystem::IndicUnknown: return Str::S_WRITINGSYSTEM_INDICUNKNOWN;
	case WritingSystem::Thai: return Str::SI_IDPREFS_BLOCK_THAI;
	case WritingSystem::Vietnamese: return Str::S_WRITINGSYSTEM_VIETNAMESE;
	case WritingSystem::ChineseSimplified: return Str::SI_IDPREFS_BLOCK_SIMPLIFIED;
	case WritingSystem::ChineseTraditional: return Str::SI_IDPREFS_BLOCK_TRADITIONAL;
	case WritingSystem::ChineseUnknown: return Str::S_WRITINGSYSTEM_CHINESEUNKNOWN;
	case WritingSystem::Japanese: return Str::S_WRITINGSYSTEM_JAPANESE;
	case WritingSystem::Korean: return Str::SI_IDPREFS_BLOCK_HANGUL;
	case WritingSystem::Cherokee: return Str::SI_IDPREFS_BLOCK_CHEROKEE;
	default:
		OP_ASSERT(!"Unknown writing system, please update about module");
	case WritingSystem::Unknown:
		return Str::NOT_A_STRING;
    }
}
#endif

OP_STATUS OpPageInfo::OpenTable()
{
	return m_url.WriteDocumentData(URL::KNormal, UNI_L("<table>\n"));
}

OP_STATUS OpPageInfo::AddTableRow(Str::LocaleString id, Str::LocaleString data, const URL *link)
{
	OpString data_string;
	RETURN_IF_ERROR(g_languageManager->GetString(data, data_string));
	return AddTableRow(id, data_string, link);
}

OP_STATUS OpPageInfo::AddTableRow(Str::LocaleString id, const OpStringC &data, const URL *link)
{
	if (data.HasContent())
	{
		OpString heading;
		RETURN_IF_ERROR(g_languageManager->GetString(id, heading));
		return AddTableRow(heading, data, link);
	}

	return OpStatus::OK;
}

OP_STATUS OpPageInfo::AddTableRow(const OpStringC &heading, const OpStringC &data, const URL *link)
{
	if (heading.HasContent() && data.HasContent())
	{
		uni_char *htmlified_data = HTMLify_string(data.CStr(), data.Length(), TRUE);
		if (!htmlified_data)
		{
			return OpStatus::ERR_NO_MEMORY;
		}

		uni_char *htmlified_link = NULL;
		if (link)
		{
			OpString urlname;
			link->GetAttribute(URL::KUniName_With_Fragment, urlname);
			htmlified_link = HTMLify_string(urlname.CStr(), urlname.Length(), TRUE);
		}

		if (htmlified_link)
		{
#ifdef ABOUT_SIMPLIFIED_PAGEINFO
// TODO: instead of using a table and tr with align statements it might be
// better to use div elements with css. Then there is no more #ifdef required
// here:
# define ABOUT_PAGEINFO_FORMAT UNI_L(" <tr align=\"left\"><th>%s</th></tr><tr align=\"left\"><td><a href=\"%s\" target=\"_blank\">%s</a></td></tr>\n")
#else // !ABOUT_SIMPLIFIED_PAGEINFO
# define ABOUT_PAGEINFO_FORMAT UNI_L(" <tr><th>%s</th><td><a href=\"%s\" target=\"_blank\">%s</a></td></tr>\n")
#endif // ABOUT_SIMPLIFIED_PAGEINFO
			m_url.WriteDocumentDataUniSprintf(ABOUT_PAGEINFO_FORMAT,
											  heading.CStr(), htmlified_link, htmlified_data);
#undef ABOUT_PAGEINFO_FORMAT
		}
		else
		{
#ifdef ABOUT_SIMPLIFIED_PAGEINFO
# define ABOUT_PAGEINFO_FORMAT UNI_L(" <tr align=\"left\"><th>%s</th></tr><tr align=\"left\"><td>%s</td></tr>\n")
#else // !ABOUT_SIMPLIFIED_PAGEINFO
# define ABOUT_PAGEINFO_FORMAT UNI_L(" <tr><th>%s</th><td>%s</td></tr>\n")
#endif // ABOUT_SIMPLIFIED_PAGEINFO
			m_url.WriteDocumentDataUniSprintf(ABOUT_PAGEINFO_FORMAT, heading.CStr(), htmlified_data);
#undef ABOUT_PAGEINFO_FORMAT
		}
		delete[] htmlified_data;
		delete[] htmlified_link;
	}
	return OpStatus::OK;
}

OP_STATUS OpPageInfo::CloseTable()
{
	return m_url.WriteDocumentData(URL::KNormal, UNI_L("</table>\n"));
}

OP_STATUS OpPageInfo::AddListItemLink(const uni_char *urlname, const uni_char *extra)
{
	uni_char *htmlified = HTMLify_string(urlname, uni_strlen(urlname), TRUE);
	if (!htmlified)
		return OpStatus::ERR_NO_MEMORY;

	m_url.WriteDocumentDataUniSprintf(UNI_L(" <li><a target=\"_blank\" href=\"%s\">%s%s</a></li>\n"),
		htmlified, htmlified, (extra ? extra : UNI_L("")));
	delete[] htmlified;
	return OpStatus::OK;
}

#if defined AB_USE_IMAGE_METADATA && defined IMAGE_METADATA_SUPPORT
Str::LocaleString OpPageInfo::GetExifLocaleId(int meta_data_id)
{
	return static_cast<Str::LocaleString>(exif_locale_id[meta_data_id]);
}
#endif

OP_STATUS OpPageInfo::GenerateData()
{
	// Sanity check
	if (!m_logdoc || !m_frmdoc)
		return OpStatus::ERR_NULL_POINTER;

	// Cache global pointers
	OpLocale *oplocale = g_oplocale;

	// Temporary strings
	OpString tmp_locale_string;

	// Get URL object
	URL url = m_frmdoc->GetURL();
#if defined _MIME_SUPPORT_ && defined TRUST_RATING
	URL real_url = m_frmdoc->GetURL(); // in case we go inside a MHTML archive
#else
# define real_url url
#endif

	// Get logical document profile
	HLDocProfile *hldoc = m_logdoc->GetHLDocProfile();

#ifdef _LOCALHOST_SUPPORT_
	RETURN_IF_ERROR(OpenDocument(Str::S_INFOPANEL_TITLE, PrefsCollectionFiles::StyleInfoPanelFile));
#else
	RETURN_IF_ERROR(OpenDocument(Str::S_INFOPANEL_TITLE));
#endif
	RETURN_IF_ERROR(OpenBody());

	// Page title
	{
		TempBuffer buffer;
		const uni_char *page_title = m_frmdoc->Title(&buffer);
		if (!page_title || !*page_title)
		{
			// Use file name if no title
			OP_STATUS rc;
			TRAP_AND_RETURN_VALUE_IF_ERROR(rc, url.GetAttributeL(URL::KSuggestedFileName_L, tmp_locale_string), rc);
			if (tmp_locale_string.IsEmpty())
			{
				RETURN_IF_ERROR(g_languageManager->GetString(Str::S_INFOPANEL_NOTITLE, tmp_locale_string));
			}
			page_title = tmp_locale_string.CStr();
		}
		if (page_title)
		{
#ifdef ABOUT_SIMPLIFIED_PAGEINFO
			RETURN_IF_ERROR(OpenTable());
			RETURN_IF_ERROR(AddTableRow(Str::M_LOC_OPERA_PAGE_INFO_PAGE_TITLE, page_title));
			RETURN_IF_ERROR(CloseTable());
#else // !ABOUT_SIMPLIFIED_PAGEINFO
			uni_char *htmlified_title = HTMLify_string(page_title, uni_strlen(page_title), TRUE);
			if (htmlified_title)
			{
				m_url.WriteDocumentDataUniSprintf(UNI_L("<h1>%s</h1>\n"), htmlified_title);
				delete[] htmlified_title;
			}
			else
			{
				return OpStatus::ERR_NO_MEMORY;
			}
#endif // ABOUT_SIMPLIFIED_PAGEINFO
		}
	}

	// start of table of page stats
	RETURN_IF_ERROR(OpenTable());

	// Page URL
	OpString url_name;
	RETURN_IF_ERROR(url.GetAttribute(URL::KUniName, url_name));
#ifdef ABOUT_SIMPLIFIED_PAGEINFO
	RETURN_IF_ERROR(AddTableRow(Str::S_EZX_ADDRESS_LABEL, url_name));
#else // !ABOUT_SIMPLIFIED_PAGEINFO
	RETURN_IF_ERROR(AddTableRow(Str::S_INFOPANEL_URL, url_name));
#endif // ABOUT_SIMPLIFIED_PAGEINFO

#ifdef M2_SUPPORT
	// If this is an e-mail message, fetch the mail to get meta data from it
	// instead.
	Message message;
	Message *m2msg = NULL;
	Window *win = m_frmdoc->GetWindow();
	if (win && win->IsMailOrNewsfeedWindow())
	{
		OpStringC urlname(url.GetAttribute(URL::KUniName));
		int mail_id;
		if (urlname.CStr() && uni_sscanf(urlname, UNI_L("operaemail:/%d/"), &mail_id) == 1)
		{
			RETURN_IF_ERROR(g_m2_engine->GetMessage(message, mail_id, TRUE));
			m2msg = &message;
		}
	}
#else // !M2_SUPPORT
	const int m2msg = 0;
#endif // M2_SUPPORT
#ifndef ABOUT_SIMPLIFIED_PAGEINFO
	// If this is an MHTML archive, we need to step into the IFRAME that
	// holds the main part. We identify MHTML archived by the "is
	// generated by Opera" flag, and that they have one iframe with
	// an attachment URL.
	// FIXME: Need proper API (bug#241054)
	FramesDocElm * OP_MEMORY_VAR ifrm_root = m_frmdoc->GetIFrmRoot();
#ifdef _MIME_SUPPORT_
	if (!m2msg && ifrm_root && m_frmdoc->IsGeneratedByOpera())
	{
		FramesDocElm *first_iframe = static_cast<FramesDocElm *>(ifrm_root->FirstChild());
		OpString current_url_name;
		RETURN_IF_ERROR(first_iframe->GetCurrentURL().GetAttribute(URL::KUniName, current_url_name));
		if (first_iframe && !first_iframe->Next() &&
		    current_url_name.Compare("attachment", 10) == 0)
		{
			// Get data from this document instead
			FramesDocument *iframe_document = first_iframe->GetCurrentDoc();
			if (iframe_document)
			{
				m_frmdoc = iframe_document;
				url = m_frmdoc->GetURL();
				m_logdoc = m_frmdoc->GetLogicalDocument();
			}
		}

		ifrm_root = m_frmdoc->GetIFrmRoot();
	}
#endif // _MIME_SUPPORT_

	// Check if this is an image or plugin in a HTML wrapper
	BOOL is_image_wrapper = !!url.GetAttribute(URL::KIsImage, TRUE);
	BOOL is_wrapper_doc =
		m_frmdoc->IsWrapperDoc() ||
		static_cast<URLContentType>(url.GetAttribute(URL::KContentType,TRUE)) == URL_TEXT_CONTENT;

	// Page encoding
	{
		OpString encoding_string;

#ifdef M2_SUPPORT
		if (m2msg)
		{
			OpString8 charset8;
			RETURN_IF_ERROR(m2msg->GetCharset(charset8));
			encoding_string.SetFromUTF8(charset8);
		}
		else
#endif // M2_SUPPORT
		{
			OpString operaencoding;
			RETURN_IF_ERROR(operaencoding.SetFromUTF8(hldoc ? hldoc->GetCharacterSet() : ""));
			if (operaencoding.HasContent())
			{
				// This is a text-based type which actually has an encoding
				OpString serverencoding;
				RETURN_IF_ERROR(serverencoding.SetFromUTF8(url.GetAttribute(URL::KMIME_CharSet).CStr()));
				if (0 == serverencoding.Length())
				{
					RETURN_IF_ERROR(g_languageManager->GetString(Str::S_INFOPANEL_NO_SUPPLIED_ENCODING, serverencoding));
				}

				RETURN_IF_ERROR(encoding_string.AppendFormat(UNI_L("%s (%s)"), serverencoding.CStr(), operaencoding.CStr()));
			}
		}

		RETURN_IF_ERROR(AddTableRow(Str::S_INFOPANEL_ENCODING, encoding_string));
	}

#ifdef DISPLAY_WRITINGSYSTEM_HEURISTIC
	// Detected script
	WritingSystem::Script detected = hldoc->GetGuessedScript();
	if (detected != WritingSystem::Unknown)
	{
	    RETURN_IF_ERROR(AddTableRow(Str::S_INFOPANEL_WRITINGSYSTEM, ScriptToLocaleString(detected)));
	}
#endif

	// Page MIME type
	OpString mimetype;
#ifdef M2_SUPPORT
	if (m2msg)
	{
		Header::HeaderValue header_value;
		RETURN_IF_ERROR(m2msg->GetHeaderValue("Content-Type", header_value));
		RETURN_IF_ERROR(mimetype.Set(header_value));
		int semicolon = mimetype.FindFirstOf(';');
		if (semicolon != KNotFound)
		{
			mimetype[semicolon] = 0;
		}
	}
	else
#endif // M2_SUPPORT
	{
		RETURN_IF_ERROR(mimetype.SetFromUTF8(url.GetAttribute(URL::KMIME_Type).CStr()));
	}
	RETURN_IF_ERROR(AddTableRow(Str::S_INFOPANEL_MIME, mimetype));
#endif // !ABOUT_SIMPLIFIED_PAGEINFO
	// Various bits of information
	RETURN_IF_ERROR(g_languageManager->GetString(Str::SI_IDSTR_BYTES, tmp_locale_string));
	{
		OpFileLength size =
#ifdef M2_SUPPORT
			m2msg ? m2msg->GetMessageSize() :
#endif
			url.GetContentLoaded();

		OpString pagesize;
		if (!pagesize.Reserve(64))
		{
			return OpStatus::ERR_NO_MEMORY;
		}
		if (oplocale->NumberFormat(pagesize.CStr(), pagesize.Capacity(), size, TRUE) == -1)
			return OpStatus::ERR;
		RETURN_IF_ERROR(pagesize.Append(" "));
		RETURN_IF_ERROR(pagesize.Append(tmp_locale_string /* SI_IDSTR_BYTES */));
#ifdef ABOUT_SIMPLIFIED_PAGEINFO
		RETURN_IF_ERROR(AddTableRow(Str::D_EZX_SIZE_LABEL, pagesize));
#else // !ABOUT_SIMPLIFIED_PAGEINFO
		RETURN_IF_ERROR(AddTableRow(Str::S_INFOPANEL_SIZE, pagesize));
#endif // ABOUT_SIMPLIFIED_PAGEINFO
	}
#ifndef ABOUT_SIMPLIFIED_PAGEINFO
	ImageLoadingInfo image_info;
	m_frmdoc->GetImageLoadingInfo(image_info);

	if (!m2msg && !is_wrapper_doc && (image_info.loaded_count || image_info.total_count || image_info.loaded_size))
	{
		// Number of inline elements
		OpString inlines;
		uni_char total_count[64]; /* ARRAY OK 2008-09-08 adame */
		uni_char imageloadedsizenumber[64]; /* ARRAY OK 2008-09-08 adame */
		if (oplocale->NumberFormat(total_count,  ARRAY_SIZE(total_count),  image_info.total_count, TRUE) == -1)
			return OpStatus::ERR;
		if (oplocale->NumberFormat(imageloadedsizenumber, ARRAY_SIZE(imageloadedsizenumber), image_info.loaded_size, TRUE) == -1)
			return OpStatus::ERR;
		RETURN_IF_ERROR(inlines.AppendFormat(UNI_L("%s (%s %s)"), total_count, imageloadedsizenumber, tmp_locale_string.CStr() /* SI_IDSTR_BYTES */));
		RETURN_IF_ERROR(AddTableRow(Str::S_INFOPANEL_INLINES, inlines));
	}

	if (!m2msg && !is_wrapper_doc && hldoc)
	{
		// Display mode
		RETURN_IF_ERROR(g_languageManager->GetString(
			hldoc->IsInStrictMode()
				? Str::S_INFOPANEL_COMPAT_MODE_STRICT
				: Str::S_INFOPANEL_COMPAT_MODE_QUIRKS,
			tmp_locale_string));
		RETURN_IF_ERROR(AddTableRow(Str::S_INFOPANEL_COMPAT_MODE, tmp_locale_string));
	}

#if LANGUAGE_DATABASE_VERSION >= 879
	// Image info
	Image img;
	if (is_image_wrapper)
	{
		img = UrlImageContentProvider::GetImageFromUrl(url);
		if (!img.IsEmpty())
		{
			// Dimensions and frame count
			int width = static_cast<int>(img.Width()),
				height = static_cast<int>(img.Height());
			if (width && height)
			{
				OpString dimensions;
				dimensions.Reserve(64);
				uni_char width_str[32]; /* ARRAY OK 2008-09-11 adame */
				uni_char height_str[32]; /* ARRAY OK 2008-09-11 adame */
				if (oplocale->NumberFormat(width_str, ARRAY_SIZE(width_str), width, TRUE) == -1)
					return OpStatus::ERR;
				if (oplocale->NumberFormat(height_str, ARRAY_SIZE(height_str), height, TRUE) == -1)
					return OpStatus::ERR;
				RETURN_IF_ERROR(dimensions.AppendFormat(UNI_L("%s \xD7 %s "), width_str, height_str));
				RETURN_IF_ERROR(AppendLocaleString(&dimensions, Str::SI_IDSTR_PIXELS));
				if (img.GetFrameCount() > 1)
				{
					OpString animation_template;
					RETURN_IF_ERROR(animation_template.Set(" "));
					RETURN_IF_ERROR(AppendLocaleString(&animation_template, Str::SI_IDSTR_ANIMATED_IN_FRAME));
					RETURN_IF_ERROR(dimensions.AppendFormat(animation_template.CStr(), img.GetFrameCount()));
				}

				RETURN_IF_ERROR(AddTableRow(Str::S_INFOPANEL_DIMENSIONS, dimensions));
			}

			// Depth
			if (img.GetBitsPerPixel())
			{
				OpString bpp;
				bpp.Reserve(64);
				uni_char bpp_str[32]; /* ARRAY OK 2008-09-11 adame */
				if (oplocale->NumberFormat(bpp_str, ARRAY_SIZE(bpp_str), img.GetBitsPerPixel(), TRUE) == -1)
					return OpStatus::ERR;
				RETURN_IF_ERROR(bpp.Set(bpp_str));
				RETURN_IF_ERROR(bpp.Append(" "));
				RETURN_IF_ERROR(AppendLocaleString(&bpp, Str::SI_IDSTR_BPP));
				RETURN_IF_ERROR(AddTableRow(Str::S_INFOPANEL_DEPTH, bpp));
			}
		}
	}

	// Last modified
	time_t last_modified = 0;
	url.GetAttribute(URL::KVLastModified, &last_modified);
	if (last_modified)
	{
		struct tm *tm_p = op_localtime(&last_modified);
		if (tm_p)
		{
			uni_char timestring[128]; /* ARRAY OK 2008-09-11 adame */
			g_oplocale->op_strftime(timestring, ARRAY_SIZE(timestring), UNI_L("%x %X"), tm_p);
			RETURN_IF_ERROR(AddTableRow(Str::S_INFOPANEL_LAST_MODIFIED, timestring));
		}
	}

	// Downloaded
	time_t downloaded_time = 0;
	url.GetAttribute(URL::KVLocalTimeLoaded, &downloaded_time);
	if (!m2msg && downloaded_time)
	{
		struct tm *tm_p = op_localtime(&downloaded_time);
		if (tm_p)
		{
			uni_char timestring[128]; /* ARRAY OK 2008-09-11 adame */
			g_oplocale->op_strftime(timestring, ARRAY_SIZE(timestring), UNI_L("%x %X"), tm_p);
			RETURN_IF_ERROR(AddTableRow(Str::S_INFOPANEL_DOWNLOADED, timestring));
		}
	}
#endif // LANGUAGE_DATABASE_VERSION >= 879

	// Cache info
#ifndef RAMCACHE_ONLY
	OpString cachefilename;
	TRAP_AND_RETURN(rc, url.GetAttributeL(URL::KFilePathName_L, cachefilename, TRUE));
	if (cachefilename.IsEmpty())
	{
		RETURN_IF_ERROR(g_languageManager->GetString(Str::S_INFOPANEL_NOCACHE, cachefilename));
	}

	RETURN_IF_ERROR(AddTableRow(Str::S_INFOPANEL_CACHE, cachefilename));
#endif // RAMCACHE_ONLY
#endif // !ABOUT_SIMPLIFIED_PAGEINFO
	// end of table of page stats
#if !defined(ABOUT_SIMPLIFIED_PAGEINFO)
// For a simplified pageinfo, the security section will be included in the
// same table ...
	RETURN_IF_ERROR(CloseTable());
#endif // !ABOUT_SIMPLIFIED_PAGEINFO

	// Security section
	if (!m2msg)
	{
#if !defined(ABOUT_SIMPLIFIED_PAGEINFO)
// For a simplified pageinfo, the security section will be included in the
// same table -  and the table does not need a new heading:
		RETURN_IF_ERROR(Heading(Str::S_INFOPANEL_SECURITY));

		// start of table of security stats
		RETURN_IF_ERROR(OpenTable());
#endif // !ABOUT_SIMPLIFIED_PAGEINFO

		OpStringC securitytext(url.GetAttribute(URL::KSecurityText));
		if (securitytext.IsEmpty())
		{
			RETURN_IF_ERROR(g_languageManager->GetString(Str::S_INFOPANEL_NO_SECURITY, tmp_locale_string));
			securitytext = tmp_locale_string.CStr();
		}
#ifdef TRUST_RATING
		Window *window = m_frmdoc->GetWindow();
		if (OpStatus::IsSuccess(window->CheckTrustRating(FALSE, TRUE)) && window->GetTrustRating() == Phishing)
		{
			RETURN_IF_ERROR(g_languageManager->GetString(Str::D_NEW_SECURITY_INFORMATION_FRAUD_SITE, tmp_locale_string));
			securitytext = tmp_locale_string.CStr();
		}
#endif // TRUST_RATING

#ifdef _SECURE_INFO_SUPPORT
		m_security_url->UnsetURL();
		URL sec_url = url.GetAttribute(URL::KSecurityInformationURL);
		if (!sec_url.IsEmpty())
		{
			m_security_url->SetURL(sec_url);
# ifdef ABOUT_SIMPLIFIED_PAGEINFO
			RETURN_IF_ERROR(AddTableRow(Str::S_INFOPANEL_SECURITY_PROTOCOL, securitytext, &sec_url));
# else // !ABOUT_SIMPLIFIED_PAGEINFO
			RETURN_IF_ERROR(AddTableRow(Str::S_INFOPANEL_SUMMARY, securitytext, &sec_url));
# endif // ABOUT_SIMPLIFIED_PAGEINFO
		}
		else
#endif // _SECURE_INFO_SUPPORT
		{
#ifdef ABOUT_SIMPLIFIED_PAGEINFO
			RETURN_IF_ERROR(AddTableRow(Str::S_INFOPANEL_SECURITY_PROTOCOL, securitytext));
#else // !ABOUT_SIMPLIFIED_PAGEINFO
			RETURN_IF_ERROR(AddTableRow(Str::S_INFOPANEL_SUMMARY, securitytext));
#endif // ABOUT_SIMPLIFIED_PAGEINFO
		}

#ifdef TRUST_RATING
		// Create a fraud-check URL for network URLs, but only if enabled
		if (!g_url_api->OfflineLoadable(static_cast<URLType>(real_url.GetAttribute(URL::KType))) &&
		    g_pcnet->GetIntegerPref(PrefsCollectionNetwork::EnableTrustRating,
									reinterpret_cast<const ServerName *>(real_url.GetAttribute(URL::KServerName, (void*)0))))
		{
			URL trust_url;

			if (OpStatus::IsSuccess(TrustInfoParser::GenerateRequestURL(real_url, trust_url, TRUE)))
			{
				RETURN_IF_ERROR(g_languageManager->GetString(Str::D_NEW_SECURITY_INFORMATION_TRUST_PAGE_DOWNLOAD_BUTTON_TEXT, tmp_locale_string));
				// Make sure any HTML-special characters in the fraud check
				// URL are escaped.
				OpString trust_url_name;
				RETURN_IF_ERROR(trust_url.GetAttribute(URL::KUniName, trust_url_name));
				uni_char *htmlified_link = HTMLify_string(trust_url_name.CStr(), trust_url_name.Length(), TRUE);
				if (!htmlified_link)
					return OpStatus::ERR_NO_MEMORY;
				m_url.WriteDocumentDataUniSprintf(UNI_L(" <tr><td colspan=\"2\"><a href=\"%s\" target=\"_blank\">%s</a></td></tr>\n"),
				                                  htmlified_link, tmp_locale_string.CStr());
				delete[] htmlified_link;
			}
		}
#endif // TRUST_RATING

#if !defined(ABOUT_SIMPLIFIED_PAGEINFO)
// For a simplified pageinfo, the security section will be included in the
// same table - for the normal pageinfo we have to close the security
// section's table:
		// end of table of security stats
		RETURN_IF_ERROR(CloseTable());
#endif // !ABOUT_SIMPLIFIED_PAGEINFO
	}

#ifdef ABOUT_SIMPLIFIED_PAGEINFO
// For a simplified pageinfo, the security section will be included in the
// same table - which is closed after the security section:
	RETURN_IF_ERROR(CloseTable());

#else // !ABOUT_SIMPLIFIED_PAGEINFO
	RETURN_IF_ERROR(g_languageManager->GetString(Str::S_LIST_ENTRY, tmp_locale_string));

	// Meta data from document
	HTML_Element *meta_element = m_logdoc->GetFirstHE(HE_META);
	if (meta_element)
	{
		unsigned int items = 0;

		while (meta_element)
		{
			// Extract title and content from the meta element; prefer http-equiv
			// to name if both are specified
			const uni_char *name = meta_element->GetStringAttr(ATTR_HTTP_EQUIV);
			if (!name)
			{
				name = meta_element->GetStringAttr(ATTR_NAME);
			}
			OpString name_with_colon;
			if (name)
			{
				// List style consistent with other lists
				if (OpStatus::IsSuccess(name_with_colon.AppendFormat(tmp_locale_string.CStr() /*S_LIST_ENTRY*/, name)))
				{
					name = name_with_colon.CStr();
				}
			}

			const uni_char *content = meta_element->GetStringAttr(ATTR_CONTENT);

			if (name && *name && content && *content)
			{
				// Need to HTMLifiy these
				uni_char *name_htmlified    = HTMLify_string(name, uni_strlen(name), TRUE);
				uni_char *content_htmlified = HTMLify_string(content); /* allow links */
				OP_STATUS rc = OpStatus::OK;
				if (name_htmlified && content_htmlified)
				{
					if (1 == ++ items)
					{
						rc = Heading(Str::S_INFOPANEL_META);
						if (OpStatus::IsSuccess(rc))
						{
							rc = OpenTable();
						}
					}

					if (OpStatus::IsSuccess(rc))
					{
						m_url.WriteDocumentDataUniSprintf(UNI_L(" <tr><th>%s</th><td>%s</td></tr>\n"), name_htmlified, content_htmlified);
					}
				}
				else
				{
					rc = OpStatus::ERR_NO_MEMORY;
				}

				delete[] name_htmlified;
				delete[] content_htmlified;

				RETURN_IF_ERROR(rc);
			}

			// Find the next meta element
			do
			{
				meta_element = meta_element->NextActual();
			} while (meta_element && !meta_element->IsMatchingType(HE_META, NS_HTML));
		}

		if (items)
		{
			RETURN_IF_ERROR(CloseTable());
		}
	}
#if LANGUAGE_DATABASE_VERSION >= 879
#if defined AB_USE_IMAGE_METADATA && defined IMAGE_METADATA_SUPPORT
	else if (!img.IsEmpty() && img.HasMetaData())
	{
		// Use EXIF data if image
		int items = 0;
		OpString metadatastr;
		metadatastr.Reserve(64);

		// If this assert fails, it means that there are some metadata types
		// available that we haven't added translation strings for and correctly
		// mapped in the table above.
		OP_ASSERT(ARRAY_SIZE(exif_locale_id) == IMAGE_METADATA_COUNT);

		for (size_t i = 0; i < ARRAY_SIZE(exif_locale_id); ++ i)
		{
			const char *meta_data = img.GetMetaData((ImageMetaData)i);
			if (meta_data && *meta_data)
			{
				RETURN_IF_ERROR(metadatastr.Set(meta_data));
				if (1 == ++ items)
				{
					RETURN_IF_ERROR(Heading(Str::S_INFOPANEL_META));
					RETURN_IF_ERROR(OpenTable());
				}
				RETURN_IF_ERROR(AddTableRow(GetExifLocaleId(i), metadatastr));
			}
		}
		if (items)
		{
			RETURN_IF_ERROR(CloseTable());
		}
	}
#endif // AB_USE_IMAGE_METADATA && IMAGE_METADATA_SUPPORT
#endif // LANGUAGE_DATABASE_VERSION >= 879

	// Navigation links section
#ifdef LINK_TRAVERSAL_SUPPORT
	OpAutoVector<OpElementInfo> links;
	m_frmdoc->GetLinkElements(&links);
	unsigned int link_items = links.GetCount();
	if (link_items)
	{
		unsigned int items = 0;
		for (unsigned int i = 0; i < link_items; ++ i)
		{
			OpElementInfo *link = links.Get(i);
			// Find navigation links, i.e links with REL or REV attributes.
			// Ignore style sheets as they have already been listed.
			if (link->GetRel() || (link->GetRev() &&
				!(link->GetRel() && uni_strni_eq_upper(link->GetRel(), "STYLESHEET", 11))))
			{
				OP_STATUS rc = OpStatus::OK;

				const uni_char *name = link->GetTitle();

				// If LINK has both a name and a REL or REV attribute, we
				// want to list both in the panel
				OpString name_string;
				if (name)
				{
					const uni_char *rel_or_rev =
						link->GetRel() ? link->GetRel() : link->GetRev();
					if (rel_or_rev && *rel_or_rev)
					{
						OP_STATUS rc2 = name_string.AppendFormat(UNI_L("%s (%s)"), rel_or_rev, name);
						if (OpStatus::IsSuccess(rc2))
							name = name_string.CStr();
					}
				}

				if (!name) name = link->GetRel();
				if (!name) name = link->GetRev();

				OpString name_with_colon;
				if (name)
				{
					// List style consistent with other lists
					if (OpStatus::IsSuccess(name_with_colon.AppendFormat(tmp_locale_string.CStr() /*S_LIST_ENTRY*/, name)))
					{
						name = name_with_colon.CStr();
					}
				}
				if (name && link->GetURL())
				{
					if (++ items == 1)
					{
						rc = Heading(Str::S_INFOPANEL_LINK);
						if (OpStatus::IsSuccess(rc))
							rc = OpenTable();
					}
					if (OpStatus::IsSuccess(rc))
					{
						uni_char *htmlified_name = HTMLify_string(name, uni_strlen(name), TRUE);
						OpString link_name_fragment;
						rc = link->GetURL()->GetAttribute(URL::KUniName_With_Fragment, link_name_fragment);
						if (OpStatus::IsSuccess(rc))
							rc = AddTableRow(htmlified_name, link_name_fragment, link->GetURL());
						delete[] htmlified_name;
					}
				}

				RETURN_IF_ERROR(rc);
			}
		}

		if (items)
		{
			RETURN_IF_ERROR(CloseTable());
		}
	}
#endif // LINK_TRAVERSAL_SUPPORT

	// Styles section
	if (!is_image_wrapper && hldoc)
	{
#if LANGUAGE_DATABASE_VERSION >= 863
		RETURN_IF_ERROR(g_languageManager->GetString(Str::S_INFOPANEL_STYLE_ALTERNATE, tmp_locale_string));
#else
		tmp_locale_string.Set(" (alternate)");
#endif

		unsigned int stylesheets = 0;

		CSSCollection::Iterator iter(hldoc->GetCSSCollection(), CSSCollection::Iterator::STYLESHEETS);
		CSSCollectionElement* elm;

		while ((elm = iter.Next()))
		{
			CSS* css = static_cast<CSS*>(elm);
			const URL* cssurl = css->GetHRef(m_logdoc);
			if (cssurl)
			{
				// Check for duplicates
				BOOL skip = FALSE;

				CSSCollection::Iterator dup_iter(hldoc->GetCSSCollection(), CSSCollection::Iterator::STYLESHEETS);
				CSSCollectionElement* dup_elm;
				while ((dup_elm = dup_iter.Next()) && dup_elm != elm)
				{
					CSS* dupcss = static_cast<CSS*>(dup_elm);

					// Must be both the same URL and the same type for to be
					// considered a duplicate.
					const URL* dupcssurl = dupcss->GetHRef(m_logdoc);
					if (dupcssurl && css->GetKind() == dupcss->GetKind() &&
					    dupcssurl->operator==(*cssurl))
					{
						// Ignore this duplicate.
						skip = TRUE;
						break;
					}
				}
				if (skip)
				{
					continue;
				}

				if (++stylesheets == 1)
				{
					RETURN_IF_ERROR(Heading(Str::S_INFOPANEL_STYLE));
					RETURN_IF_ERROR(OpenUnorderedList());
				}
				OpString css_url_name;
				RETURN_IF_ERROR(cssurl->GetAttribute(URL::KUniName, css_url_name));
				RETURN_IF_ERROR(AddListItemLink(css_url_name.CStr(),
				                (css->GetKind() == CSS::STYLE_ALTERNATE) ? tmp_locale_string.CStr() /*S_INFOPANEL_STYLE_ALTERNATE*/ : NULL));
			}
		}

		if (stylesheets)
		{
			RETURN_IF_ERROR(CloseList());
		}
	}

	// Scripts section
	const OpVector<URL> *script_list = hldoc ? hldoc->GetScriptList() : NULL;

	unsigned int numscripts = script_list ? script_list->GetCount() : 0;
	unsigned int linked_scripts = 0;
	if (numscripts > 0)
	{
		for (unsigned int scriptnum = 0; scriptnum < numscripts; ++ scriptnum)
		{
			URL *script_url = script_list->Get(scriptnum);
			if (!(url == *script_url))
			{
				if (++ linked_scripts == 1)
				{
					RETURN_IF_ERROR(Heading(Str::S_INFOPANEL_SCRIPT));
					RETURN_IF_ERROR(OpenUnorderedList());
				}
				OpString script_url_name;
				RETURN_IF_ERROR(script_url->GetAttribute(URL::KUniName, script_url_name));
				RETURN_IF_ERROR(AddListItemLink(script_url_name.CStr()));
			}
		}

		if (linked_scripts)
		{
			RETURN_IF_ERROR(CloseList());
		}
	}


#ifdef XSLT_SUPPORT
	// Transformations section
	unsigned int num_xslt = m_logdoc->GetXSLTStylesheetsCount();
	if (num_xslt)
	{
		RETURN_IF_ERROR(Heading(Str::S_INFOPANEL_XSLT));
		RETURN_IF_ERROR(OpenUnorderedList());

		for (unsigned int i = 0; i < num_xslt; ++ i)
		{
			OpString xslt_css_url;
			RETURN_IF_ERROR(m_logdoc->GetXSLTStylesheetURL(i).GetAttribute(URL::KUniName, xslt_css_url));
			RETURN_IF_ERROR(AddListItemLink(xslt_css_url.CStr()));
		}

		RETURN_IF_ERROR(CloseList());
	}
#endif // XSLT_SUPPORT

	// Frames section
	FramesDocElm *frm_root = m_frmdoc->GetFrmDocRoot();
	if (!m2msg && (frm_root || ifrm_root))
	{
		RETURN_IF_ERROR(Heading(frm_root ? Str::S_INFOPANEL_FRAMES : Str::S_INFOPANEL_IFRAMES));
		RETURN_IF_ERROR(OpenUnorderedList());

		for (FramesDocElm *tmp_elm = static_cast<FramesDocElm *>((frm_root ? frm_root : ifrm_root)->FirstChild());
		     tmp_elm;
		     tmp_elm = static_cast<FramesDocElm *>(tmp_elm->Next()))
		{
			// List all children for IFRAMEs, but only actual content frames
			// for FRAMESETs.
			if (!frm_root || !tmp_elm->IsFrameset())
			{
				OpString current_url_name;
				RETURN_IF_ERROR(tmp_elm->GetCurrentURL().GetAttribute(URL::KUniName_With_Fragment, current_url_name));
				RETURN_IF_ERROR(AddListItemLink(current_url_name.CStr()));
			}
		}
		RETURN_IF_ERROR(CloseList());
	}
#endif // !ABOUT_SIMPLIFIED_PAGEINFO
	/* Finish document */
	return CloseDocument();
}

#endif
