/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#if defined(VEGA_SUPPORT) && (defined(VEGA_3DDEVICE) || defined(CANVAS3D_SUPPORT))

#include "modules/libvega/vega3ddevice.h"
#include "modules/libvega/src/vegaswbuffer.h"
#include "modules/libvega/src/3ddevice/vega3dcompatindexbuffer.h"
#include "modules/prefs/prefsmanager/collections/pc_core.h"

#ifdef VEGA_BACKENDS_USE_BLOCKLIST
# include "platforms/vega_backends/vega_blocklist_file.h"
#endif // VEGA_BACKENDS_USE_BLOCKLIST

// static
OP_STATUS VEGA3dDeviceFactory::Create(VEGA3dDevice** dev, VEGA3dDevice::UseCase use_case VEGA_3D_NATIVE_WIN_DECL)
{
#ifdef PREFS_HAVE_PREFERRED_RENDERER
	unsigned int preferred_renderer = g_pccore->GetIntegerPref(PrefsCollectionCore::PreferredRenderer);
#else
	unsigned int preferred_renderer = 0;
#endif // PREFS_HAVE_PREFERRED_RENDERER

	// Set up a list over the back-end indices in prioritized order.
	const unsigned int count = DeviceTypeCount();
	if (count == 0)
		return OpStatus::ERR;

#ifdef PREFS_HAVE_PREFERRED_RENDERER
	BYTE backends[PrefsCollectionCore::DeviceCount];
#else
	BYTE backends[1];
#endif // PREFS_HAVE_PREFERRED_RENDERER
	backends[0] = preferred_renderer;

	unsigned int renderer = 0;
	for (unsigned int i = 1; i < count; ++i)
	{
		if (renderer == preferred_renderer)
			++renderer;
		backends[i] = renderer++;
	}

	OP_STATUS status = OpStatus::ERR;
	for (unsigned int i = 0; i < count;)
	{
		OP_STATUS s = Create(backends[i], dev VEGA_3D_NATIVE_WIN_PASS);
		if (OpStatus::IsSuccess(s))
		{
			s = (*dev)->ConstructDevice(use_case);
			if (OpStatus::IsSuccess(s))
				return s;
			(*dev)->Destroy();
			OP_DELETE(*dev);
			*dev = 0;
		}
		if (OpStatus::IsMemoryError(s)) // OOM is more important than generic error
			status = s;
		++i;
	}
	return status;
}

#ifdef VEGA_ENABLE_PERF_EVENTS
#include "modules/ecmascript/ecmascript.h"
#include "modules/dom/domenvironment.h"

static int DOMSetPerfMarker(ES_Value *argv, int argc, ES_Value *return_value, DOM_Environment::CallbackSecurityInfo *security_info)
{
	if (argc > 0 && argv[0].type == VALUE_STRING)
		g_vegaGlobals.vega3dDevice->SetPerfMarker(argv[0].value.string);
	return ES_FAILED;
}

static int DOMBeginPerfEvent(ES_Value *argv, int argc, ES_Value *return_value, DOM_Environment::CallbackSecurityInfo *security_info)
{
	if (argc > 0 && argv[0].type == VALUE_STRING)
		g_vegaGlobals.vega3dDevice->BeginPerfEvent(argv[0].value.string);
	return ES_FAILED;
}

static int DOMEndPerfEvent(ES_Value *argv, int argc, ES_Value *return_value, DOM_Environment::CallbackSecurityInfo *security_info)
{
	g_vegaGlobals.vega3dDevice->EndPerfEvent();
	return ES_FAILED;
}
#endif //VEGA_ENABLE_PERF_EVENTS

#ifdef CANVAS3D_SUPPORT
// See declaration
const unsigned VEGA3dShaderProgram::nComponentsForType[VEGA3dShaderProgram::N_SHDCONSTS]
    = { 1, 2, 3, 4, 1, 2, 3, 4, 1, 2, 3, 4, 4, 9, 16, 1, 1, 0 };
#endif // CANVAS3D_SUPPORT

OP_STATUS VEGA3dDevice::ConstructDevice(UseCase use_case)
{
#ifdef VEGA_ENABLE_PERF_EVENTS
	DOM_Environment::AddCallback(DOMBeginPerfEvent, DOM_Environment::OPERA_CALLBACK, "beginPerfEvent", "");
	DOM_Environment::AddCallback(DOMEndPerfEvent, DOM_Environment::OPERA_CALLBACK, "endPerfEvent", "");
	DOM_Environment::AddCallback(DOMSetPerfMarker, DOM_Environment::OPERA_CALLBACK, "setPerfMarker", "");
#endif //VEGA_ENABLE_PERF_EVENTS

#ifdef VEGA_BACKENDS_USE_BLOCKLIST
	// create blocklist
	VEGABlocklistFile bl;
	RETURN_IF_ERROR(bl.Load(GetBlocklistType()));
	OpAutoPtr<VEGABlocklistDevice::DataProvider> provider(CreateDataProvider());
	RETURN_OOM_IF_NULL(provider.get());
	VEGABlocklistFileEntry* e;
	RETURN_IF_ERROR(bl.FindMatchingEntry(provider.get(), e));
	if (e)
	{
		m_blocklistStatus2D = e->status2d;
		m_blocklistStatus3D = e->status3d;
	}
	RETURN_IF_ERROR(CheckUseCase(use_case));
	return Construct(e);
#else  // VEGA_BACKENDS_USE_BLOCKLIST
	return Construct();
#endif // VEGA_BACKENDS_USE_BLOCKLIST
}

OP_STATUS VEGA3dDevice::CheckUseCase(UseCase use_case)
{
#ifdef VEGA_BACKENDS_USE_BLOCKLIST
	switch (use_case)
	{
	case For2D:
		if (m_blocklistStatus2D != VEGABlocklistDevice::Supported)
			return OpStatus::ERR;
		break;
	case For3D:
		if (m_blocklistStatus3D != VEGABlocklistDevice::Supported)
			return OpStatus::ERR;
		break;
	case Force:
		// always use
		break;
	}
#endif // VEGA_BACKENDS_USE_BLOCKLIST
	return OpStatus::OK;
}

