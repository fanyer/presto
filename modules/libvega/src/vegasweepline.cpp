/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#if defined(VEGA_SUPPORT) && defined(VEGA_3DDEVICE)

#include "modules/libvega/vegapath.h"
#include "modules/libvega/src/vegasweepline.h"

VEGASweepLine::VEGASweepLine(VEGAPath& path)
	: m_path(path), m_numLines(path.getNumLines()), m_numUnique(0), m_maxLines(0)
	, m_vertices(NULL)
	, m_edges(NULL)
	, m_order(NULL)
#ifdef VEGA_DEBUG_SWEEP_LINE_IMAGE_OUTPUT
	, m_renderer(NULL), m_renderTarget(NULL), m_bitmap(NULL)
#endif // VEGA_DEBUG_SWEEP_LINE_IMAGE_OUTPUT
{
	OP_ASSERT(m_path.isClosed());
	OP_ASSERT(!m_path.hasMultipleSubPaths());
}

VEGASweepLine::~VEGASweepLine()
{
	OP_DELETEA(m_order);
	OP_DELETEA(m_edges);
	OP_DELETEA(m_vertices);

#ifdef VEGA_DEBUG_SWEEP_LINE_IMAGE_OUTPUT
	OP_DELETE(m_renderer);
	VEGARenderTarget::Destroy(m_renderTarget);
	OP_DELETE(m_bitmap);
#endif // VEGA_DEBUG_SWEEP_LINE_IMAGE_OUTPUT
}

static
int vertComp(void* arg, const void* p1, const void* p2)
{
	VEGASLVertex* vertices = reinterpret_cast<VEGASLVertex*>(arg);
	const VEGASLVertex& v1 = vertices[*(unsigned short*)p1];
	const VEGASLVertex& v2 = vertices[*(unsigned short*)p2];

	// compare y values if x values are equal
	int r = v1.compare(v2);

	return r;
}

OP_STATUS VEGASweepLine::init()
{
	OP_ASSERT(m_numLines >= 3);

	RETURN_IF_ERROR(initVertices());
	if (m_numUnique < 3)
		// This is a straight line.
		return OpStatus::OK;
	RETURN_IF_ERROR(initEdges());

	// Sort vertices according to x coord.
	op_qsort_s(m_order, m_numUnique, sizeof(*m_order), vertComp, m_vertices);

#ifdef VEGA_DEBUG_SWEEP_LINE_IMAGE_OUTPUT
	RETURN_IF_ERROR(initDebugRT());
#endif // VEGA_DEBUG_SWEEP_LINE_IMAGE_OUTPUT

#ifdef DEBUG_ENABLE_OPASSERT
	// Initialize unused order to access outside allocated data, so
	// asserts can catch this later.
	for (unsigned int i = m_numUnique; i < m_numLines; ++i)
		m_order[i] = USHRT_MAX;
#endif // DEBUG_ENABLE_OPASSERT

	return OpStatus::OK;
}

