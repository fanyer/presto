/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch_system_includes.h"

#ifdef VEGA_BACKEND_OPENGL
#include "platforms/vega_backends/opengl/vegaglapi.h"

#include "platforms/vega_backends/opengl/vegagldevice.h"
#include "platforms/vega_backends/opengl/vegagltexture.h"

#include "modules/libvega/src/vegaswbuffer.h"

static const unsigned int VEGACubeSideToGLTarget[] = { GL_TEXTURE_2D,
                                                       GL_TEXTURE_CUBE_MAP_POSITIVE_X,
                                                       GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
                                                       GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
                                                       GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
                                                       GL_TEXTURE_CUBE_MAP_POSITIVE_Z,
                                                       GL_TEXTURE_CUBE_MAP_NEGATIVE_Z };

static const unsigned int Tex2DInitialized = 0x1;
static const unsigned int CubeInitialized = 0x7e;

VEGAGlTexture::VEGAGlTexture(unsigned int width, unsigned int height,
							 ColorFormat fmt) :
		m_width(width), m_height(height), m_format(fmt),
		swrap(WRAP_CLAMP_EDGE), twrap(WRAP_CLAMP_EDGE),
		minFilter(FILTER_LINEAR), magFilter(FILTER_LINEAR),
		wrapChanged(true), filterChanged(true), texture(0),
		texture_initialized(0), is_cubemap(false), device(NULL)
{
	op_memset(m_mipInfo, 0, sizeof(UINT32) * NUM_CUBE_SIDES);
}

VEGAGlTexture::VEGAGlTexture(unsigned int cubeSize,
							 ColorFormat fmt) :
		m_width(cubeSize), m_height(cubeSize), m_format(fmt),
		swrap(WRAP_CLAMP_EDGE), twrap(WRAP_CLAMP_EDGE),
		minFilter(FILTER_LINEAR), magFilter(FILTER_LINEAR),
		wrapChanged(true), filterChanged(true), texture(0),
		texture_initialized(0), is_cubemap(true), device(NULL)
{
	op_memset(m_mipInfo, 0, sizeof(UINT32) * NUM_CUBE_SIDES);
}

VEGAGlTexture::~VEGAGlTexture()
{
	if (device)
	{
		device->clearActiveTexture(this);
		clearDevice();
	}
}

void VEGAGlTexture::setMipLevel(unsigned int level, VEGA3dTexture::CubeSide cs)
{
	OP_ASSERT(level < 32 && cs < NUM_CUBE_SIDES);
	m_mipInfo[cs] |= 1 << level;
}

bool VEGAGlTexture::isMipLevelSet(unsigned int level, VEGA3dTexture::CubeSide cs)
{
	OP_ASSERT(level < 32 && cs < NUM_CUBE_SIDES);
	return (m_mipInfo[cs] & (1 << level)) != 0;
}

bool VEGAGlTexture::isLevel0Set()
{
	if (is_cubemap)
		return (m_mipInfo[CUBE_SIDE_POS_X] & m_mipInfo[CUBE_SIDE_NEG_X] & m_mipInfo[CUBE_SIDE_POS_Y] &
		       m_mipInfo[CUBE_SIDE_NEG_Y] & m_mipInfo[CUBE_SIDE_POS_Z] & m_mipInfo[CUBE_SIDE_NEG_Z] & 1) != 0;
	else
		return (m_mipInfo[CUBE_SIDE_NONE] & 1) != 0;
}

void VEGAGlTexture::setAllMiplevels()
{
	unsigned int fullMask = (1 << (VEGA3dDevice::ilog2_floor(MAX(m_width, m_height)) + 1)) - 1;
	if (is_cubemap)
	{
		m_mipInfo[CUBE_SIDE_POS_X] |= fullMask;
		m_mipInfo[CUBE_SIDE_NEG_X] |= fullMask;
		m_mipInfo[CUBE_SIDE_POS_Y] |= fullMask;
		m_mipInfo[CUBE_SIDE_NEG_Y] |= fullMask;
		m_mipInfo[CUBE_SIDE_POS_Z] |= fullMask;
		m_mipInfo[CUBE_SIDE_NEG_Z] |= fullMask;
	}
	else
		m_mipInfo[CUBE_SIDE_NONE] |= fullMask;

}

void VEGAGlTexture::clearDevice()
{
	Out();

	if (texture)
		glDeleteTextures(1, (GLuint*)&texture);
	texture = 0;

	device = NULL;
}

OP_STATUS VEGAGlTexture::Construct(VEGAGlDevice* dev)
{
	device = dev;
	glGenTextures(1, (GLuint*)&texture);
	if (!texture)
		return OpStatus::ERR;

	return OpStatus::OK;
}

