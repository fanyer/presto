/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Erman Doser (ermand)
 */
#include "core/pch.h"

#include "adjunct/quick/controller/ImagePropertiesController.h"

#include "adjunct/desktop_util/adt/typedobjectcollection.h"
#include "adjunct/desktop_util/string/stringutils.h"
#include "adjunct/quick_toolkit/widgets/QuickDialog.h"
#include "adjunct/quick_toolkit/widgets/QuickLabel.h"

#include "modules/about/oppageinfo.h"
#include "modules/encodings/detector/charsetdetector.h"
#include "modules/logdoc/urlimgcontprov.h"
#include "modules/pi/OpLocale.h"
#include "modules/util/htmlify.h"

ImagePropertiesController::ImagePropertiesController(
		const URL& url, const OpStringC& alt, const OpStringC& longdesc)
	: m_url(url)
	, m_alt(alt)
	, m_longdesc(longdesc)
{
}

void ImagePropertiesController::InitL()
{
	LEAVE_IF_ERROR(SetDialog("Image Properties Dialog"));

	QuickBrowserView* details_view = m_dialog->GetWidgetCollection()->GetL<QuickBrowserView>("Imagedetails_browserview");
	details_view->SetListener(this);

	LEAVE_IF_ERROR(InitProperties());
}

OP_STATUS ImagePropertiesController::InitProperties()
{
	OpString name;
	RETURN_IF_LEAVE(m_url.GetAttributeL(URL::KSuggestedFileName_L, name, TRUE));
	if (name.HasContent())
	{
		OpString title;
		RETURN_IF_ERROR(title.AppendFormat(UNI_L("%s - %s"), m_dialog->GetTitle(), name.CStr()));
		RETURN_IF_ERROR(m_dialog->SetTitle(title));
	}

	m_image = UrlImageContentProvider::GetImageFromUrl(m_url);
	if (m_image.IsEmpty())
		return OpStatus::OK;

	OpString animated;
	if (m_image.GetFrameCount() > 1)
	{
		RETURN_IF_ERROR(StringUtils::GetFormattedLanguageString(
					animated, Str::SI_IDSTR_ANIMATED_IN_FRAME, m_image.GetFrameCount()));
	}

	OpString pixels;
	RETURN_IF_ERROR(g_languageManager->GetString(Str::SI_IDSTR_PIXELS, pixels));
	OpString bpp;
	RETURN_IF_ERROR(g_languageManager->GetString(Str::SI_IDSTR_BPP, bpp));

	OpString width_string;
	OpString height_string;
	RETURN_OOM_IF_NULL(width_string.Reserve(24));
	RETURN_OOM_IF_NULL(height_string.Reserve(24));
	g_oplocale->NumberFormat(width_string.DataPtr(), width_string.Capacity(), int(m_image.Width()), TRUE);
	g_oplocale->NumberFormat(height_string.DataPtr(), height_string.Capacity(), int(m_image.Height()), TRUE);
	OpString dimensions;
	RETURN_IF_ERROR(dimensions.AppendFormat(UNI_L("%s x %s %s @ %i %s"),
				width_string.CStr(), height_string.CStr(), pixels.CStr(), m_image.GetBitsPerPixel(), bpp.CStr()));
	if (animated.HasContent())
	{
		RETURN_IF_ERROR(dimensions.Append(" "));
		RETURN_IF_ERROR(dimensions.Append(animated));
	}
	RETURN_IF_ERROR(SetWidgetText<QuickLabel>("Image_dimension_content", dimensions));

	const char* focal_length  = m_image.GetMetaData(IMAGE_METADATA_FOCALLENGTH);
	const char* f_number      = m_image.GetMetaData(IMAGE_METADATA_FNUMBER);
	const char* exposure_time = m_image.GetMetaData(IMAGE_METADATA_EXPOSURETIME);
	const char* iso_speed     = m_image.GetMetaData(IMAGE_METADATA_ISOSPEEDRATING);
	const char* camera_model  = m_image.GetMetaData(IMAGE_METADATA_MODEL);
	if (focal_length && f_number && exposure_time && iso_speed)
	{
		OpString8 nice_exposure_time;
		RETURN_IF_ERROR(nice_exposure_time.Set(exposure_time));
		RETURN_IF_ERROR(MakeNiceExposureTime(nice_exposure_time));

		OpString8 camera_settings;
		RETURN_IF_ERROR(camera_settings.AppendFormat("%smm, f/%s, %ss @ ISO %s",
					focal_length, f_number, nice_exposure_time.CStr(), iso_speed));

		if (camera_model)
		{
			camera_settings.Append(", ");
			camera_settings.Append(camera_model);
		}

		RETURN_IF_ERROR(SetWidgetText<QuickLabel>("Image_camera_settings_content", camera_settings));
	}

	// Displayed address shall never show a password if present in url
	OpString url;
	RETURN_IF_ERROR(m_url.GetAttribute(URL::KUniName_Username_Password_Hidden, url));
	RETURN_IF_ERROR(SetWidgetText<QuickLabel>("Image_address_content", url));

	const char* type = NULL;
	switch (m_url.ContentType())
	{
#ifdef _WML_SUPPORT_
		case URL_WBMP_CONTENT:
			type = "WBMP";
			break;
#endif	//_WML_SUPPORT_
		case URL_GIF_CONTENT:
			type = "GIF";
			break;
		case URL_JPG_CONTENT:
			type = "JPEG";
			break;
		case URL_XBM_CONTENT:
			type = "XBM";
			break;
		case URL_BMP_CONTENT:
			type = "BMP";
			break;
		case URL_PNG_CONTENT:
			type = "PNG";
			break;
		case URL_ICO_CONTENT:
			type = "ICO";
			break;
		case URL_WEBP_CONTENT:
			type = "WebP";
			break;
	}
	if (type != NULL)
		RETURN_IF_ERROR(SetWidgetText<QuickLabel>("Image_type_content", type));
	else
		RETURN_IF_ERROR(SetWidgetText<QuickLabel>("Image_type_content", Str::SI_IDSTR_UNKNOWN_IMAGE_TYPE));

	OpFileLength image_size = m_url.GetContentSize();
	if (image_size <= 0)
	{
		image_size = m_url.GetContentLoaded();
	}

	OpString formatted_string;
	if (formatted_string.Reserve(256))
	{
		StrFormatByteSize(formatted_string, (UINT64)image_size, SFBS_DETAILS);
		RETURN_IF_ERROR(SetWidgetText<QuickLabel>("Image_filesize_content", formatted_string));
	}

	RETURN_IF_ERROR(SetWidgetText<QuickLabel>("Image_description_content", m_alt));
	RETURN_IF_ERROR(SetWidgetText<QuickLabel>("Image_long_desc_content", m_longdesc));

	return OpStatus::OK;
}

