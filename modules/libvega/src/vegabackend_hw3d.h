/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef VEGABACKEND_HW3D_H
#define VEGABACKEND_HW3D_H

#if defined(VEGA_SUPPORT) && defined(VEGA_3DDEVICE)
#include "modules/libvega/src/vegabackend.h"
#include "modules/libvega/vega3ddevice.h"
#include "modules/libvega/vegapath.h"
#include "modules/libvega/src/vegarasterizer.h"

class VEGA3dShaderProgram;

class VEGABackend_HW3D : public VEGARendererBackend
	, public VEGASpanConsumer
{
public:
	VEGABackend_HW3D();
	~VEGABackend_HW3D();

	OP_STATUS init(unsigned int w, unsigned int h, unsigned int q);

	bool bind(VEGARenderTarget* rt);
	void unbind();

	void flush(const OpRect* update_rects = NULL, unsigned int num_rects = 0);
	/** sometimes draw operations are scheduled to use a custom render
	 * state. since drawing is delayed these need to be flushed before
	 * any such render state is deleted. */
	void flushIfUsingRenderState(VEGA3dRenderState* rs);
	void flushIfContainsTexture(VEGA3dTexture* tex);

	void clear(int x, int y, unsigned int w, unsigned int h, unsigned int color, VEGATransform* transform);
	OP_STATUS fillPath(VEGAPath *path, VEGAStencil *stencil) { return fillPath(path, stencil, NULL); }
	OP_STATUS fillPath(VEGAPath *path, VEGAStencil *stencil, VEGA3dRenderState* renderState);
	OP_STATUS fillRect(int x, int y, unsigned int width, unsigned int height, VEGAStencil* stencil, bool blend);
	OP_STATUS invertRect(int x, int y, unsigned int width, unsigned int height);
	OP_STATUS invertPath(VEGAPath* path);
	OP_STATUS moveRect(int x, int y, unsigned int width, unsigned int height, int dx, int dy);

	virtual OP_STATUS updateQuality();

	static inline unsigned int qualityToSamples(unsigned int quality)
	{
		// Scale the quality value by 2 for a better
		// quality<->number-of-samples mapping.
		return quality * 2;
	}

#ifdef VEGA_OPPAINTER_SUPPORT
	virtual OP_STATUS drawString(class VEGAFont* font, int x, int y, const struct ProcessedString* processed_string, VEGAStencil* stencil);
# if defined(VEGA_NATIVE_FONT_SUPPORT) || defined(CSS_TRANSFORMS)
	virtual OP_STATUS drawString(class VEGAFont* font, int x, int y, const uni_char* text, UINT32 len, INT32 extra_char_space, short word_width, VEGAStencil* stencil);
# endif // VEGA_NATIVE_FONT_SUPPORT || CSS_TRANSFORMS
#endif // VEGA_OPPAINTER_SUPPORT

	OP_STATUS applyFilter(VEGAFilter* filter, const VEGAFilterRegion& region);

	OP_STATUS createBitmapRenderTarget(VEGARenderTarget** rt, OpBitmap* bmp);
#ifdef VEGA_OPPAINTER_SUPPORT
	OP_STATUS createWindowRenderTarget(VEGARenderTarget** rt, VEGAWindow* window);
#endif // VEGA_OPPAINTER_SUPPORT
	OP_STATUS createIntermediateRenderTarget(VEGARenderTarget** rt, unsigned int w, unsigned int h);

	OP_STATUS createStencil(VEGAStencil** sten, bool component, unsigned int w, unsigned int h);

#ifdef VEGA_LIMIT_BITMAP_SIZE
	static unsigned maxBitmapSide() { return g_vegaGlobals.vega3dDevice->getMaxTextureSize(); }
#endif // VEGA_LIMIT_BITMAP_SIZE

	static OP_STATUS createBitmapStore(VEGABackingStore** store, unsigned w, unsigned h, bool is_indexed);
	virtual bool supportsStore(VEGABackingStore* store);

#ifdef CANVAS3D_SUPPORT
	OP_STATUS createImage(VEGAImage* fill, class VEGA3dTexture* tex, bool isPremultiplied);
#endif // CANVAS3D_SUPPORT

	unsigned calculateArea(VEGA_FIX minx, VEGA_FIX miny, VEGA_FIX maxx, VEGA_FIX maxy);

	OP_STATUS createPremulRenderState()
	{
		// used to composite non premultiplied data onto premultiplied data
		if (!m_premultiplyRenderState)
		{
			VEGA3dDevice* dev3d = g_vegaGlobals.vega3dDevice;
			RETURN_IF_ERROR(dev3d->createRenderState(&m_premultiplyRenderState, false));
			m_premultiplyRenderState->enableScissor(true);
			m_premultiplyRenderState->enableBlend(true);
			m_premultiplyRenderState->setBlendFunc(VEGA3dRenderState::BLEND_SRC_ALPHA,
			                                       VEGA3dRenderState::BLEND_ONE_MINUS_SRC_ALPHA,
			                                       VEGA3dRenderState::BLEND_ONE,
			                                       VEGA3dRenderState::BLEND_ONE_MINUS_SRC_ALPHA);
		}
		return OpStatus::OK;
	}

private:
	enum BatchMode
	{
		BM_AUTO = 0, // BM_QUAD if numVerts == 4, BM_TRIANGLE_FAN otherwise
		BM_QUAD,
		BM_TRIANGLE_FAN,
		BM_INDEXED_TRIANGLES
	};
	struct DrawTriangleArgs
	{
		void setVert(VEGA3dDevice::Vega2dVertex* vert, unsigned idx);

		VEGAPath* path;
		BatchMode mode;
		const OpRect* affected;
		OpRect* clampRect;
		bool needsClamp;
		unsigned int col;
		const VEGATransform* textrans;
		VEGA3dTexture* tex;
		VEGA3dTexture* stencilTex;
		VEGA3dRenderState* renderState;
		VEGA3dVertexLayout* layout;
		VEGA3dShaderProgram* prog;

		unsigned short* indexedTriangles;
		int indexedTriangleCount;
	};

	void drawTriangles(DrawTriangleArgs& args);
	void scheduleTriangles(DrawTriangleArgs& args);

	virtual void drawSpans(VEGASpanInfo* raster_spans, unsigned int span_count);
	VEGARasterizer rasterizer;
	UINT8* m_curRasterBuffer;
	OpRect m_curRasterArea;
	unsigned int m_lastRasterPos;

	/** Update the alpha texture used for stencil testing. */
	void updateAlphaTextureStencil(VEGAPath* path);
	/// utility function: update strans, ttrans and stencilTex according to stencil
	void updateStencil(VEGAStencil* stencil, VEGA_FIX* strans, VEGA_FIX* ttrans, VEGA3dTexture*& stencilTex);

	VEGA3dBuffer* m_vbuffer;
	VEGA3dVertexLayout* m_vlayout2d;
	VEGA3dVertexLayout* m_vlayout2dTexGen;
	VEGA3dDevice::Vega2dVertex* m_vert;
	VEGA3dRenderState* m_defaultRenderState;
	VEGA3dRenderState* m_defaultNoBlendRenderState;
	VEGA3dRenderState* m_additiveRenderState;
	VEGA3dRenderState* m_stencilModulateRenderState;
	VEGA3dRenderState* m_updateStencilRenderState;
	VEGA3dRenderState* m_updateStencilXorRenderState;
	VEGA3dRenderState* m_invertRenderState;
	VEGA3dRenderState* m_premultiplyRenderState;
#ifdef VEGA_SUBPIXEL_FONT_BLENDING
	VEGA3dRenderState* m_subpixelRenderState;
	OP_STATUS createSubpixelRenderState();
#endif

	/** GPU representation of triangle indices, used to batch indexed
	  * triangles - updated from flushBatches if needed */
	VEGA3dBuffer* m_triangleIndexBuffer;
	/** buffer for all triangle indices, synced to
	 * m_triangleIndexBuffer from flushBatches */
	unsigned short* m_triangleIndices;

	/**
	 * Create the requested text shader and corresponding vlayout, shader variables etc.
	 */
	OP_STATUS createTextShader(unsigned char traits);

	void releaseShaderPrograms();
	VEGA3dShaderProgram* shader2dV[VEGA3dShaderProgram::WRAP_MODE_COUNT];
	VEGA3dShaderProgram* shader2dTexGenV[VEGA3dShaderProgram::WRAP_MODE_COUNT];

	/**
	 * These text shader features are orthogonal to each other,
	 * they can appear in any combination. So each of these feature
	 * should have its own bit and if more are added make sure they
	 * occupy contiguous bits starting from 1.
	 *
	 * NOTE createTextShader may need to be updated if this enum is
	 * changed
	 */
	enum TextShaderTrait
	{
		Stencil = 0x1,
#ifdef VEGA_SUBPIXEL_FONT_BLENDING
		ExtBlend = 0x2,
#ifdef VEGA_SUBPIXEL_FONT_INTERPOLATION_WORKAROUND
		Interpolation = 0x4,
#endif
#endif
		MaximumTraitValuePlusOne
	};

	static const unsigned int TEXT_SHADER_WITH_STENCIL_COUNT = (MaximumTraitValuePlusOne-1);
	static const unsigned int TEXT_SHADER_COUNT = 2*(MaximumTraitValuePlusOne-1);

	VEGA3dVertexLayout* m_vlayout2dText[TEXT_SHADER_COUNT];
	VEGA3dShaderProgram* shader2dText[TEXT_SHADER_COUNT];
	bool textShaderLoaded[TEXT_SHADER_COUNT];

	int textAlphaCompLocation[TEXT_SHADER_COUNT];					// For text shaders without ExtBlend
	int textTexGenTransSLocation[TEXT_SHADER_WITH_STENCIL_COUNT];	// For text shaders with stencil
	int textTexGenTransTLocation[TEXT_SHADER_WITH_STENCIL_COUNT];	// For text shaders with stencil

	int worldProjMatrix2dLocationV[VEGA3dShaderProgram::WRAP_MODE_COUNT];

	int texTransSLocationV[VEGA3dShaderProgram::WRAP_MODE_COUNT];
	int texTransTLocationV[VEGA3dShaderProgram::WRAP_MODE_COUNT];
	int stencilCompBasedLocation[VEGA3dShaderProgram::WRAP_MODE_COUNT];
	int straightAlphaLocation[VEGA3dShaderProgram::WRAP_MODE_COUNT];

	// Batching functionality
	unsigned int m_numRemainingBatchVerts;
	unsigned int m_firstBatchVert;
	/** number of triangle indices used so far */
	unsigned int m_triangleIndexCount;
	struct BatchOperation
	{
		unsigned int firstVertex;
		unsigned int numVertices;
		VEGA3dTexture* texture;
		VEGA3dTexture::WrapMode texWrapS;
		VEGA3dTexture::WrapMode texWrapT;
		VEGA3dTexture::FilterMode texMinFilter;
		VEGA3dTexture::FilterMode texMagFilter;
		VEGAFont* font;
		VEGA3dRenderState* renderState;
		OpRect affectedRect;
		OpRect clipRect;
		bool requiresClipping;
		BatchMode mode;
		int indexCount;
		bool inProgress;
	};
	BatchOperation* m_batchList;
	unsigned int m_numBatchOperations;
	void scheduleBatch(VEGA3dTexture* tex, unsigned int numVerts, const OpRect& affected, OpRect* clip, VEGA3dRenderState* renderState = NULL, BatchMode mode = BM_AUTO, int indexCount = -1);
	void scheduleFontBatch(VEGAFont* fnt, unsigned int numVerts, const OpRect& affected);
	void finishCurrentFontBatch();
#ifdef VEGA_NATIVE_FONT_SUPPORT
	OpRect m_nativeFontRect;
	void scheduleNativeFontBatch(const OpRect& affected);
#endif

	/** append vertices from batch entry with index source_batch_idx
	  * to existing batch entry with index target_batch_idx, using
	  * temp_storage as intermediate storage. also update target's
	  * affected rect and vertex count. only firstVertex and
	  * numVertices are used from source, which is assumed to be
	  * compatible with target.
	  */
	void appendVertexDataToBatch(const unsigned int source_batch_idx,
	                             const unsigned int target_batch_idx,
	                             VEGA3dDevice::Vega2dVertex* temp_storage,
	                             const OpRect& affected);

public:
	void flushBatches(VEGAStencil* textStencil = NULL);
	void onFontDeleted(VEGAFont* font);

	unsigned int m_currentFrame;
};

