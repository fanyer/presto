/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch_system_includes.h"

#ifdef VEGA_BACKEND_DIRECT3D10

#include <d3d10_1.h>
// If WebGL is enabled we need both the DXSDK and the D3DCompiler header. To find out if we
// have both we include the D3DCompiler header here. If the header isn't installed we will
// fall back and include our dummy D3DCompiler header which will define NO_DXSDK. That will
// in turn disable compilation of the DX backend.
#ifdef CANVAS3D_SUPPORT
#include <D3DCompiler.h>
#endif //CANVAS3D_SUPPORT
#ifndef NO_DXSDK

#include "platforms/vega_backends/d3d10/vegad3d10texture.h"
#include "platforms/vega_backends/d3d10/vegad3d10device.h"
#include "modules/libvega/src/vegaswbuffer.h"

int VEGACubeSideToArraySlice(int i)
{
	static const int s_vegaCubeSideToArraySlice[] = { 0, 0, 1, 2, 3, 4, 5 };
	OP_ASSERT(i >= 0 && i < ARRAY_SIZE(s_vegaCubeSideToArraySlice));
	return s_vegaCubeSideToArraySlice[i];
}

VEGAD3d10Texture::VEGAD3d10Texture(unsigned int size, ColorFormat fmt, bool featureLevel9)
: m_texture(size, size, fmt, true, featureLevel9),swrap(WRAP_CLAMP_EDGE),
  twrap(WRAP_CLAMP_EDGE), m_magFilter(FILTER_LINEAR),
#ifdef VEGA_BACKEND_D2D_INTEROPERABILITY
  m_activeD2DFBO(NULL),
#endif // VEGA_BACKEND_D2D_INTEROPERABILITY
  m_bindCount(0), m_featureLevel9(featureLevel9), m_device(NULL)
{
}

VEGAD3d10Texture::VEGAD3d10Texture(unsigned int width, unsigned int height, ColorFormat fmt, bool featureLevel9)
: m_texture(width, height, fmt, false, featureLevel9),
  swrap(WRAP_CLAMP_EDGE), twrap(WRAP_CLAMP_EDGE),
  m_magFilter(FILTER_LINEAR),
#ifdef VEGA_BACKEND_D2D_INTEROPERABILITY
  m_activeD2DFBO(NULL),
#endif // VEGA_BACKEND_D2D_INTEROPERABILITY
  m_bindCount(0), m_featureLevel9(featureLevel9), m_device(NULL)
{
}

VEGAD3d10Texture::~VEGAD3d10Texture()
{
	if (m_bindCount)
	{
		static_cast<VEGAD3d10Device*>(g_vegaGlobals.vega3dDevice)->unbindTexture(this);
	}
}

OP_STATUS VEGAD3d10Texture::Construct(ID3D10Device1* dev, bool mipmaps)
{
	m_device = dev;
	return m_texture.Construct(dev, mipmaps);
}

