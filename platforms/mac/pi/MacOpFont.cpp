/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "platforms/mac/pi/MacOpFont.h"
#include "platforms/mac/pi/MacOpFontManager.h"
#include "platforms/mac/util/CTextConverter.h"
#include "modules/hardcore/mem/mem_man.h"
#include "platforms/mac/util/systemcapabilities.h"
#include "platforms/mac/util/MachOCompatibility.h"
#include "modules/libvega/vegawindow.h"
#include "platforms/mac/pi/CocoaVEGAWindow.h"
#include "modules/libvega/src/oppainter/vegaoppainter.h"
#include "modules/libvega/src/vegaswbuffer.h"
#include "modules/mdefont/processedstring.h"
#include "platforms/mac/quick_support/CocoaQuickSupport.h"
#include "platforms/mac/CocoaVegaDefines.h"

#ifdef _DEBUG
#include "adjunct/desktop_util/string/string_convert.h"
#endif

CGrafPtr MacOpFont::sOffscreen = NULL;
CGContextRef MacOpFont::sCGContext = NULL;
MacOpFont* MacOpFont::sContextFont = NULL;

#if defined(SVG_SUPPORT) && defined(SVG_CAP_HAS_SVGPATH_API)
#include "modules/svg/SVGManager.h"
#include "modules/svg/svg_path.h"
#include "modules/svg/svg_number.h"
BOOL MacOpFont::mInitializedOutlinecallbacks = FALSE;
ATSCubicMoveToUPP MacOpFont::mMoveToUPP = NULL;
ATSCubicLineToUPP MacOpFont::mLineToUPP = NULL;
ATSCubicCurveToUPP MacOpFont::mCurveToUPP = NULL;
ATSCubicClosePathUPP MacOpFont::mClosePathUPP = NULL;
ATSQuadraticNewPathUPP MacOpFont::mQuadNewPathUPP = NULL;
ATSQuadraticLineUPP MacOpFont::mQuadLineUPP = NULL;
ATSQuadraticCurveUPP MacOpFont::mQuadCurveUPP = NULL;
ATSQuadraticClosePathUPP MacOpFont::mQuadClosePathUPP = NULL;
#endif // SVG_SUPPORT && SVG_CAP_HAS_SVGPATH_API

GlyphID *MacOpFont::mGlyphBuffer = new GlyphID[GLYPH_BUFFER_SIZE];
CGSize *MacOpFont::mAdvanceBuffer = new CGSize[GLYPH_BUFFER_SIZE];
Boolean MacOpFont::s_advanceBufferInitialized = false;

MacOpFont::MacOpFont(MacOpFontManager* manager, ATSFontRef inFont, CTFontRef fontRef, SInt16 inSize, SInt16 inFace, OpFontInfo::FontType fontType, int blur_radius):
                     mFontManager(manager)
                     ,mATSUid(inFont)
                     ,mFontType(fontType)
                     ,mWinHeight(0)
					 ,mPenColorRed(0.0f)
                     ,mPenColorGreen(0.0f)
                     ,mPenColorBlue(0.0f)
                     ,mPenColorAlpha(0.0f)
                     ,mCGFontChanged(FALSE)
					 ,m_blur_radius(blur_radius)
		             ,mSize(inSize)
		             ,mStyle(inFace)
		             ,mAscent(0)
		             ,mDescent(0)
		             ,mAvgCharWidth(0)
		             ,mOverhang(0)
			         ,mCGFontRef(NULL)
			         ,mCTFontRef(fontRef)
			         ,mDictionary(NULL)
			         ,mFontName(NULL)
			         ,mTextAlphaMask(NULL)
					 ,mIsStronglyContextualFont(FALSE)
					 ,mVegaScale(1.0)
{
	CreateCTFont();

	Initialize();

	VEGAFont::Construct();
}

UINT32 MacOpFont::Ascent()
{
	if (HasInsanelyLargeBoundingBox())
		return Height() - Descent();
	else
		return mAscent;
}

UINT32 MacOpFont::Descent()
{
	if (HasInsanelyLargeBoundingBox())
		return -mBoundingBox.origin.y;
	else
		return mDescent;
}

UINT32 MacOpFont::InternalLeading()
{
	return mAscent + mDescent - mSize;
}

UINT32 MacOpFont::Height()
{
	if (HasInsanelyLargeBoundingBox())
		return mBoundingBox.size.height;
	else
		return mAscent + mDescent;
}

UINT32 MacOpFont::AveCharWidth()
{
	return mVegaScale*mAvgCharWidth;
}

UINT32 MacOpFont::Overhang()
{
	return mVegaScale*mOverhang;
}

UINT32 MacOpFont::StringWidth(const uni_char* str, UINT32 len, INT32 extra_char_spacing /*= 0*/)
{
//	painter->SetFont(this);
//	return ((MacOpPainter*)painter)->StringWidth(str, len, extra_char_spacing);
	return StringWidth(str, len, NULL, extra_char_spacing);
}