OP_STATUS VEGAGlTexture::createTexture()
{
	if (texture_initialized == (is_cubemap ? CubeInitialized : Tex2DInitialized))
		return OpStatus::OK;
	if (is_cubemap)
		device->setCubeTexture(0, this);
	else
		device->setTexture(0, this);
	unsigned int colorMode;
	unsigned int type;
	switch (m_format)
	{
	case FORMAT_LUMINANCE8:
		colorMode = GL_LUMINANCE;
		type = GL_UNSIGNED_BYTE;
		break;
	case FORMAT_ALPHA8:
		colorMode = GL_ALPHA;
		type = GL_UNSIGNED_BYTE;
		break;
	case FORMAT_LUMINANCE8_ALPHA8:
		colorMode = GL_LUMINANCE_ALPHA;
		type = GL_UNSIGNED_BYTE;
		break;
	case FORMAT_RGBA8888:
		colorMode = GL_RGBA;
		type = GL_UNSIGNED_BYTE;
		break;
	case FORMAT_RGB888:
		colorMode = GL_RGB;
		type = GL_UNSIGNED_BYTE;
		break;
	case FORMAT_RGB565:
		colorMode = GL_RGB;
		type = GL_UNSIGNED_SHORT_5_6_5;
		break;
	case FORMAT_RGBA5551:
		colorMode = GL_RGBA;
		type = device->supportRGB5_A1_fbo ? GL_UNSIGNED_SHORT_5_5_5_1 : GL_UNSIGNED_BYTE;
		break;
	case FORMAT_RGBA4444:
		colorMode = GL_RGBA;
		type = device->supportRGBA4_fbo ? GL_UNSIGNED_SHORT_4_4_4_4 : GL_UNSIGNED_BYTE;
		break;
	default:
		return OpStatus::ERR_NOT_SUPPORTED;
	}

	/* Make sure any previous error does not get counted as a failure of the code below. */
	OP_STATUS err = CheckGLError();
	OP_ASSERT(OpStatus::IsSuccess(err));

	if (is_cubemap)
	{
		for (unsigned int j = VEGA3dTexture::CUBE_SIDE_POS_X; j < VEGA3dTexture::NUM_CUBE_SIDES; ++j)
			if ((texture_initialized & (1 << j)) == 0)
				glTexImage2D(VEGACubeSideToGLTarget[j], 0, colorMode, m_width, m_height, 0, colorMode, type, NULL);
	}
	else
		glTexImage2D(GL_TEXTURE_2D, 0, colorMode, m_width, m_height, 0, colorMode, type, NULL);

	RETURN_IF_ERROR(CheckGLError());

	texture_initialized = is_cubemap ? CubeInitialized : Tex2DInitialized;
	return OpStatus::OK;
}