OP_STATUS VEGASweepLine::initVertices()
{
	OP_NEW_DBG("VEGASweepLine::initVertices", "vega.sweepline");

	OP_ASSERT(!m_vertices && !m_order);

	m_vertices = OP_NEWA(VEGASLVertex, m_numLines);
	RETURN_OOM_IF_NULL(m_vertices);

	// Used to order vertices according to x coord.
	m_order = OP_NEWA(unsigned short, m_numLines);
	RETURN_OOM_IF_NULL(m_order);

	OP_ASSERT(!m_numUnique);

	// Initialize vertices.
	unsigned int pruned = 0;
	VEGA_FIX* p = m_path.getLine(m_numLines-1);
	for (unsigned int i = 0; i < m_numLines; ++i)
	{
		m_vertices[i].idx = i;
		VEGA_FIX* v = m_path.getLine(i);
		m_vertices[i].x = VEGA_FIXTOFLT(v[VEGALINE_STARTX]);
		m_vertices[i].y = VEGA_FIXTOFLT(v[VEGALINE_STARTY]);
		m_vertices[i].startOf = m_vertices[i].endOf = 0;
		m_vertices[i].reflex_handled = false;

		bool redundant = false;

		// Needs special handling if the first vertex was discarded,
		// or v[VEGALINE_END*] won't correspond to the right vertex.
		if (i == m_numLines-1 && m_order[0] > 0 && m_numUnique > 0)
		{
			VEGA_FIX* n = m_path.getLine(m_order[0]);
			VEGA_FIX rv[4] = {
				v[0], v[1],
				n[0], n[1]
			};
			redundant = isRedundant(p, rv);
		}
		else
			redundant = isRedundant(p, v);

		// Get rid of duplicates, redundant vertices and spikes.
		if (redundant)
		{
			OP_DBG(("pruning vertex %u", i));

			++pruned;
			m_order[m_numLines - pruned] = i;
			continue;
		}

		m_order[m_numUnique] = i;
		++m_numUnique;

		p = v;
	}

	// deal with any remaining identical vertex
	if (m_numUnique > 1 && !m_vertices[m_order[0]].compare(m_vertices[m_order[m_numUnique-1]]))
	{
		OP_DBG(("pruning vertex %u - first and last identical", m_order[m_numUnique-1]));

		++pruned;
		--m_numUnique;

		// must keep m_order in order, bubble backwards
		unsigned i = m_numUnique;
		while (i+1 < m_numLines && m_order[i] < m_order[i+1])
		{
			op_swap(m_order[i], m_order[i+1]);
			++i;
		}
		for (i = m_numUnique; i < m_numLines-1; ++i)
			OP_ASSERT(m_order[i] > m_order[i+1]);
	}
	OP_ASSERT(m_numUnique < 2 || m_vertices[m_order[0]].compare(m_vertices[m_order[m_numUnique-1]]));

	OP_ASSERT(m_numUnique + pruned == m_numLines);

	return OpStatus::OK;
}

OP_STATUS VEGASweepLine::initEdges()
{
	OP_NEW_DBG("VEGASweepLine::initEdges", "vega.sweepline");

	OP_ASSERT(!m_edges);

	if (!m_maxLines)
		m_maxLines = m_numLines;
	OP_ASSERT(m_maxLines >= m_numLines);

	// The edges of the path.
	m_edges = OP_NEWA(VEGASLEdge, m_maxLines);
	RETURN_OOM_IF_NULL(m_edges);

	unsigned int pruned = m_numLines - m_numUnique;

	// Initialize edges.
	unsigned int from = m_order[m_numUnique-1];
	for (unsigned int to = 0; to < m_numLines; ++to)
	{
		// to has been pruned, it should never be used
		if (pruned && m_order[m_numUnique + pruned - 1] == to)
		{
			m_edges[to].lt     = NULL;
			m_edges[to].rb     = NULL;
			m_edges[to].first  = NULL;
			m_edges[to].second = NULL;
#ifdef _DEBUG
			m_edges[to].idx = USHRT_MAX;
#endif // _DEBUG

			OP_DBG(("vertex %u has been pruned - skipping", to));

			--pruned;
			continue;
		}

		m_edges[from].init(m_vertices+from, m_vertices+to);
#ifdef _DEBUG
		m_edges[from].idx = from;
#endif // _DEBUG
		m_vertices[from].startOf  = m_edges+from;
		m_vertices[to]  .endOf    = m_edges+from;

		from = to;
	}
	OP_ASSERT(!pruned);

#ifdef _DEBUG
	for (unsigned int i = m_numLines; i < m_maxLines; ++i)
	{
		m_edges[i].idx    = i;
		m_edges[i].lt     = NULL;
		m_edges[i].rb     = NULL;
		m_edges[i].first  = NULL;
		m_edges[i].second = NULL;
	}
#endif // _DEBUG

	return OpStatus::OK;
}

static inline
bool isDuplicate(const VEGA_FIX* p, const VEGA_FIX* v)
{
	return
		(p[VEGALINE_STARTX] == v[VEGALINE_STARTX] &&
		 p[VEGALINE_STARTY] == v[VEGALINE_STARTY]);
}