OP_STATUS ImagePropertiesController::MakeNiceExposureTime(OpString8& exposure_time)
{
	double t  = 1.0 / atof(exposure_time.CStr());
	double t_ = op_floor(t + 0.5);

	if (op_abs(t - t_) < 0.001)
	{
		exposure_time.Empty();
		RETURN_IF_ERROR(exposure_time.AppendFormat("1/%d", (int)t_));
	}
	return OpStatus::OK;
}

void ImagePropertiesController::OnBrowserViewReady(BrowserView& browser_view)
{
	if (m_image.HasMetaData())
	{
		OpAutoPtr<OpPage> page(OP_NEW(OpPage, (NULL)));
		if (page.get() == NULL)
			return;
		RETURN_VOID_IF_ERROR(page->Init());
		RETURN_VOID_IF_ERROR(browser_view.SetPage(page.release()));

		browser_view.SetTabStop(FALSE);
		browser_view.GetWindowCommander()->SetScale(100);
		browser_view.GetWindowCommander()->SetScriptingDisabled(TRUE);

		URL url = browser_view.BeginWrite();
		ImagePropertiesHTMLView imagehtml(url, &m_image);
		imagehtml.GenerateData();
		browser_view.EndWrite(url);
	}
	else
	{
		QuickBrowserView* quick_browser_view = m_dialog->GetWidgetCollection()->Get<QuickBrowserView>("Imagedetails_browserview");
		if (quick_browser_view)
			quick_browser_view->Hide();
	}

}

OP_STATUS ImagePropertiesController::ImagePropertiesHTMLView::GenerateData()
{
	OpenDocument();

	m_url.WriteDocumentData(URL::KNormal, UNI_L("<style type=\"text/css\">pre { font: message-box; margin: 0 0 2px 0; padding: 0; }</style>\n"));

	OpenBody(Str::NOT_A_STRING);

	for (UINT metadata_index = 0; metadata_index < IMAGE_METADATA_COUNT; ++metadata_index)
	{
		const char* meta_data = m_image->GetMetaData((ImageMetaData)metadata_index);
		if (meta_data != 0)
		{
			OpString metadata_title;
			RETURN_IF_ERROR(g_languageManager->GetString(
						OpPageInfo::GetExifLocaleId(metadata_index), metadata_title));

			OpString metadata_value;
			const char* charset = NULL; 
			int meta_len = op_strlen(meta_data);

			OpString user_lang;
			OpString8 user_lang8;
			RETURN_IF_ERROR(g_op_system_info->GetUserLanguages(&user_lang));
			RETURN_IF_ERROR(user_lang8.SetUTF8FromUTF16(user_lang.CStr()));

			CharsetDetector cd(NULL,NULL,NULL,user_lang8.CStr());
			cd.PeekAtBuffer(meta_data,meta_len);
			charset = cd.GetDetectedCharset();

			if (charset)
				RETURN_IF_ERROR(SetFromEncoding(&metadata_value, charset, meta_data, meta_len, NULL));
			else
				RETURN_IF_ERROR(metadata_value.Set(meta_data));

			if (metadata_title.HasContent() && metadata_value.HasContent())
			{
				m_url.WriteDocumentData(URL::KNormal, UNI_L("<pre><strong>"));
				m_url.WriteDocumentData(URL::KNormal, metadata_title.CStr());
				m_url.WriteDocumentData(URL::KNormal, UNI_L("</strong> "));

				uni_char *escaped_string = HTMLify_string(metadata_value.CStr());
				if(!escaped_string)
				{
					break;
				}
				m_url.WriteDocumentData(URL::KNormal, escaped_string);
				OP_DELETEA(escaped_string);

				m_url.WriteDocumentData(URL::KNormal, UNI_L("</pre>\n"));
			}
		}
	}

	return CloseDocument();
}