OP_STATUS VEGAD3d10Texture::update(unsigned int x, unsigned int y, const VEGASWBuffer* pixels, int rowAlignment, CubeSide side, int level, bool yFlip, bool premultiplyAlpha)
{
#ifdef VEGA_INTERNAL_FORMAT_BGRA8888
	if (m_texture.getFormat() == FORMAT_RGBA8888 && !pixels->IsIndexed() && rowAlignment <= 4 && !yFlip && !premultiplyAlpha)
	{
		D3D10_BOX box;
		box.left = x;
		box.right = x+pixels->width;
		box.top = y;
		box.bottom = y+pixels->height;
		box.front = 0;
		box.back = 1;
		m_device->UpdateSubresource(getTextureResource(), D3D10CalcSubresource(level,VEGACubeSideToArraySlice(side),m_texture.getMipLevels()), &box, pixels->GetRawBufferPtr(), pixels->width*4, 0);
		return OpStatus::OK;
	}
#endif // VEGA_INTERNAL_FORMAT_BGRA8888

	VEGAPixelStoreFormat surfaceFormat,
	                     requestedFormat = VPSF_SAME;
	switch (m_texture.getFormat())
	{
	case FORMAT_RGB888:
		surfaceFormat = VPSF_BGRX8888;
		break;
	case FORMAT_ALPHA8:
		surfaceFormat = m_featureLevel9?VPSF_BGRA8888:VPSF_ALPHA8;
		break;
	case FORMAT_LUMINANCE8:
		surfaceFormat = VPSF_BGRX8888;
		break;
	case FORMAT_LUMINANCE8_ALPHA8:
		surfaceFormat = VPSF_BGRA8888;
		break;
	case FORMAT_RGBA8888:
		surfaceFormat = VPSF_BGRA8888;
		break;
	case FORMAT_RGBA4444:
		requestedFormat = VPSF_ABGR4444;
		surfaceFormat = VPSF_BGRA8888;
		break;
	case FORMAT_RGBA5551:
		requestedFormat = VPSF_RGBA5551;
		surfaceFormat = VPSF_BGRA8888;
		break;
	case FORMAT_RGB565:
		requestedFormat = VPSF_RGB565;
		surfaceFormat = VPSF_BGRX8888;
		break;
	default:
		OP_ASSERT(FALSE);
		return OpStatus::ERR;
		break;
	}

	VEGAPixelStoreWrapper psw;
	RETURN_IF_ERROR(psw.Allocate(surfaceFormat, pixels->width, pixels->height, rowAlignment));
	VEGAPixelStore& ps = psw.ps;
	pixels->CopyToPixelStore(&ps, yFlip, premultiplyAlpha, requestedFormat);

	D3D10_BOX box;
	box.left = x;
	box.right = x+pixels->width;
	box.top = y;
	box.bottom = y+pixels->height;
	box.front = 0;
	box.back = 1;
	m_device->UpdateSubresource(getTextureResource(), D3D10CalcSubresource(level,VEGACubeSideToArraySlice(side),m_texture.getMipLevels()), &box, ps.buffer, ps.stride, 0);

	psw.Free();
	return OpStatus::OK;
}

OP_STATUS VEGAD3d10Texture::update(unsigned int x, unsigned int y, unsigned int w, unsigned int h, UINT16* pixels, unsigned int rowAlignment, CubeSide side, int level, bool yFlip, bool premultiplyAlpha)
{
	VEGAPixelStoreFormat surfaceFormat,
	                     indataFormat = VPSF_SAME;
	switch (m_texture.getFormat())
	{
	case FORMAT_RGBA4444:
		indataFormat = VPSF_ABGR4444;
		surfaceFormat = VPSF_BGRA8888;
		break;
	case FORMAT_RGBA5551:
		indataFormat = VPSF_RGBA5551;
		surfaceFormat = VPSF_BGRA8888;
		break;
	case FORMAT_RGB565:
		indataFormat = VPSF_RGB565;
		surfaceFormat = VPSF_BGRX8888;
		break;
	default:
		return OpStatus::ERR;
	}
	return internalUpdate(x, y, w, h, pixels, rowAlignment, side, level, yFlip, premultiplyAlpha, surfaceFormat, indataFormat);
}

OP_STATUS VEGAD3d10Texture::internalUpdate(unsigned int x, unsigned int y, unsigned int w, unsigned int h, void* pixels, unsigned int rowAlignment, CubeSide side, int level, bool yFlip, bool premultiplyAlpha, VEGAPixelStoreFormat surfaceFormat, VEGAPixelStoreFormat indataFormat)
{
	// Make sure we're not trying to set a cube side on a regular m_texture and vice versa.
	if ((side != CUBE_SIDE_NONE) != m_texture.isCubeMap())
		return OpStatus::ERR;

	OpAutoArray<char> _pixels;
	// Check if we need to do any of the slow paths that need to mutate the data before sending it along.
	bool slowPath = yFlip || premultiplyAlpha || indataFormat != VPSF_SAME;
	if (slowPath)
	{
		RETURN_IF_ERROR(slowPathUpdate(w, h, pixels, rowAlignment, yFlip, premultiplyAlpha, surfaceFormat, indataFormat));
		_pixels.reset((char*)pixels);
	}

	unsigned int bytesPerRow = w * VPSF_BytesPerPixel(surfaceFormat);
	unsigned int paddedBytesPerRow = bytesPerRow;
	if (paddedBytesPerRow % rowAlignment)
		paddedBytesPerRow += rowAlignment - bytesPerRow % rowAlignment;

	D3D10_BOX box;
	box.left = x;
	box.right = x+w;
	box.top = y;
	box.bottom = y+h;
	box.front = 0;
	box.back = 1;
	m_device->UpdateSubresource(getTextureResource(), D3D10CalcSubresource(level,VEGACubeSideToArraySlice(side),m_texture.getMipLevels()), &box, pixels, paddedBytesPerRow, 0);

	return OpStatus::OK;
}

