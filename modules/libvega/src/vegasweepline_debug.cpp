/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#if defined(VEGA_SUPPORT) && defined(VEGA_3DDEVICE)

# ifdef _DEBUG
#  include "modules/libvega/src/vegasweepline.h"
#  include "modules/libvega/src/vegatriangulate.h"
#  ifdef VEGA_DEBUG_SWEEP_LINE_IMAGE_OUTPUT
#   include "modules/img/imagedump.h"
#   include "modules/libvega/vegapath.h"
#   include "modules/libvega/vegarenderer.h"
#   include "modules/libvega/src/canvas/canvas.h"
#   include "modules/libvega/src/canvas/canvascontext2d.h"
#   include "modules/libvega/src/oppainter/vegaopfont.h"

const UINT32 g_vega_triangulation_debug_colors[] = {
	0xff0000,
	0x00ff00,
	0x0000ff,
	0xffff00,
	0xff00ff,
	0x00ffff,
};

UINT32 VEGASweepLine::DebugGetColor(unsigned int i, unsigned char alpha/* = 0xff*/)
{
	const unsigned int idx = i % ARRAY_SIZE(g_vega_triangulation_debug_colors);
	return (alpha << 24) | g_vega_triangulation_debug_colors[idx];
}

void VEGASweepLine::DebugDumpRT(const char* name, unsigned idx/* = 0*/)
{
	OpString s;
	RETURN_VOID_IF_ERROR(s.Append(VEGA_DEBUG_SWEEP_LINE_IMAGE_OUTPUT_FOLDER));
	RETURN_VOID_IF_ERROR(s.Append(name));
	if (idx)
	{
		RETURN_VOID_IF_ERROR(s.AppendFormat(UNI_L("_%d"), idx));
	}
	RETURN_VOID_IF_ERROR(s.Append(UNI_L(".png")));
	m_renderer->flush();
	DumpOpBitmapToPNG(m_bitmap, s.CStr(), TRUE);
}

static inline
OP_STATUS pathCircle(VEGAPath& path, const VEGA_FIX cx, const VEGA_FIX cy, const VEGA_FIX r)
{
	RETURN_IF_ERROR(path.prepare(1024));
	RETURN_IF_ERROR(path.moveTo(cx-r, cy));
	RETURN_IF_ERROR(path.arcTo (cx+r, cy, r, r, 0, true, false, VEGA_INTTOFIX(1)));
	RETURN_IF_ERROR(path.arcTo (cx-r, cy, r, r, 0, true, false, VEGA_INTTOFIX(1)));
	return OpStatus::OK;
}

void VEGASweepLine::DebugDrawOutline(bool lines/* = true*/, bool verts/* = true*/, bool nums/* = true*/,
                                     unsigned int from/* = 0*/, unsigned int to/* = 0*/)
{
	if (!to)
		to = m_numUnique;
	OP_ASSERT(to <= m_numUnique);
	OP_ASSERT(from < to);

	m_renderer->setColor(0xffffffff);
	m_renderer->fillRect(0, 0, m_dw, m_dh);

	const short font_size = 8;
	OpAutoPtr<VEGAFont> font(reinterpret_cast<VEGAOpFontManager*>(styleManager->GetFontManager())->GetVegaFont(UNI_L("bitstream vera sans"), font_size, 4, false, false, 0));

	VEGAPath path;

	// larger green dot for the first vertex
	float x, y;
	if (verts)
	{
		x = m_vertices[from].x;
		y = m_vertices[from].y;
		m_renderer->setColor(0xff00ff00);
		m_renderer->setColor(0xff00ff00);

		if (OpStatus::IsSuccess(pathCircle(path, DebugMapX(x), DebugMapY(y), VEGA_INTTOFIX(4))))
			m_renderer->fillPath(&path);
	}

	m_renderer->setColor(0xff000000);

	for (unsigned i = from; i < to; ++i)
	{
		unsigned idx = i;
		x = m_vertices[idx].x;
		y = m_vertices[idx].y;
		const VEGA_FIX sx = DebugMapX(x);
		const VEGA_FIX sy = DebugMapY(y);

		++ idx;
		if (idx >= m_numUnique)
			idx = 0;
		x = m_vertices[idx].x;
		y = m_vertices[idx].y;
		const VEGA_FIX ex = DebugMapX(x);
		const VEGA_FIX ey = DebugMapY(y);

		if (lines)
		{
			if (OpStatus::IsError(path.prepare(3)))
				return;
			path.moveTo(sx, sy);
			path.lineTo(ex, ey);
			VEGAPath stroke;
			if (OpStatus::IsSuccess(path.createOutline(&stroke, VEGA_INTTOFIX(1))))
				m_renderer->fillPath(&stroke);
		}

		if (verts)
		{
			if (OpStatus::IsSuccess(pathCircle(path, sx, sy, VEGA_INTTOFIX(2))))
				m_renderer->fillPath(&path);
		}

		if (nums)
		{
			// FIXME: maybe get normal from connecting lines and place
			// according to that
			uni_char txt[8]; // ARRAY OK 2012-06-19 wonko
			uni_snprintf(txt, ARRAY_SIZE(txt), UNI_L("%u"), i);
			const unsigned py = (2*sy > m_dh) ? sy-font_size : sy+2*font_size;
			if (font.get())
				m_renderer->drawString(font.get(), sx, py, txt, uni_strlen(txt));
		}
	}
}