VEGA3dRenderState::VEGA3dRenderState() :
	m_changed(true),
		m_blendEnabled(false), m_blendOp(BLEND_OP_ADD), m_blendOpA(BLEND_OP_ADD),
		m_blendSrc(BLEND_ONE), m_blendDst(BLEND_ZERO), m_blendSrcA(BLEND_ONE), m_blendDstA(BLEND_ZERO),
		m_colorMaskR(true), m_colorMaskG(true), m_colorMaskB(true), m_colorMaskA(true),
		m_cullFaceEnabled(false), m_cullFace(FACE_BACK),
		m_depthTestEnabled(false), m_depthTest(DEPTH_LESS), m_depthWriteEnabled(true),
		m_isFrontFaceCCW(true),
		m_stencilEnabled(false), m_stencilTest(STENCIL_ALWAYS), m_stencilTestBack(STENCIL_ALWAYS),
		m_stencilReference(0), m_stencilMask(~0u), m_stencilWriteMask(~0u),
		m_stencilOpFail(STENCIL_KEEP), m_stencilOpZFail(STENCIL_KEEP), m_stencilOpPass(STENCIL_KEEP),
		m_stencilOpFailBack(STENCIL_KEEP), m_stencilOpZFailBack(STENCIL_KEEP), m_stencilOpPassBack(STENCIL_KEEP),
		m_scissorEnabled(false), m_ditherEnabled(true), m_polygonOffsetFillEnabled(false),
		m_polygonOffsetFactor(0), m_polygonOffsetUnits(0),
		m_sampleToAlphaCoverageEnabled(false), m_sampleCoverageEnabled(false),
		m_sampleCoverageInvert(false), m_sampleCoverageValue(1.0f),  m_lineWidth(1.0f)
{
	m_blendConstColor[0] = 0.f;
	m_blendConstColor[1] = 0.f;
	m_blendConstColor[2] = 0.f;
	m_blendConstColor[3] = 0.f;
}

VEGA3dRenderState::~VEGA3dRenderState()
{}


void VEGA3dRenderState::enableBlend(bool enabled)
{
	if (enabled != m_blendEnabled)
	{
		m_changed = true;
		m_blendEnabled = enabled;
	}
}

bool VEGA3dRenderState::isBlendEnabled()
{
	return m_blendEnabled;
}

void VEGA3dRenderState::setBlendColor(float r, float g, float b, float a)
{
	if (m_blendConstColor[0] != r || m_blendConstColor[1] != g || m_blendConstColor[2] != b || m_blendConstColor[3] != a)
	{
		m_changed = true;
		m_blendConstColor[0] = r;
		m_blendConstColor[1] = g;
		m_blendConstColor[2] = b;
		m_blendConstColor[3] = a;
	}
}

void VEGA3dRenderState::getBlendColor(float& r, float& g, float& b, float& a)
{
	r = m_blendConstColor[0];
	g = m_blendConstColor[1];
	b = m_blendConstColor[2];
	a = m_blendConstColor[3];
}

bool VEGA3dRenderState::blendColorChanged(const VEGA3dRenderState& current)
{
	return m_blendConstColor[0] != current.m_blendConstColor[0] ||
		m_blendConstColor[1] != current.m_blendConstColor[1] ||
		m_blendConstColor[2] != current.m_blendConstColor[2] ||
		m_blendConstColor[3] != current.m_blendConstColor[3];
}

void VEGA3dRenderState::setBlendEquation(BlendOp op, BlendOp opA)
{
	if (opA >= NUM_BLEND_OPS)
		opA = op;
	if (op != m_blendOp || opA != m_blendOpA)
	{
		m_changed = true;
		m_blendOp = op;
		m_blendOpA = opA;
	}
}

void VEGA3dRenderState::getBlendEquation(BlendOp& op, BlendOp& opA)
{
	op = m_blendOp;
	opA = m_blendOpA;
}
bool VEGA3dRenderState::blendEquationChanged(const VEGA3dRenderState& current)
{
	return m_blendOp != current.m_blendOp || m_blendOpA != current.m_blendOpA;
}

void VEGA3dRenderState::setBlendFunc(BlendWeight src, BlendWeight dst, BlendWeight srcA, BlendWeight dstA)
{
	if (srcA >= NUM_BLEND_WEIGHTS)
		srcA = src;
	if (dstA >= NUM_BLEND_WEIGHTS)
		dstA = dst;
	if (src != m_blendSrc || dst != m_blendDst || srcA != m_blendSrcA || dstA != m_blendDstA)
	{
		m_changed = true;
		m_blendSrc = src;
		m_blendDst = dst;
		m_blendSrcA = srcA;
		m_blendDstA = dstA;
	}
}

void VEGA3dRenderState::getBlendFunc(BlendWeight& src, BlendWeight& dst, BlendWeight& srcA, BlendWeight& dstA)
{
	src = m_blendSrc;
	dst = m_blendDst;
	srcA = m_blendSrcA;
	dstA = m_blendDstA;
}

bool VEGA3dRenderState::blendFuncChanged(const VEGA3dRenderState& current)
{
	return m_blendSrc != current.m_blendSrc || m_blendDst != current.m_blendDst ||
		m_blendSrcA != current.m_blendSrcA || m_blendDstA != current.m_blendDstA;
}

bool VEGA3dRenderState::blendChanged(const VEGA3dRenderState& current)
{
	if (!m_blendEnabled)
		return false;
	return blendEquationChanged(current) || blendFuncChanged(current) || blendColorChanged(current);
}

void VEGA3dRenderState::setColorMask(bool r, bool g, bool b, bool a)
{
	if (r != m_colorMaskR || g != m_colorMaskG || b != m_colorMaskB || a != m_colorMaskA)
	{
		m_changed = true;
		m_colorMaskR = r;
		m_colorMaskG = g;
		m_colorMaskB = b;
		m_colorMaskA = a;
	}
}

void VEGA3dRenderState::getColorMask(bool& r, bool& g, bool& b, bool& a)
{
	r = m_colorMaskR;
	g = m_colorMaskG;
	b = m_colorMaskB;
	a = m_colorMaskA;
}

bool VEGA3dRenderState::colorMaskChanged(const VEGA3dRenderState& current)
{
	return m_colorMaskR != current.m_colorMaskR || m_colorMaskG != current.m_colorMaskG || 
		m_colorMaskB != current.m_colorMaskB || m_colorMaskA != current.m_colorMaskA;
}

void VEGA3dRenderState::enableCullFace(bool enabled)
{
	if (m_cullFaceEnabled != enabled)
	{
		m_changed = true;
		m_cullFaceEnabled = enabled;
	}
}

bool VEGA3dRenderState::isCullFaceEnabled()
{
	return m_cullFaceEnabled;
}

void VEGA3dRenderState::setCullFace(Face face)
{
	if (m_cullFace != face)
	{
		m_changed = true;
		m_cullFace = face;
	}
}

void VEGA3dRenderState::getCullFace(Face& face)
{
	face = m_cullFace;
}

bool VEGA3dRenderState::cullFaceChanged(const VEGA3dRenderState& current)
{
	return m_cullFaceEnabled && m_cullFace != current.m_cullFace;
}

