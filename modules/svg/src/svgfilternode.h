/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.	It may not be distributed
** under any circumstances.
*/

#ifndef SVG_FILTER_NODE_H
#define SVG_FILTER_NODE_H

#ifdef SVG_SUPPORT
#ifdef SVG_SUPPORT_FILTERS

#include "modules/svg/src/SVGVector.h"
#include "modules/svg/src/SVGNumberPair.h"
#include "modules/svg/src/svgpaintserver.h"
#include "modules/svg/src/svgpainter.h"

#include "modules/libvega/vegarenderer.h"

struct SVGCompFunc
{
	SVGCompFunc() : type(SVGFUNC_IDENTITY), table(NULL) {}
	~SVGCompFunc() { SVGObject::DecRef(table); }

	SVGFuncType type;
	SVGVector* table;
	SVGNumber slope;
	SVGNumber intercept;
	SVGNumber amplitude;
	SVGNumber exponent;
	SVGNumber offset;
};

struct SVGLightSource
{
	SVGLightSource() : type(SVGLIGHTSOURCE_DISTANT), x(1) {}

	enum SVGLightSourceType
	{
		SVGLIGHTSOURCE_DISTANT,
		SVGLIGHTSOURCE_POINT,
		SVGLIGHTSOURCE_SPOT
	};

	SVGLightSourceType type;

	// Used for all types of lightsources
	// Direction vector for distant lights
	// Position for point and spot lights
	SVGNumber x;
	SVGNumber y;
	SVGNumber z;

	// For spotlights only
	SVGNumber pointsAtX;
	SVGNumber pointsAtY;
	SVGNumber pointsAtZ;

	// For spotlights only
	SVGNumber spec_exponent;
	SVGNumber cone_angle;
	BOOL has_cone_angle;
};

struct SVGFilterNodeInfo;
class SVGFilterContext;
class VEGAFilter;

enum ColorSpaceType
{
	COLORSPACE_SRGB,
	COLORSPACE_LINEARRGB,

	NUM_COLORSPACES
};

class SVGFilterNode
{
public:
	virtual ~SVGFilterNode() {}

	virtual OP_STATUS Apply(SVGFilterContext* context) = 0;

	virtual void ResolveColorSpace(SVGColorInterpolation colorinterp);
	virtual void CalculateDefaultSubregion(const SVGRect& filter_region) { m_region = filter_region; }

	struct ExtentModifier
	{
		enum Type
		{
			IDENTITY,		/* No modification */
			EXTEND,			/* Extend ("outset") by <extend.x, extend.y> */
			EXTEND_MIN_1PX,	/* As EXTEND, but always extend by at least one pixel */
			AREA			/* Union with <area> */
		};

		ExtentModifier() : type(IDENTITY) {}
		ExtentModifier(Type t, SVGNumber extend_x, SVGNumber extend_y)
			: type(t), extend(extend_x, extend_y) {}
		ExtentModifier(Type t, const SVGRect& ar)
			: type(t), area(ar) {}

		void Apply(OpRect& area);
		void Apply(SVGBoundingBox& extents);

		Type type;
		SVGNumberPair extend;
		SVGRect area;
	};

	virtual void GetExtentModifier(SVGFilterContext* context, ExtentModifier& modifier) {}

	void SetRegion(const SVGRect& region) { m_region = region; }
	const SVGRect& GetRegion() const { return m_region; }

	ColorSpaceType GetColorSpace() const { return m_colorspace; }

	void SetAlphaOnly(BOOL alpha_only) { m_alpha_only = alpha_only; }
	BOOL IsAlphaOnly() const { return m_alpha_only; }

	void SetNodeId(int node_id) { m_node_id = node_id; }
	int GetNodeId() const { return m_node_id; }

	virtual unsigned GetInputCount() { return 0; }
	virtual SVGFilterNode** GetInputNodes() { return NULL; }

protected:
	SVGFilterNode() : m_node_id(-1), m_alpha_only(FALSE), m_colorspace(COLORSPACE_SRGB) {}

	OP_STATUS CommonApply(SVGFilterContext* context,
						  VEGAFilter* filter,
						  SVGFilterNodeInfo* dst, SVGFilterNodeInfo* src,
						  BOOL src_is_alpha_only,
						  BOOL do_inverted_area_clear = FALSE,
						  int ofs_x = 0, int ofs_y = 0);