BOOL MacOpFont::GetAlphaMask(const uni_char* str, UINT32 len, INT32 extra_char_spacing, short adjust, const OpRect& clip, const UINT8** mask, OpPoint* size, unsigned int* rowStride, unsigned int* pixelStride)
{
	int width =  (signed)StringWidth(str, len, extra_char_spacing);
	int height = (int)Height();
	size->x = 0;
	size->y = 0;

#ifdef PIXEL_SCALE_RENDERING_SUPPORT
	width = TO_DEVICE_PIXEL(mScaler, width);
	height = TO_DEVICE_PIXEL(mScaler, height);
#endif // PIXEL_SCALE_RENDERING_SUPPORT

	const size_t bytes_per_line = width * sizeof(UINT32);

	CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
	CGBitmapInfo alpha = kCGBitmapByteOrderVegaInternal;
	if (mTextAlphaMask)
	{
		free (mTextAlphaMask);
	}

	const size_t buffer_size = height * bytes_per_line;
	UINT32* mTextAlphaMask = (UINT32*)malloc(buffer_size);

	if (!mTextAlphaMask)
		return FALSE;

	memset(mTextAlphaMask, 0, buffer_size);
	CGContextRef sCGContextAlphaMask = CGBitmapContextCreate(mTextAlphaMask, width, height, 8, bytes_per_line, colorSpace, alpha);
	CGColorSpaceRelease(colorSpace);

	if (sCGContextAlphaMask)
	{
		UINT32 color = 0xFFFFFFFF;
		OpPoint pos(0,0);

		OpRect clip_rect(0,0,width,height);
		DrawString(pos, str, len, extra_char_spacing, clip_rect, color, sCGContextAlphaMask, CGBitmapContextGetHeight(sCGContextAlphaMask));

		*mask = (UINT8*)mTextAlphaMask;

		size->x = width;
		size->y = height;
		*rowStride = bytes_per_line;
		*pixelStride = 4;

		CGContextRelease(sCGContextAlphaMask);

		return TRUE;
	}
	return FALSE;
}

UINT32 MacOpFont::MaxAdvance()
{
	return mVegaScale*ceil(mBoundingBox.size.width + mOverhang);
}

#ifdef VEGA_3DDEVICE
UINT32 MacOpFont::MaxGlyphHeight()
{
	return mVegaScale*Height()+2+ExtraPadding();
}
#endif

void MacOpFont::unloadGlyph(VEGAGlyph& glyph)
{
	OP_DELETEA(((UINT8*)glyph.m_handle));
}

void MacOpFont::getGlyphBuffer(VEGAGlyph& glyph, const UINT8*& buffer, unsigned int& stride)
{
	buffer = (UINT8*)glyph.m_handle;
	stride = glyph.width;
}

