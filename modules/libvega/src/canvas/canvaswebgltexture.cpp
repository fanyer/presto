/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#ifdef CANVAS3D_SUPPORT

#include "modules/libvega/src/canvas/canvaswebgltexture.h"
#include "modules/dom/src/canvas/webglconstants.h"

CanvasWebGLTexture::CanvasWebGLTexture() : m_minFilter(VEGA3dTexture::FILTER_NEAREST_MIPMAP_LINEAR),
	m_magFilter(VEGA3dTexture::FILTER_LINEAR), m_sWrap(VEGA3dTexture::WRAP_REPEAT),
	m_tWrap(VEGA3dTexture::WRAP_REPEAT), m_initialized2d(FALSE), m_initializedCube(FALSE),
	m_hasBeenBound(FALSE)
{
}

CanvasWebGLTexture::~CanvasWebGLTexture()
{
	m_mipChains.Clear();
	m_mipData.Clear();
}

OP_STATUS CanvasWebGLTexture::create(VEGA3dTexture::ColorFormat fmt, unsigned int width, unsigned int height, bool cube, MipChain **retMip)
{
	if ((m_initialized2d || m_initializedCube) && ((cube != m_initializedCube) || (getTexture() && fmt != getTexture()->getFormat())))
		return OpStatus::ERR;

	bool mipMappedFilter = m_minFilter != VEGA3dTexture::FILTER_NEAREST && m_minFilter != VEGA3dTexture::FILTER_LINEAR;
	VEGA3dTexture *tex = NULL;
	if (cube)
		RETURN_IF_ERROR(g_vegaGlobals.vega3dDevice->createCubeTexture(&tex, width, fmt, mipMappedFilter));
	else
		RETURN_IF_ERROR(g_vegaGlobals.vega3dDevice->createTexture(&tex, width, height, fmt, mipMappedFilter));
	tex->setWrapMode(m_sWrap, m_tWrap);
	tex->setFilterMode(m_minFilter, m_magFilter);
	MipChain *mipChain = OP_NEW(MipChain, ());
	if (mipChain == NULL)
	{
		VEGARefCount::DecRef(tex);
		return OpStatus::ERR_NO_MEMORY;
	}
	mipChain->setTexture(tex, cube);
	mipChain->Into(&m_mipChains);
	m_initialized2d = !cube;
	m_initializedCube = cube;
	if (retMip)
		*retMip = mipChain;

	return OpStatus::OK;
}

void CanvasWebGLTexture::setFilterMode(VEGA3dTexture::FilterMode minFilter, VEGA3dTexture::FilterMode magFilter)
{
	m_minFilter = minFilter;
	m_magFilter = magFilter;
	if (getTexture())
		getTexture()->setFilterMode(m_minFilter, m_magFilter);
}

void CanvasWebGLTexture::setWrapMode(VEGA3dTexture::WrapMode sWrap, VEGA3dTexture::WrapMode tWrap)
{
	m_sWrap = sWrap;
	m_tWrap = tWrap;
	if (getTexture())
		getTexture()->setWrapMode(m_sWrap, m_tWrap);
}

void CanvasWebGLTexture::generateMipMaps(unsigned int hintMode)
{
	VEGA3dTexture *tex = getTexture();
	if (tex && m_mipChains.First()->isLevel0Set())
	{
		VEGA3dTexture::MipMapHint hint = hintMode == WEBGL_NICEST ? VEGA3dTexture::HINT_NICEST : hintMode == WEBGL_FASTEST ? VEGA3dTexture::HINT_FASTEST : VEGA3dTexture::HINT_DONT_CARE;
		tex->generateMipMaps(hint);
		m_mipChains.First()->setAllMipLevels();
	}
}

OP_STATUS CanvasWebGLTexture::getMipData(unsigned int level, VEGA3dTexture::CubeSide cubeSide, CanvasWebGLTexture::MipData *&mipData)
{
	// Note that m_mipData is sorted ascending on level and cubeSide. Find the correct location.
	mipData = m_mipData.First();
	for (; mipData != NULL && (level < mipData->level || level == mipData->level && cubeSide < mipData->cubeSide); mipData = mipData->Suc()) {}

	// If there's no matching element.
	if (!mipData || mipData->level != level || mipData->cubeSide != cubeSide)
	{
		MipData *pred = mipData;

		mipData = OP_NEW(MipData, ());
		RETURN_OOM_IF_NULL(mipData);

		// Insert the mipdata at the right place, keeping the list sorted.
		if (!pred)
			mipData->Into(&m_mipData);
		else
			mipData->Precede(pred);
	}
	return OpStatus::OK;
}