	SVGRect m_region;

	int m_node_id;
	BOOL m_alpha_only;
	ColorSpaceType m_colorspace;
};

class SVGUnaryFilterNode : public SVGFilterNode
{
public:
	virtual void ResolveColorSpace(SVGColorInterpolation colorinterp);
	virtual void CalculateDefaultSubregion(const SVGRect& filter_region);

	void SetInput(SVGFilterNode* input) { m_input = input; }

	virtual unsigned GetInputCount() { return 1; }
	virtual SVGFilterNode** GetInputNodes() { return &m_input; }

protected:
	SVGUnaryFilterNode() : m_input(NULL) {}

	SVGFilterNode* m_input;
};

class SVGSimpleUnaryFilterNode : public SVGUnaryFilterNode
{
public:
	virtual ~SVGSimpleUnaryFilterNode()
	{
		if (m_type == COMPONENTTRANSFER)
			OP_DELETEA(m_compxfer.funcs);
		else if (m_type == COLORMATRIX)
			SVGObject::DecRef(m_colmatrix.mat);
	}

	static SVGSimpleUnaryFilterNode* CreateComponentTransfer(SVGCompFunc* funcs)
	{
		SVGSimpleUnaryFilterNode* node = OP_NEW(SVGSimpleUnaryFilterNode, ());
		if (!node)
		{
			OP_DELETEA(funcs);
			return NULL;
		}

		node->m_type = COMPONENTTRANSFER;
		node->m_compxfer.funcs = funcs;
		return node;
	}

	static SVGSimpleUnaryFilterNode* CreateColorMatrix(SVGColorMatrixType type, SVGVector* mat)
	{
		SVGSimpleUnaryFilterNode* node = OP_NEW(SVGSimpleUnaryFilterNode, ());
		if (!node)
			return NULL;

		node->m_type = COLORMATRIX;
		node->m_colmatrix.type = type;
		SVGObject::IncRef(mat);
		node->m_colmatrix.mat = mat;
		return node;
	}

	static SVGSimpleUnaryFilterNode* CreateGaussian(const SVGNumberPair& stddev)
	{
		SVGSimpleUnaryFilterNode* node = OP_NEW(SVGSimpleUnaryFilterNode, ());
		if (!node)
			return NULL;

		node->m_type = GAUSSIAN;
		node->m_value = stddev;
		return node;
	}

	static SVGSimpleUnaryFilterNode* CreateOffset(const SVGNumberPair& offset)
	{
		SVGSimpleUnaryFilterNode* node = OP_NEW(SVGSimpleUnaryFilterNode, ());
		if (!node)
			return NULL;

		node->m_type = OFFSET;
		node->m_value = offset;
		return node;
	}

	static SVGSimpleUnaryFilterNode* CreateMorphology(SVGMorphologyOperator oper,
													  const SVGNumberPair& radii)
	{
		SVGSimpleUnaryFilterNode* node = OP_NEW(SVGSimpleUnaryFilterNode, ());
		if (!node)
			return NULL;

		node->m_type = MORPHOLOGY;
		node->m_value = radii;
		node->m_morph.oper = oper;
		return node;
	}

	static SVGSimpleUnaryFilterNode* CreateTile(const SVGRect& tile_region)
	{
		SVGSimpleUnaryFilterNode* node = OP_NEW(SVGSimpleUnaryFilterNode, ());
		if (!node)
			return NULL;

		node->m_type = TILE;
		node->m_tile_region = tile_region;
		return node;
	}

	virtual OP_STATUS Apply(SVGFilterContext* context);

	virtual void CalculateDefaultSubregion(const SVGRect& filter_region);

	virtual void GetExtentModifier(SVGFilterContext* context, ExtentModifier& modifier);

private:
	enum
	{
		INVALID,
		COMPONENTTRANSFER,
		COLORMATRIX,
		GAUSSIAN,
		OFFSET,
		MORPHOLOGY,
		TILE
	} m_type;

	SVGSimpleUnaryFilterNode() : m_type(INVALID) {}

	// This should preferably be a union, but certain properties of
	// the the SVGNumber-based types prevents that.

	// Tile
	SVGRect m_tile_region;

	// Gaussian Blur (stddev) / Offset (dx, dy) / Morphology (radii)
	SVGNumberPair m_value;

