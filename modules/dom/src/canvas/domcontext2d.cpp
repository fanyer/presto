/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef CANVAS_SUPPORT

#include "modules/dom/src/canvas/domcontext2d.h"

#include "modules/libvega/src/canvas/canvas.h"
#include "modules/libvega/src/canvas/canvascontext2d.h"

#ifdef SVG_SUPPORT
#include "modules/svg/svg_image.h"
#include "modules/svg/SVGManager.h"
#include "modules/dom/src/domsvg/domsvgelement.h"
#include "modules/dom/src/domrestartobject.h"
#endif // SVG_SUPPORT

#ifdef MEDIA_HTML_SUPPORT
#include "modules/media/mediaelement.h"
#include "modules/media/mediaplayer.h"
#endif // MEDIA_HTML_SUPPORT

#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/dom/src/domhtml/htmlelem.h"
#include "modules/dom/domutils.h"

#include "modules/doc/frm_doc.h" // for FramesDocument
#include "modules/dochand/win.h"

#include "modules/logdoc/htm_elm.h"
#include "modules/logdoc/urlimgcontprov.h"

#include "modules/style/css.h"

#include "modules/ecmascript_utils/esasyncif.h"

#include "modules/security_manager/include/security_manager.h"

#include "modules/img/imageloadernopremultiply.h"


static BOOL CanvasOriginCheck(DOM_Object *this_object, const URL& url1, const URL& url2, ES_Runtime* origining_runtime)
{
#ifdef GADGET_SUPPORT
	if (origining_runtime && origining_runtime->GetFramesDocument() && origining_runtime->GetFramesDocument()->GetWindow()->GetGadget())
	{
		BOOL allowed = FALSE;
		OpSecurityState secstate;
		OpStatus::Ignore(OpSecurityManager::CheckSecurity(OpSecurityManager::DOM_LOADSAVE,
															OpSecurityContext(static_cast<DOM_Runtime *>(origining_runtime)),
															OpSecurityContext(url1, url2),
															allowed,
															secstate));
		OP_ASSERT(!secstate.suspended);	// We're assuming that the host already has been resolved.

		return allowed;
	}
	else
#endif // GADGET_SUPPORT
	{
		return this_object->OriginCheck(url1, url2);
	}
}

struct DrawImageParams
{
	double src[4];
	double dst[4];
};

#ifdef SVG_SUPPORT

class DOMCanvasContext2D_ElmRefListener
{
public:
	virtual void Unlink() = 0;
};

class SVGIMGResourceListenerElementRef : public ElementRef
{
public:
	SVGIMGResourceListenerElementRef(HTML5ELEMENT* elm,
						  DOMCanvasContext2D_ElmRefListener* listener)
		: ElementRef(elm),
		  m_listener(listener)
	{
	}

	virtual ~SVGIMGResourceListenerElementRef()
	{
		HTML_Element* element = GetElm();
		if (element)
			element->RemoveSVGIMGResourceListener();
	}

	virtual void OnDelete(FramesDocument *document)
	{
		Reset();
		m_listener->Unlink();
	}

	virtual void OnRemove(FramesDocument *document)
	{
		Reset();
		m_listener->Unlink();
	}

private:
	DOMCanvasContext2D_ElmRefListener* m_listener;
};

class DOMCanvasContext2D_RestartCreatePattern
	: public DOM_RestartObject,
	  public DOMCanvasContext2D_ElmRefListener,
	  public SVGIMGResourceListener
{
private:
	DOMCanvasContext2D_RestartCreatePattern(DOM_Object* object, HTML_Element* element, VEGAFill::Spread xsp, VEGAFill::Spread ysp)
		: m_xsp(xsp)
		, m_ysp(ysp)
		, m_object(object)
		, m_elm_ref(element, this)
	{
	}

public:
	static OP_STATUS Make(DOMCanvasContext2D_RestartCreatePattern*& new_obj,
						  DOM_Object* object, HTML_Element* element,
						  VEGAFill::Spread xsp, VEGAFill::Spread ysp,
						  DOM_Runtime* origining_runtime)
	{
		DOMCanvasContext2D_RestartCreatePattern* tmp_obj =
			OP_NEW(DOMCanvasContext2D_RestartCreatePattern, (object, element, xsp, ysp));

		RETURN_IF_ERROR(DOMSetObjectRuntime(tmp_obj, origining_runtime));
		RETURN_IF_ERROR(tmp_obj->KeepAlive());

		new_obj = tmp_obj;
		return OpStatus::OK;
	}

	void CopyTo(VEGAFill::Spread* xsp, VEGAFill::Spread* ysp)
	{
		*xsp = m_xsp;
		*ysp = m_ysp;
	}

	DOM_Object* GetObject()
	{
		return m_object;
	}

	virtual void ResumeAndUnlink()
	{
		Resume();

		HTML_Element* element = m_elm_ref.GetElm();
		if (element)
			element->RemoveSVGIMGResourceListener();
	}

	virtual void SignalFinished()
	{
		ResumeAndUnlink();
	}

	virtual void Unlink()
	{
		ResumeAndUnlink();
	}

private:
	VEGAFill::Spread m_xsp, m_ysp;
	DOM_Object* m_object;
	SVGIMGResourceListenerElementRef m_elm_ref;
};

class DOMCanvasContext2D_RestartDrawImage
	: public DOM_RestartObject,
	  public DOMCanvasContext2D_ElmRefListener,
	  public SVGIMGResourceListener
{
private:
	DOMCanvasContext2D_RestartDrawImage(DOM_Object* object, HTML_Element* element, DrawImageParams& params)
		: m_params(params)
		, m_object(object)
		, m_elm_ref(element, this)
	{
	}

public:
	static OP_STATUS Make(DOMCanvasContext2D_RestartDrawImage*& new_obj,
						  DOM_Object* object, HTML_Element* element,
						  DrawImageParams& params,
						  DOM_Runtime* origining_runtime)
	{
		DOMCanvasContext2D_RestartDrawImage* tmp_obj =
			OP_NEW(DOMCanvasContext2D_RestartDrawImage, (object, element, params));

		RETURN_IF_ERROR(DOMSetObjectRuntime(tmp_obj, origining_runtime));
		RETURN_IF_ERROR(tmp_obj->KeepAlive());

		new_obj = tmp_obj;
		return OpStatus::OK;
	}

	void CopyTo(DrawImageParams* params)
	{
		*params = m_params;
	}

	DOM_Object* GetObject()
	{
		return m_object;
	}

	virtual void ResumeAndUnlink()
	{
		Resume();

		HTML_Element* element = m_elm_ref.GetElm();
		if (element)
			element->RemoveSVGIMGResourceListener();
	}

	virtual void SignalFinished()
	{
		ResumeAndUnlink();
	}

	virtual void Unlink()
	{
		ResumeAndUnlink();
	}

private:
	DrawImageParams m_params;
	DOM_Object* m_object;
	SVGIMGResourceListenerElementRef m_elm_ref;
};
#endif // SVG_SUPPORT

// Helper class to load images

#define DECODE_FAILED_IF_ERROR(expr) \
	do { \
		OP_STATUS DECODE_FAILED_IF_ERROR_TMP = expr; \
		if (OpStatus::IsMemoryError(DECODE_FAILED_IF_ERROR_TMP)) \
			return DECODE_OOM; \
		if (OpStatus::IsError(DECODE_FAILED_IF_ERROR_TMP)) \
			return DECODE_FAILED; \
	} while (0)