class VEGABackingStore_Texture;
/* Backing by a FrameBuffer Object - rw */
class VEGABackingStore_FBO : public VEGABackingStore_Lockable
{
public:
	VEGABackingStore_FBO() : m_fbo(NULL), m_msfbo(NULL), m_msActive(false), m_writeRTDirty(false), m_msSamples(0), m_textureStore(NULL), m_batcher(NULL), m_lastFrame(0), m_lastMSFrame(0) {}
	virtual ~VEGABackingStore_FBO();

	OP_STATUS Construct(unsigned width, unsigned height);
	OP_STATUS Construct(VEGA3dWindow* win3d);
	OP_STATUS Construct(VEGABackingStore_Texture* tex);

	virtual bool IsA(Type type) const { return type == FRAMEBUFFEROBJECT; }

	virtual void SetColor(const OpRect& rect, UINT32 color);

	virtual OP_STATUS CopyRect(const OpPoint& dstp, const OpRect& srcr, VEGABackingStore* store);

	virtual unsigned GetWidth() const { return m_fbo->getWidth(); }
	virtual unsigned GetHeight() const { return m_fbo->getHeight(); }
	virtual unsigned GetBytesPerLine() const { return m_fbo->getWidth() * 4; }
	virtual unsigned GetBytesPerPixel() const { return 4; }