void VEGA3dRenderState::enableDepthTest(bool enabled)
{
	if (enabled != m_depthTestEnabled)
	{
		m_changed = true;
		m_depthTestEnabled = enabled;
	}
}

bool VEGA3dRenderState::isDepthTestEnabled()
{
	return m_depthTestEnabled;
}

void VEGA3dRenderState::setDepthFunc(DepthFunc test)
{
	if (test != m_depthTest)
	{
		m_changed = true;
		m_depthTest = test;
	}
}

void VEGA3dRenderState::getDepthFunc(DepthFunc& test)
{
	test = m_depthTest;
}

bool VEGA3dRenderState::depthFuncChanged(const VEGA3dRenderState& current)
{
	return m_depthTestEnabled && m_depthTest != current.m_depthTest;
}

void VEGA3dRenderState::enableDepthWrite(bool write)
{
	if (write != m_depthWriteEnabled)
	{
		m_changed = true;
		m_depthWriteEnabled = write;
	}
}

bool VEGA3dRenderState::isDepthWriteEnabled()
{
	return m_depthWriteEnabled;
}

void VEGA3dRenderState::enableDither(bool enable)
{
	if (enable != m_ditherEnabled)
	{
		m_changed = true;
		m_ditherEnabled = enable;
	}
}

bool VEGA3dRenderState::isDitherEnabled()
{
	return m_ditherEnabled;
}

void VEGA3dRenderState::enablePolygonOffsetFill(bool enable)
{
	if (enable != m_polygonOffsetFillEnabled)
	{
		m_changed = true;
		m_polygonOffsetFillEnabled = enable;
	}
}

bool VEGA3dRenderState::isPolygonOffsetFillEnabled()
{
	return m_polygonOffsetFillEnabled;
}

bool VEGA3dRenderState::polygonOffsetChanged(const VEGA3dRenderState& current) const
{
	return m_polygonOffsetFactor != current.m_polygonOffsetFactor ||
	       m_polygonOffsetUnits != current.m_polygonOffsetUnits;
}

void VEGA3dRenderState::enableSampleToAlphaCoverage(bool enable)
{
	if (enable != m_sampleToAlphaCoverageEnabled)
	{
		m_changed = true;
		m_sampleToAlphaCoverageEnabled = enable;
	}
}

bool VEGA3dRenderState::isSampleToAlphaCoverageEnabled()
{
	return m_sampleToAlphaCoverageEnabled;
}

void VEGA3dRenderState::enableSampleCoverage(bool enable)
{
	if (enable != m_sampleCoverageEnabled)
	{
		m_changed = true;
		m_sampleCoverageEnabled = enable;
	}
}

bool VEGA3dRenderState::isSampleCoverageEnabled()
{
	return m_sampleCoverageEnabled;
}

void VEGA3dRenderState::lineWidth(float w)
{
	if (w != m_lineWidth)
	{
		m_changed = true;
		m_lineWidth = w;
	}
}

float VEGA3dRenderState::getLineWidth()
{
	return m_lineWidth;
}

float VEGA3dRenderState::getPolygonOffsetFactor()
{
	return m_polygonOffsetFactor;
}

float VEGA3dRenderState::getPolygonOffsetUnits()
{
	return m_polygonOffsetUnits;
}

bool VEGA3dRenderState::getSampleCoverageInvert()
{
	return m_sampleCoverageInvert;
}

float VEGA3dRenderState::getSampleCoverageValue()
{
	return m_sampleCoverageValue;
}

void VEGA3dRenderState::polygonOffset(float factor, float units)
{
	if (factor != m_polygonOffsetFactor || units != m_polygonOffsetUnits)
	{
		m_changed = true;
		m_polygonOffsetFactor = factor;
		m_polygonOffsetUnits = units;
	}
}

void VEGA3dRenderState::sampleCoverage(bool invert, float value)
{
	if (invert != m_sampleCoverageInvert || value != m_sampleCoverageValue)
	{
		m_changed = true;
		m_sampleCoverageInvert = invert;
		m_sampleCoverageValue = value;
	}
}

void VEGA3dRenderState::setFrontFaceCCW(bool front)
{
	if (front != m_isFrontFaceCCW)
	{
		m_changed = true;
		m_isFrontFaceCCW = front;
	}
}

bool VEGA3dRenderState::isFrontFaceCCW()
{
	return m_isFrontFaceCCW;
}

void VEGA3dRenderState::enableStencil(bool enabled)
{
	if (enabled != m_stencilEnabled)
	{
		m_changed = true;
		m_stencilEnabled = enabled;
	}
}

bool VEGA3dRenderState::isStencilEnabled()
{
	return m_stencilEnabled;
}

void VEGA3dRenderState::setStencilFunc(StencilFunc func, StencilFunc funcBack, unsigned int reference, unsigned int mask)
{
	if (func != m_stencilTest || funcBack != m_stencilTestBack || reference != m_stencilReference || mask != m_stencilMask)
	{
		m_changed = true;
		m_stencilTest = func;
		m_stencilTestBack = funcBack;
		m_stencilReference = reference;
		m_stencilMask = mask;
	}
}

void VEGA3dRenderState::getStencilFunc(StencilFunc& func, StencilFunc& funcBack, unsigned int& reference, unsigned int& mask)
{
	func = m_stencilTest;
	funcBack = m_stencilTestBack;
	reference = m_stencilReference;
	mask = m_stencilMask;
}

void VEGA3dRenderState::setStencilWriteMask(unsigned int mask)
{
	if (mask != m_stencilWriteMask)
	{
		m_changed = true;
		m_stencilWriteMask = mask;
	}
}

void VEGA3dRenderState::getStencilWriteMask(unsigned int& mask)
{
	mask = m_stencilWriteMask;
}

void VEGA3dRenderState::setStencilOp(StencilOp fail, StencilOp zFail, StencilOp pass,
									 StencilOp failBack, StencilOp zFailBack, StencilOp passBack)
{
	if (failBack >= NUM_STENCIL_OPS)
		failBack = fail;
	if (zFailBack >= NUM_STENCIL_OPS)
		zFailBack = zFail;
	if (passBack >= NUM_STENCIL_OPS)
		passBack = pass;
	if (fail != m_stencilOpFail || zFail != m_stencilOpZFail || pass != m_stencilOpPass ||
		failBack != m_stencilOpFailBack || zFailBack != m_stencilOpZFailBack || passBack != m_stencilOpPassBack)
	{
		m_changed = true;
		m_stencilOpFail = fail;
		m_stencilOpZFail = zFail;
		m_stencilOpPass = pass;
		m_stencilOpFailBack = failBack;
		m_stencilOpZFailBack = zFailBack;
		m_stencilOpPassBack = passBack;
	}
}