/* static */ CanvasResourceDecoder::DecodeStatus
CanvasResourceDecoder::DecodeResource(DOM_Object* domres, DOM_HTMLCanvasElement* domcanvas, DOM_Runtime* origining_runtime,
                     int dest_width, int dest_height, BOOL wantFill, URL& src_url, OpBitmap* &bitmap, VEGAFill* &fill,
                     VEGASWBuffer **non_premultiplied, BOOL ignoreColorSpaceConversions, Image& img, BOOL& releaseImage,
                     BOOL& releaseBitmap, unsigned int& width, unsigned int& height, BOOL& tainted)
{
	bitmap = NULL;
	fill = NULL;
	releaseImage = FALSE;
	releaseBitmap = FALSE;
#ifdef SVG_SUPPORT
	if (domres && domres->IsA(DOM_TYPE_SVG_ELEMENT))
	{
		LogicalDocument* element_logdoc = domres->GetLogicalDocument();
		if (!element_logdoc)
			return DECODE_FAILED;
		HTML_Element* element = static_cast<DOM_SVGElement*>(domres)->GetThisElement();
		SVGImage* image = g_svg_manager->GetSVGImage(element_logdoc, element);
		if (!image)
			return DECODE_FAILED;

		int svg_width = dest_width;
		int svg_height = dest_height;
		svg_width = MIN(svg_width, 4000); // To avoid obvious hang problem
		svg_height= MIN(svg_height, 2000); // To avoid obvious hang problem
		DECODE_FAILED_IF_ERROR(image->PaintToBuffer(bitmap, 0, svg_width, svg_height));
		releaseBitmap = TRUE;
		width = bitmap->Width();
		height = bitmap->Height();

		src_url = domres->GetRuntime()->GetOriginURL();
		if (!image->IsSecure())
			tainted = TRUE;
	}
	else
#endif // SVG_SUPPORT
	if (domres && domres->IsA(DOM_TYPE_HTML_CANVASELEMENT))
	{
		HTML_Element* element = static_cast<DOM_HTMLElement*>(domres)->GetThisElement();
		Canvas* canvas = static_cast<Canvas*>(element->GetSpecialAttr(Markup::VEGAA_CANVAS, ITEM_TYPE_COMPLEX, NULL, SpecialNs::NS_OGP));
		if (!canvas)
			return DECODE_FAILED;
		if (OpStatus::IsError(canvas->Realize(element)))
			return DECODE_FAILED;
		src_url = domres->GetRuntime()->GetOriginURL();
		width = canvas->GetWidth(element);
		height = canvas->GetHeight(element);

		if (!canvas->IsSecure())
			tainted = TRUE;
		if (non_premultiplied && !canvas->IsPremultiplied())
		{
			*non_premultiplied = OP_NEW(VEGASWBuffer, ());
			if (!*non_premultiplied)
				return DECODE_OOM;
			VEGAPixelStore ps;
			OP_STATUS err = canvas->GetNonPremultipliedBackBuffer(&ps);
			if (OpStatus::IsSuccess(err))
			{
				err = (*non_premultiplied)->Create(ps.width, ps.height);
				if (OpStatus::IsSuccess(err))
					(*non_premultiplied)->CopyFromPixelStore(&ps);
				canvas->ReleaseNonPremultipliedBackBuffer();
			}
			if (OpStatus::IsMemoryError(err))
			{
				OP_DELETE(*non_premultiplied);
				return DECODE_OOM;
			}
			else if (OpStatus::IsError(err))
			{
				OP_DELETE(*non_premultiplied);
				return DECODE_FAILED;
			}
			return DECODE_OK;
		}
		if (wantFill)
			fill = canvas->GetBackBuffer();
		if (!fill || canvas == static_cast<Canvas*>(domcanvas->GetThisElement()->GetSpecialAttr(Markup::VEGAA_CANVAS, ITEM_TYPE_COMPLEX, NULL, SpecialNs::NS_OGP)))
		{
			// Make sure the canvas is invalidated and up to date when painting it.
			static_cast<DOM_HTMLCanvasElement*>(domres)->Invalidate();
			bitmap = canvas->GetOpBitmap();
			if (!bitmap)
				return DECODE_FAILED;
		}
	}
	else
#ifdef MEDIA_HTML_SUPPORT
	if (domres && domres->IsA(DOM_TYPE_HTML_VIDEOELEMENT))
	{
		HTML_Element* element = static_cast<DOM_HTMLElement*>(domres)->GetThisElement();
		MediaElement* media = element->GetMediaElement();
		if (!media)
			return DECODE_FAILED;

		if (media->GetReadyState() == MediaReady::NOTHING || media->GetReadyState() == MediaReady::METADATA)
			return DECODE_FAILED;

		if (!media->GetPlayer() || OpStatus::IsError(media->GetPlayer()->GetFrame(bitmap)))
			return DECODE_FAILED;

		width = media->GetIntrinsicWidth();
		height = media->GetIntrinsicHeight();

		src_url = media->GetPlayer()->GetOriginURL();
	}
	else
#endif // MEDIA_HTML_SUPPORT
	if (domres && domres->IsA(DOM_TYPE_HTML_ELEMENT))
	{
		LogicalDocument* element_logdoc = domres->GetLogicalDocument();
		if (!element_logdoc)
			return DECODE_FAILED;
		HTML_Element* element = static_cast<DOM_HTMLElement*>(domres)->GetThisElement();

		if (element->Type() != HE_IMG)
			return DECODE_EXCEPTION_TYPE_MISMATCH;

		URL img_url = element->GetImageURL(TRUE, element_logdoc);

		if (img_url.IsEmpty())
			return DECODE_FAILED; // Should be silent (CORE-27420)

		URLStatus stat = (URLStatus)img_url.GetAttribute(URL::KLoadStatus, TRUE);
		// The spec is not clear if this should be silent fail or an exception, going with silent fail for now
		if (stat == URL_LOADING || stat == URL_UNLOADED)
			return DECODE_FAILED;

		HEListElm* helm = element->GetHEListElm(FALSE);
		if (!helm)
			return DECODE_FAILED;
		img = helm->GetImage();
		if (img.IsEmpty())
			return DECODE_FAILED;
		UrlImageContentProvider *prov = helm->GetUrlContentProvider();

#ifdef SVG_SUPPORT
		if (img_url.ContentType() == URL_SVG_CONTENT)
		{
			FramesDocument* top_doc = domcanvas->GetFramesDocument();
			if (!top_doc)
				return DECODE_FAILED;

			UrlImageContentProvider* prov = UrlImageContentProvider::FindImageContentProvider(img_url);
			if (!prov->GetSVGImageRef())
				prov->UpdateSVGImageRef(element_logdoc);

			SVGImageRef* ref = prov->GetSVGImageRef();
			if (!ref)
				 return DECODE_FAILED;
			SVGImage* svg_image = ref->GetSVGImage();
			if (!svg_image)
			{
				return DECODE_SVG_NOT_READY;
			}

			int svg_width = dest_width;
			int svg_height = dest_height;
			svg_width = MIN(svg_width, 4000); // To avoid obvious hang problem
			svg_height= MIN(svg_height, 2000); // To avoid obvious hang problem
			DECODE_FAILED_IF_ERROR(svg_image->PaintToBuffer(bitmap, 0, svg_width, svg_height));

			releaseBitmap = TRUE;
			width = bitmap->Width();
			height = bitmap->Height();
			if (!svg_image->IsSecure())
				tainted = TRUE;
		}
		else
#endif // SVG_SUPPORT
		{

			// Do not mark the canvas as insecure when using data urls
			if (img_url.Type() == URL_DATA)
				src_url = domcanvas->GetRuntime()->GetOriginURL();
#ifdef CORS_SUPPORT
			else if (helm->IsCrossOriginAllowed())
				src_url = domcanvas->GetRuntime()->GetOriginURL();
#endif // CORS_SUPPORT
			else
			{
				src_url = img_url.GetAttribute(URL::KMovedToURL, TRUE);
				if (src_url.IsEmpty())
					src_url = img_url;
			}

			if (non_premultiplied && prov)
			{
#ifdef IMGLOADER_NO_PREMULTIPLY
				DECODE_FAILED_IF_ERROR(ImageLoaderNoPremultiply::Load(prov, non_premultiplied, ignoreColorSpaceConversions));
				if (*non_premultiplied)
				{
					width = (*non_premultiplied)->width;
					height = (*non_premultiplied)->height;
				}
				return DECODE_OK;
#else
				return DECODE_FAILED;
#endif
			}

			DECODE_FAILED_IF_ERROR(img.IncVisible(null_image_listener));
			if (!img.ImageDecoded() && prov)
			{
				OP_STATUS err = img.OnLoadAll(prov);
				if (OpStatus::IsMemoryError(err))
				{
					img.DecVisible(null_image_listener);
					return DECODE_OOM;
				}
				if (OpStatus::IsError(err))
				{
					img.DecVisible(null_image_listener);
					return DECODE_FAILED;
				}
				DOM_Object::GetCurrentThread(origining_runtime)->RequestTimeoutCheck();
			}

			releaseImage = TRUE;
			bitmap = img.GetBitmap(NULL);
			if (!bitmap)
			{
				img.DecVisible(null_image_listener);
				return DECODE_FAILED;
			}
			width = bitmap->Width();
			height = bitmap->Height();
		}
	}
	else
		return DECODE_EXCEPTION_TYPE_MISMATCH;

	// If non_premultiplied is non-NULL then we need data that is unpremultiplied.
	if (bitmap && non_premultiplied)
	{
		DecodeStatus decodeStatus = DECODE_OK;
		bool using_pointer;
		OP_STATUS err;

		*non_premultiplied = OP_NEW(VEGASWBuffer, ());
		if (!*non_premultiplied)
			decodeStatus = DECODE_OOM;
		else if (OpStatus::IsMemoryError(err = (*non_premultiplied)->CreateFromBitmap(bitmap, using_pointer)))
			decodeStatus = DECODE_OOM;
		else if (OpStatus::IsError(err))
			decodeStatus = DECODE_FAILED;
		else if (using_pointer)
		{
			// We need mutable data so if it's using a pointer directly we need to clone the buffer.
			VEGASWBuffer *cloned_buffer = OP_NEW(VEGASWBuffer, ());
			if (!cloned_buffer)
				decodeStatus = DECODE_OOM;
			else if (OpStatus::IsMemoryError(err = (*non_premultiplied)->Clone(*cloned_buffer)))
			{
				OP_DELETE(cloned_buffer);
				decodeStatus = DECODE_OOM;
			}
			else if (OpStatus::IsError(err))
			{
				OP_DELETE(cloned_buffer);
				decodeStatus = DECODE_FAILED;
			}
			else
			{
				OP_DELETE(*non_premultiplied);
				*non_premultiplied = cloned_buffer;
				(*non_premultiplied)->UnPremultiply();
			}
			bitmap->ReleasePointer(FALSE);
		}
		else
			(*non_premultiplied)->UnPremultiply();

		if (releaseImage)
		{
			img.ReleaseBitmap();
			img.DecVisible(null_image_listener);
		}
		if (releaseBitmap)
			OP_DELETE(bitmap);
		bitmap = NULL;
		releaseImage = FALSE;
		releaseBitmap = FALSE;
		if (decodeStatus != DECODE_OK)
			OP_DELETE(*non_premultiplied);
		return decodeStatus;
	}
	else
		return DECODE_OK;
}
#undef DECODE_FAILED_IF_ERROR

// Helper functions to handle colors

BOOL DOMCanvasColorUtil::DOMGetCanvasColor(const uni_char *colstr, UINT32& color)
{
	COLORREF c;
	if (!ParseColor(colstr, uni_strlen(colstr), c))
		return FALSE;
	color = c;
	return TRUE;
}

ES_GetState DOMCanvasColorUtil::DOMSetCanvasColor(ES_Value *value, UINT32 color)
{
	unsigned char alpha = (color >> 24);
	if (alpha < 255)
	{
		unsigned char rgb[3];
		rgb[0] = GetRValue(color);
		rgb[1] = GetGValue(color);
		rgb[2] = GetBValue(color);

		TempBuffer *buffer = DOM_Object::GetEmptyTempBuf();
		GET_FAILED_IF_ERROR(buffer->Append("rgba("));
		for (int i = 0; i < 3; i++)
		{
			GET_FAILED_IF_ERROR(buffer->AppendUnsignedLong(rgb[i]));
			GET_FAILED_IF_ERROR(buffer->Append(", "));
		}

		OP_ASSERT(alpha < 255);
		if (alpha == 0)
			GET_FAILED_IF_ERROR(buffer->Append("0.0)"));
		else
		{
			GET_FAILED_IF_ERROR(buffer->AppendDoubleToPrecision(alpha/255., 5));
			GET_FAILED_IF_ERROR(buffer->Append(')'));
		}

		DOM_Object::DOMSetString(value, buffer);
	}
	else
	{
		uni_char colstr[8]; // ARRAY OK 2008-02-28 jl
		unsigned char comp;
		colstr[0] = '#';
		comp = GetRValue(color);
		if (comp / 16 >= 10)
			colstr[1] = 'a' + comp/16 - 10;
		else
			colstr[1] = '0' + comp/16;
		if (comp % 16 >= 10)
			colstr[2] = 'a' + comp%16 - 10;
		else
			colstr[2] = '0' + comp%16;
		comp = GetGValue(color);
		if (comp / 16 >= 10)
			colstr[3] = 'a' + comp/16 - 10;
		else
			colstr[3] = '0' + comp/16;
		if (comp % 16 >= 10)
			colstr[4] = 'a' + comp%16 - 10;
		else
			colstr[4] = '0' + comp%16;
		comp = GetBValue(color);
		if (comp / 16 >= 10)
			colstr[5] = 'a' + comp/16 - 10;
		else
			colstr[5] = '0' + comp/16;
		if (comp % 16 >= 10)
			colstr[6] = 'a' + comp%16 - 10;
		else
			colstr[6] = '0' + comp%16;
		colstr[7] = 0;

		TempBuffer *buffer = DOM_Object::GetEmptyTempBuf();
		GET_FAILED_IF_ERROR(buffer->Append(colstr));

		DOM_Object::DOMSetString(value, buffer);
	}

	return GET_SUCCESS;
}

#ifdef CANVAS_TEXT_SUPPORT
static inline BOOL ContainsInherit(CSS_property_list* props)
{
	// Check any value is 'inherit'
	CSS_decl* decl = props->GetFirstDecl();
	while (decl)
	{
		if (decl->GetDeclType() == CSS_DECL_TYPE &&
			decl->TypeValue() == CSS_VALUE_inherit)
			return TRUE;

		decl = decl->Suc();
	}
	return FALSE;
}

CSS_property_list* DOMGetCanvasFont(const uni_char* font_spec, HLDocProfile* hld_profile)
{
	CSS_property_list* font_props = NULL;
	RETURN_VALUE_IF_ERROR(ParseFontShorthand(font_spec, uni_strlen(font_spec),
											 hld_profile, font_props), NULL);

	// Ignore invalid values (including 'inherit')
	if (font_props && (font_props->IsEmpty() || ContainsInherit(font_props)))
	{
		font_props->Unref();
		font_props = NULL;
	}

	return font_props;
}