OP_STATUS VEGAD3d10Texture::update(unsigned int x, unsigned int y, unsigned int w, unsigned int h, UINT8* pixels, unsigned int rowAlignment, CubeSide side, int level, bool yFlip, bool premultiplyAlpha)
{
	if ((m_texture.getFormat() == FORMAT_ALPHA8 || m_texture.getFormat() == FORMAT_LUMINANCE8) && !m_featureLevel9)
	{
		unsigned int stride = (w+rowAlignment-1) & (~rowAlignment+1);
		D3D10_BOX box;
		box.left = x;
		box.right = x+w;
		box.top = y;
		box.bottom = y+h;
		box.front = 0;
		box.back = 1;
		m_device->UpdateSubresource(getTextureResource(), D3D10CalcSubresource(level,VEGACubeSideToArraySlice(side),m_texture.getMipLevels()), &box, pixels, stride, 0);
		return OpStatus::OK;
	}

	VEGAPixelStoreFormat surfaceFormat,
	                     indataFormat = VPSF_SAME;
	switch (m_texture.getFormat())
	{
	case FORMAT_ALPHA8:
		indataFormat = VPSF_ALPHA8;
		surfaceFormat = VPSF_BGRA8888;
		break;
	case FORMAT_LUMINANCE8:
		indataFormat = VPSF_ALPHA8;
		surfaceFormat = VPSF_BGRA8888;
		break;
	case FORMAT_LUMINANCE8_ALPHA8:
		indataFormat = VPSF_LUMINANCE8_ALPHA8;
		surfaceFormat = VPSF_BGRA8888;
		break;
	case FORMAT_RGB888:
		indataFormat = VPSF_RGB888;
		surfaceFormat = VPSF_BGRX8888;
		break;
	case FORMAT_RGBA8888:
		indataFormat = VPSF_RGBA8888;
		surfaceFormat = VPSF_BGRA8888;
		break;
	default:
		return OpStatus::ERR;
	}
	return internalUpdate(x, y, w, h, pixels, rowAlignment, side, level, yFlip, premultiplyAlpha, surfaceFormat, indataFormat);
}

OP_STATUS VEGAD3d10Texture::setFilterMode(FilterMode minFilter, FilterMode magFilter)
{
	m_magFilter = magFilter;
	return m_texture.setMinFilter(minFilter);
}

OP_STATUS VEGAD3d10Texture::generateMipMaps(MipMapHint)
{
	return m_texture.generateMipMaps();
}

#ifdef VEGA_BACKEND_D2D_INTEROPERABILITY
#include "platforms/vega_backends/d3d10/vegad3d10fbo.h"
void VEGAD3d10Texture::setActiveD2DFBO(VEGAD3d10FramebufferObject* fbo)
{
	if (fbo == m_activeD2DFBO)
		return;
	// need temp pointer to avoid endless recursion
	VEGAD3d10FramebufferObject* oldfbo = m_activeD2DFBO;
	m_activeD2DFBO = NULL;
	if (oldfbo)
		oldfbo->StopDrawingD2D();
	m_activeD2DFBO = fbo;
}
#endif // VEGA_BACKEND_D2D_INTEROPERABILITY