void VEGASweepLine::DebugDumpPathSegment(unsigned int from, unsigned int to, const char* fn)
{
	float minx, miny, maxx, maxy;

	minx = maxx = m_vertices[from].x;
	miny = maxy = m_vertices[from].y;
	for (unsigned int i = from+1; i <=to; ++i)
	{
		const float& x = m_vertices[i].x;
		if (x < minx)
			minx = x;
		if (x > maxx)
			maxx = x;

		const float& y = m_vertices[i].y;
		if (y < miny)
			miny = y;
		if (y > maxy)
			maxy = y;
	}

	// temporarily override bounding box
	const float
		o_bx = m_bx,
		o_by = m_by,
		o_bw = m_bw,
		o_bh = m_bh;
	m_bx = minx;
	m_by = miny;
	m_bw = maxx - minx;
	m_bh = maxy - miny;

	DebugDrawOutline(true, true, true, from, to+1);

	// restore
	m_bx = o_bx;
	m_by = o_by;
	m_bw = o_bw;
	m_bh = o_bh;

	DebugDumpRT(fn);
}

OP_STATUS VEGASweepLine::initDebugRT()
{
	OP_ASSERT(!m_renderer);
	OP_ASSERT(!m_bitmap);
	OP_ASSERT(!m_renderTarget);

	m_dw = VEGA_DEBUG_SWEEP_LINE_IMAGE_OUTPUT_WIDTH;
	m_dh = VEGA_DEBUG_SWEEP_LINE_IMAGE_OUTPUT_HEIGHT;
	m_px = VEGA_DEBUG_SWEEP_LINE_IMAGE_OUTPUT_PAD_X;
	m_py = VEGA_DEBUG_SWEEP_LINE_IMAGE_OUTPUT_PAD_Y;

	m_renderer = OP_NEW(VEGARenderer, ());
	RETURN_IF_ERROR(m_renderer->Init(m_dw, m_dh));

	RETURN_IF_ERROR(OpBitmap::Create(&m_bitmap, m_dw, m_dh));
	RETURN_IF_ERROR(m_renderer->createBitmapRenderTarget(&m_renderTarget, m_bitmap));
	m_renderer->setRenderTarget(m_renderTarget);

	m_path.getBoundingBox(m_bx, m_by, m_bw, m_bh);
	return OpStatus::OK;
}

#   ifdef VEGA_DEBUG_SELF_INTERSECT_IMAGE_OUTPUT_OUTLINE
void VEGASelfIntersect::DebugDumpOutline()
{
	static bool checking = false;
	// path-dumping code breaks on re-entry
	if (!checking)
	{
		checking = true;
		DebugDrawOutline();
		DebugDumpRT("si_outline");
		checking = false;
	}
}
#   endif // VEGA_DEBUG_SELF_INTERSECT_IMAGE_OUTPUT_OUTLINE

#   ifdef VEGA_DEBUG_TRIANGULATION_IMAGE_OUTPUT_TRIANGLE_RUNS
void VEGATriangulator::DebugDrawTriangles(const unsigned short* vertexIndices, unsigned numTris, unsigned idx)
{
	VEGAPath path;
	float x, y;
	for (unsigned i = 0; i < numTris; ++i)
	{
		if (OpStatus::IsError(path.prepare(4)))
		    return;
		x = m_vertices[vertexIndices[3*i+0]].x;
		y = m_vertices[vertexIndices[3*i+0]].y;
		path.moveTo(DebugMapX(x), DebugMapY(y));
		x = m_vertices[vertexIndices[3*i+1]].x;
		y = m_vertices[vertexIndices[3*i+1]].y;
		path.lineTo(DebugMapX(x), DebugMapY(y));
		x = m_vertices[vertexIndices[3*i+2]].x;
		y = m_vertices[vertexIndices[3*i+2]].y;
		path.lineTo(DebugMapX(x), DebugMapY(y));
		path.close(true);

		m_renderer->setColor(DebugGetColor(idx, 0x40));
		m_renderer->fillPath(&path);

		VEGAPath stroke;
		if (OpStatus::IsSuccess(path.createOutline(&stroke, VEGA_INTTOFIX(1))))
		{
			m_renderer->setColor(DebugGetColor(idx));
			m_renderer->fillPath(&stroke);
		}
	}
}