	union
	{
		// Componenttransfer
		struct
		{
			SVGCompFunc* funcs;
		} m_compxfer;

		// Colormatrix
		struct
		{
			SVGVector* mat;
			SVGColorMatrixType type;
		} m_colmatrix;

		// Morphology
		struct
		{
			SVGMorphologyOperator oper;
		} m_morph;
	};

	OP_STATUS SetupComponentTransfer(VEGARenderer* renderer, VEGAFilter*& filter);
	OP_STATUS SetupColorMatrix(VEGARenderer* renderer, VEGAFilter*& filter);

	OP_STATUS CreateTile(SVGFilterContext* context);
};

class SVGScaledUnaryFilterNode : public SVGUnaryFilterNode
{
public:
	virtual ~SVGScaledUnaryFilterNode()
	{
		if (m_type == CONVOLVE)
			SVGObject::DecRef(m_convolve.kernmat);
	}

	static SVGScaledUnaryFilterNode* CreateLighting(const SVGLightSource& light,
													SVGNumber surface_scale,
													UINT32 light_color, bool is_specular,
													SVGNumber const_fact, SVGNumber spec_exp = 0)
	{
		SVGScaledUnaryFilterNode* node = OP_NEW(SVGScaledUnaryFilterNode, ());
		if (!node)
			return NULL;

		node->m_type = LIGHTING;
		node->m_light = light;
		node->m_lighting_surface_scale = surface_scale;
		node->m_lighting.color = light_color;
		node->m_lighting.is_specular = is_specular;
		node->m_lighting_const_fact = const_fact;
		node->m_lighting_specular_exp = spec_exp;
		return node;
	}

	static SVGScaledUnaryFilterNode* CreateConvolve(SVGVector* kernmat,
													const SVGNumberPair& order,
													const SVGNumberPair& tgt,
													SVGNumber divisor, SVGNumber bias,
													bool preserve_alpha,
													SVGConvolveEdgeMode edge_mode)
	{
		SVGScaledUnaryFilterNode* node = OP_NEW(SVGScaledUnaryFilterNode, ());
		if (!node)
			return NULL;

		node->m_type = CONVOLVE;
		node->m_convolve_order = order;
		node->m_convolve_target = tgt;
		node->m_convolve_divisor = divisor;
		node->m_convolve_bias = bias;
		SVGObject::IncRef(kernmat);
		node->m_convolve.kernmat = kernmat;
		node->m_convolve.edge_mode = edge_mode;
		node->m_convolve.preserve_alpha = preserve_alpha;
		return node;
	}

	virtual OP_STATUS Apply(SVGFilterContext* context);

	virtual void GetExtentModifier(SVGFilterContext* context, ExtentModifier& modifier);

	void SetUnitLength(const SVGNumberPair& unit_len)
	{
		m_unit_len = unit_len;
		m_has_unit_len = TRUE;
	}

private:
	enum
	{
		INVALID,
		CONVOLVE,
		LIGHTING
	} m_type;

	SVGScaledUnaryFilterNode() : m_type(INVALID), m_has_unit_len(FALSE) {}

	OP_STATUS ScaledApply(SVGFilterContext* context,
						  VEGAFilter* filter,
						  const SVGNumberPair& unit_len,
						  SVGFilterNodeInfo* dst, SVGFilterNodeInfo* src,
						  BOOL src_is_alpha_only);
	OP_STATUS ScaleSurface(VEGARenderer* dst_r, SVGFilterNodeInfo* dst, SVGFilterNodeInfo* src);

	// See comment about unions in SVGSimpleUnaryFilterNode.

	// Shared (unit length)
	SVGNumberPair m_unit_len;
	BOOL m_has_unit_len;

	// Convolve
	SVGNumberPair m_convolve_order;
	SVGNumberPair m_convolve_target;
	SVGNumber m_convolve_divisor;
	SVGNumber m_convolve_bias;

	// Lighting
	SVGLightSource m_light;
	SVGNumber m_lighting_surface_scale;
	SVGNumber m_lighting_const_fact;
	SVGNumber m_lighting_specular_exp;

	union
	{
		// Convolve
		struct
		{
			SVGVector* kernmat;
			SVGConvolveEdgeMode edge_mode;
			bool preserve_alpha;
		} m_convolve;

