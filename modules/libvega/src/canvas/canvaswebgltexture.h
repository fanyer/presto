/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef CANVASWEBGLTEXTURE_H
#define CANVASWEBGLTEXTURE_H

#ifdef CANVAS3D_SUPPORT

#include "modules/libvega/src/canvas/webglutils.h"
#include "modules/libvega/src/vegaswbuffer.h"

class CanvasWebGLTexture : public WebGLRefCounted
{
public:
	/**
	 * MipData holds data for miplevels which could belong to not yet created
	 * mipchains. I.e. dimensions 2x1, 1x2 and 1x1. These are applied to new
	 * mipchains that fit the dimension and level as they are created.
	 */
	struct MipData : public ListElement<MipData>
	{
		MipData() : data(NULL) { }
		~MipData() { OP_DELETEA(data); }

		VEGA3dTexture::ColorFormat fmt;
		VEGA3dTexture::CubeSide cubeSide;
		unsigned int level;
		unsigned int width;
		unsigned int height;
		unsigned int unpackAlignment;
		bool unpackFlipY;
		bool unpackPremultiplyAlpha;

		unsigned char *data;
		VEGASWBuffer swbuffer;

		// Type of raw data saved, either 8- or 16-bit data saved in the 'data'
		// member or 32-bit data in the 'swbuffer'.
		enum DataType
		{
			MIPDATA_UINT8,
			MIPDATA_UINT16,
			MIPDATA_SWBUFFER
		} dataType;
	};

	/**
	 * Since WebGL textures can be set to any size at any miplevel at any time
	 * and shouldn't forget or invalidate any other miplevel we need to track the
	 * state of the texture with one or more MipChain entries.
	 * Each MipChain holds its own VEGA3dTexture and info on what mip levels are
	 * set etc.
	 */
	class MipChain : public ListElement<MipChain>
	{
	public:
		MipChain() : m_isCube(FALSE), m_texture(NULL), m_validMips(0), m_maxMipLevels(0),
						m_validCubeMipPX(0), m_validCubeMipNX(0), m_validCubeMipPY(0),
						m_validCubeMipNY(0), m_validCubeMipPZ(0), m_validCubeMipNZ(0) { }
		~MipChain();

		void setTexture(VEGA3dTexture* tex, bool cubeMap);
		VEGA3dTexture* getTexture() { return m_texture; }

		void clearMipLevels();
		void setMipLevel(unsigned int lvl, VEGA3dTexture::CubeSide cubeSide, bool value);
		void setAllMipLevels();

		bool isComplete() const;
		bool isCubeComplete() const;
		bool isEmpty() const;
		bool isCube() const { return m_isCube; }
		bool isLevel0Set() const;
		unsigned int getMaxMipLevels() const { return m_maxMipLevels; }

	private:
		unsigned int *getMipMask(VEGA3dTexture::CubeSide cubeSide);

		bool m_isCube;
		VEGA3dTexture* m_texture;
		unsigned int m_validMips;      // Bitmask of valid miplevels. LSB is level 0.
		unsigned int m_maxMipLevels;
		unsigned int m_validCubeMipPX;
		unsigned int m_validCubeMipNX;
		unsigned int m_validCubeMipPY;
		unsigned int m_validCubeMipNY;
		unsigned int m_validCubeMipPZ;
		unsigned int m_validCubeMipNZ;
	};

	CanvasWebGLTexture();
	~CanvasWebGLTexture();

	OP_STATUS create(VEGA3dTexture::ColorFormat fmt, unsigned int width, unsigned int height, bool cube, MipChain **mipChain = NULL);

	void destroyInternal();

	VEGA3dTexture* getTexture(){return m_mipChains.Empty() ? NULL : m_mipChains.First()->getTexture(); }
	bool isComplete() const { return !m_mipChains.Empty() && m_mipChains.First()->isComplete(); }
	bool isCubeComplete() const { return !m_mipChains.Empty() && m_mipChains.First()->isCubeComplete(); }
	BOOL isCubeMap() const   { return m_initializedCube; }
	BOOL hasBeenBound() const     { return m_hasBeenBound; }
	BOOL isNPOT()            { return getTexture() && !(VEGA3dDevice::isPow2(getTexture()->getWidth()) && VEGA3dDevice::isPow2(getTexture()->getHeight())); }
	void setIsBound()    { m_hasBeenBound = true; }

	void setFilterMode(VEGA3dTexture::FilterMode minFilter, VEGA3dTexture::FilterMode magFilter);
	VEGA3dTexture::FilterMode getMinFilter(){return m_minFilter;}
	VEGA3dTexture::FilterMode getMagFilter(){return m_magFilter;}
	void setWrapMode(VEGA3dTexture::WrapMode sWrap, VEGA3dTexture::WrapMode tWrap);
	VEGA3dTexture::WrapMode getWrapModeS(){return m_sWrap;}
	VEGA3dTexture::WrapMode getWrapModeT(){return m_tWrap;}

	void generateMipMaps(unsigned int hintMode);

	// Get a mipdata structure for the specified level and cubeside or create one it it doesn't exist.
	OP_STATUS getMipData(unsigned int level, VEGA3dTexture::CubeSide cubeSide, MipData *&mipData);

	// Return a pointer to the matching mipchain or NULL if none exists.
	MipChain *getMipChain(unsigned int width, unsigned int height, unsigned int level, VEGA3dTexture::ColorFormat colFmt);

	// Clear the specified miplevel from all mipchains. If you pass a chain as dontDelete it'll stay
	// in the list even if it's empty. If you set isImplicit to true then levels in mipchains that have
	// matching mipdata will not be marked as cleared.
	void clearMipLevel(unsigned int level, VEGA3dTexture::CubeSide cubeSide, const MipChain *dontDelete, bool isImplicit);

	// Set the mipchain as active (i.e. it'll be used to render with).
	void setActiveMipChain(MipChain *mipChain);

	// Apply previously set up mipdata to this mipchain.
	OP_STATUS applyMipData(MipChain *mipChain);

private:
	List<MipChain> m_mipChains;
	List<MipData> m_mipData;

	VEGA3dTexture::FilterMode m_minFilter;
	VEGA3dTexture::FilterMode m_magFilter;
	VEGA3dTexture::WrapMode m_sWrap;
	VEGA3dTexture::WrapMode m_tWrap;

	bool m_initialized2d;
	bool m_initializedCube;

	bool m_hasBeenBound;
};

typedef WebGLSmartPointer<CanvasWebGLTexture> CanvasWebGLTexturePtr;

#endif //CANVAS3D_SUPPORT
#endif //CANVASWEBGLTEXTURE_H