VEGAD3d10Texture::TextureMipWrapper::TextureMipWrapper(unsigned int w, unsigned int h, ColorFormat fmt, bool cubeMap, bool featureLevel9)
: m_width(w), m_height(h), m_format(fmt), m_mipLevels(1), texture(NULL), shaderResourceView(NULL),
  textureAlt(NULL), shaderResourceViewAlt(NULL), m_minFilter(FILTER_LINEAR),
  m_device(NULL), m_featureLevel9(featureLevel9), m_is_cubemap(cubeMap), m_mipmapped(false)
{
	m_isPOT = VEGA3dDevice::isPow2(w) && VEGA3dDevice::isPow2(h);
	m_mipLevels = VEGA3dDevice::ilog2_floor(MAX(w, h)) + 1;
}

VEGAD3d10Texture::TextureMipWrapper::~TextureMipWrapper()
{
	if (shaderResourceView)
		shaderResourceView->Release();
	if (texture)
		texture->Release();
	if (shaderResourceViewAlt)
		shaderResourceViewAlt->Release();
	if (textureAlt)
		textureAlt->Release();
}

OP_STATUS VEGAD3d10Texture::TextureMipWrapper::Construct(ID3D10Device1* dev, bool mipmaps)
{
	m_mipmapped = mipmaps && m_isPOT;
	m_device = dev;
	return ConstructInternal(m_mipmapped, &texture, &shaderResourceView);
}

OP_STATUS VEGAD3d10Texture::TextureMipWrapper::ConstructInternal(bool mipmaps, ID3D10Texture2D **tex, ID3D10ShaderResourceView **rview)
{
	if (m_width == 0 || m_height == 0)
		return OpStatus::OK;

	D3D10_TEXTURE2D_DESC texDesc;
	op_memset(&texDesc, 0, sizeof(D3D10_TEXTURE2D_DESC));
	texDesc.Width = m_width;
	texDesc.Height = m_height;
	texDesc.MipLevels = mipmaps ? m_mipLevels : 1;
	texDesc.ArraySize = m_is_cubemap ? 6 : 1;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Usage = D3D10_USAGE_DEFAULT;
	texDesc.BindFlags = D3D10_BIND_SHADER_RESOURCE|D3D10_BIND_RENDER_TARGET;
	texDesc.CPUAccessFlags = 0;
	texDesc.MiscFlags = (m_is_cubemap ?  D3D10_RESOURCE_MISC_TEXTURECUBE : 0)|(mipmaps ? D3D10_RESOURCE_MISC_GENERATE_MIPS : 0);

	switch (m_format)
	{
	case FORMAT_RGB888:
	case FORMAT_RGB565:
		texDesc.Format = DXGI_FORMAT_B8G8R8X8_UNORM;
		break;
	case FORMAT_ALPHA8:
		texDesc.Format = m_featureLevel9?DXGI_FORMAT_B8G8R8A8_UNORM:DXGI_FORMAT_A8_UNORM;
		break;
	case FORMAT_LUMINANCE8:
		// Wastes memory, but there is no luminance texture in d3d 10 and we cannot rely on shaders since the shaders can come from webgl
		texDesc.Format = DXGI_FORMAT_B8G8R8X8_UNORM;
		break;
	case FORMAT_LUMINANCE8_ALPHA8:
		// Wastes memory, but there is no luminance alpha texture in d3d 10 and we cannot rely on shaders since the shaders can come from webgl
		texDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
		break;
	case FORMAT_RGBA5551:
	case FORMAT_RGBA8888:
	case FORMAT_RGBA4444:
	default:
		texDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
		break;
	}

	if (FAILED(m_device->CreateTexture2D(&texDesc, NULL, tex)))
		return OpStatus::ERR;
	D3D10_SHADER_RESOURCE_VIEW_DESC srvDesc;
	op_memset(&srvDesc, 0, sizeof(D3D10_SHADER_RESOURCE_VIEW_DESC));
	srvDesc.Format = texDesc.Format;
	if (m_is_cubemap)
	{
		srvDesc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURECUBE;
		srvDesc.TextureCube.MostDetailedMip = 0;
		srvDesc.TextureCube.MipLevels = mipmaps ? m_mipLevels : 1;;
	}
	else
	{
		srvDesc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.MipLevels = mipmaps ? m_mipLevels : 1;;
	}
	if (FAILED(m_device->CreateShaderResourceView(*tex, &srvDesc, rview)))
	{
		return OpStatus::ERR;
	}
	return OpStatus::OK;
}