// true if all three points are on the same line.
static inline
bool isOnLineOrSpike(const VEGA_FIX* p, const VEGA_FIX* v)
{
	const VEGA_DBLFIX dx = VEGA_FIXTODBLFIX(v[VEGALINE_STARTX] - p[VEGALINE_STARTX]);
	const VEGA_DBLFIX dy = VEGA_FIXTODBLFIX(v[VEGALINE_STARTY] - p[VEGALINE_STARTY]);
	const VEGA_DBLFIX px = VEGA_FIXTODBLFIX(v[VEGALINE_ENDX]   - p[VEGALINE_STARTX]);
	const VEGA_DBLFIX py = VEGA_FIXTODBLFIX(v[VEGALINE_ENDY]   - p[VEGALINE_STARTY]);
	return (VEGA_FIXMUL_DBL(px, dy) == VEGA_FIXMUL_DBL(py, dx));
}

bool VEGASweepLine::isRedundant(const VEGA_FIX* p, const VEGA_FIX* v) const
{
	return (isDuplicate(p, v) || isOnLineOrSpike(p, v));
}

void VEGASLEdge::init(VEGASLVertex* from, VEGASLVertex* to)
{
	first  = from;
	second = to;
	const int comp = first->compare(*second);
	OP_ASSERT(comp); // Two consecutive vertices should never be equal.
	if (comp < 0)
	{
		lt = first;
		rb = second;
	}
	else
	{
		lt = second;
		rb = first;
	}
}

// Compute angle to l2, using l1 as center-point. The angle is in
// range [0; 2*PI[, with 0 to the right of center-point, PI/2 straight
// upwards of center-point and so on.
static inline
float vertAngleAbs(const VEGASLVertex* v1, const VEGASLVertex* v2)
{
	const float dx = v2->x - v1->x;
	const float dy = v1->y - v2->y; // y grows downwards.

	float a = (float)op_atan2(dy, dx);
	if (a < 0.f)
		a += 2.f*(float)M_PI;
	return a;
}

// Convert an absolute angle [0; 3*PI[ to relative [-PI; PI[.
static inline
float absToRel(float a)
{
	const float twoPI = 2.f*(float)M_PI;
	if (a >= (float)M_PI)
		a -= twoPI;
	OP_ASSERT(a >= -(float)M_PI && a < (float)M_PI);
	return a;
}

// Convert a relative angle [-2*PI; PI[ to absolute [0; 2*PI[.
static inline
float relToAbs(float r)
{
	const float twoPI = 2.f*(float)M_PI;
	if (r < 0.f)
		r += twoPI;
	OP_ASSERT(r >= 0.f && r < twoPI);
	return r;
}

/* static */
float VEGASLEdge::getAngleDelta(const VEGASLVertex* pivot,
                                 const VEGASLEdge* l1, const VEGASLEdge* l2,
                                 bool l1Dir, bool l2Dir)
{
	OP_ASSERT(l1 != l2);

	const VEGASLEdge* const s1 = l1;
	const VEGASLEdge* const s2 = l2;

	float prev_ang = 0;
	const float twoPI = 2.f*(float)M_PI;

	bool switched = false;

	do
	{
		const VEGASLVertex* p1 = l1->secondPoint(l1Dir);
		const VEGASLVertex* p2 = l2->secondPoint(l2Dir);

		// this direction is a dead end, try other direction and negate result
		if (p1 == p2)
		{
			if (switched)
				return 0.f;

			switched = true;
			prev_ang = 0;
			l1Dir = !l1Dir;
			l2Dir = !l2Dir;
			p1 = l1->secondPoint(l1Dir);
			p2 = l2->secondPoint(l2Dir);
		}

		const float a1abs = vertAngleAbs(pivot, p1);
		float a1 = a1abs;
		a1 -= prev_ang;
		if (a1 < 0.f)
			a1 += twoPI;
		a1 = absToRel(a1);

		float a2 = vertAngleAbs(pivot, p2);
		a2 -= prev_ang;
		if (a2 < 0.f)
			a2 += twoPI;
		a2 = absToRel(a2);

		if (a1 != a2)
			return switched ? (a2-a1) : (a1-a2);
		prev_ang = a1abs;

		const float d1 = pivot->squareDistanceTo(*p1);
		const float d2 = pivot->squareDistanceTo(*p2);

		if (d2 < d1)
		{
			pivot = p2;
			l2 = l2->nextEdge(l2Dir);
		}
		else
		{
			pivot = p1;
			l1 = l1->nextEdge(l1Dir);
			if (d1 == d2)
				l2 = l2->nextEdge(l2Dir);
		}
	}
	while (l1 != l2 && (l1 != s1 || l2 != s2));

	// We've gone full circle, all bets are off.
	return 0.f;
}