CanvasWebGLTexture::MipChain *CanvasWebGLTexture::getMipChain(unsigned int width, unsigned int height, unsigned int level, VEGA3dTexture::ColorFormat colFmt)
{
	for (MipChain *mipChain = m_mipChains.First(); mipChain != NULL; mipChain = mipChain->Suc())
		if (mipChain->getMaxMipLevels() >= level &&
		    MAX(1, mipChain->getTexture()->getWidth() >> level) == width &&
		    MAX(1, mipChain->getTexture()->getHeight() >> level) == height &&
		    mipChain->getTexture()->getFormat() == colFmt)
		{
			// If it's level 0 it has to match completely, without the clamping of width and height.
			if (level != 0 || (mipChain->getTexture()->getWidth() == width && mipChain->getTexture()->getHeight() == height))
				return mipChain;
		}
	return NULL;
}

void CanvasWebGLTexture::clearMipLevel(unsigned int level, VEGA3dTexture::CubeSide cubeSide, const MipChain *dontDelete, bool isImplicit)
{
	for (MipChain *mipChain = m_mipChains.First(); mipChain != NULL;)
	{
		bool hasApplicableMipData = false;
		// If this is caused by an implicit set (i.e. mipdata application to a newly
		// created mipchain) then we shouldn't clear out miplevels on any chains
		// where mipdata can be applied.
		if (isImplicit)
		{
			for (MipData *mipData = m_mipData.First(); mipData != NULL; mipData = mipData->Suc())
			{
				if ((mipData->cubeSide != VEGA3dTexture::CUBE_SIDE_NONE) == mipChain->isCube())
				{
					if ((mipChain->getTexture()->getWidth() >> mipData->level) == mipData->width &&
						(mipChain->getTexture()->getHeight() >> mipData->level) == mipData->height)
					{
						hasApplicableMipData = true;
						break;
					}
				}
			}
		}

		// If this chain has applicable mipdata, then don't flag it as cleared.
		if (!hasApplicableMipData)
			mipChain->setMipLevel(level, cubeSide, false);

		// If the chain is now empty and not flagged as dontDelete then we don't need it any more.
		if (mipChain->isEmpty() && mipChain != dontDelete)
		{
			MipChain *empty = mipChain;
			mipChain = mipChain->Suc();
			empty->Out();
			OP_DELETE(empty);
		}
		else
			mipChain = mipChain->Suc();
	}
}

void CanvasWebGLTexture::setActiveMipChain(MipChain *mipChain)
{
	OP_ASSERT(mipChain->InList());
	mipChain->Out();
	mipChain->IntoStart(&m_mipChains);
}

OP_STATUS CanvasWebGLTexture::applyMipData(MipChain *mipChain)
{
	for (MipData *mipData = m_mipData.First(); mipData != NULL; mipData = mipData->Suc())
	{
		if ((mipData->cubeSide != VEGA3dTexture::CUBE_SIDE_NONE) == mipChain->isCube())
		{
			if ((mipChain->getTexture()->getWidth() >> mipData->level) == mipData->width &&
				(mipChain->getTexture()->getHeight() >> mipData->level) == mipData->height)
			{
				clearMipLevel(mipData->level, mipData->cubeSide, mipChain, true);
				mipChain->setMipLevel(mipData->level, mipData->cubeSide, true);
				switch (mipData->dataType)
				{
				case CanvasWebGLTexture::MipData::MIPDATA_UINT8:
					RETURN_IF_ERROR(mipChain->getTexture()->update(0, 0, mipData->width, mipData->height, (UINT8 *)mipData->data, mipData->unpackAlignment, mipData->cubeSide, mipData->level, mipData->unpackFlipY, mipData->unpackPremultiplyAlpha));
					break;
				case CanvasWebGLTexture::MipData::MIPDATA_UINT16:
					RETURN_IF_ERROR(mipChain->getTexture()->update(0, 0, mipData->width, mipData->height, (UINT16 *)mipData->data, mipData->unpackAlignment, mipData->cubeSide, mipData->level, mipData->unpackFlipY, mipData->unpackPremultiplyAlpha));
					break;
				}
			}
		}
	}
	return OpStatus::OK;
}

CanvasWebGLTexture::MipChain::~MipChain()
{
	VEGARefCount::DecRef(m_texture);
}