static inline BOOL CSS_IsDefaultValue(CSS_decl* decl)
{
	switch (decl->GetProperty())
	{
	case CSS_PROPERTY_font_style:
	case CSS_PROPERTY_font_variant:
		return decl->GetDeclType() == CSS_DECL_TYPE && decl->TypeValue() == CSS_VALUE_normal;
	case CSS_PROPERTY_font_weight:
		return
			decl->GetDeclType() == CSS_DECL_TYPE && decl->TypeValue() == CSS_VALUE_normal ||
			decl->GetDeclType() == CSS_DECL_NUMBER && decl->GetNumberValue(0) == 4;
	}
	return FALSE;
}

static void FontPropertyToStringL(CSS_property_list* props, TempBuffer* buffer)
{
	// Borrowed from css_dom.cpp
	CSS_decl* prop_values[6] = { NULL, NULL, NULL, NULL, NULL, NULL };

	CSS_decl* decl = props->GetFirstDecl();
	while (decl)
	{
		switch (decl->GetProperty())
		{
		case CSS_PROPERTY_font_style:	prop_values[0] = decl; break;
		case CSS_PROPERTY_font_variant:	prop_values[1] = decl; break;
		case CSS_PROPERTY_font_weight:	prop_values[2] = decl; break;
		case CSS_PROPERTY_font_size:	prop_values[3] = decl; break;
			// line-height forced to 'normal'
			// case CSS_PROPERTY_line_height:	prop_values[4] = decl; break;
		case CSS_PROPERTY_font_family:	prop_values[5] = decl; break;
		}

		decl = decl->Suc();
	}

	// Check if all values are 'inherit'
	int first_non_null = -1, i;
	for (i = 0; i < 6; i++)
	{
		if (!prop_values[i])
			continue;

		first_non_null = i;

		if (prop_values[i]->GetDeclType() == CSS_DECL_TYPE &&
			prop_values[i]->TypeValue() == CSS_VALUE_inherit)
			continue;

		break;
	}

	if (i == 6)
	{
		OP_ASSERT(first_non_null >= 0);

		CSS::FormatDeclarationL(buffer, prop_values[first_non_null], FALSE, CSS_FORMAT_COMPUTED_STYLE);
		return;
	}

	BOOL is_set = FALSE;
	BOOL default_dropped = FALSE;

	do
	{
		BOOL all_default = default_dropped;
		int first = 0; // set to one above the index of the last non-null property
		short system_font = -1;
		for (i = 0; i < 6; i++)
		{
			if (!prop_values[i])
				continue;

			if (!all_default &&
				prop_values[i]->GetDeclType() == CSS_DECL_TYPE &&
				CSS_is_font_system_val(prop_values[i]->TypeValue()))
				system_font = prop_values[i]->TypeValue();
			else
			{
				if (all_default || !CSS_IsDefaultValue(prop_values[i]))
				{
					CSS::FormatDeclarationL(buffer, prop_values[i], first != 0, CSS_FORMAT_COMPUTED_STYLE);
					first = i + 1;
				}
				else
					default_dropped = TRUE;
			}
		}

		if (system_font != -1)
		{
			if (first == 0)
				buffer->AppendL(CSS_GetKeywordString(system_font));
			else
				buffer->Clear();
		}

		is_set = (first != 0 || system_font != -1);

	} while (!is_set && default_dropped);
}

ES_GetState DOMSetCanvasFont(ES_Value *value, CSS_property_list* font_props)
{
	TempBuffer *buffer = DOM_Object::GetEmptyTempBuf();
	if (!font_props || font_props->IsEmpty())
	{
		GET_FAILED_IF_ERROR(buffer->Append("10px sans-serif"));
	}
	else
	{
		TRAPD(err, FontPropertyToStringL(font_props, buffer));
		GET_FAILED_IF_ERROR(err);
	}

	DOM_Object::DOMSetString(value, buffer);
	return GET_SUCCESS;
}
#endif // CANVAS_TEXT_SUPPORT

////////////////////////////////////

DOMCanvasGradient::DOMCanvasGradient(CanvasGradient *grad) : m_gradient(grad)
{
	m_gradient->SetDOM_Object(this);
}

DOMCanvasGradient::~DOMCanvasGradient()
{
	OP_DELETE(m_gradient);
}

/* virtual */ void
DOMCanvasGradient::GCTrace()
{
	GetRuntime()->RecordExternalAllocation(CanvasGradient::allocationCost(m_gradient));
}

/* static */ OP_STATUS
DOMCanvasGradient::Make(DOMCanvasGradient*& ctx, DOM_Environment* environment)
{
	DOM_Runtime *runtime = static_cast<DOM_EnvironmentImpl*>(environment)->GetDOMRuntime();
	ES_Value value;

	CanvasGradient *grad = OP_NEW(CanvasGradient, ());
	if (!grad)
		return OpStatus::ERR_NO_MEMORY;

	ctx = OP_NEW(DOMCanvasGradient, (grad));
	if (!ctx)
		return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_ERROR(DOMSetObjectRuntime(ctx, runtime, runtime->GetPrototype(DOM_Runtime::CANVASGRADIENT_PROTOTYPE), "CanvasGradient"));
	return OpStatus::OK;
}

int
DOMCanvasGradient::addColorStop(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domgrad, DOM_TYPE_CANVASGRADIENT, DOMCanvasGradient);
	DOM_CHECK_ARGUMENTS("ns");

	double stop = argv[0].value.number;
	UINT32 color;
	if (!DOMCanvasColorUtil::DOMGetCanvasColor(argv[1].value.string, color))
		return DOM_CALL_DOMEXCEPTION(SYNTAX_ERR);
	if (stop < 0. || stop > 1.)
		return DOM_CALL_DOMEXCEPTION(INDEX_SIZE_ERR);

	CALL_FAILED_IF_ERROR(domgrad->m_gradient->addColorStop(stop, color));

	return ES_FAILED;
}

DOMCanvasPattern::DOMCanvasPattern(CanvasPattern *pat) : m_pattern(pat)
{
	m_pattern->SetDOM_Object(this);
}

DOMCanvasPattern::~DOMCanvasPattern()
{
	OP_DELETE(m_pattern);
}

/* virtual */ void
DOMCanvasPattern::GCTrace()
{
	GetRuntime()->RecordExternalAllocation(CanvasPattern::allocationCost(m_pattern));
}

/* static */ OP_STATUS
DOMCanvasPattern::Make(DOMCanvasPattern*& ctx, DOM_Environment* environment, URL& src_url, BOOL tainted)
{
	DOM_Runtime *runtime = static_cast<DOM_EnvironmentImpl*>(environment)->GetDOMRuntime();
	ES_Value value;

	CanvasPattern *pat = OP_NEW(CanvasPattern, ());
	if (!pat)
		return OpStatus::ERR_NO_MEMORY;

	ctx = OP_NEW(DOMCanvasPattern, (pat));
	if (!ctx)
	{
		OP_DELETE(pat);
		return OpStatus::ERR_NO_MEMORY;
	}
	ctx->m_source_url = src_url;
	ctx->m_tainted = tainted;

	return DOMSetObjectRuntime(ctx, runtime, runtime->GetPrototype(DOM_Runtime::CANVASPATTERN_PROTOTYPE), "CanvasPattern");
}

DOMCanvasImageData::DOMCanvasImageData(unsigned int w, unsigned int h, ES_Object* pix)
	: m_width(w),
	  m_height(h),
	  m_pixel_array(pix)
{
}

static inline BOOL ValidateInt(double v)
{
	return v >= INT_MIN && v <= INT_MAX;
}

static inline BOOL ValidateSize(double v, unsigned int m)
{
	return (static_cast<int>(v) > 0 && v <= static_cast<double>(m));
}

static inline BOOL CheckImageDataSize(unsigned int w, unsigned int h)
{
	unsigned int height_bytes = h * 4;

	if ((height_bytes / 4) != h)
		return FALSE;

	unsigned int num_bytes = w * height_bytes;

	if ((num_bytes / height_bytes) != w)
		return FALSE;

	return TRUE;
}

/* virtual */ void
DOMCanvasImageData::GCTrace()
{
	// Make sure our pixel array is not destroyed
	GCMark(m_pixel_array);
}

/* static */ OP_STATUS
DOMCanvasImageData::Make(DOMCanvasImageData *&image_data, DOM_Runtime *runtime, unsigned int w, unsigned int h, UINT8 *clone_data)
{
	/* The callers are responsible for validating the dimensions of the instance. */
	OP_ASSERT(CheckImageDataSize(w, h));
	unsigned size_in_bytes = w * h * 4;

	ES_Object *array_buffer;
	RETURN_IF_ERROR(runtime->CreateNativeArrayBufferObject(&array_buffer, size_in_bytes));

	ES_Object *pixels;
	RETURN_IF_ERROR(runtime->CreateNativeTypedArrayObject(&pixels, ES_Runtime::TYPED_ARRAY_UINT8CLAMPED, array_buffer));

	RETURN_IF_ERROR(DOMSetObjectRuntime(image_data = OP_NEW(DOMCanvasImageData, (w, h, pixels)), runtime, runtime->GetPrototype(DOM_Runtime::CANVASIMAGEDATA_PROTOTYPE), "ImageData"));

	if (clone_data)
		op_memcpy(image_data->GetPixels(), clone_data, size_in_bytes);

	ES_Value value;

	DOMSetNumber(&value, w);
	RETURN_IF_ERROR(image_data->Put(UNI_L("width"), value, TRUE));

	DOMSetNumber(&value, h);
	RETURN_IF_ERROR(image_data->Put(UNI_L("height"), value, TRUE));

	DOMSetObject(&value, pixels);
	RETURN_IF_ERROR(image_data->Put(UNI_L("data"), value, TRUE));

	return OpStatus::OK;
}

/* static */ OP_STATUS
DOMCanvasImageData::Clone(DOMCanvasImageData *source_object, ES_Runtime *target_runtime, DOMCanvasImageData *&target_object)
{
	return DOMCanvasImageData::Make(target_object, static_cast<DOM_Runtime *>(target_runtime), source_object->m_width, source_object->m_height, source_object->GetPixels());
}

#ifdef ES_PERSISTENT_SUPPORT

class DOM_CanvasImageData_PersistentItem
	: public ES_PersistentItem
{
public:
	virtual ~DOM_CanvasImageData_PersistentItem()
	{
		op_free(m_pixels);
	}

	virtual BOOL IsA(int tag) { return tag == DOM_TYPE_CANVASIMAGEDATA; }

	unsigned m_width, m_height;
	unsigned char *m_pixels;
};

/* static */ OP_STATUS
DOMCanvasImageData::Clone(DOMCanvasImageData *source_object, ES_PersistentItem *&target_item)
{
	DOM_CanvasImageData_PersistentItem *item = OP_NEW(DOM_CanvasImageData_PersistentItem, ());
	unsigned npixels = source_object->m_width * source_object->m_height * 4;

	if (!item || !(item->m_pixels = static_cast<unsigned char *>(op_malloc(npixels))))
	{
		OP_DELETE(item);
		return OpStatus::ERR_NO_MEMORY;
	}

	item->m_width = source_object->m_width;
	item->m_height = source_object->m_height;

	op_memcpy(item->m_pixels, source_object->GetPixels(), npixels);

	target_item = item;
	return OpStatus::OK;
}