void VEGA3dRenderState::getStencilOp(StencilOp& fail, StencilOp& zFail, StencilOp& pass, 
									 StencilOp& failBack, StencilOp& zFailBack, StencilOp& passBack)
{
	fail = m_stencilOpFail;
	zFail = m_stencilOpZFail;
	pass = m_stencilOpPass;
	failBack = m_stencilOpFailBack;
	zFailBack = m_stencilOpZFailBack;
	passBack = m_stencilOpPassBack;
}

bool VEGA3dRenderState::stencilChanged(const VEGA3dRenderState& current)
{
	if (!m_stencilEnabled)
		return false;
	return m_stencilTest != current.m_stencilTest || m_stencilTestBack != current.m_stencilTestBack || 
		m_stencilReference != current.m_stencilReference || m_stencilMask != current.m_stencilMask || 
		m_stencilWriteMask != current.m_stencilWriteMask || m_stencilOpFail != current.m_stencilOpFail ||
		m_stencilOpZFail != current.m_stencilOpZFail || m_stencilOpPass != current.m_stencilOpPass ||
		m_stencilOpFailBack != current.m_stencilOpFailBack || m_stencilOpZFailBack != current.m_stencilOpZFailBack || 
		m_stencilOpPassBack != current.m_stencilOpPassBack;
}

void VEGA3dRenderState::enableScissor(bool enabled)
{
	if (enabled != m_scissorEnabled)
	{
		m_changed = true;
		m_scissorEnabled = enabled;
	}
}
bool VEGA3dRenderState::isScissorEnabled()
{
	return m_scissorEnabled;
}

OP_STATUS VEGA3dShaderProgram::setOrthogonalProjection()
{
	VEGA3dDevice* dev = g_vegaGlobals.vega3dDevice;
	VEGA3dRenderTarget* rt = dev->getRenderTarget();
	if (!rt)
		return OpStatus::ERR;

	return setOrthogonalProjection(rt->getWidth(), rt->getHeight());
}

OP_STATUS VEGA3dShaderProgram::setOrthogonalProjection(unsigned int width, unsigned int height)
{
	VEGA3dDevice* dev = g_vegaGlobals.vega3dDevice;
	if (!orthogonalProjectionNeedsUpdate() &&
	    m_orthoWidth  >= 0 && width  == static_cast<unsigned int>(m_orthoWidth) &&
	    m_orthoHeight >= 0 && height == static_cast<unsigned int>(m_orthoHeight))
		return OpStatus::OK;

	VEGATransform3d trans;
	dev->loadOrthogonalProjection(trans, width, height);
	m_orthoWidth  = width;
	m_orthoHeight = height;
	if (m_worldProjLocation < 0)
		m_worldProjLocation = getConstantLocation("worldProjMatrix");
	return setMatrix4(m_worldProjLocation, &trans[0], 1, true);
}



VEGA3dCompatIndexBuffer::~VEGA3dCompatIndexBuffer()
{
	VEGARefCount::DecRef(m_buffer);
	OP_DELETEA(m_cache);
}

void VEGA3dCompatIndexBuffer::Destroy()
{
	VEGARefCount::DecRef(m_buffer);
	OP_DELETEA(m_cache);
	m_buffer = NULL;
	m_cache = NULL;
}

OP_STATUS VEGA3dCompatIndexBuffer::GetIndexBuffer(VEGA3dDevice *device, const VEGA3dBuffer *&buffer, CompatType type, unsigned int numVertices, unsigned int &numIndices, const unsigned char *indices, unsigned int bytesPerIndex, unsigned int offset)
{
	// Only allow rewrites of byte and short indices and always generate short indices.
	OP_ASSERT(bytesPerIndex == 1 || bytesPerIndex == 2);

	// Calculate the number of indices that will be required.
	if (indices == NULL)
		numIndices = numVertices;
	if (type == LINE_LOOP)
		numIndices = numIndices + 1;
	else
		numIndices = (numIndices - 2) * 3;

	// Make sure we have a large enough cache and index buffer.
	if (m_buffer && m_buffer->getSize() < numIndices * sizeof(UINT16))
	{
		VEGARefCount::DecRef(m_buffer);
		m_buffer = NULL;
		OP_DELETEA(m_cache);
	}
	if (m_buffer == NULL)
	{
		RETURN_IF_ERROR(device->createBuffer(&m_buffer, numIndices * sizeof(UINT16), VEGA3dBuffer::DYNAMIC, true));
		m_cache = OP_NEWA(UINT16, numIndices);
		if (!m_cache)
		{
			VEGARefCount::DecRef(m_buffer);
			m_buffer = NULL;
			return OpStatus::ERR_NO_MEMORY;
		}
		m_state = CACHE_INVALID;
	}

	if (indices == NULL)
	{
		// Set the state to invalid to change the buffer instead of appending to it.
		if (m_currOffset != offset)
			m_state = CACHE_INVALID;
		else if (((type == LINE_LOOP && m_state == CACHE_LINE_LOOP) || (type == TRI_FAN && m_state == CACHE_FAN)) && m_currLength >= numIndices)
		{
			buffer = m_buffer;
			return OpStatus::OK;
		}

		// Restore the part of the cache (if any) that isn't set right.
		if (type == LINE_LOOP)
		{
			for (unsigned int i = m_state != CACHE_LINE_LOOP ? 0 : m_currLength; i < numIndices - 1; ++i)
				m_cache[i] = offset + i;
			m_cache[numIndices - 1] = offset;
			m_currLength = numIndices;
			m_currOffset = offset;
			m_state = CACHE_LINE_LOOP;
		}
		else
		{
			for (unsigned int i = m_state != CACHE_FAN ? 0 : m_currLength; i < numIndices; i += 3)
			{
				m_cache[i] = offset;
				m_cache[i + 1] = offset + i / 3 + 1;
				m_cache[i + 2] = offset + i / 3 + 2;
			}

			m_currLength = numIndices;
			m_currOffset = offset;
			m_state = CACHE_FAN;
		}
	}
	else
	{
		// Generate indices.
		if (type == LINE_LOOP)
		{
			if (bytesPerIndex == 1)
			{
				for (unsigned int i = 0; i < numIndices - 1; ++i)
					m_cache[i] = indices[offset + i];
				m_cache[numIndices - 1] = indices[offset + 0];
			}
			else
			{
				const unsigned short *shortindices = (const unsigned short *)indices;
				for (unsigned int i = 0; i < numIndices - 1; ++i)
					m_cache[i] = shortindices[offset + i];
				m_cache[numIndices - 1] = shortindices[offset + 0];
			}
			m_currLength = numIndices;
			m_state = CACHE_INVALID;
		}
		else
		{
			if (bytesPerIndex == 1)
			{
				for (unsigned int i = 0; i < numIndices; i += 3)
				{
					m_cache[i] = indices[offset + 0];
					m_cache[i + 1] = indices[offset + i / 3 + 1];
					m_cache[i + 2] = indices[offset + i / 3 + 2];
				}
			}
			else
			{
				const unsigned short *shortindices = (const unsigned short *)indices;
				for (unsigned int i = 0; i < numIndices; i += 3)
				{
					m_cache[i] = shortindices[offset + 0];
					m_cache[i + 1] = shortindices[offset + i / 3 + 1];
					m_cache[i + 2] = shortindices[offset + i / 3 + 2];
				}
			}
			m_currLength = numIndices;
			m_state = CACHE_INVALID;
		}
	}

	buffer = m_buffer;
	return m_buffer->writeAtOffset(0, m_currLength*sizeof(UINT16), m_cache);
}