void CanvasWebGLTexture::MipChain::setTexture(VEGA3dTexture* tex, bool cube)
{
	m_texture = tex;
	m_isCube = cube;
	clearMipLevels();
	m_maxMipLevels = 0;

	if (tex)
	{
		unsigned int width = tex->getWidth();
		unsigned int height = tex->getHeight();
		if (VEGA3dDevice::isPow2(width) && VEGA3dDevice::isPow2(height))
		{
			unsigned size = MAX(width, height);
			while (size > 1)
			{
				++m_maxMipLevels;
				size /= 2;
			}
		}
	}
}

void CanvasWebGLTexture::MipChain::clearMipLevels()
{
	m_validMips = 0;
	m_validCubeMipPX = 0;
	m_validCubeMipNX = 0;
	m_validCubeMipPY = 0;
	m_validCubeMipNY = 0;
	m_validCubeMipPZ = 0;
	m_validCubeMipNZ = 0;
}

unsigned int *CanvasWebGLTexture::MipChain::getMipMask(VEGA3dTexture::CubeSide cubeSide)
{
	switch (cubeSide)
	{
	case VEGA3dTexture::CUBE_SIDE_POS_X:	return &m_validCubeMipPX;
	case VEGA3dTexture::CUBE_SIDE_NEG_X:	return &m_validCubeMipNX;
	case VEGA3dTexture::CUBE_SIDE_POS_Y:	return &m_validCubeMipPY;
	case VEGA3dTexture::CUBE_SIDE_NEG_Y:	return &m_validCubeMipNY;
	case VEGA3dTexture::CUBE_SIDE_POS_Z:	return &m_validCubeMipPZ;
	case VEGA3dTexture::CUBE_SIDE_NEG_Z:	return &m_validCubeMipNZ;
	default:								return &m_validMips;
	}
}

void CanvasWebGLTexture::MipChain::setMipLevel(unsigned int lvl, VEGA3dTexture::CubeSide cubeSide, bool value)
{
	if (value)
		*getMipMask(cubeSide) |= 1 << lvl;
	else
		*getMipMask(cubeSide) &= ~(1 << lvl);
}

void CanvasWebGLTexture::MipChain::setAllMipLevels()
{
	unsigned int fullMask = (1 << (m_maxMipLevels + 1)) - 1;
	if (m_isCube)
	{
		m_validCubeMipPX |= fullMask;
		m_validCubeMipNX |= fullMask;
		m_validCubeMipPY |= fullMask;
		m_validCubeMipNY |= fullMask;
		m_validCubeMipPZ |= fullMask;
		m_validCubeMipNZ |= fullMask;
	}
	else
		m_validMips |= fullMask;
}

bool CanvasWebGLTexture::MipChain::isComplete() const
{
	if (!m_texture)
		return false;
	unsigned int fullMask = (1 << (m_maxMipLevels + 1)) - 1;
	if (!m_isCube)
		return fullMask == m_validMips;
	else
		return  m_validCubeMipPX == fullMask && m_validCubeMipNX == fullMask &&
				m_validCubeMipPY == fullMask && m_validCubeMipNY == fullMask &&
				m_validCubeMipPZ == fullMask && m_validCubeMipNZ == fullMask;
}

bool CanvasWebGLTexture::MipChain::isCubeComplete() const
{
	return  (m_validCubeMipPX & 0x1) && (m_validCubeMipNX & 0x1) &&
			(m_validCubeMipPY & 0x1) && (m_validCubeMipNY & 0x1) &&
			(m_validCubeMipPZ & 0x1) && (m_validCubeMipNZ & 0x1);
}

bool CanvasWebGLTexture::MipChain::isLevel0Set() const
{
	if (!m_texture)
		return false;
	if (!m_isCube)
		return (m_validMips & 1) != 0;
	else
		return (m_validCubeMipPX & m_validCubeMipNX & m_validCubeMipPY &
		        m_validCubeMipNY & m_validCubeMipPZ & m_validCubeMipNZ & 1) != 0;
}

bool CanvasWebGLTexture::MipChain::isEmpty() const
{
	return  !m_isCube ? !m_validMips : !(m_validCubeMipPX || m_validCubeMipNX ||
			m_validCubeMipPY || m_validCubeMipNY || m_validCubeMipPZ || m_validCubeMipNZ);
}


void CanvasWebGLTexture::destroyInternal()
{
	m_mipChains.Clear();
}


#endif // CANVAS3D_SUPPORT