OP_STATUS MacOpFont::loadGlyph(VEGAGlyph& glyph, UINT8* data, unsigned int stride, BOOL isIndex, BOOL blackOnWhite)
{
	if (!glyph.glyph)
		return OpStatus::ERR;
	
	UINT8* local_data;
	UnicodePoint up = glyph.glyph;
	
	float scale = 1.0;
#ifdef PIXEL_SCALE_RENDERING_SUPPORT
	scale = float(mScaler.GetScale()) / 100.0f;
#endif // PIXEL_SCALE_RENDERING_SUPPORT
	glyph.left = mBoundingBox.origin.x;
	glyph.top = 0;
	glyph.width = MaxAdvance() + 1;
	glyph.height = scale*Height();
#ifdef VEGA_SUBPIXEL_FONT_BLENDING
	// sorry for "bytes_per_line"; it's stride, but I didn't want to change parameter name
	const size_t bytes_per_line = glyph.width * sizeof(UINT32);
#else
	const size_t bytes_per_line = glyph.width;
#endif
	const size_t buffer_size = bytes_per_line * glyph.height;
	if (data)
	{
		local_data = data;
		glyph.m_handle = NULL;
	}
	else if (stride == 0)
	{
		local_data = OP_NEWA(UINT8, buffer_size);
		if (NULL == local_data)
			return OpStatus::ERR_NO_MEMORY;
		glyph.m_handle = local_data;
		stride = glyph.width; // ismailp - this is potentially wrong; in subpixel case, we may need to multiply this with 4.
	}

	UINT32* image_data = (UINT32*)malloc(buffer_size);
    if (image_data)
    {
        if (blackOnWhite)
            memset(image_data, 255, buffer_size);
        else
            for (unsigned int i=0; i<(buffer_size/4); i++)
                image_data[i] = 0xFF000000;
        CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
        const CGBitmapInfo alpha = kCGBitmapByteOrderVegaInternal;
        CGFontRef fallbackFont = NULL;
        CGContextRef ctx;
        if (ctx = CGBitmapContextCreate(image_data, glyph.width, glyph.height, 8, bytes_per_line, colorSpace, alpha))
        {
            if (blackOnWhite)
            {
                CGContextSetRGBFillColor(ctx, 0, 0, 0, 1.0);
                CGContextSetRGBStrokeColor(ctx, 0, 0, 0, 1.0);
            }
            else
            {
                CGContextSetRGBFillColor(ctx, 1.0, 1.0, 1.0, 1.0);
                CGContextSetRGBStrokeColor(ctx, 1.0, 1.0, 1.0, 1.0);
            }
            if (mFallbackFontNames.Contains((const void *)up))
            {
                CFStringRef fallbackFontName;
                mFallbackFontNames.GetData((const void *)up, (void**)&fallbackFontName);
                fallbackFont = CGFontCreateWithFontName(fallbackFontName);
                up -= MAGIC_NUMBER;
            }

            if (fallbackFont)
                CGContextSetFont(ctx, fallbackFont);
            else
                CGContextSetFont(ctx, GetCGFontRef());

            CGContextSetFontSize(ctx, scale*mSize);
            CGContextSetShouldSmoothFonts(ctx, mSize > mFontManager->GetAntialisingTreshhold());
            int x_pos = -mBoundingBox.origin.x;
            int y_pos = scale*Descent();

            if (GetShouldSynthesizeItalic()) {
                static const float degrees2rad = M_PI / 180.0;
                static const float skew = tan(14.0 * degrees2rad);
                static const CGAffineTransform italicCTM = ::CGAffineTransformMake(1, 0, skew, 1, 0, 0);
                CGContextConcatCTM(ctx, italicCTM);
                // Since we are positioning the baseline from the bottom of the bitmap context,
                // we need to negatively offset the x-position, so that the glyph crosses the baseline at the same point as before.
                // Therefore multiply descent by angle of skew.
                x_pos -= roundf(skew*scale*Descent());
            }

            if (m_blur_radius)
            {
                CGSize offset = {-OFF_SCREEN,0};
                CGContextSetShadowWithColor(ctx, offset, m_blur_radius, CGColorGetConstantColor(kCGColorBlack));
            }
			CGGlyph g = up;
            CGPoint positions = {m_blur_radius?x_pos+OFF_SCREEN:x_pos,y_pos};
            CGContextShowGlyphsAtPositions(ctx, &g, &positions, 1);
            if (GetShouldSynthesizeBold())
            {
                positions.x++;
                CGContextShowGlyphsAtPositions(ctx, &g, &positions, 1);
            }
            CGContextFlush(ctx);

            for (size_t y = 0; y < (size_t)glyph.height; ++y)
            {
                for (size_t x = 0; x < (size_t)glyph.width; ++x)
                {
                    UINT8 r = ((image_data[y*glyph.width+x]&0x00FF0000) >> 16);
                    UINT8 g = ((image_data[y*glyph.width+x]&0x0000FF00) >>  8);
                    UINT8 b =  (image_data[y*glyph.width+x]&0x000000FF);
                    if (blackOnWhite)
                    {
                        r = 255-r;
                        g = 255-g;
                        b = 255-b;
                    }
                    const UINT8 a = (r+g+b)/3;
#ifdef VEGA_SUBPIXEL_FONT_BLENDING
                    data[y*stride+x*4] = b;
                    data[y*stride+x*4+1] = g;
                    data[y*stride+x*4+2] = r;
                    data[y*stride+x*4+3] = a;
#else
                    data[y*stride+x+2] = a;
#endif
                }
            }

            CGFontRelease(fallbackFont);
            CGColorSpaceRelease(colorSpace);
            CGContextRelease(ctx);
        }

        free(image_data);
    }

	return OpStatus::OK;
}

#ifdef VEGA_NATIVE_FONT_SUPPORT

BOOL MacOpFont::nativeRendering()
{
#ifdef VEGA_3DDEVICE
	return g_opera->libvega_module.rasterBackend != LibvegaModule::BACKEND_HW3D;
#else
	return TRUE;
#endif
}