VEGA3dDevice::VEGA3dDevice() : m_tempBuffer(NULL), m_ibuffer(NULL), m_tempTexture(NULL), m_tempFramebuffer(NULL),
	m_alphaTexture(NULL), m_alphaTextureBuffer(NULL),
	m_tempTexture2(NULL), m_tempFramebuffer2(NULL), m_2dVertexLayout(NULL),
	m_default2dRenderState(NULL), m_default2dNoBlendRenderState(NULL), m_default2dNoBlendNoScissorRenderState(NULL)
	, m_triangleIndexBuffer(NULL), m_triangleIndices(NULL)
#ifdef VEGA_BACKENDS_USE_BLOCKLIST
	, m_blocklistStatus2D(VEGABlocklistDevice::Supported), m_blocklistStatus3D(VEGABlocklistDevice::Supported)
#endif // VEGA_BACKENDS_USE_BLOCKLIST
{}

VEGA3dDevice::~VEGA3dDevice()
{
	m_renderStateStack.DeleteAll();
}

OP_STATUS VEGA3dDevice::createDefault2dObjects()
{
	unsigned int maxVerts = getVertexBufferSize();
	unsigned int maxQuads = maxVerts / 4;
	if (!m_tempBuffer)
	{
		RETURN_IF_ERROR(createBuffer(&m_tempBuffer, maxVerts*sizeof(Vega2dVertex), VEGA3dBuffer::STREAM_DISCARD, false));
	}
	if (!m_ibuffer)
	{
		RETURN_IF_ERROR(createBuffer(&m_ibuffer, 2*6*maxQuads, VEGA3dBuffer::STATIC, true));
		unsigned short *temp = OP_NEWA(unsigned short, 6*maxQuads);
		if (!temp)
		{
			VEGARefCount::DecRef(m_ibuffer);
			m_ibuffer = NULL;
			return OpStatus::ERR_NO_MEMORY;
		}
		ANCHOR_ARRAY(unsigned short, temp);
		for (unsigned int i = 0; i < maxQuads; ++i)
		{
			temp[i*6] = i*4;
			temp[i*6+1] = i*4+1;
			temp[i*6+2] = i*4+2;

			temp[i*6+3] = i*4;
			temp[i*6+4] = i*4+2;
			temp[i*6+5] = i*4+3;
		}
		m_ibuffer->writeAtOffset(0, 2*6*maxQuads, temp);
	}
	if (!m_2dVertexLayout)
	{
		VEGA3dShaderProgram* sprog;
		RETURN_IF_ERROR(createShaderProgram(&sprog, VEGA3dShaderProgram::SHADER_VECTOR2D, VEGA3dShaderProgram::WRAP_CLAMP_CLAMP));
		RETURN_IF_ERROR(createVertexLayout(&m_2dVertexLayout, sprog));
		RETURN_IF_ERROR(m_2dVertexLayout->addComponent(m_tempBuffer, 0, 0, sizeof(Vega2dVertex), VEGA3dVertexLayout::FLOAT2, false));
		RETURN_IF_ERROR(m_2dVertexLayout->addComponent(m_tempBuffer, 1, 8, sizeof(Vega2dVertex), VEGA3dVertexLayout::FLOAT2, false));
		RETURN_IF_ERROR(m_2dVertexLayout->addComponent(m_tempBuffer, 2, 16, sizeof(Vega2dVertex), VEGA3dVertexLayout::UBYTE4, true));
		VEGARefCount::DecRef(sprog);
	}
	if (!m_default2dRenderState)
	{
		RETURN_IF_ERROR(createRenderState(&m_default2dRenderState, false));
		m_default2dRenderState->setBlendFunc(VEGA3dRenderState::BLEND_ONE, VEGA3dRenderState::BLEND_ONE_MINUS_SRC_ALPHA);
		m_default2dRenderState->enableBlend(true);
		m_default2dRenderState->enableScissor(true);
	}
	if (!m_default2dNoBlendRenderState)
	{
		RETURN_IF_ERROR(createRenderState(&m_default2dNoBlendRenderState, false));
		m_default2dNoBlendRenderState->enableScissor(true);
	}
	if (!m_default2dNoBlendNoScissorRenderState)
	{
		RETURN_IF_ERROR(createRenderState(&m_default2dNoBlendNoScissorRenderState, false));
	}
	if (!m_triangleIndices)
	{
		const size_t numTriangleIndices = 3*(maxVerts-2);
		RETURN_OOM_IF_NULL(m_triangleIndices = OP_NEWA(unsigned short, numTriangleIndices));
		const OP_STATUS s = createBuffer(&m_triangleIndexBuffer,
		                                 numTriangleIndices*sizeof(*m_triangleIndices),
		                                 VEGA3dBuffer::STREAM_DISCARD,
		                                 true);
		if (OpStatus::IsError(s))
		{
			OP_DELETEA(m_triangleIndices);
			m_triangleIndices = NULL;
			return s;
		}
	}
	return OpStatus::OK;
}