	VEGA3dRenderTarget* GetReadRenderTarget();
	VEGA3dRenderTarget* GetWriteRenderTarget(unsigned int frame);
	bool IsReadRenderTarget(VEGA3dRenderTarget* rt){return (rt == m_fbo);}
	void DisableMultisampling();

	/** If the function returns true that means multisampling mode was changed by this call (it was not already enabled).
	 * If using VEGA_NATIVE_FONT_SUPPORT a return value of true means the fonts have been flushed and the affected rect
	 * for native fonts should be emptied. */
	bool EnableMultisampling(unsigned int quality, unsigned int frame);
	void ResolveMultisampling();
	bool IsMultisampled(){return m_msActive;}

	VEGA3dTexture* SwapTexture(VEGA3dTexture* tex);

	void setCurrentBatcher(VEGABackend_HW3D* batcher);
protected:
	virtual OP_STATUS LockRect(const OpRect& rect, AccessType acc);
	virtual OP_STATUS UnlockRect(BOOL commit);

	VEGA3dDevice::FramebufferData m_lock_data;
	VEGA3dRenderTarget* m_fbo;
	VEGA3dFramebufferObject* m_msfbo;
	bool m_msActive;
	bool m_writeRTDirty;
	unsigned int m_msSamples;
	VEGABackingStore_Texture* m_textureStore;
	VEGABackend_HW3D* m_batcher;
	unsigned int m_lastFrame;
	unsigned int m_lastMSFrame;
};