OP_STATUS VEGAGlTexture::update(unsigned int x, unsigned int y, const VEGASWBuffer* pixels, int rowAlignment, CubeSide side, int level, bool yFlip, bool premultiplyAlpha)
{
	if (!texture)
		return OpStatus::ERR;

	// Make sure we're not trying to set a cube side on a regular texture and vice versa.
	if ((side != CUBE_SIDE_NONE) != is_cubemap)
		return OpStatus::ERR;

	unsigned int texFormat;
	unsigned int texType;
	VEGAPixelStoreFormat storeFormat,
	                     requestedFormat = VPSF_SAME;
	switch (m_format)
	{
	case FORMAT_RGBA8888:
		storeFormat = VPSF_RGBA8888;
		texFormat = GL_RGBA;
		texType = GL_UNSIGNED_BYTE;
		break;
	case FORMAT_RGBA4444:
		texFormat = GL_RGBA;
		if (device->supportRGBA4_fbo)
		{
			storeFormat = VPSF_ABGR4444;
			texType = GL_UNSIGNED_SHORT_4_4_4_4;
		}
		else
		{
			storeFormat = VPSF_RGBA8888;
			texType = GL_UNSIGNED_BYTE;
			requestedFormat = VPSF_ABGR4444;
		}
		break;
	case FORMAT_RGB565:
		storeFormat = VPSF_RGB565;
		texFormat = GL_RGB;
		texType = GL_UNSIGNED_SHORT_5_6_5;
		break;
	case FORMAT_RGB888:
		storeFormat = VPSF_RGB888;
		texFormat = GL_RGB;
		texType = GL_UNSIGNED_BYTE;
		break;
	case FORMAT_RGBA5551:
		texFormat = GL_RGBA;
		if (device->supportRGB5_A1_fbo)
		{
			storeFormat = VPSF_RGBA5551;
			texType = GL_UNSIGNED_SHORT_5_5_5_1;
		}
		else
		{
			storeFormat = VPSF_RGBA8888;
			texType = GL_UNSIGNED_BYTE;
			requestedFormat = VPSF_RGBA5551;
		}
		break;
	case FORMAT_ALPHA8:
		storeFormat = VPSF_ALPHA8;
		texFormat = GL_ALPHA;
		texType = GL_UNSIGNED_BYTE;
		break;
	case FORMAT_LUMINANCE8:
		storeFormat = VPSF_ALPHA8;
		texFormat = GL_LUMINANCE;
		texType = GL_UNSIGNED_BYTE;
		break;
	case FORMAT_LUMINANCE8_ALPHA8:
		storeFormat = VPSF_LUMINANCE8_ALPHA8;
		texFormat = GL_LUMINANCE_ALPHA;
		texType = GL_UNSIGNED_BYTE;
		break;
	default:
		return OpStatus::ERR;
		break;
	}

	VEGAPixelStoreWrapper psw;
	void* buffer;
	bool pswNeedsFree = false;

#ifdef VEGA_INTERNAL_FORMAT_RGBA8888
	if (m_format == FORMAT_RGBA8888 && !pixels->IsIndexed() && rowAlignment <= 4 && !yFlip && !premultiplyAlpha)
	{
		buffer = pixels->GetRawBufferPtr();
	}
	else
#endif
	{
		RETURN_IF_ERROR(psw.Allocate(storeFormat, pixels->width, pixels->height, rowAlignment));
		VEGAPixelStore& ps = psw.ps;

		// Does the required format conversion.
		pixels->CopyToPixelStore(&psw.ps, yFlip, premultiplyAlpha, requestedFormat);
		buffer = ps.buffer;
		pswNeedsFree = true;
	}

	if (is_cubemap)
		device->setCubeTexture(0, this);
	else
		device->setTexture(0, this);

	unsigned int target = VEGACubeSideToGLTarget[side];

	if (!isMipLevelSet(level, side))
	{
		if (x == 0 && y == 0 && pixels->width == m_width && pixels->height == m_height)
		{
			glTexImage2D(target, level, texFormat, m_width, m_height, 0, texFormat, texType, buffer);
		}
		else
		{
			glTexImage2D(target, level, texFormat, m_width, m_height, 0, texFormat, texType, NULL);
			glTexSubImage2D(target, level, x, y, pixels->width, pixels->height, texFormat, texType, buffer);
		}
		if (level == 0)
			texture_initialized  |= 1 << side;
		setMipLevel(level, side);
	}
	else
	{
		glTexSubImage2D(target, level, x, y, pixels->width, pixels->height, texFormat, texType, buffer);
	}

	if (pswNeedsFree)
		psw.Free();

	return OpStatus::OK;
}



OP_STATUS VEGAGlTexture::update(unsigned int x, unsigned int y, unsigned int w, unsigned int h,
								UINT8* pixels, unsigned int rowAlignment, CubeSide side, int level, bool yFlip, bool premultiplyAlpha)
{
	unsigned int colorMode;
	VEGAPixelStoreFormat storeFormat;
	switch (m_format)
	{
	case FORMAT_LUMINANCE8:
		colorMode = GL_LUMINANCE;
		storeFormat = VPSF_ALPHA8;
		break;
	case FORMAT_ALPHA8:
		colorMode = GL_ALPHA;
		storeFormat = VPSF_ALPHA8;
		break;
	case FORMAT_LUMINANCE8_ALPHA8:
		colorMode = GL_LUMINANCE_ALPHA;
		storeFormat = VPSF_LUMINANCE8_ALPHA8;
		break;
	case FORMAT_RGBA8888:
		colorMode = GL_RGBA;
		storeFormat = VPSF_RGBA8888;
		break;
	case FORMAT_RGB888:
		colorMode = GL_RGB;
		storeFormat = VPSF_RGB888;
		break;
	default:
		return OpStatus::ERR;
	}

	return internalUpdate(x, y, w, h, pixels, rowAlignment, side, level, colorMode, GL_UNSIGNED_BYTE, yFlip, premultiplyAlpha, storeFormat, VPSF_SAME);
}