/* static */ OP_STATUS
DOMCanvasImageData::Clone(ES_PersistentItem *&source_item0, ES_Runtime *target_runtime, DOMCanvasImageData *&target_object)
{
	DOM_CanvasImageData_PersistentItem *source_item = static_cast<DOM_CanvasImageData_PersistentItem *>(source_item0);
	return DOMCanvasImageData::Make(target_object, static_cast<DOM_Runtime *>(target_runtime), source_item->m_width, source_item->m_height, source_item->m_pixels);
}

#endif // ES_PERSISTENT_SUPPORT

DOMCanvasContext2D::DOMCanvasContext2D(DOM_HTMLCanvasElement *domcanvas, CanvasContext2D *c2d) :
	m_domcanvas(domcanvas), m_context(c2d)
{
	// Add a reference to the context
	m_context->Reference();
}

DOMCanvasContext2D::~DOMCanvasContext2D()
{
	// Remove a reference to the context
	m_context->Release();
}

/*static*/ OP_STATUS
DOMCanvasContext2D::Make(DOMCanvasContext2D *&ctx, DOM_HTMLCanvasElement *domcanvas)
{
	DOM_Runtime *runtime = domcanvas->GetRuntime();
	ES_Value value;

	HTML_Element *htm_elem = domcanvas->GetThisElement();

	Canvas* canvas = static_cast<Canvas*>(htm_elem->GetSpecialAttr(Markup::VEGAA_CANVAS, ITEM_TYPE_COMPLEX, NULL, SpecialNs::NS_OGP));
	if (!canvas)
		return OpStatus::ERR;

	CanvasContext2D *c2d = canvas->GetContext2D();
	if (!c2d)
		return OpStatus::ERR;


	RETURN_IF_ERROR(DOMSetObjectRuntime(ctx = OP_NEW(DOMCanvasContext2D, (domcanvas, c2d)), runtime, runtime->GetPrototype(DOM_Runtime::CANVASCONTEXT2D_PROTOTYPE), "CanvasRenderingContext2D"));
	runtime->RecordExternalAllocation(CanvasContext2D::allocationCost(c2d));
	return OpStatus::OK;
}

/* virtual */ ES_GetState
DOMCanvasContext2D::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_canvas:
		DOMSetObject(value, m_domcanvas);
		return GET_SUCCESS;

	case OP_ATOM_globalAlpha:
		DOMSetNumber(value, m_context->getAlpha());
		return GET_SUCCESS;

	case OP_ATOM_globalCompositeOperation:
		switch (m_context->getGlobalCompositeOperation())
		{
		case CanvasContext2D::CANVAS_COMP_SRC_ATOP:
			DOMSetString(value, UNI_L("source-atop"));
			break;
		case CanvasContext2D::CANVAS_COMP_SRC_IN:
			DOMSetString(value, UNI_L("source-in"));
			break;
		case CanvasContext2D::CANVAS_COMP_SRC_OUT:
			DOMSetString(value, UNI_L("source-out"));
			break;
		case CanvasContext2D::CANVAS_COMP_DST_ATOP:
			DOMSetString(value, UNI_L("destination-atop"));
			break;
		case CanvasContext2D::CANVAS_COMP_DST_IN:
			DOMSetString(value, UNI_L("destination-in"));
			break;
		case CanvasContext2D::CANVAS_COMP_DST_OUT:
			DOMSetString(value, UNI_L("destination-out"));
			break;
		case CanvasContext2D::CANVAS_COMP_DST_OVER:
			DOMSetString(value, UNI_L("destination-over"));
			break;
		case CanvasContext2D::CANVAS_COMP_LIGHTER:
			DOMSetString(value, UNI_L("lighter"));
			break;
		case CanvasContext2D::CANVAS_COMP_COPY:
			DOMSetString(value, UNI_L("copy"));
			break;
		case CanvasContext2D::CANVAS_COMP_XOR:
			DOMSetString(value, UNI_L("xor"));
			break;
		case CanvasContext2D::CANVAS_COMP_SRC_OVER:
		default:
			DOMSetString(value, UNI_L("source-over"));
			break;
		}
		return GET_SUCCESS;

	case OP_ATOM_strokeStyle:
		if (m_context->getStrokePattern(0))
			DOMSetObject(value, m_context->getStrokePattern(0)->GetDOM_Object());
		else if (m_context->getStrokeGradient(0))
			DOMSetObject(value, m_context->getStrokeGradient(0)->GetDOM_Object());
		else
			return DOMCanvasColorUtil::DOMSetCanvasColor(value, m_context->getStrokeColor());
		return GET_SUCCESS;

	case OP_ATOM_fillStyle:
		if (m_context->getFillPattern(0))
			DOMSetObject(value, m_context->getFillPattern(0)->GetDOM_Object());
		else if (m_context->getFillGradient(0))
			DOMSetObject(value, m_context->getFillGradient(0)->GetDOM_Object());
		else
			return DOMCanvasColorUtil::DOMSetCanvasColor(value, m_context->getFillColor());
		return GET_SUCCESS;

	case OP_ATOM_lineWidth:
		DOMSetNumber(value, m_context->getLineWidth());
		return GET_SUCCESS;

	case OP_ATOM_lineCap:
		switch(m_context->getLineCap())
		{
		case CanvasContext2D::CANVAS_LINECAP_ROUND:
			DOMSetString(value, UNI_L("round"));
			break;

		case CanvasContext2D::CANVAS_LINECAP_SQUARE:
			DOMSetString(value, UNI_L("square"));
			break;

		default:
			DOMSetString(value, UNI_L("butt"));
		}
		return GET_SUCCESS;

	case OP_ATOM_lineJoin:
		switch(m_context->getLineJoin())
		{
		case CanvasContext2D::CANVAS_LINEJOIN_ROUND:
			DOMSetString(value, UNI_L("round"));
			break;

		case CanvasContext2D::CANVAS_LINEJOIN_BEVEL:
			DOMSetString(value, UNI_L("bevel"));
			break;

		default:
			DOMSetString(value, UNI_L("miter"));
		}
		return GET_SUCCESS;

	case OP_ATOM_miterLimit:
		DOMSetNumber(value, m_context->getMiterLimit());
		return GET_SUCCESS;

#ifdef CANVAS_TEXT_SUPPORT
	case OP_ATOM_font:
		return DOMSetCanvasFont(value, m_context->getFont());

	case OP_ATOM_textAlign:
		{
			const uni_char* text_align;
			switch (m_context->getTextAlign())
			{
			default:
			case CanvasContext2D::CANVAS_TEXTALIGN_START:
				text_align = UNI_L("start");
				break;
			case CanvasContext2D::CANVAS_TEXTALIGN_END:
				text_align = UNI_L("end");
				break;
			case CanvasContext2D::CANVAS_TEXTALIGN_LEFT:
				text_align = UNI_L("left");
				break;
			case CanvasContext2D::CANVAS_TEXTALIGN_RIGHT:
				text_align = UNI_L("right");
				break;
			case CanvasContext2D::CANVAS_TEXTALIGN_CENTER:
				text_align = UNI_L("center");
				break;
			}
			DOMSetString(value, text_align);
			return GET_SUCCESS;
		}

	case OP_ATOM_textBaseline:
		{
			const uni_char* text_baseline;
			switch (m_context->getTextBaseline())
			{
			case CanvasContext2D::CANVAS_TEXTBASELINE_TOP:
				text_baseline = UNI_L("top");
				break;
			case CanvasContext2D::CANVAS_TEXTBASELINE_HANGING:
				text_baseline = UNI_L("hanging");
				break;
			case CanvasContext2D::CANVAS_TEXTBASELINE_MIDDLE:
				text_baseline = UNI_L("middle");
				break;
			default:
			case CanvasContext2D::CANVAS_TEXTBASELINE_ALPHABETIC:
				text_baseline = UNI_L("alphabetic");
				break;
			case CanvasContext2D::CANVAS_TEXTBASELINE_IDEOGRAPHIC:
				text_baseline = UNI_L("ideographic");
				break;
			case CanvasContext2D::CANVAS_TEXTBASELINE_BOTTOM:
				text_baseline = UNI_L("bottom");
				break;
			}
			DOMSetString(value, text_baseline);
			return GET_SUCCESS;
		}
#endif // CANVAS_TEXT_SUPPORT

	case OP_ATOM_shadowOffsetX:
		DOMSetNumber(value, m_context->getShadowOffsetX());
		return GET_SUCCESS;

	case OP_ATOM_shadowOffsetY:
		DOMSetNumber(value, m_context->getShadowOffsetY());
		return GET_SUCCESS;

	case OP_ATOM_shadowBlur:
		DOMSetNumber(value, m_context->getShadowBlur());
		return GET_SUCCESS;

	case OP_ATOM_shadowColor:
		return DOMCanvasColorUtil::DOMSetCanvasColor(value, m_context->getShadowColor());

	default:
		return DOM_Object::GetName(property_name, value, origining_runtime);
	}
}