void VEGA3dDevice::destroyDefault2dObjects()
{
	VEGARefCount::DecRef(m_alphaTexture);
	OP_DELETEA(m_alphaTextureBuffer);
	m_alphaTextureBuffer = NULL;
	VEGARefCount::DecRef(m_tempBuffer);
	m_tempBuffer = NULL;
	VEGARefCount::DecRef(m_ibuffer);
	m_ibuffer = NULL;
	VEGARefCount::DecRef(m_tempTexture);
	m_tempTexture = NULL;
	VEGARefCount::DecRef(m_tempFramebuffer);
	m_tempFramebuffer = NULL;
	VEGARefCount::DecRef(m_tempTexture2);
	m_tempTexture2 = NULL;
	VEGARefCount::DecRef(m_tempFramebuffer2);
	m_tempFramebuffer2 = NULL;
	VEGARefCount::DecRef(m_2dVertexLayout);
	m_2dVertexLayout = NULL;
	OP_DELETE(m_default2dRenderState);
	m_default2dRenderState = NULL;
	OP_DELETE(m_default2dNoBlendRenderState);
	m_default2dNoBlendRenderState = NULL;
	OP_DELETE(m_default2dNoBlendNoScissorRenderState);
	m_default2dNoBlendNoScissorRenderState = NULL;
	VEGARefCount::DecRef(m_triangleIndexBuffer);
	m_triangleIndexBuffer = NULL;
	OP_DELETEA(m_triangleIndices);
	m_triangleIndices = NULL;
}

VEGA3dBuffer* VEGA3dDevice::getTempBuffer(unsigned int size)
{
	return (size <= getVertexBufferSize()*sizeof(Vega2dVertex))?m_tempBuffer:NULL;
}

VEGA3dBuffer* VEGA3dDevice::getQuadIndexBuffer(unsigned int numvert)
{
	return (numvert <= 6*(getVertexBufferSize()/4))?m_ibuffer:NULL;
}

VEGA3dTexture* VEGA3dDevice::getTempTexture(unsigned int minWidth, unsigned int minHeight)
{
	if (m_tempTexture)
	{
		unsigned int width = m_tempTexture->getWidth();
		unsigned int height = m_tempTexture->getHeight();

		if (width > minWidth)
			minWidth = width;
		if (height > minHeight)
			minHeight = height;
		if (width < minWidth || height < minHeight)
		{
			VEGARefCount::DecRef(m_tempTexture);
			m_tempTexture = NULL;
		}
	}
	if (!m_tempFramebuffer)
		if (OpStatus::IsError(createFramebuffer(&m_tempFramebuffer)))
			return NULL;
	if (!m_tempTexture)
	{
		VEGA3dRenderbufferObject* sten;
		if (OpStatus::IsError(createRenderbuffer(&sten, minWidth, minHeight, VEGA3dRenderbufferObject::FORMAT_STENCIL8, 0)))
			return NULL;
		OP_STATUS status = m_tempFramebuffer->attachStencil(sten);
		VEGARefCount::DecRef(sten);
		if (OpStatus::IsError(status) || OpStatus::IsError(createTexture(&m_tempTexture, minWidth, minHeight, VEGA3dTexture::FORMAT_RGBA8888)))
			return NULL;
		if (OpStatus::IsError(m_tempFramebuffer->attachColor(m_tempTexture)))
		{
			VEGARefCount::DecRef(m_tempTexture);
			m_tempTexture = NULL;
			return NULL;
		}
		// Clear the newly created texture
		VEGA3dRenderTarget* oldrt = getRenderTarget();
		setRenderTarget(m_tempFramebuffer);
		VEGA3dRenderState* oldstate;
		status = createRenderState(&oldstate, true);
		if (OpStatus::IsError(status))
		{
			VEGARefCount::DecRef(m_tempTexture);
			m_tempTexture = NULL;
			return NULL;
		}
		setRenderState(getDefault2dNoBlendNoScissorRenderState());
		clear(true, true, true, 0, 1.f, 0);
		setRenderTarget(oldrt);
		setRenderState(oldstate);
		OP_DELETE(oldstate);
	}
	return m_tempTexture;
}

VEGA3dFramebufferObject* VEGA3dDevice::getTempTextureRenderTarget()
{
	return m_tempFramebuffer;
}

VEGA3dTexture* VEGA3dDevice::getAlphaTexture()
{
	if (!m_alphaTexture)
	{
		m_alphaTextureBuffer = OP_NEWA(UINT8, VEGA_ALPHA_TEXTURE_SIZE*VEGA_ALPHA_TEXTURE_SIZE);
		if (!m_alphaTextureBuffer)
			return NULL;
		if (OpStatus::IsError(createTexture(&m_alphaTexture, VEGA_ALPHA_TEXTURE_SIZE, VEGA_ALPHA_TEXTURE_SIZE, VEGA3dTexture::FORMAT_ALPHA8, false, false)))
		{
			OP_DELETEA(m_alphaTextureBuffer);
			m_alphaTextureBuffer = NULL;
			return NULL;
		}
	}
	return m_alphaTexture;
}

UINT8* VEGA3dDevice::getAlphaTextureBuffer()
{
	if (!getAlphaTexture())
		return NULL;
	return m_alphaTextureBuffer;
}

VEGA3dTexture* VEGA3dDevice::createTempTexture2(unsigned int minWidth, unsigned int minHeight)
{
	if (m_tempTexture2)
	{
		unsigned int width = m_tempTexture2->getWidth();
		unsigned int height = m_tempTexture2->getHeight();

		if (width >= minWidth && height >= minHeight)
		{
			VEGARefCount::IncRef(m_tempTexture2);
			return m_tempTexture2;
		}
		if (minWidth < width)
			minWidth = width;
		if (minHeight < height)
			minHeight = height;
		VEGARefCount::DecRef(m_tempTexture2);
		m_tempTexture2 = NULL;
		if (m_tempFramebuffer2)
			m_tempFramebuffer2->attachColor((VEGA3dTexture*)NULL);
	}
	if (OpStatus::IsError(createTexture(&m_tempTexture2, minWidth, minHeight, VEGA3dTexture::FORMAT_RGBA8888)))
		return NULL;
	if (m_tempFramebuffer2 && OpStatus::IsError(m_tempFramebuffer2->attachColor(m_tempTexture2)))
	{
		VEGARefCount::DecRef(m_tempFramebuffer2);
		m_tempFramebuffer2 = NULL;
	}
	VEGARefCount::IncRef(m_tempTexture2);
	return m_tempTexture2;
}

VEGA3dFramebufferObject* VEGA3dDevice::createTempTexture2RenderTarget()
{
	if (!m_tempFramebuffer2)
	{
		if (OpStatus::IsError(createFramebuffer(&m_tempFramebuffer2)))
			return NULL;
		if (m_tempTexture2 && OpStatus::IsError(m_tempFramebuffer2->attachColor(m_tempTexture2)))
		{
			VEGARefCount::DecRef(m_tempFramebuffer2);
			m_tempFramebuffer2 = NULL;
			return NULL;
		}
	}
	VEGARefCount::IncRef(m_tempFramebuffer2);
	return m_tempFramebuffer2;
}