void VEGATriangulator::DebugDumpTriangleRuns()
{
	const int numTris = m_numUnique-2;
	const unsigned int numVerts = numTris*3;
	unsigned short* tris = OP_NEWA(unsigned short, numVerts);
	if (!tris)
		return;
	OpAutoArray<unsigned short> _tris(tris);

	DebugDrawOutline(false);

	int count = 0;
	for (unsigned int i = 0; i < m_numChains; ++i)
	{

		const unsigned int numVerts = determineVertexOrder(m_chains[i]);
		if (numVerts < 3)
			continue;
		const int nTris = triangulateTrapezoidRun(numVerts, tris + 3*count);
		if (count + (int)numVerts-2 <= numTris)
			DebugDrawTriangles(tris+3*count, nTris, i);
		count += nTris;
	}

	DebugDumpRT("triangulated_path_triangle_runs");
}
#   endif // VEGA_DEBUG_TRIANGULATION_IMAGE_OUTPUT_TRIANGLE_RUNS
#  endif // VEGA_DEBUG_SWEEP_LINE_IMAGE_OUTPUT

void DebugDumpEdgesAsTestBlock(const char* name, const VEGASLEdge& e1, const VEGASLEdge& e2, bool firstAbove)
{
	unsigned int n = 1;

	const bool l1StartDir = e1.lt == e1.first;
	const bool l2StartDir = e2.lt == e2.first;
	const VEGASLEdge *l1, *l2;

	// find out how many edges are needed
	l1 = &e1; l2 = &e2;
	if (l1->firstPoint(l1StartDir)->x == l2->firstPoint(l2StartDir)->x &&
	    l1->firstPoint(l1StartDir)->y == l2->firstPoint(l2StartDir)->y)
	{
		while (l1->secondPoint(l1StartDir)->x == l2->secondPoint(l2StartDir)->x &&
		       l1->secondPoint(l1StartDir)->y == l2->secondPoint(l2StartDir)->y)
		{
			l1 = l1->nextEdge(l1StartDir);
			l2 = l2->nextEdge(l2StartDir);
			++ n;
		}
	}

	fprintf(stderr, "\t// %s\n", name);
	fprintf(stderr, "\t{\n");

	fprintf(stderr, "\t\tVEGA_FIX x[] = {\n");
	fprintf(stderr, "\t\t\t");
	l1 = &e1;
	fprintf(stderr, "%f,", l1->firstPoint(l1StartDir)->x);
	for (unsigned int i = 0; i < n; ++i)
	{
		fprintf(stderr, " %f,", l1->secondPoint(l1StartDir)->x);
		l1 = l1->nextEdge(l1StartDir);
	}
	l2 = &e2;
	fprintf(stderr, "\n\t\t\t");
	fprintf(stderr, "%f", l2->firstPoint(l2StartDir)->x);
	for (unsigned int i = 0; i < n; ++i)
	{
		fprintf(stderr, ", %f", l2->secondPoint(l2StartDir)->x);
		l2 = l2->nextEdge(l2StartDir);
	}
	fprintf(stderr, "};\n");

	fprintf(stderr, "\t\tVEGA_FIX y[] = {\n");
	fprintf(stderr, "\t\t\t");
	l1 = &e1;
	fprintf(stderr, "%f,", l1->firstPoint(l1StartDir)->y);
	for (unsigned int i = 0; i < n; ++i)
	{
		fprintf(stderr, " %f,", l1->secondPoint(l1StartDir)->y);
		l1 = l1->nextEdge(l1StartDir);
	}
	l2 = &e2;
	fprintf(stderr, "\n\t\t\t");
	fprintf(stderr, "%f", l2->firstPoint(l2StartDir)->y);
	for (unsigned int i = 0; i < n; ++i)
	{
		fprintf(stderr, ", %f,", l2->secondPoint(l2StartDir)->y);
		l2 = l2->nextEdge(l2StartDir);
	}
	fprintf(stderr, "};\n");

	fprintf(stderr, "\t\tsetupLineSegments(verts, edges,  x, y, %u, %u);\n", n, n);
	fprintf(stderr, "\t\tVEGASLEdge* first  = edges;\n");
	fprintf(stderr, "\t\tVEGASLEdge* second = edges+%u;\n", n);
	if (firstAbove)
		fprintf(stderr, "\t\tverify(first->above(*second));\n");
	else
		fprintf(stderr, "\t\tverify(second->above(*first));\n");
	fprintf(stderr, "\t}\n");
}