/* virtual */ ES_PutState
DOMCanvasContext2D::PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_canvas:
		return PUT_SUCCESS;

	case OP_ATOM_globalAlpha:
	case OP_ATOM_shadowOffsetX:
	case OP_ATOM_shadowOffsetY:
		if (value->type != VALUE_NUMBER)
			return PUT_NEEDS_NUMBER;
		else if (op_isfinite(value->value.number))
		{
			switch (property_name)
			{
			case OP_ATOM_shadowOffsetX:
				m_context->setShadowOffsetX(value->value.number);
				break;

			case OP_ATOM_shadowOffsetY:
				m_context->setShadowOffsetY(value->value.number);
				break;

			default: // globalAlpha; Range check in setAlpha()
				m_context->setAlpha(value->value.number);
				break;
			}
		}
		return PUT_SUCCESS;

	case OP_ATOM_shadowBlur:
		if (value->type != VALUE_NUMBER)
			return PUT_NEEDS_NUMBER;
		else if (op_isfinite(value->value.number) && value->value.number >= 0.0)
			m_context->setShadowBlur(value->value.number);
		return PUT_SUCCESS;

	case OP_ATOM_lineWidth:
	case OP_ATOM_miterLimit:
		if (value->type != VALUE_NUMBER)
			return PUT_NEEDS_NUMBER;
		else if (op_isfinite(value->value.number) && value->value.number > 0.0)
		{
			switch (property_name)
			{
			case OP_ATOM_lineWidth:
				m_context->setLineWidth(value->value.number);
				break;

			default: // miterLimit
				m_context->setMiterLimit(value->value.number);
				break;
			}
		}
		return PUT_SUCCESS;

	case OP_ATOM_strokeStyle:
		if (value->type == VALUE_STRING)
		{
			UINT32 color;
			if (DOMCanvasColorUtil::DOMGetCanvasColor(value->value.string, color))
				m_context->setStrokeColor(color);
		}
		else if (value->type == VALUE_OBJECT)
		{
			DOM_Object *domo = DOM_Utils::GetDOM_Object(value->value.object);
			if (domo)
			{
				if (DOM_Utils::IsA(domo, DOM_TYPE_CANVASGRADIENT))
					m_context->setStrokeGradient(static_cast<DOMCanvasGradient*>(domo)->GetCanvasGradient());
				else if (DOM_Utils::IsA(domo, DOM_TYPE_CANVASPATTERN))
				{
					DOMCanvasPattern* canvas_pattern = static_cast<DOMCanvasPattern*>(domo);
					if (canvas_pattern->IsTainted() ||
						!CanvasOriginCheck(this, canvas_pattern->GetURL(), GetRuntime()->GetOriginURL(), origining_runtime))
					{
						m_domcanvas->MakeInsecure();
					}
					m_context->setStrokePattern(canvas_pattern->GetCanvasPattern());
				}
			}
		}
		return PUT_SUCCESS;

	case OP_ATOM_fillStyle:
		if (value->type == VALUE_STRING)
		{
			UINT32 color;
			if (DOMCanvasColorUtil::DOMGetCanvasColor(value->value.string, color))
				m_context->setFillColor(color);
		}
		else if (value->type == VALUE_OBJECT)
		{
			DOM_Object *domo = DOM_Utils::GetDOM_Object(value->value.object);

			if (domo)
			{
				if (DOM_Utils::IsA(domo, DOM_TYPE_CANVASGRADIENT))
					m_context->setFillGradient(static_cast<DOMCanvasGradient*>(domo)->GetCanvasGradient());
				else if (DOM_Utils::IsA(domo, DOM_TYPE_CANVASPATTERN))
				{
					DOMCanvasPattern* canvas_pattern = static_cast<DOMCanvasPattern*>(domo);
					if (canvas_pattern->IsTainted() ||
						!CanvasOriginCheck(this, canvas_pattern->GetURL(), GetRuntime()->GetOriginURL(), origining_runtime))
					{
						m_domcanvas->MakeInsecure();
					}
					m_context->setFillPattern(canvas_pattern->GetCanvasPattern());
				}
			}
		}
		return PUT_SUCCESS;

	case OP_ATOM_globalCompositeOperation:
		if (value->type != VALUE_STRING)
			return PUT_NEEDS_STRING;
		else if (uni_str_eq(value->value.string, "source-atop"))
			m_context->setGlobalCompositeOperation(CanvasContext2D::CANVAS_COMP_SRC_ATOP);
		else if (uni_str_eq(value->value.string, "source-in"))
			m_context->setGlobalCompositeOperation(CanvasContext2D::CANVAS_COMP_SRC_IN);
		else if (uni_str_eq(value->value.string, "source-out"))
			m_context->setGlobalCompositeOperation(CanvasContext2D::CANVAS_COMP_SRC_OUT);
		else if (uni_str_eq(value->value.string, "source-over"))
			m_context->setGlobalCompositeOperation(CanvasContext2D::CANVAS_COMP_SRC_OVER);
		else if (uni_str_eq(value->value.string, "destination-atop"))
			m_context->setGlobalCompositeOperation(CanvasContext2D::CANVAS_COMP_DST_ATOP);
		else if (uni_str_eq(value->value.string, "destination-in"))
			m_context->setGlobalCompositeOperation(CanvasContext2D::CANVAS_COMP_DST_IN);
		else if (uni_str_eq(value->value.string, "destination-out"))
			m_context->setGlobalCompositeOperation(CanvasContext2D::CANVAS_COMP_DST_OUT);
		else if (uni_str_eq(value->value.string, "destination-over"))
			m_context->setGlobalCompositeOperation(CanvasContext2D::CANVAS_COMP_DST_OVER);
		else if (uni_str_eq(value->value.string, "lighter"))
			m_context->setGlobalCompositeOperation(CanvasContext2D::CANVAS_COMP_LIGHTER);
		else if (uni_str_eq(value->value.string, "copy"))
			m_context->setGlobalCompositeOperation(CanvasContext2D::CANVAS_COMP_COPY);
		else if (uni_str_eq(value->value.string, "xor"))
			m_context->setGlobalCompositeOperation(CanvasContext2D::CANVAS_COMP_XOR);
		return PUT_SUCCESS;

	case OP_ATOM_lineCap:
		if (value->type != VALUE_STRING)
			return PUT_NEEDS_STRING;
		else if (uni_str_eq(value->value.string, "round"))
			m_context->setLineCap(CanvasContext2D::CANVAS_LINECAP_ROUND);
		else if (uni_str_eq(value->value.string, "square"))
			m_context->setLineCap(CanvasContext2D::CANVAS_LINECAP_SQUARE);
		else if (uni_str_eq(value->value.string, "butt"))
			m_context->setLineCap(CanvasContext2D::CANVAS_LINECAP_BUTT);
		return PUT_SUCCESS;

	case OP_ATOM_lineJoin:
		if (value->type != VALUE_STRING)
			return PUT_NEEDS_STRING;
		else if (uni_str_eq(value->value.string, "round"))
			m_context->setLineJoin(CanvasContext2D::CANVAS_LINEJOIN_ROUND);
		else if (uni_str_eq(value->value.string, "bevel"))
			m_context->setLineJoin(CanvasContext2D::CANVAS_LINEJOIN_BEVEL);
		else if (uni_str_eq(value->value.string, "miter"))
			m_context->setLineJoin(CanvasContext2D::CANVAS_LINEJOIN_MITER);
		return PUT_SUCCESS;

#ifdef CANVAS_TEXT_SUPPORT
	case OP_ATOM_font:
		if (value->type != VALUE_STRING)
			return PUT_NEEDS_STRING;
		else
		{
			HLDocProfile* hld_profile = NULL;
			if (LogicalDocument* logdoc = m_domcanvas->GetLogicalDocument())
				hld_profile = logdoc->GetHLDocProfile();

			if (CSS_property_list* font = DOMGetCanvasFont(value->value.string, hld_profile))
			{
				FramesDocument* frm_doc = m_domcanvas->GetFramesDocument();
				HTML_Element* element = m_domcanvas->GetThisElement();
				m_context->setFont(font, frm_doc, element);
			}
		}
		return PUT_SUCCESS;

	case OP_ATOM_textAlign:
		if (value->type != VALUE_STRING)
			return PUT_NEEDS_STRING;
		else if (uni_str_eq(value->value.string, "start"))
			m_context->setTextAlign(CanvasContext2D::CANVAS_TEXTALIGN_START);
		else if (uni_str_eq(value->value.string, "end"))
			m_context->setTextAlign(CanvasContext2D::CANVAS_TEXTALIGN_END);
		else if (uni_str_eq(value->value.string, "left"))
			m_context->setTextAlign(CanvasContext2D::CANVAS_TEXTALIGN_LEFT);
		else if (uni_str_eq(value->value.string, "right"))
			m_context->setTextAlign(CanvasContext2D::CANVAS_TEXTALIGN_RIGHT);
		else if (uni_str_eq(value->value.string, "center"))
			m_context->setTextAlign(CanvasContext2D::CANVAS_TEXTALIGN_CENTER);
		return PUT_SUCCESS;

	case OP_ATOM_textBaseline:
		if (value->type != VALUE_STRING)
			return PUT_NEEDS_STRING;
		else if (uni_str_eq(value->value.string, "top"))
			m_context->setTextBaseline(CanvasContext2D::CANVAS_TEXTBASELINE_TOP);
		else if (uni_str_eq(value->value.string, "hanging"))
			m_context->setTextBaseline(CanvasContext2D::CANVAS_TEXTBASELINE_HANGING);
		else if (uni_str_eq(value->value.string, "middle"))
			m_context->setTextBaseline(CanvasContext2D::CANVAS_TEXTBASELINE_MIDDLE);
		else if (uni_str_eq(value->value.string, "alphabetic"))
			m_context->setTextBaseline(CanvasContext2D::CANVAS_TEXTBASELINE_ALPHABETIC);
		else if (uni_str_eq(value->value.string, "ideographic"))
			m_context->setTextBaseline(CanvasContext2D::CANVAS_TEXTBASELINE_IDEOGRAPHIC);
		else if (uni_str_eq(value->value.string, "bottom"))
			m_context->setTextBaseline(CanvasContext2D::CANVAS_TEXTBASELINE_BOTTOM);
		return PUT_SUCCESS;
#endif // CANVAS_TEXT_SUPPORT

	case OP_ATOM_shadowColor:
		if (value->type != VALUE_STRING)
			return PUT_NEEDS_STRING;
		else
		{
			UINT32 color;
			if (DOMCanvasColorUtil::DOMGetCanvasColor(value->value.string, color))
				m_context->setShadowColor(color);
		}
		return PUT_SUCCESS;

	default:
		return DOM_Object::PutName(property_name, value, origining_runtime);
	}
}

int
DOMCanvasContext2D::save(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domcontext, DOM_TYPE_CANVASCONTEXT2D, DOMCanvasContext2D);

	CALL_FAILED_IF_ERROR(domcontext->m_context->save());
	return ES_FAILED;
}

int
DOMCanvasContext2D::restore(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domcontext, DOM_TYPE_CANVASCONTEXT2D, DOMCanvasContext2D);

	domcontext->m_context->restore();
	return ES_FAILED;
}

int
DOMCanvasContext2D::scale(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domcontext, DOM_TYPE_CANVASCONTEXT2D, DOMCanvasContext2D);
	DOM_CHECK_ARGUMENTS_SILENT("nn");

	domcontext->m_context->scale(argv[0].value.number, argv[1].value.number);
	return ES_FAILED;
}

int
DOMCanvasContext2D::rotate(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domcontext, DOM_TYPE_CANVASCONTEXT2D, DOMCanvasContext2D);
	DOM_CHECK_ARGUMENTS_SILENT("n");

	domcontext->m_context->rotate(argv[0].value.number);
	return ES_FAILED;
}

int
DOMCanvasContext2D::translate(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domcontext, DOM_TYPE_CANVASCONTEXT2D, DOMCanvasContext2D);
	DOM_CHECK_ARGUMENTS_SILENT("nn");

	domcontext->m_context->translate(argv[0].value.number, argv[1].value.number);
	return ES_FAILED;
}

int
DOMCanvasContext2D::transform(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domcontext, DOM_TYPE_CANVASCONTEXT2D, DOMCanvasContext2D);
	DOM_CHECK_ARGUMENTS_SILENT("nnnnnn");

	domcontext->m_context->transform(argv[0].value.number, argv[1].value.number, argv[2].value.number, argv[3].value.number, argv[4].value.number, argv[5].value.number);
	return ES_FAILED;
}

int
DOMCanvasContext2D::setTransform(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domcontext, DOM_TYPE_CANVASCONTEXT2D, DOMCanvasContext2D);
	DOM_CHECK_ARGUMENTS_SILENT("nnnnnn");

	domcontext->m_context->setTransform(argv[0].value.number, argv[1].value.number, argv[2].value.number, argv[3].value.number, argv[4].value.number, argv[5].value.number);
	return ES_FAILED;
}

int
DOMCanvasContext2D::createLinearGradient(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT_UNUSED(DOM_TYPE_CANVASCONTEXT2D);
	DOM_CHECK_ARGUMENTS("nnnn");

	DOMCanvasGradient *grad;
	CALL_FAILED_IF_ERROR(DOMCanvasGradient::Make(grad, this_object->GetEnvironment()));
	CALL_FAILED_IF_ERROR(grad->GetCanvasGradient()->initLinear(argv[0].value.number, argv[1].value.number, argv[2].value.number, argv[3].value.number));

	DOMSetObject(return_value, grad);
	return ES_VALUE;
}