BOOL MacOpFont::DrawString(const OpPoint &pos, const uni_char* str, UINT32 len, INT32 extra_char_spacing, const OpRect& clip, UINT32 color, CGContextRef ctx, size_t win_height)
{
	if (!str || (len == 0) || ((len == 1) && uni_isspace(*str)))
		return TRUE;

	int glyph_height = (int)Height();
#ifdef PIXEL_SCALE_RENDERING_SUPPORT
	glyph_height = TO_DEVICE_PIXEL(mScaler, glyph_height);
#endif // PIXEL_SCALE_RENDERING_SUPPORT

	// Don't bother rendering outside clip rect.
	if (pos.x > clip.Right() || pos.y > clip.Bottom() || pos.y + glyph_height < clip.y)
		return TRUE;

	// Do not call StringWidth unless needed. If it looks unlikely that this will be visible, check string width,
	// otherwise just render it anyway (probably won't be visible, but won't suffer performance compared to not doing this test).
	// An unneeded StringWidth is costly, but not more so than an unneeded render. Should probably tweak the number in the
	// first test, so the second part of the test doen't fail too often. 50-100px seems reasonable choices for word length.
	if (pos.x + 100 < clip.x)
	{
		int string_width = (signed)StringWidth(str, len, extra_char_spacing);
#ifdef PIXEL_SCALE_RENDERING_SUPPORT
		string_width = TO_DEVICE_PIXEL(mScaler, string_width);
#endif // PIXEL_SCALE_RENDERING_SUPPORT
		if (pos.x + string_width < clip.x)
			return TRUE;
	}
	
	OP_ASSERT(len <= GLYPH_BUFFER_SIZE);
	
	float r = float((color >> 16) & 0xFF)/255.0;
	float g = float((color >> 8) & 0xFF)/255.0;
	float b = float(color & 0xFF)/255.0;
	float a = float((color >> 24) & 0xFF)/255.0;
	mPenColorRed = r;
	mPenColorGreen = g;
	mPenColorBlue = b;
	mPenColorAlpha = a;

	CGContextSaveGState(ctx);
	CGContextSetShouldAntialias(ctx, TRUE);
	CGContextSetRGBFillColor(ctx, r, g, b, a);
	CGContextSetFont(ctx, GetCGFontRef());
	CGContextSetFontSize(ctx, mSize);
	CGContextSetShouldSmoothFonts(ctx, mSize > mFontManager->GetAntialisingTreshhold());
    if (m_blur_radius)
    {
        CGSize offset = {-OFF_SCREEN,0};
        CGColorRef shadow_color = CGColorCreateGenericRGB(r, g, b, a);
        CGContextSetShadowWithColor(ctx, offset, m_blur_radius, shadow_color);
        CGColorRelease(shadow_color);
    }

#ifdef PIXEL_SCALE_RENDERING_SUPPORT
	float scale = float(mScaler.GetScale()) / 100.0f;
	CGAffineTransform text_mat_origin = CGContextGetTextMatrix(ctx);
	CGAffineTransform textmatrix = CGAffineTransformScale(text_mat_origin, scale, scale);
	CGContextSetTextMatrix(ctx, textmatrix);
#endif // PIXEL_SCALE_RENDERING_SUPPORT

	SetClipRect(ctx, clip);

	Boolean success = true;
	unsigned int ioNumGlyphs = GLYPH_BUFFER_SIZE;

	success = GetGlyphs(str, len, mGlyphBuffer, ioNumGlyphs);
	if (success)
	{
		float x_pos = pos.x;
		float ascent = (float)Ascent();
#ifdef PIXEL_SCALE_RENDERING_SUPPORT
		ascent = TO_DEVICE_PIXEL(mScaler, ascent);
#endif // PIXEL_SCALE_RENDERING_SUPPORT
		float y_pos = win_height-(pos.y+ascent);
		if (GetShouldSynthesizeItalic())
		{
			static const float degrees2rad = M_PI / 180.0;
			static const float skew = tan(14.0 * degrees2rad);
			static const CGAffineTransform italicCTM = CGAffineTransformMake(1, 0, skew, 1, 0, 0);
			CGContextConcatCTM(ctx, italicCTM);
			x_pos -= skew * y_pos;
		}

        CGPoint positions = {m_blur_radius?x_pos+OFF_SCREEN:x_pos,y_pos};
		CGContextSetTextPosition(ctx, positions.x, positions.y);
		
		for (unsigned int i=0; i<ioNumGlyphs; i++)
		{
			mAdvanceBuffer[i].width = TO_DEVICE_PIXEL(mScaler, GetGlyphAdvance(mGlyphBuffer[i])) + extra_char_spacing;
		}

		CGContextShowGlyphsWithAdvances(ctx, (const CGGlyph*)mGlyphBuffer, mAdvanceBuffer, ioNumGlyphs);
	}
	else
	{
		DrawStringUsingFallback(pos, str, len, extra_char_spacing, ctx, win_height);
	}

#ifdef PIXEL_SCALE_RENDERING_SUPPORT
	CGContextSetTextMatrix(ctx, text_mat_origin);
#endif // PIXEL_SCALE_RENDERING_SUPPORT

	CGContextRestoreGState(ctx);
	
	return TRUE;				
}

BOOL MacOpFont::DrawString(const OpPoint &pos, const uni_char* str, UINT32 len, INT32 extra_char_spacing, short adjust, const OpRect& clip, UINT32 color, VEGAWindow* dest) 
{
	CGContextRef ctx=((CocoaVEGAWindow*)dest)->getPainter();
	return DrawString(pos, str, len, extra_char_spacing, clip, color, ctx, CGBitmapContextGetHeight(ctx));
}