OP_STATUS VEGA3dDevice::createVega2dVertexLayout(VEGA3dVertexLayout** layout, VEGA3dShaderProgram::ShaderType shdtype)
{
	VEGARefCount::IncRef(m_2dVertexLayout);
	*layout = m_2dVertexLayout;
	return OpStatus::OK;
}

OP_STATUS VEGA3dTexture::slowPathUpdate(unsigned int w, unsigned int h, void *&pixels, unsigned int rowAlignment, bool yFlip, bool premultiplyAlpha, VEGAPixelStoreFormat destinationFormat, VEGAPixelStoreFormat indataFormat)
{
	// Calculate the row length including padding to get the proper row alignment.
	unsigned int rowLen = w * VPSF_BytesPerPixel(indataFormat == VPSF_SAME ? destinationFormat : indataFormat);
	unsigned int paddedRowLen = rowLen;
	if (rowLen % rowAlignment)
		paddedRowLen = (rowLen / rowAlignment + 1) * rowAlignment;

	// In some instances the target row length including padding will be different than the source
	// i.e. when we're backing a RGBA4444 texture with a RGBA8888 texture etc.
	unsigned int tgtRowLen = w * VPSF_BytesPerPixel(destinationFormat);
	unsigned int tgtPaddedRowLen = tgtRowLen;
	if (tgtRowLen % rowAlignment)
		tgtPaddedRowLen = (tgtRowLen / rowAlignment + 1) * rowAlignment;

	// Allocate some working memory.
	VEGAPixelStoreWrapper psw(w, h, destinationFormat, tgtRowLen, pixels);
	RETURN_IF_ERROR(psw.Allocate(destinationFormat, w, h, rowAlignment));
	VEGAPixelStore& ps = psw.ps;

	// If a RGBA4444 texture was requested but we're backing it with an RGBA8888 texture we
	// need unpack and quantize the data.
	if (indataFormat == VPSF_ABGR4444 && destinationFormat == VPSF_RGBA8888)
	{
		for (unsigned int y = 0; y < h; ++y)
		{
			unsigned short *srcRow = (unsigned short *)(((char *)pixels) + (yFlip ? h - 1 - y : y) * paddedRowLen);
			unsigned char *dst = ((unsigned char *)ps.buffer) + y * tgtPaddedRowLen;
			for (unsigned short *src = srcRow; src < srcRow + w; ++src)
			{
				*dst++ = ((*src & 0xf000) >> 12) * 17;
				*dst++ = ((*src & 0x0f00) >> 8) * 17;
				*dst++ = ((*src & 0x00f0) >> 4) * 17;
				*dst++ = ((*src & 0x000f) >> 0) * 17;
			}
		}
	}
	// Same as above but with BGRA instead of RGBA.
	else if (indataFormat == VPSF_ABGR4444 && destinationFormat == VPSF_BGRA8888)
	{
		for (unsigned int y = 0; y < h; ++y)
		{
			unsigned short *srcRow = (unsigned short *)(((char *)pixels) + (yFlip ? h - 1 - y : y) * paddedRowLen);
			unsigned char *dst = (((unsigned char *)ps.buffer) + y * tgtPaddedRowLen);
			for (unsigned short *src = srcRow; src < srcRow + w; ++src)
			{
				*dst++ = ((*src & 0x00f0) >> 4) * 17;
				*dst++ = ((*src & 0x0f00) >> 8) * 17;
				*dst++ = ((*src & 0xf000) >> 12) * 17;
				*dst++ = ((*src & 0x000f) >> 0) * 17;
			}
		}
	}
	// If a RGBA5551 texture was requested but we're backing it with an RGBA8888 texture we
	// need unpack and quantize the data.
	else if (indataFormat == VPSF_RGBA5551 && destinationFormat == VPSF_RGBA8888)
	{
		for (unsigned int y = 0; y < h; ++y)
		{
			unsigned short *srcRow = (unsigned short *)(((char *)pixels) + (yFlip ? h - 1 - y : y) * paddedRowLen);
			unsigned char *dst = ((unsigned char *)ps.buffer) + y * tgtPaddedRowLen;
			for (unsigned short *src = srcRow; src < srcRow + w; ++src)
			{
				*dst++ = ((*src & 0xf800) >> 11) * 33L / 4;
				*dst++ = ((*src >> 6) & 0x1f) * 33L / 4;
				*dst++ = ((*src & 0x003e) >> 1) * 33L / 4;
				*dst++ = ((*src & 0x0001) >> 0) * 255;
			}
		}
	}
	// Same as above but with BGRA instead of ARGB.
	else if (indataFormat == VPSF_RGBA5551 && destinationFormat == VPSF_BGRA8888)
	{
		for (unsigned int y = 0; y < h; ++y)
		{
			unsigned short *srcRow = (unsigned short *)(((char *)pixels) + (yFlip ? h - 1 - y : y) * paddedRowLen);
			unsigned char *dst = ((unsigned char *)ps.buffer) + y * tgtPaddedRowLen;
			for (unsigned short *src = srcRow; src < srcRow + w; ++src)
			{
				*dst++ = ((*src & 0x003e) >> 1) * 33L / 4;
				*dst++ = ((*src >> 6) & 0x1f) * 33L / 4;
				*dst++ = ((*src & 0xf800) >> 11) * 33L / 4;
				*dst++ = ((*src & 0x0001) >> 0) * 255;
			}
		}
	}
	else if (indataFormat == VPSF_RGB565 && destinationFormat == VPSF_BGRX8888)
	{
		for (unsigned int y = 0; y < h; ++y)
		{
			unsigned short *srcRow = (unsigned short *)(((char *)pixels) + (yFlip ? h - 1 - y : y) * paddedRowLen);
			unsigned char *dst = ((unsigned char *)ps.buffer) + y * tgtPaddedRowLen;
			for (unsigned short *src = srcRow; src < srcRow + w; ++src)
			{
				*dst++ = (*src & 0x001f) * 33L / 4;
				*dst++ = ((*src >> 5) & 0x3f) * 65L / 16;
				*dst++ = ((*src >> 11) & 0x001f) * 33L / 4;
				*dst++ = 0;
			}
		}
	}
	else if (indataFormat == VPSF_RGB888 && destinationFormat == VPSF_BGRX8888)
	{
		for (unsigned int y = 0; y < h; ++y)
		{
			unsigned char *srcRow = ((unsigned char *)pixels) + (yFlip ? h - 1 - y : y) * paddedRowLen;
			unsigned char *dst = ((unsigned char *)ps.buffer) + y * tgtPaddedRowLen;
			for (unsigned char *src = srcRow; src < srcRow + w * 3; src += 3)
			{
				*dst++ = src[2];
				*dst++ = src[1];
				*dst++ = src[0];
				*dst++ = 0;
			}
		}
	}
	else if (indataFormat == VPSF_ALPHA8 && destinationFormat == VPSF_BGRA8888)
	{
		for (unsigned int y = 0; y < h; ++y)
		{
			unsigned char *srcRow = ((unsigned char *)pixels) + (yFlip ? h - 1 - y : y) * paddedRowLen;
			unsigned char *dst = ((unsigned char *)ps.buffer) + y * tgtPaddedRowLen;
			for (unsigned char *src = srcRow; src < srcRow + w; ++src, dst += 4)
			{
				dst[0] = dst[1] = dst[2] = 0;
				dst[3] = *src;
			}
		}
	}
	else if (indataFormat == VPSF_LUMINANCE8_ALPHA8 && destinationFormat == VPSF_BGRA8888)
	{
		for (unsigned int y = 0; y < h; ++y)
		{
			unsigned char *srcRow = ((unsigned char *)pixels) + (yFlip ? h - 1 - y : y) * paddedRowLen;
			unsigned char *dst = (unsigned char *)(((unsigned char *)ps.buffer) + y * tgtPaddedRowLen);
			for (unsigned char *src = srcRow; src < srcRow + w * 2; src += 2, dst += 4)
			{
				dst[0] = dst[1] = dst[2] = src[0];
				dst[3] = src[1];
			}
		}
	}
	else if (indataFormat == VPSF_RGBA8888 && destinationFormat == VPSF_BGRA8888)
	{
		for (unsigned int y = 0; y < h; ++y)
		{
			unsigned char *srcRow = ((unsigned char *)pixels) + (yFlip ? h - 1 - y : y) * paddedRowLen;
			unsigned char *dst = ((unsigned char *)ps.buffer) + y * tgtPaddedRowLen;
			for (unsigned char *src = srcRow; src < srcRow + w * 4; src += 4)
			{
				*dst++ = src[2];
				*dst++ = src[1];
				*dst++ = src[0];
				*dst++ = src[3];
			}
		}
	}
	else
	{
		// Need to add extra format handling.
		OP_ASSERT(indataFormat == VPSF_SAME);

		// Copy the data to the pixelstore (either because it needed to be y-flipped or premultiplied).
		if (!yFlip)
			op_memcpy(ps.buffer, pixels, h * paddedRowLen);
		else
			for (unsigned int i = 0; i < h; ++i)
				op_memcpy(((char *)ps.buffer) + i * paddedRowLen, ((char *)pixels) + (h - i - 1) * paddedRowLen, rowLen);
	}

	// If it should be premultiplied, let the Copy To/From PixelStore do the job.
	if (premultiplyAlpha)
	{
		VEGASWBuffer buffer;
		OP_STATUS err = buffer.Create(w, h);
		if (OpStatus::IsError(err))
		{
			OP_DELETEA((char *)ps.buffer);
			return err;
		}
		buffer.CopyFromPixelStore(&ps);
		buffer.CopyToPixelStore(&ps, false, premultiplyAlpha, indataFormat);
		buffer.Destroy();
	}
	pixels = ps.buffer;
	return OpStatus::OK;
}