float VEGASLEdge::getAngleDelta(const VEGASLEdge& o) const
{
	const VEGASLEdge *l1 = this, *l2 = &o;

	// true if moving in path order.
	const bool l1Dir = (l1->lt == l1->first);
	const bool l2Dir = (l2->lt == l2->first);

	const VEGASLVertex* p1 = l1->firstPoint(l1Dir);
	const VEGASLVertex* p2 = l2->firstPoint(l2Dir);
	const VEGASLVertex* pivot = (p2->compare(*p1) < 0) ? p1 : p1;

	return getAngleDelta(pivot, this, &o, l1Dir, l2Dir);
}

bool VEGASLEdge::above(const VEGASLEdge& o) const
{
	OP_ASSERT(idx != o.idx);

	// This should never be called for intersecting lines!

	if (lt->x > o.lt->x)
		return !o.above(*this);

	// need to use double precision here to stay consistent with
	// isOnLineOrSpike

	// perp-dot
	const double nx = (double)(rb->y - lt->y);
	const double ny = (double)(lt->x - rb->x);
	const VEGASLVertex* verts[] = { o.lt, o.rb };

	for (unsigned int i = 0; i < ARRAY_SIZE(verts); ++i)
	{
		if (verts[i] == lt || verts[i] == rb)
			continue;

		const double dx = (double)(verts[i]->x - lt->x);
		const double dy = (double)(verts[i]->y - lt->y);
		const double ang = nx*dx + ny*dy;
		if (ang)
			return ang < 0;
	}

	const float delta = getAngleDelta(o);
	return delta > 0;
}

static inline
float sign(const float& d)
{
	return d < 0.f ? -1.f : (d > 0.f ? 1.f : 0.f);
}