BOOL MacOpFont::DrawString(const OpPoint &pos, const uni_char* str, UINT32 len, INT32 extra_char_spacing, short adjust, const OpRect& clip, UINT32 color, VEGASWBuffer* dest)
{
	CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
	CGBitmapInfo alpha = kCGBitmapByteOrderVegaInternal;
	CGContextRef ctx = CGBitmapContextCreate(dest->GetRawBufferPtr(), dest->width, dest->height, 8, dest->pix_stride*dest->bpp, colorSpace, alpha);
	CGColorSpaceRelease(colorSpace);
	BOOL result = DrawString(pos, str, len, extra_char_spacing, clip, color, ctx, CGBitmapContextGetHeight(ctx));
	CGContextRelease(ctx);
	return result;
}

bool FontBlock::Construct(FontBlock** result, const OpRect &r, unsigned int bytes_per_pixel)
{
	RETURN_VALUE_IF_NULL(result, false);
	*result = NULL;
	FontBlock* p = OP_NEW(FontBlock, ());
	RETURN_VALUE_IF_NULL(p, false);
	const size_t image_size = r.width * r.height * bytes_per_pixel;
	void* image_data = malloc(image_size);
	if (NULL == image_data) {
		p->Release();
		return false;
	}
	memset(image_data, 0, image_size);
	p->image_data = image_data;
	p->rect = r;
	p->bytes_per_pixel = bytes_per_pixel;
	*result = p;
	return true;
}

FontBlock::FontBlock()
: c_ref(1)
, image_data(NULL)
, color_(0)
{
}

FontBlock::~FontBlock()
{
	free(image_data);
}

FontBlock* MacOpFont::GetFontBlock(const OpRect& clip, unsigned int bytes_per_pixel)
{
	/**
	 the problem here is hash table stores this pointer directly, instead of its value, which is what we are interested in.
	 therefore, we should either strdup this, or (a better way?) we quit using hash table and use a suffix trie instead.
	 final alternative would be to establish a little more flexible structure that can accomodate BVHs. this simplifies
	 merging significantly, I think... design a flexible, light and mergable BVH.
	 */
	char *buf = (char*)malloc(128);
	memset(buf, 0, 128);
	snprintf(buf, 127, "%d-%d-%d-%d-%u", clip.x, clip.y, clip.width, clip.height, bytes_per_pixel);
	FontBlock* result = NULL;
	if (!OpStatus::IsSuccess(image_data_map.GetData(buf, (void**)&result)))
	{
		if (!FontBlock::Construct(&result, clip, bytes_per_pixel))
			return result;
		if (OpStatus::IsError(image_data_map.Add(buf, result)))
		{
			result->Release();
			result = NULL;
		}
	}
	return result;
}

void FontBlock::SetText(const uni_char* s, UINT32 len)
{
	OpString8 ss;
	ss.SetUTF8FromUTF16(s, len);
	text_.Append(ss);
}

void FontBlock::SetModified(BOOL f)
{
	is_modified = f;
}