OP_STATUS VEGAGlTexture::update(unsigned int x, unsigned int y, unsigned int w, unsigned int h,
								UINT16* pixels, unsigned int rowAlignment, CubeSide side, int level, bool yFlip, bool premultiplyAlpha)
{
	unsigned int colorMode;
	unsigned int colorType;
	VEGAPixelStoreFormat storeFormat,
	                     requestedFormat = VPSF_SAME;
	switch (m_format)
	{
	case FORMAT_RGBA4444:
		colorMode = GL_RGBA;
		if (device->supportRGBA4_fbo)
		{
			colorType = GL_UNSIGNED_SHORT_4_4_4_4;
			storeFormat = VPSF_ABGR4444;
		}
		else
		{
			requestedFormat = VPSF_ABGR4444;
			colorType = GL_UNSIGNED_BYTE;
			storeFormat = VPSF_RGBA8888;
		}
		break;
	case FORMAT_RGBA5551:
		colorMode = GL_RGBA;
		if (device->supportRGB5_A1_fbo)
		{
			colorType = GL_UNSIGNED_SHORT_5_5_5_1;
			storeFormat = VPSF_RGBA5551;
		}
		else
		{
			requestedFormat = VPSF_RGBA5551;
			colorType = GL_UNSIGNED_BYTE;
			storeFormat = VPSF_RGBA8888;
		}
		break;
	case FORMAT_RGB565:
		colorMode = GL_RGB;
		colorType = GL_UNSIGNED_SHORT_5_6_5;
		storeFormat = VPSF_RGB565;
		break;
	default:
		return OpStatus::ERR;
	}

	return internalUpdate(x, y, w, h, pixels, rowAlignment, side, level, colorMode, colorType, yFlip, premultiplyAlpha, storeFormat, requestedFormat);
}

OP_STATUS VEGAGlTexture::internalUpdate(unsigned int x, unsigned int y, unsigned int w, unsigned int h,
										 void* pixels, unsigned int rowAlignment, CubeSide side, int level, 
										 unsigned int colorMode, unsigned int colorType, bool yFlip, bool premultiplyAlpha, VEGAPixelStoreFormat storeFormat, VEGAPixelStoreFormat requestedFormat)
{
	// Make sure we're not trying to set a cube side on a regular texture and vice versa.
	if ((side != CUBE_SIDE_NONE) != is_cubemap)
		return OpStatus::ERR;

	if (is_cubemap)
		device->setCubeTexture(0, this);
	else
		device->setTexture(0, this);
	if (rowAlignment != 4)
		glPixelStorei(GL_UNPACK_ALIGNMENT, rowAlignment);

	// Check if we need to do any of the slow paths that need to mutate the data before sending it along.
	bool slowPath = yFlip || premultiplyAlpha || requestedFormat != VPSF_SAME;
	if (slowPath)
	{
		RETURN_IF_ERROR(slowPathUpdate(w, h, pixels, rowAlignment, yFlip, premultiplyAlpha, storeFormat, requestedFormat));
	}

	unsigned int target = VEGACubeSideToGLTarget[side];

	if (!isMipLevelSet(level, side))
	{
		unsigned int mipW = MAX(1, m_width >> level);
		unsigned int mipH = MAX(1, m_height >> level);
		if (x == 0 && y == 0 && w == mipW && h == mipH)
		{
			glTexImage2D(target, level, colorMode, mipW, mipH, 0, colorMode, colorType, pixels);
		}
		else
		{
			glTexImage2D(target, level, colorMode, mipW, mipH, 0, colorMode, colorType, NULL);
			glTexSubImage2D(target, level, x, y, w, h, colorMode, colorType, pixels);
		}
		if (level == 0)
			texture_initialized  |= 1 << side;
		setMipLevel(level, side);
	}
	else
		glTexSubImage2D(target, level, x, y, w, h, colorMode, colorType, pixels);

	if (slowPath)
		OP_DELETEA((char *)pixels);

	if (rowAlignment != 4)
		glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

	return OpStatus::OK;
}

OP_STATUS VEGAGlTexture::generateMipMaps(MipMapHint hint)
{
	if (isLevel0Set())
	{
		glHint(GL_GENERATE_MIPMAP_HINT, hint == VEGA3dTexture::HINT_NICEST ? GL_NICEST : hint == VEGA3dTexture::HINT_FASTEST ? GL_FASTEST : GL_DONT_CARE);
		GLenum type = is_cubemap?GL_TEXTURE_CUBE_MAP:GL_TEXTURE_2D;
		if (is_cubemap)
			device->setCubeTexture(0, this);
		else
			device->setTexture(0, this);
		glGenerateMipmap(type);
		setAllMiplevels();
	}
	return OpStatus::OK;
}

void VEGAGlTexture::setWrapMode(WrapMode swrap, WrapMode twrap)
{
	if (swrap == this->swrap && twrap == this->twrap)
		return;
	this->swrap = swrap;
	this->twrap = twrap;
	wrapChanged = true;
}

OP_STATUS VEGAGlTexture::setFilterMode(FilterMode minFilter, FilterMode magFilter)
{
	if (this->minFilter == minFilter && this->magFilter == magFilter)
		return OpStatus::OK;
	this->minFilter = minFilter;
	this->magFilter = magFilter;
	filterChanged = true;
	return OpStatus::OK;
}

#endif // VEGA_BACKEND_OPENGL