void VEGAPath::DebugDumpPathAsTriangulationSelftest(const char* title)
{
	fprintf(stderr, "test(\"%s\")\n", title);
	fprintf(stderr, "{\n");
	fprintf(stderr, "\tconst unsigned numLines = %u;\n", numLines);
	fprintf(stderr, "\tVEGAPath path;\n");
	fprintf(stderr, "\tRETURN_IF_ERROR(path.prepare(numLines+1));\n");
	VEGA_FIX* l = getLine(0);
	fprintf(stderr, "\tverify_success(path.moveTo(VEGA_FLTTOFIX(%11.6ff), VEGA_FLTTOFIX(%11.6ff)));\n", l[VEGALINE_STARTX], l[VEGALINE_STARTY]);
	for (unsigned i = 1; i < numLines; ++i)
	{
		const bool warp  = isLineWarp(i);
		const bool close = isLineClose(i);
		VEGA_FIX* l = getLine(i);
		fprintf(stderr, "\tverify_success(path.%s(VEGA_FLTTOFIX(%11.6ff), VEGA_FLTTOFIX(%11.6ff)));\n",
					warp ? "moveTo" : "lineTo", l[VEGALINE_STARTX], l[VEGALINE_STARTY]);
		if (close)
			fprintf(stderr, "\tverify_success(path.close(%s));\n",
					warp ? "false" : "true");
	}
	fprintf(stderr, "\tverify(path.getCategory() == VEGAPath::SIMPLE);\n");
	fprintf(stderr, "\tverify(path.getNumLines() == numLines);\n");
	fprintf(stderr, "\tverify_success(verifyPath(path));\n");
	fprintf(stderr, "}\n");
}

void VEGATriangulator::DebugDumpChains(List<VEGATrapezoid>*& chains, unsigned int numChains)
{
	VEGAPath path;
	VEGAPath stroke;

	OP_NEW_DBG("VEGATriangulator::DebugDumpChains", "vega.triangulate");
	for (unsigned int i = 0; i < numChains; ++i)
	{
		OP_DBG(("trapezoid chain %lu", i));
		for (VEGATrapezoid* t = chains[i].First(); t; t = t->Suc())
		{
			OP_DBG(("* trapezoid %d", t->idx));
			OP_DBG(("  upper edge is %d", t->upper->idx));
			OP_DBG(("  lower edge is %d", t->lower->idx));
		}

#  ifdef VEGA_DEBUG_TRIANGULATION_IMAGE_OUTPUT_TRAPEZOID_CHAINS
		DebugDrawOutline();
		m_renderer->setColor(DebugGetColor(i));

		VEGATrapezoid* t;

		// upper
		if (OpStatus::IsError(path.prepare(chains[i].Cardinal()+1)))
			continue;
		t = chains[i].First();
		path.moveTo(DebugMapX(t->upper->lt->x), DebugMapY(t->upper->lt->y));
		for (; t; t = t->Suc())
			path.lineTo(DebugMapX(t->upper->rb->x), DebugMapY(t->upper->rb->y));
		if (OpStatus::IsSuccess(path.createOutline(&stroke, VEGA_INTTOFIX(1))))
		    m_renderer->fillPath(&stroke);

		// lower
		if (OpStatus::IsError(path.prepare(chains[i].Cardinal()+4)))
			continue;
		t = chains[i].First();
		path.moveTo(DebugMapX(t->upper->lt->x), DebugMapY(t->upper->lt->y));
		path.lineTo(DebugMapX(t->lower->lt->x), DebugMapY(t->lower->lt->y));
		for (; t->Suc(); t = t->Suc())
			path.lineTo(DebugMapX(t->lower->rb->x), DebugMapY(t->lower->rb->y));
		path.lineTo(DebugMapX(t->lower->rb->x), DebugMapY(t->lower->rb->y));
		path.lineTo(DebugMapX(t->upper->rb->x), DebugMapY(t->upper->rb->y));
		if (OpStatus::IsSuccess(path.createOutline(&stroke, VEGA_INTTOFIX(1))))
			m_renderer->fillPath(&stroke);

		DebugDumpRT("triangulated_path_chain", i+1);
#  endif // VEGA_DEBUG_TRIANGULATION_IMAGE_OUTPUT_TRAPEZOID_CHAINS
	}
}
# endif // _DEBUG
#endif // VEGA_SUPPORT && VEGA_3DDEVICE