		// Lighting
		struct
		{
			UINT32 color;
			bool is_specular;
		} m_lighting;
	};

	OP_STATUS SetupConvolution(VEGARenderer* renderer, VEGAFilter*& filter);
	OP_STATUS SetupLighting(SVGFilterContext* context, SVGFilterNodeInfo* nodeinfo,
							VEGAFilter*& filter);
};

class SVGBinaryFilterNode : public SVGFilterNode
{
public:
	static SVGBinaryFilterNode* CreateBlend(SVGBlendMode mode)
	{
		SVGBinaryFilterNode* node = OP_NEW(SVGBinaryFilterNode, ());
		if (!node)
			return NULL;

		node->m_type = BLEND;
		node->m_blend.mode = mode;
		return node;
	}
	static SVGBinaryFilterNode* CreateComposite(SVGCompositeOperator oper,
												SVGNumber k1, SVGNumber k2, SVGNumber k3, SVGNumber k4)
	{
		SVGBinaryFilterNode* node = OP_NEW(SVGBinaryFilterNode, ());
		if (!node)
			return NULL;

		node->m_type = COMPOSITE;
		node->m_composite_k1 = k1;
		node->m_composite_k2 = k2;
		node->m_composite_k3 = k3;
		node->m_composite_k4 = k4;
		node->m_composite.oper = oper;
		return node;
	}
	static SVGBinaryFilterNode* CreateDisplacement(SVGDisplacementSelector x_disp, SVGDisplacementSelector y_disp,
												   SVGNumber scale)
	{
		SVGBinaryFilterNode* node = OP_NEW(SVGBinaryFilterNode, ());
		if (!node)
			return NULL;

		node->m_type = DISPLACEMENT;
		node->m_displacement.x_disp = x_disp;
		node->m_displacement.y_disp = y_disp;
		node->m_displacement_scale = scale;
		return node;
	}

	virtual OP_STATUS Apply(SVGFilterContext* context);

	virtual void ResolveColorSpace(SVGColorInterpolation colorinterp);
	virtual void CalculateDefaultSubregion(const SVGRect& filter_region);

	virtual void GetExtentModifier(SVGFilterContext* context, ExtentModifier& modifier);

	virtual unsigned GetInputCount() { return 2; }
	virtual SVGFilterNode** GetInputNodes() { return m_input; }

	void SetFirstInput(SVGFilterNode* input) { m_input[0] = input; }
	void SetSecondInput(SVGFilterNode* input2) { m_input[1] = input2; }

private:
	SVGBinaryFilterNode() { m_input[0] = m_input[1] = NULL; }

	SVGFilterNode* m_input[2];

	enum
	{
		BLEND,
		COMPOSITE,
		DISPLACEMENT
	} m_type;

	// See comment about unions in SVGSimpleUnaryFilterNode.

	// Composite
	SVGNumber m_composite_k1;
	SVGNumber m_composite_k2;
	SVGNumber m_composite_k3;
	SVGNumber m_composite_k4;

	// Displacement Map
	SVGNumber m_displacement_scale;

	union
	{
		// Blend
		struct
		{
			SVGBlendMode mode;
		} m_blend;

		// Composite
		struct
		{
			SVGCompositeOperator oper;
		} m_composite;

		// Displacement Map
		struct
		{
			SVGDisplacementSelector x_disp;
			SVGDisplacementSelector y_disp;
		} m_displacement;
	};

	OP_STATUS ApplyDisplacementMap(SVGFilterContext* context);
};

class SVGNaryFilterNode : public SVGFilterNode
{
public:
	virtual ~SVGNaryFilterNode() { OP_DELETEA(m_inputs); }

	static SVGNaryFilterNode* Create(unsigned input_count)
	{
		SVGNaryFilterNode* node = OP_NEW(SVGNaryFilterNode, ());
		if (node)
		{
			OP_STATUS status = node->SetNumberInputs(input_count);
			if (OpStatus::IsError(status))
			{
				OP_DELETE(node);
				node = NULL;
			}
		}
		return node;
	}

	virtual OP_STATUS Apply(SVGFilterContext* context);

	virtual void ResolveColorSpace(SVGColorInterpolation colorinterp);
	virtual void CalculateDefaultSubregion(const SVGRect& filter_region);