#include "modules/libvega/src/vegabackend_sw.h"

/** this is a two-way backing store, providing both a software (m_buffer) and
 * a hardware (m_texture) bitmap. updating can be done to either, and
 * syncing will be deferred until needed. syncing from hw to sw may
 * OOM, in which case the sw buffer is left untouched. it is invalid
 * for both buffers to be dirty at the same time. */
class VEGABackingStore_Texture : public VEGABackingStore_SWBuffer, public VEGA3dContextLostListener
{
public:
	VEGABackingStore_Texture() : m_texture(NULL), m_texture_dirty(false), m_buffer_dirty(false) {}
	virtual ~VEGABackingStore_Texture();

	OP_STATUS Construct(unsigned width, unsigned height, bool indexed);
	virtual void OnContextLost();
	virtual void OnContextRestored();

	virtual bool IsA(Type type) const
	{
		return type == TEXTURE || VEGABackingStore_SWBuffer::IsA(type);
	}

	/// fills rect in m_buffer with color and syncs to m_texture
	virtual void SetColor(const OpRect& rect, UINT32 color);

	/** copies srcr from store to dstp in this backing store. copies
	 * to m_texture if store is an FBO, otherwise to
	 * m_buffer. non-target is marked dirty. */
	virtual OP_STATUS CopyRect(const OpPoint& dstp, const OpRect& srcr, VEGABackingStore* store);

	/** for access types other than write-only, validates m_buffer by
	 * copying contents from m_texture */
	virtual VEGASWBuffer* BeginTransaction(const OpRect& rect, AccessType acc);
	virtual void EndTransaction(BOOL commit);

	/// validates m_texture by copying the contents from m_buffer
	virtual void Validate();

	VEGA3dTexture* GetTexture() { return m_texture; }
	void MarkBufferDirty(){ if (m_texture_dirty) SyncToTexture(); m_buffer_dirty = true;}
protected:
	/// utility function - returns true if rect equals that of the backing store
	bool WholeArea(const OpRect& rect) const { return !!rect.Equals(OpRect(0, 0, GetWidth(), GetHeight())) != FALSE; }

	/** syncs data in rect from m_texture to m_buffer. m_texture must
	 * not be dirty when calling.
	 * @param rect the rect to sync */
	OP_STATUS SyncToBuffer(const OpRect& rect);
	/** syncs all data from m_texture to m_buffer. m_texture must not
	 * be dirty when calling. if successful, m_buffer will no longer
	 * be dirty. */
	OP_STATUS SyncToBuffer()
	{
		RETURN_IF_ERROR(OpStatus::IsSuccess(SyncToBuffer(OpRect(0, 0, GetWidth(), GetHeight()))));
		m_buffer_dirty = false;
		return OpStatus::OK;
	}
	/** syncs data in rect from m_buffer to m_texture. m_buffer must
	 * not be dirty when calling.
	 * @param rect the rect to sync */
	void SyncToTexture(const OpRect& rect);
	/** syncs all data from m_buffer to m_texture. m_buffer must not
	 * be dirty when calling. after calling m_texture will no longer
	 * be dirty. */
	void SyncToTexture()
	{
		OP_ASSERT(!m_buffer_dirty);
		m_texture->update(0, 0, &m_buffer);
		m_texture_dirty = false;
	}

	VEGA3dTexture* m_texture;
	bool m_texture_dirty;
	bool m_buffer_dirty;
};

#endif // VEGA_SUPPORT && VEGA_3DDEVICE
#endif // VEGABACKEND_HW3D_H