int
DOMCanvasContext2D::createRadialGradient(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT_UNUSED(DOM_TYPE_CANVASCONTEXT2D);
	DOM_CHECK_ARGUMENTS("nnnnnn");

	if (argv[2].value.number < 0 || argv[5].value.number < 0)
		return DOM_CALL_DOMEXCEPTION(INDEX_SIZE_ERR);

	DOMCanvasGradient *grad;
	CALL_FAILED_IF_ERROR(DOMCanvasGradient::Make(grad, this_object->GetEnvironment()));
	CALL_FAILED_IF_ERROR(grad->GetCanvasGradient()->initRadial(argv[0].value.number, argv[1].value.number, argv[2].value.number, argv[3].value.number, argv[4].value.number, argv[5].value.number));

	DOMSetObject(return_value, grad);
	return ES_VALUE;
}

int
DOMCanvasContext2D::createPattern(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domcontext, DOM_TYPE_CANVASCONTEXT2D, DOMCanvasContext2D);
	DOMCanvasContext2D_RestartCreatePattern* restart_object;
	VEGAFill::Spread xsp = VEGAFill::SPREAD_CLAMP_BORDER, ysp = VEGAFill::SPREAD_CLAMP_BORDER;

	DOM_Object* dh;

	if (argc >= 0)
	{
		if (argc > 0 && argv[0].type != VALUE_OBJECT)
		{
			return DOM_CALL_DOMEXCEPTION(TYPE_MISMATCH_ERR);
		}
		DOM_CHECK_ARGUMENTS("os");
		dh = DOM_VALUE2OBJECT(argv[0], DOM_Object);


		if (uni_str_eq(argv[1].value.string, UNI_L("repeat")) ||
			uni_str_eq(argv[1].value.string, UNI_L("")))
		{
			xsp = VEGAFill::SPREAD_REPEAT;
			ysp = VEGAFill::SPREAD_REPEAT;
		}
		else if (uni_str_eq(argv[1].value.string, UNI_L("repeat-x")))
		{
			xsp = VEGAFill::SPREAD_REPEAT;
			ysp = VEGAFill::SPREAD_CLAMP_BORDER;
		}
		else if (uni_str_eq(argv[1].value.string, UNI_L("repeat-y")))
		{
			xsp = VEGAFill::SPREAD_CLAMP_BORDER;
			ysp = VEGAFill::SPREAD_REPEAT;
		}
		else if (uni_str_eq(argv[1].value.string, UNI_L("no-repeat")))
		{
			xsp = VEGAFill::SPREAD_CLAMP_BORDER;
			ysp = VEGAFill::SPREAD_CLAMP_BORDER;
		}
		else
		{
			return DOM_CALL_DOMEXCEPTION(SYNTAX_ERR);
		}
	}
	else
	{
		restart_object = DOM_VALUE2OBJECT(*return_value, DOMCanvasContext2D_RestartCreatePattern);
		restart_object->CopyTo(&xsp, &ysp);
		dh = restart_object->GetObject();
	}

	URL src_url;
	BOOL tainted = FALSE;
	OpBitmap *bitmap = NULL;
	VEGAFill* fill = NULL;
	Image img;
	BOOL releaseImage = FALSE;
	BOOL releaseBitmap = FALSE;
	unsigned int width, height;

	CanvasResourceDecoder::DecodeStatus status = CanvasResourceDecoder::DecodeResource(dh, domcontext->m_domcanvas, origining_runtime, -1, -1, FALSE, src_url, bitmap, fill, NULL, FALSE, img, releaseImage, releaseBitmap, width, height, tainted);

	switch (status)
	{
	case CanvasResourceDecoder::DECODE_OK:
		break;
	case CanvasResourceDecoder::DECODE_OOM:
		return ES_NO_MEMORY;
	case CanvasResourceDecoder::DECODE_SVG_NOT_READY:
	{
		OP_ASSERT(dh && dh->IsA(DOM_TYPE_HTML_ELEMENT));
		if (argc < 0)
			return ES_FAILED;

		HTML_Element* element = static_cast<DOM_HTMLElement*>(dh)->GetThisElement();
		CALL_FAILED_IF_ERROR(DOMCanvasContext2D_RestartCreatePattern::Make(restart_object, dh, element, xsp, ysp, origining_runtime));
		CALL_FAILED_IF_ERROR(element->SetSVGIMGResourceListener(restart_object));
		return restart_object->BlockCall(return_value, origining_runtime);
	}
	case CanvasResourceDecoder::DECODE_EXCEPTION_TYPE_MISMATCH:
		return DOM_CALL_DOMEXCEPTION(TYPE_MISMATCH_ERR);
	case CanvasResourceDecoder::DECODE_EXCEPTION_NOT_SUPPORTED:
		return DOM_CALL_DOMEXCEPTION(NOT_SUPPORTED_ERR);
	case CanvasResourceDecoder::DECODE_FAILED:
		DOMSetNull(return_value);
		return ES_VALUE;
	default:
		return ES_FAILED;
	}

	DOMCanvasPattern *pat;
	if (OpStatus::IsError(DOMCanvasPattern::Make(pat, domcontext->GetEnvironment(), src_url, tainted)))
	{
		if (releaseImage)
		{
			img.ReleaseBitmap();
			img.DecVisible(null_image_listener);
		}
		if (releaseBitmap)
			OP_DELETE(bitmap);
		return ES_FAILED;
	}

	if (bitmap != NULL)
	{
		if (OpStatus::IsError(pat->GetCanvasPattern()->init(bitmap, xsp, ysp)))
			goto err_no_pattern;

		DOMSetObject(return_value, pat);

		if (releaseImage)
		{
			img.ReleaseBitmap();
			img.DecVisible(null_image_listener);
		}
		if (releaseBitmap)
			OP_DELETE(bitmap);
		return ES_VALUE;
	}
err_no_pattern:
	if (releaseImage)
	{
		img.ReleaseBitmap();
		img.DecVisible(null_image_listener);
	}
	if (releaseBitmap)
		OP_DELETE(bitmap);
	return ES_FAILED;
}

int
DOMCanvasContext2D::clearRect(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domcontext, DOM_TYPE_CANVASCONTEXT2D, DOMCanvasContext2D);
	DOM_CHECK_ARGUMENTS_SILENT("nnnn");

	domcontext->m_context->clearRect(argv[0].value.number, argv[1].value.number, argv[2].value.number, argv[3].value.number);
	domcontext->m_domcanvas->ScheduleInvalidation(origining_runtime);
	return ES_FAILED;
}

int
DOMCanvasContext2D::fillRect(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domcontext, DOM_TYPE_CANVASCONTEXT2D, DOMCanvasContext2D);
	DOM_CHECK_ARGUMENTS_SILENT("nnnn");

	domcontext->m_context->fillRect(argv[0].value.number, argv[1].value.number, argv[2].value.number, argv[3].value.number);
	domcontext->m_domcanvas->ScheduleInvalidation(origining_runtime);
	return ES_FAILED;
}

int
DOMCanvasContext2D::strokeRect(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domcontext, DOM_TYPE_CANVASCONTEXT2D, DOMCanvasContext2D);
	DOM_CHECK_ARGUMENTS_SILENT("nnnn");

	domcontext->m_context->strokeRect(argv[0].value.number,	argv[1].value.number, argv[2].value.number, argv[3].value.number);
	domcontext->m_domcanvas->ScheduleInvalidation(origining_runtime);
	return ES_FAILED;
}

int
DOMCanvasContext2D::beginPath(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domcontext, DOM_TYPE_CANVASCONTEXT2D, DOMCanvasContext2D);

	CALL_FAILED_IF_ERROR(domcontext->m_context->beginPath());
	return ES_FAILED;
}

int
DOMCanvasContext2D::closePath(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domcontext, DOM_TYPE_CANVASCONTEXT2D, DOMCanvasContext2D);

	CALL_FAILED_IF_ERROR(domcontext->m_context->closePath());
	return ES_FAILED;
}

int
DOMCanvasContext2D::moveTo(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domcontext, DOM_TYPE_CANVASCONTEXT2D, DOMCanvasContext2D);
	DOM_CHECK_ARGUMENTS_SILENT("nn");

	CALL_FAILED_IF_ERROR(domcontext->m_context->moveTo(argv[0].value.number, argv[1].value.number));
	return ES_FAILED;
}

int
DOMCanvasContext2D::lineTo(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domcontext, DOM_TYPE_CANVASCONTEXT2D, DOMCanvasContext2D);
	DOM_CHECK_ARGUMENTS_SILENT("nn");

	CALL_FAILED_IF_ERROR(domcontext->m_context->lineTo(argv[0].value.number, argv[1].value.number));
	return ES_FAILED;
}

int
DOMCanvasContext2D::quadraticCurveTo(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domcontext, DOM_TYPE_CANVASCONTEXT2D, DOMCanvasContext2D);
	DOM_CHECK_ARGUMENTS_SILENT("nnnn");

	CALL_FAILED_IF_ERROR(domcontext->m_context->quadraticCurveTo(argv[0].value.number, argv[1].value.number, argv[2].value.number, argv[3].value.number));
	return ES_FAILED;
}

int
DOMCanvasContext2D::bezierCurveTo(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domcontext, DOM_TYPE_CANVASCONTEXT2D, DOMCanvasContext2D);
	DOM_CHECK_ARGUMENTS_SILENT("nnnnnn");

	CALL_FAILED_IF_ERROR(domcontext->m_context->bezierCurveTo(argv[0].value.number, argv[1].value.number, argv[2].value.number, argv[3].value.number, argv[4].value.number, argv[5].value.number));
	return ES_FAILED;
}

int
DOMCanvasContext2D::arcTo(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domcontext, DOM_TYPE_CANVASCONTEXT2D, DOMCanvasContext2D);
	DOM_CHECK_ARGUMENTS_SILENT("nnnnn");

	if (argv[4].value.number <= 0.)
		return DOM_CALL_DOMEXCEPTION(INDEX_SIZE_ERR);

	CALL_FAILED_IF_ERROR(domcontext->m_context->arcTo(argv[0].value.number, argv[1].value.number, argv[2].value.number, argv[3].value.number, argv[4].value.number));
	return ES_FAILED;
}

int
DOMCanvasContext2D::rect(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domcontext, DOM_TYPE_CANVASCONTEXT2D, DOMCanvasContext2D);
	DOM_CHECK_ARGUMENTS_SILENT("nnnn");

	CALL_FAILED_IF_ERROR(domcontext->m_context->rect(argv[0].value.number, argv[1].value.number, argv[2].value.number, argv[3].value.number));
	return ES_FAILED;
}

int
DOMCanvasContext2D::arc(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domcontext, DOM_TYPE_CANVASCONTEXT2D, DOMCanvasContext2D);
	DOM_CHECK_ARGUMENTS_SILENT("nnnnn|b");

	if (argv[2].value.number < 0.)
		return DOM_CALL_DOMEXCEPTION(INDEX_SIZE_ERR);

	BOOL anticlockwise = argc > 5 && argv[5].value.boolean;

	CALL_FAILED_IF_ERROR(domcontext->m_context->arc(argv[0].value.number, argv[1].value.number, argv[2].value.number, argv[3].value.number, argv[4].value.number, anticlockwise));
	return ES_FAILED;
}

int
DOMCanvasContext2D::fill(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domcontext, DOM_TYPE_CANVASCONTEXT2D, DOMCanvasContext2D);

	domcontext->m_context->fill();
	domcontext->m_domcanvas->ScheduleInvalidation(origining_runtime);
	return ES_FAILED;
}