bool VEGASLEdge::intersects(const VEGASLEdge& o) const
{
	// Two connected edges can never intersect.
	if (connectsTo(o))
		return false;

	const double l1[] = {
		(double)first->x,  (double)first->y,
		(double)second->x, (double)second->y
	};
	const double l2[] = {
		(double)o.first->x,  (double)o.first->y,
		(double)o.second->x, (double)o.second->y
	};
	unsigned edgenum;
	const bool r = intersects(l1, l2, &edgenum);
	if (!edgenum)
		return r;

	// Some vertex on edge, need extra checks to determine
	// intersection.
	// Gist: Traverse the four neighbours of vertex (two direct, two
	// from edge) in some direction (cw or ccw). If they occur as
	// pairs there's no intersection, if they're interleaved there is.

	const VEGASLVertex* vertex; // Vertex on edge.
	const VEGASLEdge*   edge;   // Edge with vertex on it.
	switch (edgenum)
	{
	case 1:
		vertex = this->first;
		edge = &o;
		break;
	case 2:
		vertex = this->second;
		edge = &o;
		break;
	case 3:
		vertex = o.first;
		edge = this;
		break;
	case 4:
		vertex = o.second;
		edge = this;
		break;
	default:
		OP_ASSERT(!"expected a valid vertex index");
		return false;
	}

	// "Neighbouring edges" to edge, needed for breaking ties. For
	// cases where vertex is on an endpoint of edge (else block) these
	// are the two edges connected at that endpoint. Otherwise they're
	// both the same edge, but traversed in different directions.
	const VEGASLEdge* edge_neighbour_edges[2];
	if (vertex->compare(*edge->first) && vertex->compare(*edge->second))
	{
		edge_neighbour_edges[0] = edge;
		edge_neighbour_edges[1] = edge;
	}
	else
	{
		const VEGASLVertex* identical = vertex->compare(*edge->first) ? edge->second : edge->first;
		OP_ASSERT(!vertex->compare(*identical));
		edge_neighbour_edges[0] = identical->endOf;
		edge_neighbour_edges[1] = identical->startOf;
	}

	// These are the four neighbours to vertex.
	const VEGASLVertex* vert_neighbours[] = { vertex->startOf->second, vertex->endOf->first };
	const VEGASLVertex* edge_neighbours[] = { edge_neighbour_edges[0]->first, edge_neighbour_edges[1]->second };

	// All measured angles are made relative to this one.
	const float norm = vertAngleAbs(vertex, vert_neighbours[0]);
	// This is the reference. We check whether both neighbours of edge
	// are on the same side of this.
	const float ref  = relToAbs(vertAngleAbs(vertex, vert_neighbours[1]) - norm);

	// Used for breaking ties.
	const float halfway = ref / 2.f;
	OP_ASSERT(halfway);

	// Angle to edge neighbours.
	float a[2];

	for (unsigned i = 0; i < 2; ++i)
	{
		a[i] = relToAbs(vertAngleAbs(vertex, edge_neighbours[i]) - norm);

		const VEGASLEdge* e1;
		if (!a[i]) // Angle identical to norm.
			e1 = vertex->startOf;
		else if (a[i] == ref)
			e1 = vertex->endOf;
		else
			continue;

		// Reaching this point means angle to current neighbour of
		// edge was identical to norm or ref. Tie is broken using
		// getAngleDelta.
		const VEGASLEdge* e2 = edge_neighbour_edges[i];

		const bool d1 = (e1 == vertex->startOf);
		const bool d2 = (i == 0);
		const bool same_direction =
			(e1->lt == vertex) ==
			(e2->lt == edge_neighbours[i]);

		const float tiebreak = getAngleDelta(vertex, e1, e2, d1, same_direction == d2);
		if (!tiebreak) // loop, treat as intersecting
			return true;

		a[i] -= sign(tiebreak) * halfway;
		if (a[i] >= (float)M_PI)
			a[i] -= 2.f*(float)M_PI;
		a[i] = relToAbs(a[i]);
	}

	OP_ASSERT(a[0] && a[0] != ref);
	OP_ASSERT(a[1] && a[1] != ref);

	// Intersecting if edge_neighbour_edges on different sides of ref.
	return ((a[0] > ref) != (a[1] > ref));
}

/* static */
bool VEGASLEdge::intersects(const double* l1, const double* l2, unsigned* edgenum/* = NULL*/)
{
	// Intersection is at p1 + fact*(p2-p1), where fact is num/den.

	if (edgenum)
		*edgenum = 0;

	// doubles are called for here, or we might miss intersection due
	// to precision loss - see 'biglayouttree'

	const double den =
		(l2[VEGALINE_ENDY]-l2[VEGALINE_STARTY])*
		(l1[VEGALINE_ENDX]-l1[VEGALINE_STARTX]) -
		(l2[VEGALINE_ENDX]-l2[VEGALINE_STARTX])*
		(l1[VEGALINE_ENDY]-l1[VEGALINE_STARTY]);

	// Two perpendicular lines can never intersect.
	if (!den)
		return false;

	const double num1 =
		(l2[VEGALINE_ENDX]  -l2[VEGALINE_STARTX])*
		(l1[VEGALINE_STARTY]-l2[VEGALINE_STARTY]) -
		(l2[VEGALINE_ENDY]  -l2[VEGALINE_STARTY])*
		(l1[VEGALINE_STARTX]-l2[VEGALINE_STARTX]);

	const double num2 =
		(l1[VEGALINE_ENDX]  -l1[VEGALINE_STARTX])*
		(l1[VEGALINE_STARTY]-l2[VEGALINE_STARTY]) -
		(l1[VEGALINE_ENDY]  -l1[VEGALINE_STARTY])*
		(l1[VEGALINE_STARTX]-l2[VEGALINE_STARTX]);

	if (edgenum)
	{
		if (!num1 || !num2)
		{
			*edgenum = num1 ? 3 : 1;
			return false;
		}

		if (num1 == den || num2 == den)
		{
			*edgenum = (num2 == den) ? 4 : 2;
			return false;
		}
	}

	// num / den in ]0; 1[ => intersection between endpoints
	if (den > 0)
	{
		if (num1 <= 0 || num1 >= den ||
		    num2 <= 0 || num2 >= den)
			return false;
	}
	else
	{
		if (num1 >= 0 || num1 <= den ||
		    num2 >= 0 || num2 <= den)
			return false;
	}
	return true;
}