#ifdef VEGA_USE_HW
BOOL MacOpFont::DrawString(const OpPoint &pos, const uni_char* str, UINT32 len, INT32 extra_char_spacing, short adjust, const OpRect& clip, UINT32 color, VEGA3dRenderTarget* dest, bool is_window)
{
	if (!str || (len == 0) || ((len == 1) && uni_isspace(*str)))
		return TRUE;

	int glyph_height = (int)Height();
#ifdef PIXEL_SCALE_RENDERING_SUPPORT
	glyph_height = TO_DEVICE_PIXEL(mScaler, glyph_height);
#endif // PIXEL_SCALE_RENDERING_SUPPORT

	// Don't bother rendering outside clip rect.
	if (pos.x > clip.Right() || pos.y > clip.Bottom() || pos.y + glyph_height < clip.y)
		return TRUE;

	// Do not call StringWidth unless needed. If it looks unlikely that this will be visible, check string width,
	// otherwise just render it anyway (probably won't be visible, but won't suffer performance compared to not doing this test).
	// An unneeded StringWidth is costly, but not more so than an unneeded render. Should probably tweak the number in the
	// first test, so the second part of the test doen't fail too often. 50-100px seems reasonable choices for word length.
	if (pos.x + 100 < clip.x)
	{
		int string_width = (signed)StringWidth(str, len, extra_char_spacing);
#ifdef PIXEL_SCALE_RENDERING_SUPPORT
		string_width = TO_DEVICE_PIXEL(mScaler, string_width);
#endif // PIXEL_SCALE_RENDERING_SUPPORT
		if (pos.x + string_width < clip.x)
			return TRUE;
	}

	BOOL success = FALSE;
	OpRect localClip(0,0,clip.width,clip.height);
	const int w = clip.width;
	const int h = clip.height;
	const int bpl = 4 * w;
	FontBlock* fb = GetFontBlock(clip, 4);
	void *image_data = fb->GetImageData();
	const OpPoint where(pos.x-clip.x, pos.y-clip.y);

	fb->SetModified(fb->GetColor() != color || !fb->GetText() || op_memcmp(fb->GetText(), str, len * sizeof(uni_char)));

	if (fb->IsModified())
	{
		CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
		CGBitmapInfo alpha = kCGImageAlphaPremultipliedLast;
		CGContextRef context = CGBitmapContextCreate(image_data, w, h, 8, bpl, colorSpace, alpha);
		if (context)
		{
			CGContextScaleCTM(context, 1.0, -1.0);
			CGContextTranslateCTM(context, 0, -h);
			CGColorSpaceRelease(colorSpace);
			success = DrawString(where, str, len, extra_char_spacing, localClip, color, context, h);
			if (!success)
			{
				CGContextRelease(context);
				fb->Release();
				return FALSE;
			}
			
			fb->SetText(str, len);
			fb->SetColor(color);

			VEGA3dTexture *tex;
			const OpRect r(fb->GetRect());
			VEGA3dDevice* device = g_opera->libvega_module.vega3dDevice;

			device->createTexture(&tex, r.width, r.height, VEGA3dTexture::FORMAT_RGBA8888);
			tex->update(0, 0, r.width, r.height, (UINT8*)fb->GetImageData());		
			
			VEGA3dDevice::Vega2dVertex verts[] = {
				{float(r.x), float(r.y), 0, 1.f, ~0u},
				{float(r.x+tex->getWidth()), float(r.y), 1.f, 1.f, ~0u},
				{float(r.x), float(r.y+tex->getHeight()), 0, 0, ~0u},
				{float(r.x+tex->getWidth()), float(r.y+tex->getHeight()), 1.f, 0, ~0u}
			};

			VEGA3dBuffer* b = device->getTempBuffer(sizeof(verts));
			VEGA3dVertexLayout* layout = NULL;
			RETURN_IF_ERROR(device->createVega2dVertexLayout(&layout, VEGA3dShaderProgram::SHADER_VECTOR2D));
			VEGA3dRenderTarget* rt = device->getRenderTarget();
			device->setRenderTarget(dest);
			b->update(0, sizeof(verts), verts);
			device->setTexture(0, tex);
			device->drawPrimitives(VEGA3dDevice::PRIMITIVE_TRIANGLE_STRIP, layout, 0, 4);
			device->setTexture(0, NULL);
			device->setRenderTarget(rt);
			VEGA3dVertexLayout::DecRef(layout);
			memset(fb->GetImageData(), 0, fb->GetBPP() * r.width * r.height);
			VEGA3dTexture::DecRef(tex);
			CGContextRelease(context);
		}
	}
	else
	{
		success = TRUE;
	}
	
	return success;
}
#endif // VEGA_USE_HW

OP_STATUS MacOpFont::SetClipRect(CGContextRef ctx, const OpRect &rect)
{
	if(!ctx)
		return OpStatus::ERR;

	unsigned win_height = CGBitmapContextGetHeight(ctx);
	CGRect clipRect = CGRectMake(rect.x, win_height - rect.y - rect.height, rect.width, rect.height);
	CGContextClipToRect(ctx, clipRect);
	return OpStatus::OK;
}

#endif // VEGA_NATIVE_FONT_SUPPORT

#if defined(SVG_SUPPORT) && defined(SVG_CAP_HAS_SVGPATH_API)

#ifndef SIXTY_FOUR_BIT
struct OpOutlineCallbackStruct
{
	BOOL first;
	SVGPath* path;
};

pascal OSStatus MoveToProc(const Float32Point *pt, void *callBackDataPtr)
{
	OpOutlineCallbackStruct *data = (OpOutlineCallbackStruct*)callBackDataPtr;
	
	if (!data)
		return -1;
	
	SVGPath* p = data->path;
	if (p)
	{
		p->MoveTo(pt->x, -pt->y, FALSE);
		return noErr;
	}
	
	return -1;
}

pascal OSStatus LineToProc(const Float32Point *pt, void *callBackDataPtr)
{
	OpOutlineCallbackStruct *data = (OpOutlineCallbackStruct*)callBackDataPtr;
	
	if (!data)
		return -1;
	
	SVGPath* p = data->path;
	if (p)
	{
		p->LineTo(pt->x, -pt->y, FALSE);
		return noErr;
	}
	
	return -1;
}

pascal OSStatus CurveToProc(const Float32Point *pt1, const Float32Point *pt2, const Float32Point *pt3, void *callBackDataPtr)
{
	OpOutlineCallbackStruct *data = (OpOutlineCallbackStruct*)callBackDataPtr;
	
	if (!data)
		return -1;
	
	SVGPath* p = data->path;
	if (p)
	{
		p->CubicCurveTo(pt1->x, -pt1->y, pt2->x, -pt2->y, pt3->x, -pt3->y, FALSE, FALSE);
		return noErr;
	}
	
	return -1;
}

