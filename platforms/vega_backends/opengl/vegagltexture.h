/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef VEGAGLTEXTURE_H
#define VEGAGLTEXTURE_H

#ifdef VEGA_BACKEND_OPENGL

#include "modules/libvega/vega3ddevice.h"
#include "platforms/vega_backends/opengl/vegaglfbo.h"

class VEGAGlTexture : public VEGA3dTexture, public Link
{
public:
	VEGAGlTexture(unsigned int width, unsigned int height, ColorFormat fmt);
	VEGAGlTexture(unsigned int cubeSize, ColorFormat fmt);
	~VEGAGlTexture();

	OP_STATUS Construct(VEGAGlDevice* dev);
	OP_STATUS createTexture();

	virtual OP_STATUS update(unsigned int x, unsigned int y, const VEGASWBuffer* pixels, int rowAlignment, CubeSide side, int level, bool yFlip, bool premultiplyAlpha);
	virtual OP_STATUS update(unsigned int x, unsigned int y, unsigned int w, unsigned int h,
							UINT8* pixels, unsigned int rowAlignment, CubeSide side, int level, bool yFlip, bool premultiplyAlpha);
	virtual OP_STATUS update(unsigned int x, unsigned int y, unsigned int w, unsigned int h,
							UINT16* pixels, unsigned int rowAlignment, CubeSide side, int level, bool yFlip, bool premultiplyAlpha);
	virtual OP_STATUS generateMipMaps(MipMapHint hint);

	virtual unsigned int getWidth(){return m_width;}
	virtual unsigned int getHeight(){return m_height;}
	virtual ColorFormat getFormat(){return m_format;}

	virtual void setWrapMode(WrapMode swrap, WrapMode twrap);
	virtual WrapMode getWrapModeS(){return swrap;}
	virtual WrapMode getWrapModeT(){return twrap;}

	virtual OP_STATUS setFilterMode(FilterMode minFilter, FilterMode magFilter);
	virtual FilterMode getMinFilter(){return minFilter;}
	virtual FilterMode getMagFilter(){return magFilter;}

	bool isWrapChanged(){bool ch = wrapChanged; wrapChanged = false; return ch;}
	bool isFilterChanged(){bool ch = filterChanged; filterChanged = false; return ch;}

	unsigned int getTextureId() { return texture; }

	void clearDevice();
private:
	OP_STATUS internalUpdate(unsigned int x, unsigned int y, unsigned int w, unsigned int h,
							void* pixels, unsigned int rowAlignment, CubeSide side, int level,
							unsigned int colorMode, unsigned int colorType, bool yFlip, bool premultiplyAlpha, VEGAPixelStoreFormat storeFormat, VEGAPixelStoreFormat requestedFormat);

	void setMipLevel(unsigned int level, VEGA3dTexture::CubeSide cs);
	bool isMipLevelSet(unsigned int level, VEGA3dTexture::CubeSide cs);
	bool isLevel0Set();
	void setAllMiplevels();

	UINT32 m_mipInfo[NUM_CUBE_SIDES];

	unsigned int m_width;
	unsigned int m_height;
	ColorFormat m_format;
	WrapMode swrap;
	WrapMode twrap;
	FilterMode minFilter;
	FilterMode magFilter;
	bool wrapChanged;
	bool filterChanged;
	unsigned int texture;
	unsigned int texture_initialized;
	bool is_cubemap;
	VEGAGlDevice* device;
};

#endif // VEGA_BACKEND_OPENGL
#endif // VEGAGLTEXTURE_H