	virtual unsigned GetInputCount() { return m_num_inputs; }
	virtual SVGFilterNode** GetInputNodes() { return m_inputs; }

	void SetInputAt(unsigned idx, SVGFilterNode* input)
	{
		OP_ASSERT(m_inputs && m_num_inputs);
		OP_ASSERT(idx < m_num_inputs);
		m_inputs[idx] = input;
	}

private:
	SVGNaryFilterNode() : m_inputs(NULL), m_num_inputs(0) {}

	OP_STATUS SetNumberInputs(unsigned num_inputs);

	SVGFilterNode** m_inputs;
	unsigned m_num_inputs;
};

class SVGSourceImageFilterNode : public SVGFilterNode
{
public:
	enum Type
	{
		INVALID,
		SOURCE,
		BACKGROUND,
		FILLPAINT,
		STROKEPAINT
	};

	SVGSourceImageFilterNode() : m_type(INVALID) { m_paint.Reset(); }
	virtual ~SVGSourceImageFilterNode() { SVGPaintServer::DecRef(m_paint.pserver); }

	virtual OP_STATUS Apply(SVGFilterContext* context);

	void SetType(Type type) { m_type = type; }
	void SetPaint(const SVGPaintDesc& paint)
	{
		m_paint = paint;
		SVGPaintServer::IncRef(m_paint.pserver);
	}

private:
	OP_STATUS RescaleBackgroundImage(SVGFilterContext* context,
									 VEGARenderTarget* bl_target, const OpRect& bl_target_area,
									 SVGFilterNodeInfo* nodeinfo);

	Type m_type;

	SVGPaintDesc m_paint;
};

class SVGGeneratorFilterNode : public SVGFilterNode
{
public:
	SVGGeneratorFilterNode() : m_type(INVALID) {}
	virtual ~SVGGeneratorFilterNode();

	static SVGGeneratorFilterNode* CreateNoise(SVGTurbulenceType turb_type,
											   const SVGNumberPair& base_freq,
											   SVGNumber num_octaves, SVGNumber seed,
											   SVGStitchType stitch_type)
	{
		SVGGeneratorFilterNode* node = OP_NEW(SVGGeneratorFilterNode, ());
		if (!node)
			return NULL;

		node->m_type = NOISE;
		node->m_noise_base_freq = base_freq;
		node->m_noise_num_octaves = num_octaves;
		node->m_noise_seed = seed;
		node->m_noise.type = turb_type;
		node->m_noise.stitch = stitch_type;
		return node;
	}

	static SVGGeneratorFilterNode* CreateImage(SVGPaintNode* image_paint_node)
	{
		SVGGeneratorFilterNode* node = OP_NEW(SVGGeneratorFilterNode, ());
		if (!node)
			return NULL;

		node->m_type = IMAGE;
		node->m_image.paint_node = image_paint_node;
		return node;
	}

	static SVGGeneratorFilterNode* CreateFlood(UINT32 flood_color, UINT8 flood_opacity)
	{
		SVGGeneratorFilterNode* node = OP_NEW(SVGGeneratorFilterNode, ());
		if (!node)
			return NULL;

		node->m_type = FLOOD;
		node->m_flood.color = flood_color;
		node->m_flood.opacity = flood_opacity;
		return node;
	}

	virtual OP_STATUS Apply(SVGFilterContext* context);

	virtual void ResolveColorSpace(SVGColorInterpolation colorinterp);
	virtual void CalculateDefaultSubregion(const SVGRect& filter_region);

private:
	enum
	{
		INVALID,
		NOISE,
		IMAGE,
		FLOOD
	} m_type;

	// See comment about unions in SVGSimpleUnaryFilterNode.

	// Noise
	SVGNumberPair m_noise_base_freq;
	SVGNumber m_noise_num_octaves;
	SVGNumber m_noise_seed;

	union
	{
		// Noise
		struct
		{
			SVGTurbulenceType type;
			SVGStitchType stitch;
			// Or SVGTurbulenceGenerator*
		} m_noise;

		// Image
		struct
		{
			SVGPaintNode* paint_node;
		} m_image;

		// Flood
		struct
		{
			UINT32 color;
			UINT8 opacity;
		} m_flood;
	};

	OP_STATUS CreateNoise(SVGFilterContext* context);
};