pascal OSStatus ClosePathProc(void * callBackDataPtr)
{
	OpOutlineCallbackStruct *data = (OpOutlineCallbackStruct*)callBackDataPtr;
	
	if (!data)
		return -1;
	
	SVGPath* p = data->path;
	if (p)
	{
		p->Close();
		return noErr;
	}
	
	return -1;
}

pascal OSStatus QuadNewPathProc(void * callBackDataPtr)
{
	OpOutlineCallbackStruct *data = (OpOutlineCallbackStruct*)callBackDataPtr;
	
	if (!data)
		return -1;
	
	data->first = true;
	
	return noErr;
}

pascal OSStatus QuadLineProc(const Float32Point *pt1, const Float32Point *pt2, void *callBackDataPtr)
{
	OpOutlineCallbackStruct *data = (OpOutlineCallbackStruct*)callBackDataPtr;
	
	if (!data)
		return -1;
	
	SVGPath* p = data->path;
	if (p)
	{
		if (pt1->x == pt2->x && pt1->y == pt2->y)
		{
			// Useless line
			return noErr;
		}
		if (data->first)
		{
			p->MoveTo(pt1->x, -pt1->y, FALSE);
			data->first = false;
		}
		
		
		p->LineTo(pt2->x, -pt2->y, FALSE);
		return noErr;
	}
	
	return -1;
}

pascal OSStatus QuadCurveProc(const Float32Point *pt1, const Float32Point *controlPt, const Float32Point *pt2, void *callBackDataPtr)
{
	OpOutlineCallbackStruct *data = (OpOutlineCallbackStruct*)callBackDataPtr;
	
	if (!data)
		return -1;
	
	SVGPath* p = data->path;
	if (p)
	{
		if (data->first)
		{
			p->MoveTo(pt1->x, -pt1->y, FALSE);
			data->first = false;
		}
		
		p->QuadraticCurveTo(controlPt->x, -controlPt->y, pt2->x, -pt2->y, FALSE, FALSE);
		return noErr;
	}
	
	return -1;
}

pascal OSStatus QuadClosePathProc(void * callBackDataPtr)
{
	OpOutlineCallbackStruct *data = (OpOutlineCallbackStruct*)callBackDataPtr;
	
	if (!data)
		return -1;
	
	SVGPath* p = data->path;
	if (p)
	{
		p->Close();
		return noErr;
	}
	
	return -1;
}

void MacOpFont::InitOutlineCallbacks()
{
	if (mInitializedOutlinecallbacks)
		return;
	
	if (!mMoveToUPP)
		mMoveToUPP = NewATSCubicMoveToUPP(MoveToProc);
	if (!mLineToUPP)
		mLineToUPP = NewATSCubicLineToUPP(LineToProc);
	if (!mCurveToUPP)
		mCurveToUPP = NewATSCubicCurveToUPP(CurveToProc);
	if (!mClosePathUPP)
		mClosePathUPP = NewATSCubicClosePathUPP(ClosePathProc);
	if (!mQuadNewPathUPP)
		mQuadNewPathUPP = NewATSQuadraticNewPathUPP(QuadNewPathProc);
	if (!mQuadLineUPP)
		mQuadLineUPP = NewATSQuadraticLineUPP(QuadLineProc);
	if (!mQuadCurveUPP)
		mQuadCurveUPP = NewATSQuadraticCurveUPP(QuadCurveProc);
	if (!mQuadClosePathUPP)
		mQuadClosePathUPP = NewATSQuadraticClosePathUPP(QuadClosePathProc);
	
	mInitializedOutlinecallbacks = mMoveToUPP && mLineToUPP && mCurveToUPP && mClosePathUPP &&	mQuadNewPathUPP && mQuadLineUPP && mQuadCurveUPP && mQuadClosePathUPP;
}

void MacOpFont::DeleteOutlineCallbacks()
{
	if (mMoveToUPP)
	{
		DisposeATSCubicMoveToUPP(mMoveToUPP);
		mMoveToUPP = NULL;
	}
	if (mLineToUPP)
	{
		DisposeATSCubicLineToUPP(mLineToUPP);
		mLineToUPP = NULL;
	}
	if (mCurveToUPP)
	{
		DisposeATSCubicCurveToUPP(mCurveToUPP);
		mCurveToUPP = NULL;
	}
	if (mClosePathUPP)
	{
		DisposeATSCubicClosePathUPP(mClosePathUPP);
		mClosePathUPP = NULL;
	}
	if (mQuadNewPathUPP)
	{
		DisposeATSQuadraticNewPathUPP(mQuadNewPathUPP);
		mQuadNewPathUPP = NULL;
	}
	if (mQuadLineUPP)
	{
		DisposeATSQuadraticLineUPP(mQuadLineUPP);
		mQuadLineUPP = NULL;
	}
	if (mQuadCurveUPP)
	{
		DisposeATSQuadraticCurveUPP(mQuadCurveUPP);
		mQuadCurveUPP = NULL;
	}
	if (mQuadClosePathUPP)
	{
		DisposeATSQuadraticClosePathUPP(mQuadClosePathUPP);
		mQuadClosePathUPP = NULL;
	}
	
	mInitializedOutlinecallbacks = false;
}
#endif // !SIXTY_FOUR_BIT