int
DOMCanvasContext2D::stroke(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domcontext, DOM_TYPE_CANVASCONTEXT2D, DOMCanvasContext2D);

	domcontext->m_context->stroke();
	domcontext->m_domcanvas->ScheduleInvalidation(origining_runtime);
	return ES_FAILED;
}

int
DOMCanvasContext2D::clip(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domcontext, DOM_TYPE_CANVASCONTEXT2D, DOMCanvasContext2D);

	domcontext->m_context->clip();
	return ES_FAILED;
}

int
DOMCanvasContext2D::isPointInPath(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domcontext, DOM_TYPE_CANVASCONTEXT2D, DOMCanvasContext2D);
	DOM_CHECK_ARGUMENTS("NN");

	if (!op_isfinite(argv[0].value.number) || !op_isfinite(argv[1].value.number))
		DOMSetBoolean(return_value, FALSE);
	else if (domcontext->m_context->isPointInPath(argv[0].value.number, argv[1].value.number))
		DOMSetBoolean(return_value, TRUE);
	else
		DOMSetBoolean(return_value, FALSE);

	return ES_VALUE;
}

#ifdef CANVAS_TEXT_SUPPORT
int
DOMCanvasContext2D::fillText(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domcontext, DOM_TYPE_CANVASCONTEXT2D, DOMCanvasContext2D);
	DOM_CHECK_ARGUMENTS("snn|n");

	double max_width = op_nan(NULL);
	if (argc > 3 &&
		op_isfinite(argv[3].value.number) &&
		argv[3].value.number >= 0.0)
	{
		max_width = argv[3].value.number;

		if (max_width == 0.0)
			return ES_FAILED;
	}

	DOM_HTMLCanvasElement* canvas = domcontext->m_domcanvas;
	FramesDocument* frm_doc = canvas->GetFramesDocument();
	HTML_Element* element = canvas->GetThisElement();

	domcontext->m_context->fillText(frm_doc, element, argv[0].value.string,
									argv[1].value.number,
									argv[2].value.number,
									max_width);
	domcontext->m_domcanvas->ScheduleInvalidation(origining_runtime);
	return ES_FAILED;
}

int
DOMCanvasContext2D::strokeText(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domcontext, DOM_TYPE_CANVASCONTEXT2D, DOMCanvasContext2D);
	DOM_CHECK_ARGUMENTS("snn|n");

	double max_width = op_nan(NULL);
	if (argc > 3 &&
		op_isfinite(argv[3].value.number) &&
		argv[3].value.number >= 0.0)
	{
		max_width = argv[3].value.number;

		if (max_width == 0.0)
			return ES_FAILED;
	}

	DOM_HTMLCanvasElement* canvas = domcontext->m_domcanvas;
	FramesDocument* frm_doc = canvas->GetFramesDocument();
	HTML_Element* element = canvas->GetThisElement();

	domcontext->m_context->strokeText(frm_doc, element, argv[0].value.string,
									  argv[1].value.number,
									  argv[2].value.number,
									  max_width);
	domcontext->m_domcanvas->ScheduleInvalidation(origining_runtime);
	return ES_FAILED;
}

int
DOMCanvasContext2D::measureText(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domcontext, DOM_TYPE_CANVASCONTEXT2D, DOMCanvasContext2D);
	DOM_CHECK_ARGUMENTS("s");

	DOM_HTMLCanvasElement* canvas = domcontext->m_domcanvas;
	FramesDocument* frm_doc = canvas->GetFramesDocument();
	HTML_Element* element = canvas->GetThisElement();

	double text_extent;
	CALL_FAILED_IF_ERROR(domcontext->m_context->measureText(frm_doc, element, argv[0].value.string, &text_extent));

	// Create TextMetrics-object
	DOM_Object* metrics = NULL;
	DOM_Runtime* runtime = domcontext->GetRuntime();
	CALL_FAILED_IF_ERROR(DOM_Object::DOMSetObjectRuntime(metrics = OP_NEW(DOM_Object, ()),
														 runtime,
														 runtime->GetPrototype(DOM_Runtime::TEXTMETRICS_PROTOTYPE),
														 "TextMetrics"));

	ES_Value val;
	val.type = VALUE_NUMBER;
	val.value.number = text_extent;
	CALL_FAILED_IF_ERROR(metrics->Put(UNI_L("width"), val, TRUE));

	DOMSetObject(return_value, metrics);
	return ES_VALUE;
}
#endif // CANVAS_TEXT_SUPPORT

int
DOMCanvasContext2D::drawImage(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domcontext, DOM_TYPE_CANVASCONTEXT2D, DOMCanvasContext2D);

	DOM_Object* dh;
	DOMCanvasContext2D_RestartDrawImage* restart_object;

	DrawImageParams params;
	double* src = params.src;
	double* dst = params.dst;
	dst[0] = 0;
	dst[1] = 0;
	dst[2] = -1.;
	dst[3] = -1.;
	src[0] = 0;
	src[1] = 0;
	src[2] = -1.;
	src[3] = -1.;

	if (argc >= 0)
	{
		if (argc > 0 && argv[0].type != VALUE_OBJECT)
		{
			return DOM_CALL_DOMEXCEPTION(TYPE_MISMATCH_ERR);
		}

		// blit the image
		if (argc == 3)
		{
			DOM_CHECK_ARGUMENTS_SILENT("onn");
			dst[0] = argv[1].value.number;
			dst[1] = argv[2].value.number;
		}
		else if (argc == 5)
		{
			DOM_CHECK_ARGUMENTS_SILENT("onnnn");
			dst[0] = argv[1].value.number;
			dst[1] = argv[2].value.number;
			dst[2] = argv[3].value.number;
			dst[3] = argv[4].value.number;

			if (dst[2] < 0. || dst[3] < 0.)
			{
				return DOM_CALL_DOMEXCEPTION(INDEX_SIZE_ERR);
			}
		}
		else if (argc == 9)
		{
			DOM_CHECK_ARGUMENTS_SILENT("onnnnnnnn");
			src[0] = argv[1].value.number;
			src[1] = argv[2].value.number;
			src[2] = argv[3].value.number;
			src[3] = argv[4].value.number;
			dst[0] = argv[5].value.number;
			dst[1] = argv[6].value.number;
			dst[2] = argv[7].value.number;
			dst[3] = argv[8].value.number;

			if (dst[2] < 0. || dst[3] < 0.)
			{
				return DOM_CALL_DOMEXCEPTION(INDEX_SIZE_ERR);
			}
			else if (src[0] < 0. || src[1] < 0. ||
					 src[2] < 0. || src[3] < 0.)
			{
				return DOM_CALL_DOMEXCEPTION(INDEX_SIZE_ERR);
			}
		}
		else
		{
			return ES_FAILED;
		}
		dh = DOM_VALUE2OBJECT(argv[0], DOM_Object);
	}
	else
	{
		restart_object = DOM_VALUE2OBJECT(*return_value, DOMCanvasContext2D_RestartDrawImage);
		restart_object->CopyTo(&params);
		dh = restart_object->GetObject();
	}

	URL src_url;

	OpBitmap *bitmap = NULL;
	VEGAFill* fill = NULL;
	Image img;
	BOOL releaseImage = FALSE;
	BOOL releaseBitmap = FALSE;
	BOOL tainted = FALSE;
	unsigned int width, height;

#ifdef MEDIA_HTML_SUPPORT
	BOOL scale_pixel_aspect = FALSE;
	if (dh && dh->IsA(DOM_TYPE_HTML_VIDEOELEMENT))
		scale_pixel_aspect = TRUE;
#endif // MEDIA_HTML_SUPPORT

	CanvasResourceDecoder::DecodeStatus status = CanvasResourceDecoder::DecodeResource(dh, domcontext->m_domcanvas, origining_runtime, static_cast<int>(dst[2]), static_cast<int>(dst[3]), TRUE, src_url, bitmap, fill, NULL, FALSE, img, releaseImage, releaseBitmap, width, height, tainted);

	switch (status)
	{
	case CanvasResourceDecoder::DECODE_OK:
		break;
	case CanvasResourceDecoder::DECODE_OOM:
		return ES_NO_MEMORY;
	case CanvasResourceDecoder::DECODE_SVG_NOT_READY:
	{
		OP_ASSERT(dh && dh->IsA(DOM_TYPE_HTML_ELEMENT));
		if (argc < 0)
			return ES_FAILED;

		HTML_Element* element = static_cast<DOM_HTMLElement*>(dh)->GetThisElement();
		CALL_FAILED_IF_ERROR(DOMCanvasContext2D_RestartDrawImage::Make(restart_object, dh, element, params, origining_runtime));
		CALL_FAILED_IF_ERROR(element->SetSVGIMGResourceListener(restart_object));
		return restart_object->BlockCall(return_value, origining_runtime);
	}
	case CanvasResourceDecoder::DECODE_EXCEPTION_TYPE_MISMATCH:
		return DOM_CALL_DOMEXCEPTION(TYPE_MISMATCH_ERR);
	case CanvasResourceDecoder::DECODE_EXCEPTION_NOT_SUPPORTED:
		return DOM_CALL_DOMEXCEPTION(NOT_SUPPORTED_ERR);
	default:
		return ES_FAILED;
	}

	if (src[2] < 0)
		src[2] = width;
	if (dst[2] < 0)
		dst[2] = src[2];
	if (src[3] < 0)
		src[3] = height;
	if (dst[3] < 0)
		dst[3] = src[3];

#ifdef MEDIA_HTML_SUPPORT
	// scale coordinates to bitmap size (for video where intrinsic
	// size can be different from bitmap size)
	if (bitmap && scale_pixel_aspect && width > 0 && height > 0)
	{
		// at most one dimension should increase when rendered
		// compared to the physical size in pixels
		OP_ASSERT((bitmap->Width() <= width && bitmap->Height() == height) ||
				  (bitmap->Width() == width && bitmap->Height() <= height));
		double scale_x = static_cast<double>(bitmap->Width()) / width;
		double scale_y = static_cast<double>(bitmap->Height()) / height;
		src[0] *= scale_x;
		src[1] *= scale_y;
		src[2] *= scale_x;
		src[3] *= scale_y;
	}
#endif

	if (src[2] == 0 || src[3] == 0)
	{
		if (releaseImage)
		{
			if (bitmap)
				img.ReleaseBitmap();
			img.DecVisible(null_image_listener);
		}
		if (releaseBitmap)
			OP_DELETE(bitmap);
		return DOM_CALL_DOMEXCEPTION(INDEX_SIZE_ERR);
	}

	if (tainted || !CanvasOriginCheck(domcontext, src_url, domcontext->GetRuntime()->GetOriginURL(), origining_runtime))
	{
		domcontext->m_domcanvas->MakeInsecure();
	}
	OP_STATUS res = OpStatus::ERR;
	if (bitmap)
		res = domcontext->m_context->drawImage(bitmap, src, dst);
	else if (fill)
		res = domcontext->m_context->drawImage(fill, src, dst);

	if (OpStatus::IsSuccess(res))
	{
		domcontext->m_domcanvas->ScheduleInvalidation(origining_runtime);
	}

	if (releaseImage)
	{
		if (bitmap)
			img.ReleaseBitmap();
		img.DecVisible(null_image_listener);
	}
	if (releaseBitmap)
		OP_DELETE(bitmap);
	return ES_FAILED;
}