void VEGASLVerticalEdges::insert(VEGASLEdge* l)
{
	OP_ASSERT(l);

	for (VEGASLEdge* e = First(); e; e = e->Suc())
	{
		if (l->above(*e))
		{
			l->Precede(e);
			return;
		}
	}
	l->Into(this);
}

OP_BOOLEAN VEGASelfIntersect::isSelfIntersecting()
{
	RETURN_IF_ERROR(init());

#ifdef VEGA_DEBUG_SELF_INTERSECT_IMAGE_OUTPUT_OUTLINE
	DebugDumpOutline();
#endif // VEGA_DEBUG_SELF_INTERSECT_IMAGE_OUTPUT_OUTLINE

	// Path is made up of connected edges, so at least three are
	// needed for intersection to be possible.
	if (m_numUnique < 3)
		return OpBoolean::IS_FALSE;

	// FIXME: This should be a balanced tree!
	VEGASLVerticalEdges active_edges;

	OP_NEW_DBG("VEGASelfIntersect::isSelfIntersecting", "vega.selfintersect");

	for (unsigned int v = 0; v < m_numUnique; ++v)
	{
		const unsigned int i = m_order[v];
		OP_ASSERT(i < m_numLines);

		OP_DBG(("* visiting vertex %u", i));

		VEGASLVertex* p = m_vertices+i;

		// Since it's a closed path there are two edges for each vertex.
		VEGASLEdge* s[] = {
			p->startOf,
			p->endOf
		};

		for (unsigned int j = 0; j < ARRAY_SIZE(s); ++j)
		{
			VEGASLEdge* si = s[j];
			OP_ASSERT(si->idx < m_numLines);
			const bool left = si->lt == p;

			if (left)
				active_edges.insert(si);

			OP_DBG(("* processing edge %u", si->idx));
			for (VEGASLEdge* e = active_edges.First(); e; e = e->Suc())
				OP_DBG(("  edge %3u", e->idx));

			VEGASLEdge* prev_edge = si->Pred();
			VEGASLEdge* next_edge = si->Suc();

			if (!left)
				si->Out();

			bool moved;
			do
			{
				if (left)
				{
					if (prev_edge && si->intersects(*prev_edge))
					{
						OP_DBG(("  edge %3u intersects edge %3u\n", si->idx, prev_edge->idx));
						return OpBoolean::IS_TRUE;
					}
					if (next_edge && si->intersects(*next_edge))
					{
						OP_DBG(("  edge %3u intersects edge %3u\n", si->idx, next_edge->idx));
						return OpBoolean::IS_TRUE;
					}
				}
				else
				{
					if (prev_edge && next_edge && prev_edge->intersects(*next_edge))
					{
						OP_DBG(("  edge %3u intersects edge %3u\n", prev_edge->idx, next_edge->idx));
						return OpBoolean::IS_TRUE;
					}
				}

				// HACK: Because we support duplicate edges we have to
				// follow the chains here.
				moved = false;
				if (VEGASLEdge* tmp = (prev_edge ? prev_edge->Pred() : NULL))
				{
					if (prev_edge->identicalTo(*tmp))
					{
						OP_DBG(("  advancing prev_edge"));
						prev_edge = tmp;
						moved = true;
					}
				}
				if (VEGASLEdge* tmp = ((!moved && next_edge) ? next_edge->Suc() : NULL))
				{
					if (next_edge->identicalTo(*tmp))
					{
						OP_DBG(("  advancing next_edge"));
						next_edge = tmp;
						moved = true;
					}
				}
			} while (moved);

		}
	}

	return OpBoolean::IS_FALSE;
}

#endif // VEGA_SUPPORT && VEGA_3DDEVICE