// static
size_t VEGA3dShaderProgram::GetShaderIndex(ShaderType type, WrapMode mode)
{
	size_t shader_index = (size_t)type;
	switch (type)
	{
	case VEGA3dShaderProgram::SHADER_VECTOR2DTEXGEN:
		shader_index += 8; // 8 extra blends of SHADER_VECTOR2D
	case VEGA3dShaderProgram::SHADER_VECTOR2D:
		shader_index += (size_t)mode;
		break;

	case VEGA3dShaderProgram::SHADER_MORPHOLOGY_ERODE_15:
		++ shader_index; // 1 extra blend of SHADER_MORPHOLOGY_DILATE_15
	case VEGA3dShaderProgram::SHADER_MORPHOLOGY_DILATE_15:
		++ shader_index; // 1 extra blend of SHADER_CONVOLVE_GEN_25_BIAS
	case VEGA3dShaderProgram::SHADER_CONVOLVE_GEN_25_BIAS:
		++ shader_index; // 1 extra blend of SHADER_CONVOLVE_GEN_16_BIAS
	case VEGA3dShaderProgram::SHADER_CONVOLVE_GEN_16_BIAS:
		++ shader_index; // 1 extra blend of SHADER_CONVOLVE_GEN_16
	case VEGA3dShaderProgram::SHADER_CONVOLVE_GEN_16:
		shader_index += 16; // 8 extra blends of SHADER_VECTOR2D,  8 extra blends of SHADER_VECTOR2DTEXGEN
		OP_ASSERT(mode == VEGA3dShaderProgram::WRAP_REPEAT_REPEAT || mode == VEGA3dShaderProgram::WRAP_CLAMP_CLAMP);
		if (mode == VEGA3dShaderProgram::WRAP_CLAMP_CLAMP)
			++ shader_index;
		break;

	default:
		shader_index += 16; // 8 extra blends of SHADER_VECTOR2D,  8 extra blends of SHADER_VECTOR2DTEXGEN
		OP_ASSERT(mode == VEGA3dShaderProgram::WRAP_CLAMP_CLAMP);
	}
	return shader_index;
}


#include "modules/about/operagpu.h"
// static
OP_STATUS VEGA3dDevice::GenerateBackendInfo(OperaGPU* gpu)
{
	OP_ASSERT(gpu);

#ifdef VEGA_BACKENDS_USE_BLOCKLIST
	// blocklist status of available backends
	const unsigned int count = VEGA3dDeviceFactory::DeviceTypeCount();
	for (unsigned int i = 0; i < count; ++i)
	{
		RETURN_IF_ERROR(gpu->Heading(VEGA3dDeviceFactory::DeviceTypeString(i), 2));

		if (OpStatus::IsError(VEGABlocklistDevice::CreateContent(i, gpu)))
		{
			// failed to create device
			OpStringC status = g_vega_backends_module.GetCreationStatus();
			if (status.IsEmpty())
				status = UNI_L("Unknown error");
			RETURN_IF_ERROR(gpu->OpenDefinitionList());
			RETURN_IF_ERROR(gpu->ListEntry(UNI_L("Backend not supported"), status));
			RETURN_IF_ERROR(gpu->CloseDefinitionList());
		}
	}
# endif // VEGA_BACKENDS_USE_BLOCKLIST

	return OpStatus::OK;
}

#endif // VEGA_SUPPORT && (VEGA_3DDEVICE || CANVAS3D_SUPPORT)