int
DOMCanvasContext2D::createImageData(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT_UNUSED(DOM_TYPE_CANVASCONTEXT2D);

	unsigned int width, height;
	if (argc == 1)
	{
		DOM_CHECK_ARGUMENTS("o");

		DOM_Object* dh = DOM_VALUE2OBJECT(argv[0], DOM_Object);
		if (!dh || !dh->IsA(DOM_TYPE_CANVASIMAGEDATA))
			return DOM_CALL_DOMEXCEPTION(TYPE_MISMATCH_ERR);

		DOMCanvasImageData* image_data = static_cast<DOMCanvasImageData*>(dh);
		width = image_data->GetWidth();
		height = image_data->GetHeight();
	}
	else if (argc == 2)
	{
		DOM_CHECK_ARGUMENTS("nn");

		double dbl_w = op_ceil(op_fabs(argv[0].value.number));
		double dbl_h = op_ceil(op_fabs(argv[1].value.number));

		if (!ValidateSize(dbl_w, UINT_MAX) || !ValidateSize(dbl_h, UINT_MAX))
			return DOM_CALL_DOMEXCEPTION(INDEX_SIZE_ERR);

		width = static_cast<unsigned int>(dbl_w);
		height = static_cast<unsigned int>(dbl_h);

		if (!CheckImageDataSize(width, height))
			return DOM_CALL_DOMEXCEPTION(INDEX_SIZE_ERR);
	}
	else
	{
		return DOM_CALL_DOMEXCEPTION(NOT_SUPPORTED_ERR);
	}

	DOMCanvasImageData *image_data;
	CALL_FAILED_IF_ERROR(DOMCanvasImageData::Make(image_data, this_object->GetRuntime(), width, height));

	// Set the pixel data to zero ('transparent black')
	op_memset(image_data->GetPixels(), 0, width * height * 4);

	DOMSetObject(return_value, image_data);
	return ES_VALUE;
}

int
DOMCanvasContext2D::getImageData(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domcontext, DOM_TYPE_CANVASCONTEXT2D, DOMCanvasContext2D);
	DOM_CHECK_ARGUMENTS("nnnn");

	if (argv[2].value.number == 0.0 || argv[3].value.number == 0.0)
		return DOM_CALL_DOMEXCEPTION(INDEX_SIZE_ERR);

	Canvas *canvas = domcontext->m_context->getCanvas();
	if (!canvas)
		return ES_FAILED;

	if (!canvas->IsSecure())
		return ES_EXCEPT_SECURITY;

	double x = op_floor(argv[0].value.number);
	double y = op_floor(argv[1].value.number);
	double w = op_ceil(argv[2].value.number);
	double h = op_ceil(argv[3].value.number);

	if (w < 0)
	{
		x += w;
		w = -w;
	}
	if (h < 0)
	{
		y += h;
		h = -h;
	}

	// Verify that min/max x/y are within [INT_MIN, INT_MAX], and that
	// either dimension isn't larger than INT_MAX.
	if (!ValidateInt(x) || !ValidateInt(y) ||
		!ValidateInt(x + w) || !ValidateInt(y + h) ||
		!ValidateSize(w, INT_MAX) || !ValidateSize(h, INT_MAX))
		return DOM_CALL_DOMEXCEPTION(INDEX_SIZE_ERR);

	int ix = static_cast<int>(x);
	int iy = static_cast<int>(y);
	int iw = static_cast<int>(w);
	int ih = static_cast<int>(h);

	OP_ASSERT(iw > 0 && ih > 0);

	if (!CheckImageDataSize(iw, ih))
		return DOM_CALL_DOMEXCEPTION(INDEX_SIZE_ERR);

	DOMCanvasImageData *image_data;
	CALL_FAILED_IF_ERROR(DOMCanvasImageData::Make(image_data, this_object->GetRuntime(), iw, ih));

	domcontext->m_context->getImageData(ix, iy, iw, ih, image_data->GetPixels());

	DOMSetObject(return_value, image_data);
	return ES_VALUE;
}

int
DOMCanvasContext2D::putImageData(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domcontext, DOM_TYPE_CANVASCONTEXT2D, DOMCanvasContext2D);
	if (!domcontext->m_context->getCanvas())
		return ES_FAILED;

	DOM_CHECK_ARGUMENTS("-nn|nnnn");
	if (argv[0].type != VALUE_OBJECT)
		return DOM_CALL_DOMEXCEPTION(TYPE_MISMATCH_ERR);

	if (argc > 3 && argc < 7)
		return DOM_CALL_DOMEXCEPTION(NOT_SUPPORTED_ERR);

	DOM_Object* dh = DOM_VALUE2OBJECT(argv[0], DOM_Object);
	if (!dh || !dh->IsA(DOM_TYPE_CANVASIMAGEDATA))
		return DOM_CALL_DOMEXCEPTION(TYPE_MISMATCH_ERR);

	DOMCanvasImageData* image_data = static_cast<DOMCanvasImageData*>(dh);

	int dst_x = static_cast<int>(argv[1].value.number);
	int dst_y = static_cast<int>(argv[2].value.number);

	int src_x, src_y, src_w, src_h;
	if (argc >= 7)
	{
		double dirty_x = argv[3].value.number;
		double dirty_y = argv[4].value.number;
		double dirty_w = argv[5].value.number;
		double dirty_h = argv[6].value.number;

		if (dirty_w < 0.0)
		{
			dirty_x += dirty_w;
			dirty_w = op_fabs(dirty_w);
		}
		if (dirty_h < 0.0)
		{
			dirty_y += dirty_h;
			dirty_h = op_fabs(dirty_h);
		}

		if (dirty_x < 0.0)
		{
			dirty_w += dirty_x;
			dirty_x = 0.0;
		}
		if (dirty_y < 0.0)
		{
			dirty_h += dirty_y;
			dirty_y = 0.0;
		}

		if (dirty_x + dirty_w > image_data->GetWidth())
			dirty_w = image_data->GetWidth() - dirty_x;
		if (dirty_y + dirty_h > image_data->GetHeight())
			dirty_h = image_data->GetHeight() - dirty_y;

		if (dirty_w <= 0.0 || dirty_h <= 0.0)
			return ES_FAILED;

		src_x = static_cast<int>(dirty_x);
		src_y = static_cast<int>(dirty_y);
		src_w = static_cast<int>(op_ceil(dirty_x + dirty_w)) - src_x;
		src_h = static_cast<int>(op_ceil(dirty_y + dirty_h)) - src_y;
	}
	else
	{
		OP_ASSERT(argc == 3);

		src_x = 0;
		src_y = 0;
		src_w = image_data->GetWidth();
		src_h = image_data->GetHeight();
	}

	domcontext->m_context->putImageData(dst_x, dst_y, src_x, src_y, src_w, src_h,
										image_data->GetPixels(), image_data->GetWidth());

	domcontext->m_domcanvas->ScheduleInvalidation(origining_runtime);

	return ES_FAILED;
}

/*virtual*/ void
DOMCanvasContext2D::GCTrace()
{
	// Make sure our owning canvas is not destroyed
	GCMark(m_domcanvas);

	GetRuntime()->RecordExternalAllocation(CanvasContext2D::allocationCost(m_context));
	// If a gradient or pattern is currently set, make sure they are not deleted
	List<CanvasFill> fills;
	m_context->GetReferencedFills(&fills);
	while (CanvasFill* canvas_fill = fills.First())
	{
		GCMark(canvas_fill->GetDOM_Object());

		canvas_fill->Out();
	}

	OP_ASSERT(fills.Empty());
}
// -----------------------------------------------------------

#include "modules/dom/src/domglobaldata.h"

DOM_FUNCTIONS_START(DOMCanvasGradient)
	DOM_FUNCTIONS_FUNCTION(DOMCanvasGradient, DOMCanvasGradient::addColorStop, "addColorStop", "ns-")
DOM_FUNCTIONS_END(DOMCanvasGradient)

DOM_FUNCTIONS_START(DOMCanvasContext2D)
	// WebApplications 1.0
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContext2D, DOMCanvasContext2D::save, "save", NULL)
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContext2D, DOMCanvasContext2D::restore, "restore", NULL)

	DOM_FUNCTIONS_FUNCTION(DOMCanvasContext2D, DOMCanvasContext2D::scale, "scale", "nn-")
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContext2D, DOMCanvasContext2D::rotate, "rotate", "n-")
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContext2D, DOMCanvasContext2D::translate, "translate", "nn-")
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContext2D, DOMCanvasContext2D::transform, "transform", "nnnnnn-")
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContext2D, DOMCanvasContext2D::setTransform, "setTransform", "nnnnnn-")

	DOM_FUNCTIONS_FUNCTION(DOMCanvasContext2D, DOMCanvasContext2D::createLinearGradient, "createLinearGradient", "nnnn-")
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContext2D, DOMCanvasContext2D::createRadialGradient, "createRadialGradient", "nnnnnn-")
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContext2D, DOMCanvasContext2D::createPattern, "createPattern", "-s-")

	DOM_FUNCTIONS_FUNCTION(DOMCanvasContext2D, DOMCanvasContext2D::clearRect, "clearRect", "nnnn-")
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContext2D, DOMCanvasContext2D::fillRect, "fillRect", "nnnn-")
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContext2D, DOMCanvasContext2D::strokeRect, "strokeRect", "nnnn-")

	DOM_FUNCTIONS_FUNCTION(DOMCanvasContext2D, DOMCanvasContext2D::beginPath, "beginPath", NULL)
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContext2D, DOMCanvasContext2D::closePath, "closePath", NULL)
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContext2D, DOMCanvasContext2D::moveTo, "moveTo", "nn-")
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContext2D, DOMCanvasContext2D::lineTo, "lineTo", "nn-")
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContext2D, DOMCanvasContext2D::quadraticCurveTo, "quadraticCurveTo", "nnnn-")
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContext2D, DOMCanvasContext2D::bezierCurveTo, "bezierCurveTo", "nnnnnn-")
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContext2D, DOMCanvasContext2D::arcTo, "arcTo", "nnnnn-")
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContext2D, DOMCanvasContext2D::rect, "rect", "nnnn-")
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContext2D, DOMCanvasContext2D::arc, "arc", "nnnnnb-")
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContext2D, DOMCanvasContext2D::fill, "fill", NULL)
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContext2D, DOMCanvasContext2D::stroke, "stroke", NULL)
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContext2D, DOMCanvasContext2D::clip, "clip", NULL)
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContext2D, DOMCanvasContext2D::isPointInPath, "isPointInPath", "nn-")

#ifdef CANVAS_TEXT_SUPPORT
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContext2D, DOMCanvasContext2D::fillText, "fillText", "snnn-")
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContext2D, DOMCanvasContext2D::strokeText, "strokeText", "snnn-")
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContext2D, DOMCanvasContext2D::measureText, "measureText", "s-")
#endif // CANVAS_TEXT_SUPPORT

	DOM_FUNCTIONS_FUNCTION(DOMCanvasContext2D, DOMCanvasContext2D::drawImage, "drawImage", NULL)

	DOM_FUNCTIONS_FUNCTION(DOMCanvasContext2D, DOMCanvasContext2D::createImageData, "createImageData", NULL)
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContext2D, DOMCanvasContext2D::getImageData, "getImageData", "nnnn-")
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContext2D, DOMCanvasContext2D::putImageData, "putImageData", "-nnnnnn-")
DOM_FUNCTIONS_END(DOMCanvasContext2D)

#endif // CANVAS_SUPPORT