OP_STATUS MacOpFont::GetOutline(const uni_char* in_str, UINT32 in_len, UINT32& io_str_pos, UINT32 in_last_str_pos,
					 BOOL in_writing_direction_horizontal, SVGNumber& out_advance, SVGPath** out_glyph)
{
	if (!out_glyph)
		return OpStatus::OK;

	if (io_str_pos >= in_len || in_len == 0)
		return OpStatus::ERR_OUT_OF_RANGE;

	static GlyphID* glyphids = new GlyphID[8];
	GlyphID glyphid;
	unsigned int numglyphs = 8;
	UniCharArrayOffset new_strpos;
	OSStatus err = UCFindTextBreak(NULL, kUCTextBreakCharMask, kUCTextBreakLeadingEdgeMask | (io_str_pos == 0 ? kUCTextBreakIterateMask : 0) , (const UniChar*) in_str, in_len, io_str_pos, &new_strpos);
	if (err == noErr)
	{
		GetGlyphs(&in_str[io_str_pos], new_strpos - io_str_pos, glyphids, numglyphs, TRUE);
	}
	
	// It's expected that only one glyph is returned from GetGlyphs.
	OP_ASSERT(numglyphs == 1);
	
	if (numglyphs >= 1)
	{
		OSStatus result = noErr;
		glyphid = glyphids[0];
		SVGPath* outline = NULL;
		RETURN_IF_ERROR(g_svg_manager->CreatePath(&outline));
	
		if (GetOSVersion() >= 0x1060)
		{
			if (OpStatus::IsError(GetGlyphOutline(glyphid, outline)))
				result = -1;
		}
		else
		{
#ifdef SIXTY_FOUR_BIT
			result = -1;
#else
			OpOutlineCallbackStruct data;
			InitOutlineCallbacks();
			data.path = outline;

			Boolean		isBold = (mStyle & bold) ? 1 : 0;
			Boolean		isItalic = (mStyle & italic) ? 1 : 0;
			Fixed		fontSize = mSize << 16;
			int			attCount = 4;

			ATSUAttributeTag		arributes[] = {kATSUSizeTag, kATSUFontTag , kATSUQDBoldfaceTag, kATSUQDItalicTag};
			ByteCount				sizes[] = {sizeof(Fixed), sizeof(ATSUFontID), sizeof(Boolean), sizeof(Boolean)};
			ATSUAttributeValuePtr	values[] = {&fontSize, &mATSUid, &isBold, &isItalic};
			ATSUStyle				theATSUStyle;

			ATSUCreateStyle(&theATSUStyle);
			ATSUSetAttributes(theATSUStyle, attCount, arributes, sizes, values);

			ATSCurveType curveType = kATSCubicCurveType;
			ATSUGetNativeCurveType(theATSUStyle, &curveType);

			if (kATSQuadCurveType != curveType)
				ATSUGlyphGetCubicPaths(theATSUStyle, glyphid, MacOpFont::mMoveToUPP, MacOpFont::mLineToUPP, MacOpFont::mCurveToUPP, MacOpFont::mClosePathUPP, &data, &result);
			else
				ATSUGlyphGetQuadraticPaths(theATSUStyle, glyphid, MacOpFont::mQuadNewPathUPP, MacOpFont::mQuadLineUPP, MacOpFont::mQuadCurveUPP, MacOpFont::mQuadClosePathUPP, &data, &result);

			ATSUDisposeStyle(theATSUStyle);
#endif
		}

		// Something went wrong, destroy path and return error
		if (result != noErr)
			OP_DELETE(outline);
		else
			*out_glyph = outline;
		
		if (result == noErr)
		{
			if (in_writing_direction_horizontal)
				out_advance = GetGlyphAdvance(glyphid);
			else
				out_advance = Ascent() + Descent();
			
			io_str_pos++;
		}
		
		return result == noErr ? OpStatus::OK : OpStatus::ERR;
	}
	
	return OpStatus::ERR;
}

#endif // SVG_SUPPORT && SVG_CAP_HAS_SVGPATH_API

#ifdef OPFONT_GLYPH_PROPS

OP_STATUS MacOpFont::getGlyph(UnicodePoint c, VEGAGlyph& glyph, BOOL isIndex)
{
	uni_char str = c;
	glyph.top     = (str == 'x') ? mExHeight : Ascent();
	glyph.left    = 0;
	glyph.advance = GetGlyphAdvance(c);
	glyph.width   = StringWidth(&str, 1);
	glyph.height  = glyph.top + Descent();

	return OpStatus::OK;
}
#endif // OPFONT_GLYPH_PROPS
