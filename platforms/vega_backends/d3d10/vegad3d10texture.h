/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef VEGAD3D10TEXTURE_H
#define VEGAD3D10TEXTURE_H

#ifdef VEGA_BACKEND_DIRECT3D10

#include "modules/libvega/vega3ddevice.h"
#ifdef VEGA_BACKEND_D2D_INTEROPERABILITY
class VEGAD3d10FramebufferObject;
#endif // VEGA_BACKEND_D2D_INTEROPERABILITY

class VEGAD3d10Texture : public VEGA3dTexture
{
public:
	VEGAD3d10Texture(unsigned int size, ColorFormat fmt, bool featureLevel9);
	VEGAD3d10Texture(unsigned int width, unsigned int height, ColorFormat fmt, bool featureLevel9);
	~VEGAD3d10Texture();

	OP_STATUS Construct(ID3D10Device1* dev, bool mipmaps);
	virtual OP_STATUS update(unsigned int x, unsigned int y, const VEGASWBuffer* pixels, int rowAlignment, CubeSide side, int level, bool yFlip, bool premultiplyAlpha);
	virtual OP_STATUS update(unsigned int x, unsigned int y, unsigned int w, unsigned int h, UINT8* pixels, unsigned int rowAlignment, CubeSide side, int level, bool yFlip, bool premultiplyAlpha);
	virtual OP_STATUS update(unsigned int x, unsigned int y, unsigned int w, unsigned int h, UINT16* pixels, unsigned int rowAlignment, CubeSide side, int level, bool yFlip, bool premultiplyAlpha);
	virtual OP_STATUS generateMipMaps(MipMapHint hint);

	virtual unsigned int getWidth(){return m_texture.getWidth();}
	virtual unsigned int getHeight(){return m_texture.getHeight();}
	virtual ColorFormat getFormat(){return m_texture.getFormat();}

	virtual void setWrapMode(WrapMode swrap, WrapMode twrap){this->swrap = swrap; this->twrap = twrap;}
	virtual WrapMode getWrapModeS(){return swrap;}
	virtual WrapMode getWrapModeT(){return twrap;}

	virtual OP_STATUS setFilterMode(FilterMode minFilter, FilterMode magFilter);
	virtual FilterMode getMinFilter(){return m_texture.getMinFilter();}
	virtual FilterMode getMagFilter(){return m_magFilter;}

	ID3D10Texture2D* getTextureResource() { return m_texture.getTextureResource(); }
	ID3D10ShaderResourceView* getSRView() { return m_texture.getSRView(this); }
#ifdef VEGA_BACKEND_D2D_INTEROPERABILITY
	void setActiveD2DFBO(VEGAD3d10FramebufferObject* fbo);
#endif // VEGA_BACKEND_D2D_INTEROPERABILITY
	void incBindCount(){++m_bindCount;}
	void decBindCount(){--m_bindCount;}
private:
	OP_STATUS internalUpdate(unsigned int x, unsigned int y, unsigned int w, unsigned int h,
							void* pixels, unsigned int rowAlignment, CubeSide side, int level,
							bool yFlip, bool premultiplyAlpha, VEGAPixelStoreFormat storeFormat, VEGAPixelStoreFormat requestedFormat);

	/*
	 * Unlike OpenGL, DX10 need to know if mipmaps should be used at creation time. OpenGL
	 * (and hence WebGL) has min filters ignoring mipmaps which forces us to track when
	 * the filter mode gets set and use an alternate texture. The content of mip level 0
	 * is then copied across via a GPU-GPU copy.
	 * To make this transparent we wrap up the two textures and the tracking required in
	 * a class.
	 * The only time this will incur overhead is when a POT texture is created with a min
	 * filter ignoring mips and then rendered with a min filter which doesn't or the other
	 * way around.
	 */
	class TextureMipWrapper
	{
	public:
		TextureMipWrapper(unsigned int w, unsigned int h, ColorFormat fmt, bool cubeMap, bool featureLevel9);
		~TextureMipWrapper();

		OP_STATUS Construct(ID3D10Device1* dev, bool mipmaps);

		FilterMode getMinFilter() { return m_minFilter; }
		OP_STATUS setMinFilter(FilterMode minFilter);
		OP_STATUS generateMipMaps();
		ID3D10Texture2D* getTextureResource();
		ID3D10ShaderResourceView* getSRView(VEGAD3d10Texture*parent);
		unsigned int getMipLevels();

		unsigned int getWidth() { return m_width; }
		unsigned int getHeight() { return m_height; }
		ColorFormat getFormat() { return m_format; }
		bool isCubeMap() { return m_is_cubemap; }
	private:
		OP_STATUS ConstructInternal(bool mipmaps, ID3D10Texture2D **tex, ID3D10ShaderResourceView **rview);
		void SyncAlt(bool toAlt);
		bool useAlt();

		unsigned int m_width;
		unsigned int m_height;
		ColorFormat m_format;

		bool m_is_cubemap;
		bool m_mipmapped;
		bool m_isPOT;
		unsigned int m_mipLevels;	// The number of miplevels to use if m_mipmapped is true.

		FilterMode m_minFilter;
		ID3D10Device1* m_device;
		bool m_featureLevel9;

		// The originally created texture, mipmapped or not.
		ID3D10Texture2D* texture;
		ID3D10ShaderResourceView* shaderResourceView;

		// The alternative texture we copy data across to (mostly unused).
		ID3D10Texture2D* textureAlt;
		ID3D10ShaderResourceView* shaderResourceViewAlt;
	};

	TextureMipWrapper m_texture;

	WrapMode swrap;
	WrapMode twrap;
	FilterMode m_magFilter;
#ifdef VEGA_BACKEND_D2D_INTEROPERABILITY
	VEGAD3d10FramebufferObject* m_activeD2DFBO;
#endif // VEGA_BACKEND_D2D_INTEROPERABILITY
	unsigned int m_bindCount;
	ID3D10Device1* m_device;
	bool m_featureLevel9;
};

// Map VEGACubeSide enum to DX array slice param.
int VEGACubeSideToArraySlice(int i);

#endif // VEGA_BACKEND_DIRECT3D10
#endif // !VEGAD3D10TEXTURE_H