OP_STATUS VEGAD3d10Texture::TextureMipWrapper::setMinFilter(FilterMode minFilter)
{
	// If we are currently using the alt texture is depending on the min filter mode
	// and whether the original image was created mipmapped or not so we record if
	// we're currently using the alt, then set the min filter and see if we should
	// now use the other texture.
	bool usedAlt = useAlt() && textureAlt;
	m_minFilter = minFilter;
	if (usedAlt != useAlt())
	{
		// We should switch to use the other texture. If we're about to use the alt
		// texture for the first time we need to create it.
		if (useAlt() && textureAlt == NULL)
			RETURN_IF_ERROR(ConstructInternal(!m_mipmapped, &textureAlt, &shaderResourceViewAlt));
		// And then sync the data across, either to or from the alt.
		SyncAlt(useAlt());
	}

	return OpStatus::OK;
}

void VEGAD3d10Texture::TextureMipWrapper::SyncAlt(bool toAlt)
{
	D3D10_BOX box;
	box.left = 0;
	box.right = m_width;
	box.top = 0;
	box.bottom = m_height;
	box.front = 0;
	box.back = 1;
	unsigned int dstMips = getMipLevels();
	unsigned int srcMips = dstMips == 1 ? m_mipLevels : 1;
	if (m_is_cubemap)
		for (int j = 0; j < 6; ++j)
			m_device->CopySubresourceRegion(toAlt ? textureAlt : texture, D3D10CalcSubresource(0,j,dstMips), 0, 0, 0, toAlt ? texture : textureAlt, D3D10CalcSubresource(0,j,srcMips), &box);
	else
		m_device->CopySubresourceRegion(toAlt ? textureAlt : texture, D3D10CalcSubresource(0,0,dstMips), 0, 0, 0, toAlt ? texture : textureAlt, D3D10CalcSubresource(0,0,srcMips), &box);
}

ID3D10Texture2D* VEGAD3d10Texture::TextureMipWrapper::getTextureResource()
{
	return useAlt() ? textureAlt : texture;
}

ID3D10ShaderResourceView* VEGAD3d10Texture::TextureMipWrapper::getSRView(VEGAD3d10Texture*parent)
{
#ifdef VEGA_BACKEND_D2D_INTEROPERABILITY
	parent->setActiveD2DFBO(NULL);
#endif // VEGA_BACKEND_D2D_INTEROPERABILITY
	return useAlt() ? shaderResourceViewAlt : shaderResourceView;
}

OP_STATUS VEGAD3d10Texture::TextureMipWrapper::generateMipMaps()
{
	if (m_isPOT)
	{
		// If the original texture isn't mipmapped and there's no alt texture available we
		// need to create the alt texture to be mipmapped.
		if (!m_mipmapped && textureAlt == NULL)
			RETURN_IF_ERROR(ConstructInternal(true, &textureAlt, &shaderResourceViewAlt));

		// If the non-mipmapped version is currently being used...
		if (useAlt() == m_mipmapped)
			// ...we need to sync over the data to which ever is the mipmapped version...
			SyncAlt(!m_mipmapped);
		// ...and make sure to generate mipmaps for the mipmapped version.
		m_device->GenerateMips(!m_mipmapped ? shaderResourceViewAlt : shaderResourceView);
	}
	return OpStatus::OK;
}

unsigned int VEGAD3d10Texture::TextureMipWrapper::getMipLevels()
{
	bool useMipMapped = (useAlt() != m_mipmapped) && m_isPOT;
	return useMipMapped ? m_mipLevels : 1;
}

bool VEGAD3d10Texture::TextureMipWrapper::useAlt()
{
	bool isMippedFilter = m_minFilter != VEGA3dTexture::FILTER_NEAREST && m_minFilter != VEGA3dTexture::FILTER_LINEAR;
	return  (isMippedFilter != m_mipmapped) && m_isPOT;
}

#endif //NO_DXSDK
#endif // VEGA_BACKEND_DIRECT3D10