struct SVGFilterNodeInfo
{
	OpRect area;				// Calculated area of the filter node

	VEGARenderTarget* target;	// Destination surface of the filter node
	ColorSpaceType colorspace;	// Colorspace of the surface

	unsigned int ref;			// Number of direct references
};

class SVGPainter;

class SVGFilterContext
{
public:
	SVGFilterContext(SVGPaintNode* source_node, SVGPainter* painter = NULL) :
		m_painter(painter), m_paint_node(source_node),
		m_lin_to_srgb(NULL), m_srgb_to_lin(NULL), m_s2l_tab(NULL),
		m_nodeinfo(NULL), m_num_nodes(0) {}
	~SVGFilterContext();

	void SetRegion(const SVGRect& region) { m_filter_region = region; }
	const SVGRect& GetRegion() const { return m_filter_region; }

	OP_STATUS NegotiateBufferSize(unsigned int& buffer_width, unsigned int& buffer_height,
								  unsigned int max_buf_width, unsigned int max_buf_height);
	OP_STATUS Construct(unsigned int buffer_width, unsigned int buffer_height);
	void SetupForExtentCalculation();

	OP_STATUS Analyse(SVGFilterNode* start_node);
	OP_STATUS Evaluate(SVGFilterNode* start_node);

	OP_STATUS GetResult(SVGFilterNode* node,
						VEGARenderTarget*& res_surface, OpRect& src_rect, SVGRect& rect);

	SVGFilterNodeInfo* GetImage(SVGFilterNode* context_node, SVGFilterNode* ref);
	SVGFilterNodeInfo* GetSurface(SVGFilterNode* node);
	SVGFilterNodeInfo* GetSurfaceDirty(SVGFilterNode* node);

	void ConvertSurface(SVGFilterNodeInfo* surface, ColorSpaceType new_cstype);
	UINT32 ConvertColor(UINT32 color, ColorSpaceType new_cstype);
	void ConvertToAlphaSurface(SVGFilterNodeInfo* src, SVGFilterNodeInfo* dst, const OpRect& conv_area);

	OP_STATUS CopySurface(SVGFilterNodeInfo* dst, SVGFilterNodeInfo* src,
						  const OpRect& src_area, VEGARenderer* renderer = NULL);
	void ClearSurface(SVGFilterNodeInfo* nodeinfo, OpRect cl_rect,
					  UINT32 color_value = 0, VEGARenderer* renderer = NULL);
	void ClearSurfaceInv(SVGFilterNodeInfo* nodeinfo, const OpRect& inv_rect);

	OpRect ResolveArea(const SVGRect& region);

	SVGPainter* GetPainter() { return m_painter; }
	SVGPaintNode* GetPaintNode() { return m_paint_node; }

	VEGARenderer* GetRenderer() { return &m_renderer; }
	const SVGMatrix& GetTransform() const { return m_buffer_xfrm; }
	const OpRect& GetPixelRegion() const { return m_total_area; }

	OP_STATUS SetStoreSize(unsigned int size);

	void IncRef(SVGFilterNode* node) { m_nodeinfo[node->GetNodeId()].ref++; }
	void DecRef(SVGFilterNode* node);

private:
	OP_STATUS BuildConversionFilters();

	VEGARenderer m_renderer;

	OpRect m_total_area;
	OpRect m_clipped_area;

	SVGPainter* m_painter;
	SVGPaintNode* m_paint_node;

	VEGAFilter* m_lin_to_srgb;
	VEGAFilter* m_srgb_to_lin;
	unsigned char* m_s2l_tab;

	// Helpers for Construct
	void ComputeTransform(unsigned int buffer_width, unsigned int buffer_height);
	void CalculateClipIntersection(SVGBoundingBox& bbox);

	SVGRect m_filter_region;
	SVGMatrix m_buffer_xfrm;

	SVGFilterNodeInfo* GetNodeInfo(SVGFilterNode* node);

	BOOL HasResult(SVGFilterNode* node)
	{
		return GetNodeInfo(node)->target != NULL;
	}

	void ClearNodeInfo(); // Fold into destructor?

	SVGFilterNodeInfo* m_nodeinfo;
	unsigned int m_num_nodes;
};

#endif // SVG_SUPPORT_FILTER
#endif // SVG_SUPPORT
#endif // SVG_FILTER_NODE_H
