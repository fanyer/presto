/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "modules/libvega/vegaconfig.h"

#if defined(VEGA_SUPPORT) && defined(VEGA_3DDEVICE)
# include "modules/libvega/src/vegatriangulate.h"

VEGATriangulator::VEGATriangulator(VEGAPath& path)
	: VEGASweepLine(path)
	, m_upper(NULL)
	, m_trapezoids(NULL)
	, m_reflexTrapezoids(NULL)
	, m_reflexEdges(NULL)
	, m_chains(NULL)

	  // FIXME: these are probably too big, maybe keep growing buffers instead?
	, m_maxTrapezoids(m_numLines)
	, m_maxReflexTrapezoids(m_numLines)
	, m_maxReflexEdges(m_numLines)

	, m_numTrapezoids(0)
	, m_numReflexTrapezoids(0)
	, m_numReflexEdges(0)
	, m_numChains(0)
{
}

VEGATriangulator::~VEGATriangulator()
{
	for (unsigned int i = 0; i < m_numChains; ++i)
		m_chains[i].RemoveAll();
	OP_DELETEA(m_chains);
	OP_DELETEA(m_trapezoids);
	OP_DELETEA(m_upper);
}

OP_STATUS VEGATriangulator::init()
{
	m_maxLines = m_numLines + m_maxReflexEdges;

	RETURN_IF_ERROR(VEGASweepLine::init());

	m_upper = OP_NEWA(bool, m_numLines);
	RETURN_OOM_IF_NULL(m_upper);

	const unsigned int total_trapezoids = m_maxTrapezoids + m_maxReflexTrapezoids;
	m_trapezoids = OP_NEWA(VEGATrapezoid, total_trapezoids);
	RETURN_OOM_IF_NULL(m_trapezoids);

	m_reflexTrapezoids = m_trapezoids + m_maxTrapezoids;

	m_reflexEdges = m_edges + m_numLines;

	// Lists of trapezoid runs. Each trapezoid not connected to a
	// previous trapezoid is added as a new chain.
	m_chains = OP_NEWA(List<VEGATrapezoid>, total_trapezoids);
	RETURN_OOM_IF_NULL(m_chains);

#ifdef _DEBUG
	for (unsigned i = 0; i < total_trapezoids; ++i)
		m_trapezoids[i].idx = i;
#endif // _DEBUG

	return OpStatus::OK;
}

static inline
bool linksTo(VEGATrapezoid* l, VEGATrapezoid* r)
{
	return (l->upper == r->upper || l->lower == r->lower);
}

VEGATrapezoid* VEGATriangulator::findChain(VEGATrapezoid* t)
{
	unsigned int c = 0;
	VEGATrapezoid* e;
	for (; c < m_numChains; ++c)
	{
		e = m_chains[c].Last();
		if (linksTo(e, t))
			return e;
	}
	return NULL;
}

bool VEGATriangulator::linkTrapezoid(VEGATrapezoid* t, VEGATrapezoid* chain_end/* = NULL*/)
{
	OP_NEW_DBG("VEGATriangulator::linkTrapezoid", "vega.triangulate");

	// Find any trapezoid chain that t connects to.
	if (!chain_end)
		chain_end = findChain(t);

	// Deal with left reflex vertices from here, to make sure none are
	// missed because of adding trapezoids due to right reflex
	// vertices.
	if (chain_end && chain_end->splitter && chain_end->splitter->isLeftReflex())
	{
		// Can this ever happen? if so we shouldn't split...
		OP_ASSERT(!(chain_end->upper == t->upper && chain_end->lower == t->lower));

		// This assert will trigger if a splitter could not be
		// determined for a trapezoid that needs to be split due
		// to a reflex vertex. The program will crash. See block
		// attempting to determine splitter in buildTrapezoids.
		OP_ASSERT(chain_end->splitter && t->splitter);

		splitReflex(chain_end, chain_end->splitter, t->splitter);
		chain_end = findChain(t);
	}

	if (chain_end)
	{
		// This trapezoid is the same one as p, do nothing.
		if (chain_end->upper == t->upper && chain_end->lower == t->lower)
		{
			OP_DBG(("  this trapezoid exists already (as %d)", chain_end->idx));
			return false;
		}

		// This trapezoid is connected to p.
		t->Follow(chain_end);
	}
	else
	{
		t->Into(m_chains + m_numChains);
		++m_numChains;
	}

	return true;
}

bool VEGATriangulator::splitReflex(VEGATrapezoid* toSplit, VEGASLVertex* splitStart, VEGASLVertex* splitEnd)
{
	OP_NEW_DBG("VEGATriangulator::split", "vega.triangulate");

	OP_ASSERT(!toSplit->Suc());

#ifdef VEGA_DEBUG_TRIANGULATION_DUBIOUS_ASSERTS
	// check if reflex has been split to already
	for (unsigned int r = m_numLines; r < m_numReflexEdges; ++r)
		if (m_reflexEdges[r].first == splitStart && m_reflexEdges[r].second == splitEnd)
			OP_ASSERT(!"this should not happen");
#endif // VEGA_DEBUG_TRIANGULATION_DUBIOUS_ASSERTS

	splitStart->reflex_handled = true;
	splitEnd->  reflex_handled = true;

	// Create a new edge.
	OP_ASSERT(m_numReflexEdges < m_maxReflexEdges);
	VEGASLEdge* splitEdge = m_reflexEdges+m_numReflexEdges;
	++m_numReflexEdges;
	splitEdge->init(splitStart, splitEnd);
#ifdef _DEBUG
	OP_DBG(("  introducing edge %u, connecting %u and %u", splitEdge->idx, splitStart->idx, splitEnd->idx));
#endif // _DEBUG

	// Create a new trapezoid for the lower part.
	OP_ASSERT(m_numReflexTrapezoids < m_maxReflexTrapezoids);
	VEGATrapezoid* toAdd = m_reflexTrapezoids+m_numReflexTrapezoids;
	++m_numReflexTrapezoids;

	toAdd->upper = splitEdge;
	toAdd->lower = toSplit->lower;
	toAdd->splitter = toSplit->splitter;

	// Modify split trapezoid.
	toSplit->lower = splitEdge;

	OP_DBG(("  modifying trapezoid %u, upper: %u, lower: %u", toSplit->idx, toSplit->upper->idx, toSplit->lower->idx));
	OP_DBG(("  introducing trapezoid %u, upper: %u, lower: %u", toAdd->idx, toAdd->upper->idx, toAdd->lower->idx));

	// Either toSplit or toAdd can remain in the chain toSplit was in.
	if (VEGATrapezoid* prev = toSplit->Pred())
	{
		if (!linksTo(prev, toSplit))
		{
			toSplit->Out();
#ifdef DEBUG_ENABLE_OPASSERT
			const bool new_link =
#endif // DEBUG_ENABLE_OPASSERT
			linkTrapezoid(toSplit);
			OP_ASSERT(new_link);

			toAdd->Follow(prev);
		}
	}

	if (!toAdd->InList())
	{
#ifdef DEBUG_ENABLE_OPASSERT
		const bool new_link =
#endif // DEBUG_ENABLE_OPASSERT
		linkTrapezoid(toAdd);
		OP_ASSERT(new_link);
	}

	return true;
}

OP_STATUS VEGATriangulator::buildTrapezoids()
{
	OP_NEW_DBG("VEGATriangulator::buildTrapezoids", "vega.triangulate");

	// active_edges: Edges of path currently intersected by sweep-line, ordered on y coord.
	VEGASLVerticalEdges active_edges;

#ifdef VEGA_DEBUG_TRIANGULATION_IMAGE_OUTPUT_TRAPEZOID_CHAINS
	DebugDrawOutline(false);
#endif // VEGA_DEBUG_TRIANGULATION_IMAGE_OUTPUT_TRAPEZOID_CHAINS

	// Sweep over vertices in order.
	for (unsigned int v = 0; v < m_numUnique; ++v)
	{
		unsigned int i = m_order[v];
		OP_ASSERT(i < m_numLines);

		// Iterate over all identical vertices. This is done in two
		// steps because old edges must be removed before adding new
		// ones or order might get mixed up.
		const unsigned int ov = v;
		while (1)
		{
			OP_DBG(("visiting vertex %d", i));

			VEGASLVertex* current = m_vertices+i;

			// Assert that no discarded edges are used.
			OP_ASSERT(current->startOf->idx < m_numLines);
			OP_ASSERT(current->endOf  ->idx < m_numLines);

			// Remove edges ending on current vertex.
			active_edges.removeIfEnd(current->startOf, current);
			active_edges.removeIfEnd(current->endOf,   current);

			if (v+1 < m_numUnique && !current->compare(m_vertices[m_order[v+1]]))
			{
				++v;
				i = m_order[v];
			}
			else
				break;
		}
		for (unsigned int j = ov; j <= v; ++j)
		{
			VEGASLVertex* current = m_vertices + m_order[j];
			// Add edges starting from current vertex.
			active_edges.addIfStart (current->startOf, current);
			active_edges.addIfStart (current->endOf,   current);
		}

		if (!active_edges.First())
			break;

		OP_DBG(("traversing %d edges:", active_edges.Cardinal()));
		for (VEGASLEdge* e = active_edges.First(); e; e = e->Suc())
			OP_DBG(("* edge %d", e->idx));

		// Create trapezoids.
		for (VEGASLEdge* l = active_edges.First(); l; /* Consumes two lines each iteration. */)
		{
			VEGASLEdge* upper = l;
			l = l->Suc();
			VEGASLEdge* lower = l;
			// Must be an even number of edges in list at all times.
			if (!l)
				return OpStatus::ERR;
			l = l->Suc();

			OP_ASSERT(m_numTrapezoids < m_maxTrapezoids);
			VEGATrapezoid* cur = m_trapezoids + m_numTrapezoids;
			cur->upper = upper;
			cur->lower = lower;

			cur->splitter = NULL;
			for (unsigned int j = ov; j <= v; ++j)
			{
				VEGASLVertex* s = m_vertices + m_order[j];
				if (s == cur->upper->lt || s == cur->lower->lt)
				{
					cur->splitter = s;
					break;
				}
			}
			if (!cur->splitter)
			{
				if (ov == v)
				{
					cur->splitter = m_vertices + m_order[v];
				}
				else
				{
					// Hopefully a splitter is never needed when it
					// cannot be determined - see below blocks dealing
					// with reflex vertices.
					// OP_ASSERT(!"no splitter was found");
				}
			}

			OP_DBG(("* trapezoid     %d", cur->idx));
			OP_DBG(("  upper line is %d", cur->upper->idx));
			OP_DBG(("  lower line is %d", cur->lower->idx));
			OP_DBG(("  splitter   is %d", cur->splitter ? cur->splitter->idx : -1));

			// Check for reflex vertex.
			VEGATrapezoid* prev = findChain(cur);
			if (prev)
			{
				if (prev->upper == upper && prev->lower == lower)
					continue;

				if (cur->splitter && cur->splitter->isRightReflex())
				{
					// This assert will trigger if a splitter could not be
					// determined for a trapezoid that needs to be split due
					// to a reflex vertex. The program will crash. see block
					// attempting to determine splitter above.
					OP_ASSERT(prev->splitter);
					splitReflex(prev, prev->splitter, cur->splitter);
				}

				prev = NULL; // Things have changed.
			}

			// Update linkage
			if (!linkTrapezoid(cur, prev))
				continue;

			++m_numTrapezoids;
		}
	}

	// May have missed some left reflex vertices since they're one
	// step delayed. These should be split based on trapezoid.
	for (unsigned int c = 0; c < m_numChains; ++c)
	{
		VEGATrapezoid* t = m_chains[c].Last();
		VEGATrapezoid* p = t->Pred();
		if (p)
		{
			if (t->splitter && t->splitter->isLeftReflex())
			{
				VEGASLVertex* splitEnd = t->upper->rb;
				if (t->lower->rb->leftOf(*splitEnd))
					splitEnd = t->lower->rb;
				splitReflex(t, t->splitter, splitEnd);
			}
		}
	}

	OP_ASSERT(!active_edges.Cardinal());
	OP_ASSERT(m_numChains <= m_maxTrapezoids + m_numReflexTrapezoids);
	OP_ASSERT(m_numTrapezoids <= m_maxTrapezoids);
	OP_ASSERT(m_numReflexTrapezoids <= m_maxReflexTrapezoids);
	OP_ASSERT(m_numReflexEdges <= m_maxReflexEdges);

#ifdef _DEBUG
	DebugDumpChains(m_chains, m_numChains);
#endif // _DEBUG

	return OpStatus::OK;
}

unsigned int VEGATriangulator::determineVertexOrder(List<VEGATrapezoid>& trapezoids)
{
	OP_NEW_DBG("VEGATriangulator::determineVertexOrder", "vega.triangulate");

	unsigned int v = 0; // Current vertex

	// FIXME: Apparently it's possible for duplicate vertices to sneak
	// in here, hence all the calls to Compare. Would be nice if it
	// was possible to get rid of them.

	VEGATrapezoid* trapezoid = trapezoids.First();
	if (trapezoid->upper->lt != trapezoid->lower->lt &&
	    trapezoid->upper->lt->compare(*trapezoid->lower->lt))
	{
		unsigned int upperIdx = trapezoid->lower->lt->leftOf(*trapezoid->upper->lt);
		unsigned short idx;
		idx = trapezoid->upper->lt->idx;
		setUpper(idx, true);
		m_order[upperIdx] = idx;
		idx = trapezoid->lower->lt->idx;
		setUpper(idx, false);
		m_order[1-upperIdx] = idx;
		v += 2;
	}
	else
	{
#ifdef VEGA_DEBUG_TRIANGULATION_DUBIOUS_ASSERTS
		OP_ASSERT(trapezoid->upper->lt == trapezoid->lower->lt);
#endif // VEGA_DEBUG_TRIANGULATION_DUBIOUS_ASSERTS

		const unsigned short idx = trapezoid->upper->lt->idx;
		setUpper(idx, true);
		m_order[v] = idx;
		++v;
	}

	while (trapezoid)
	{
		const bool upper = !trapezoid->lower->rb->leftOf(*trapezoid->upper->rb);
		VEGASLVertex* p =  upper ?
			trapezoid->upper->rb :
			trapezoid->lower->rb;

		const int c = p->compare(m_vertices[m_order[v-1]]);
#ifdef VEGA_DEBUG_TRIANGULATION_DUBIOUS_ASSERTS
		OP_ASSERT(c > 0);
#endif // VEGA_DEBUG_TRIANGULATION_DUBIOUS_ASSERTS
		if (c)
		{
			const unsigned short idx = p->idx;
			setUpper(idx, upper);
			m_order[v] = idx;
			++v;
		}

		VEGATrapezoid* next = trapezoid->Suc();

		// Get the last one.
		if (!next)
		{
			VEGASLVertex* o = upper ?
				trapezoid->lower->rb :
				trapezoid->upper->rb;

			if (p != o)
			{
				const int c = o->compare(*p);
#ifdef VEGA_DEBUG_TRIANGULATION_DUBIOUS_ASSERTS
				OP_ASSERT(c > 0);
#endif // VEGA_DEBUG_TRIANGULATION_DUBIOUS_ASSERTS
				if (c)
				{
					const unsigned short idx = o->idx;
					setUpper(idx, !upper);
					m_order[v] = idx;
					++v;
				}
			}
		}

		trapezoid = next;
	}

	// Make sure there are no duplicate vertices.
	for (unsigned int i = 1; i < v; ++i)
		OP_ASSERT(m_vertices[m_order[i]].compare(m_vertices[m_order[i-1]]) > 0);

	OP_DBG(("chain has %lu verts:", v));
	for (unsigned int i = 0; i < v; ++i)
		OP_DBG(("- %d", m_order[i]));

	return v;
}

bool VEGATriangulator::isConvex(unsigned short p1, unsigned short p2, unsigned short p3)
{
	const float nx = m_vertices[p2].y - m_vertices[p1].y;
	const float ny = m_vertices[p1].x - m_vertices[p2].x;
	const float dx = m_vertices[p3].x - m_vertices[p2].x;
	const float dy = m_vertices[p3].y - m_vertices[p2].y;
	const float dist = nx*dx + ny*dy;
	return (dist < 0) == isUpper(p2);
}

int VEGATriangulator::triangulateTrapezoidRun(unsigned int numVerts, unsigned short* tris)
{
	OP_ASSERT(numVerts >= 3);

	// Reusing m_order for vertex stack. Should be safe.
	unsigned short* vertices = m_order;
	unsigned int vertexCount = 2;       // Number of vertices in stack.
	unsigned int currentVertex = 2;     // Index of current input vertex.
	unsigned int outputVertexCount = 0; // Number of vertices output so far.
	int triangleCount = 0;              // Number of triangles output so far.
	while (currentVertex < numVerts)
	{
		OP_ASSERT(vertices+vertexCount <= m_order+currentVertex);

		OP_ASSERT(vertexCount);

		unsigned short p = m_order[currentVertex]; ++currentVertex;
		if (isOpposite(vertices[vertexCount-1], p))
		{
			while (vertexCount > 1)
			{
				tris[outputVertexCount] = vertices[0]; ++outputVertexCount;
				tris[outputVertexCount] = vertices[1]; ++outputVertexCount;
				tris[outputVertexCount] = p;    ++outputVertexCount;
				++triangleCount;
				--vertexCount;
				++vertices;
			}
		}
		else
		{
			while (vertexCount > 1 && isConvex(vertices[vertexCount-2], vertices[vertexCount-1], p))
			{
				tris[outputVertexCount] = vertices[vertexCount-1]; ++outputVertexCount;
				tris[outputVertexCount] = vertices[vertexCount-2]; ++outputVertexCount;
				tris[outputVertexCount] = p;       ++outputVertexCount;
				++triangleCount;
				--vertexCount;
			}
		}
		vertices[vertexCount] = p; ++vertexCount;
	}

#ifndef VEGA_TRIANGULATION_SUPPORT_EMPTY_STRETCHES
	OP_ASSERT(vertexCount < 3);
#endif // VEGA_TRIANGULATION_SUPPORT_EMPTY_STRETCHES
	return triangleCount;
}

OP_STATUS VEGATriangulator::triangulate(unsigned short*& tris, int& numTris)
{
	OP_NEW_DBG("VEGATriangulator::triangulate", "vega.triangulate");

	RETURN_IF_ERROR(init());

	if (m_numUnique < 3)
	{
		tris = NULL;
		numTris = 0;
		return OpStatus::OK;
	}

#ifdef VEGA_DEBUG_TRIANGULATION_IMAGE_OUTPUT_OUTLINE
	DebugDrawOutline();
	DebugDumpRT("triangulated_path_outline");
#endif // VEGA_DEBUG_TRIANGULATION_IMAGE_OUTPUT_OUTLINE

	numTris = m_numUnique-2;
	const unsigned int numVerts = numTris*3;
	tris = OP_NEWA(unsigned short, numVerts);
	RETURN_OOM_IF_NULL(tris);
	OpAutoArray<unsigned short> _tris(tris);

	RETURN_IF_ERROR(buildTrapezoids());

#ifdef _DEBUG
	// startOf and endOf cannot be updated when introducing splits due
	// to reflex vertices, so they should never be used after this
	// point.
	for (unsigned int i = 0; i < m_numLines; ++i)
		m_vertices[i].startOf = m_vertices[i].endOf = 0;
#endif // _DEBUG


#ifdef VEGA_DEBUG_TRIANGULATION_IMAGE_OUTPUT_TRIANGLE_RUNS
	DebugDumpTriangleRuns();
#endif // VEGA_DEBUG_TRIANGULATION_IMAGE_OUTPUT_TRIANGLE_RUNS

	// Walk trapezoid chains.
	int count = 0;
	for (unsigned int i = 0; i < m_numChains; ++i)
	{
		OP_DBG(("processing trapezoid chain %lu", i));

		const unsigned int numVerts = determineVertexOrder(m_chains[i]);

		if (numVerts < 3)
		{
#ifdef VEGA_DEBUG_TRIANGULATION_DUBIOUS_ASSERTS
			OP_ASSERT(!"trapezoid run with no triangles");
#endif // VEGA_DEBUG_TRIANGULATION_DUBIOUS_ASSERTS
			continue;
		}

		// Would lead to buffer overrun.
		if (count + (int)numVerts-2 > numTris)
		{
			OP_ASSERT(!"too many triangles");
			return OpStatus::ERR;
		}

		const int nTris = triangulateTrapezoidRun(numVerts, tris + 3*count);

#ifdef VEGA_TRIANGULATION_SUPPORT_EMPTY_STRETCHES
		if (nTris >  (int)numVerts-2)
#else  // VEGA_TRIANGULATION_SUPPORT_EMPTY_STRETCHES
		if (nTris != (int)numVerts-2)
#endif // VEGA_TRIANGULATION_SUPPORT_EMPTY_STRETCHES
		{
			// This indicates a bug in the triangulation code.
			OP_ASSERT(!"unexpected number of triangles");
			return OpStatus::ERR;
		}
		count += nTris;
	}

	OP_ASSERT(count <= numTris);

#ifndef VEGA_TRIANGULATION_SUPPORT_EMPTY_STRETCHES
	if (count != numTris)
	{
		// This indicates a bug in the triangulation code.
		OP_ASSERT(!"unexpected number of triangles");
		return OpStatus::ERR;
	}
#endif // VEGA_TRIANGULATION_SUPPORT_EMPTY_STRETCHES

#ifdef VEGA_TRIANGULATION_SUPPORT_EMPTY_STRETCHES
	// No point in wasting memory.
	if (count != numTris)
	{
		const unsigned int numVerts = 3*count;
		unsigned short* newTris = OP_NEWA(unsigned short, numVerts);
		RETURN_OOM_IF_NULL(newTris);
		op_memcpy(newTris, tris, numVerts * sizeof(*tris));
		tris = newTris;
		numTris = count;
	}
	else
#endif // VEGA_TRIANGULATION_SUPPORT_EMPTY_STRETCHES
	_tris.release();

	return OpStatus::OK;
}

OP_STATUS VEGAPath::getTriangles(unsigned short** tris, int* numTris)
{
	if (!triangulationData)
	{
		triangulationData = OP_NEW(VEGATriangulationData, ());
		RETURN_OOM_IF_NULL(triangulationData);
	}

	OP_ASSERT(triangulationData);
	if (!triangulationData->isTriangulated())
		RETURN_IF_ERROR(triangulationData->triangulate(this));

	OP_ASSERT(triangulationData->isTriangulated());
	triangulationData->getTriangles(tris, numTris);
	return OpStatus::OK;
}

OP_STATUS VEGAPath::tryToMakeMultipleSubPathsSimple(VEGAPath& target, const bool xorFill)
{
	if (!triangulationData)
	{
		triangulationData = OP_NEW(VEGATriangulationData, ());
		RETURN_OOM_IF_NULL(triangulationData);
	}

	return triangulationData->tryToMakeMultipleSubPathsSimple(this, target, xorFill);
}

bool VEGAPath::isTriangulated() const
{
	return (triangulationData != NULL) && triangulationData->isTriangulated();
}

void VEGAPath::markSubPathInfoInvalid()
{
	if (triangulationData)
		triangulationData->markSubPathInfoInvalid();
}

void VEGAPath::markTriangulationInfoInvalid()
{
	if (triangulationData)
		triangulationData->markTriangulationInfoInvalid();
}

/* Returns true if the start point of line p is strictly inside the triangle
   formed my the start points of lines a, b and c or false otherwise. */
static bool inTriangle(VEGA_FIX* a, VEGA_FIX* b, VEGA_FIX* c, VEGA_FIX* p)
{
	const VEGA_FIX xa = a[VEGALINE_STARTX];
	const VEGA_FIX xb = b[VEGALINE_STARTX];
	const VEGA_FIX xc = c[VEGALINE_STARTX];
	const VEGA_FIX xp = p[VEGALINE_STARTX];
	const VEGA_FIX ya = a[VEGALINE_STARTY];
	const VEGA_FIX yb = b[VEGALINE_STARTY];
	const VEGA_FIX yc = c[VEGALINE_STARTY];
	const VEGA_FIX yp = p[VEGALINE_STARTY];

	// Twice the signed area formed by the triangles abp, bcp and cap.
	const VEGA_FIX abp = VEGA_FIXMUL(xa-xp, yb-ya) - VEGA_FIXMUL(xa-xb, yp-ya);
	const VEGA_FIX bcp = VEGA_FIXMUL(xb-xp, yc-yb) - VEGA_FIXMUL(xb-xc, yp-yb);
	const VEGA_FIX cap = VEGA_FIXMUL(xc-xp, ya-yc) - VEGA_FIXMUL(xc-xa, yp-yc);
	if (abp == 0 || bcp == 0 || cap == 0) // p is on the boundary, considered outside
		return false;

	// Return true if abp, bcp and cap all have the same sign.
	bool posSign = (abp > 0);
	return (posSign == (bcp > 0)) && (posSign == (cap > 0));
}

static inline
VEGA_FIX squareDistBetween(const VEGA_FIX* a, const VEGA_FIX* b)
{
	return
		VEGA_FIXSQR(b[VEGALINE_STARTX]-a[VEGALINE_STARTX]) +
		VEGA_FIXSQR(b[VEGALINE_STARTY]-a[VEGALINE_STARTY]);
}

static inline
bool isDegenerate(const VEGA_FIX* a, const VEGA_FIX* b, const VEGA_FIX* c)
{
	// vertical
	if (a[VEGALINE_STARTX] == b[VEGALINE_STARTX])
		return a[VEGALINE_STARTX] == c[VEGALINE_STARTX];

	const VEGA_FIX slopeAB = VEGA_FIXDIV(
		b[VEGALINE_STARTY] - a[VEGALINE_STARTY],
		b[VEGALINE_STARTX] - a[VEGALINE_STARTX]);
	const VEGA_FIX slopeAC = VEGA_FIXDIV(
		c[VEGALINE_STARTY] - a[VEGALINE_STARTY],
		c[VEGALINE_STARTX] - a[VEGALINE_STARTX]);
	return slopeAB == slopeAC;
}

/** Returns a point that is known to be strictly inside the (closed) path.
 * Should only be called on simple (non-intersecting) paths.
 * @param idxToLeftTopVertex index to left-topmost vertex
 * @param x, y (out) The point found */
static
void getPointInPath(VEGAPath& path, const VEGATriangulationData::SubPath& subPath, unsigned int idxToLeftTopVertex, VEGA_FIX& x, VEGA_FIX& y)
{
	OP_ASSERT(idxToLeftTopVertex >= subPath.startLine &&
			  idxToLeftTopVertex <= subPath.endLine);

	// If the path is just a straight line, return mid point of that line
	if (subPath.numLines() < 3)
	{
		OP_ASSERT(subPath.numLines());
		VEGA_FIX* cl = path.getLine(subPath.startLine);
		x = VEGA_FIXDIV2(cl[VEGALINE_STARTX] + cl[VEGALINE_ENDX]);
		y = VEGA_FIXDIV2(cl[VEGALINE_STARTY] + cl[VEGALINE_ENDY]);
		return;
	}

	// Points with indices abc form a triangle inside the path.
	unsigned int a = idxToLeftTopVertex;
	unsigned int b = subPath.incIdx(a);
	unsigned int c = subPath.decIdx(a);

	VEGA_FIX* vertA = path.getLine(a);
	VEGA_FIX* vertB = path.getLine(b);
	VEGA_FIX* vertC = path.getLine(c);

	// make sure triangle is not degenerate
	while (isDegenerate(vertA, vertB, vertC))
	{
		const VEGA_FIX distAB = squareDistBetween(vertA, vertB);
		const VEGA_FIX distAC = squareDistBetween(vertA, vertC);
		if (distAB < distAC)
		{
			b = subPath.incIdx(b);
			vertB = path.getLine(b);

			a = subPath.decIdx(b);
			vertA = path.getLine(a);
		}
		else
		{
			if (distAB == distAC)
			{
				b = subPath.incIdx(b);
				vertB = path.getLine(b);
			}

			c = subPath.decIdx(c);
			vertC = path.getLine(c);

			a = subPath.incIdx(c);
			vertA = path.getLine(a);
		}
	}

	VEGA_FIX* minVert = NULL; // Vertex closest to vertA inside the triangle.
	VEGA_FIX minSquareDist = 0; // Squared distance to that vertex from vertA.
	for (unsigned int i = subPath.startLine; i <= subPath.endLine; i++)
	{
		if (i == a || i == b || i == c)
			continue;
		VEGA_FIX* cur = path.getLine(i);
		if (inTriangle(vertA, vertB, vertC, cur))
		{
			VEGA_FIX squareDistToA = squareDistBetween(cur, vertA);
			if (!minVert || squareDistToA < minSquareDist)
			{
				minSquareDist = squareDistToA;
				minVert = cur;
			}
		}
	}

	if (!minVert) // No vertex was found in the triangle formed by the points at indices abc, use centroid of that triangle.
	{
		x = (vertA[VEGALINE_STARTX] + vertB[VEGALINE_STARTX] + vertC[VEGALINE_STARTX]) / 3;
		y = (vertA[VEGALINE_STARTY] + vertB[VEGALINE_STARTY] + vertC[VEGALINE_STARTY]) / 3;
	}
	else // Use mid point of line segment between points at indices a and idx.
	{
		x = VEGA_FIXDIV2(vertA[VEGALINE_STARTX] + minVert[VEGALINE_STARTX]);
		y = VEGA_FIXDIV2(vertA[VEGALINE_STARTY] + minVert[VEGALINE_STARTY]);
	}
}

/** Compares the left most vertex of sub path p1 to the left most vertex
 * of sub path p2.
 * @return -1 if the left most vertex of p1 is strictly to the left
 * of the left most vertex of p2 or 1 if it is the other way around.
 * The size of the enclosed area by the paths are used for breaking ties
 * where a path with a larger area is considered smaller. */
static
int subPathComp(const void* p1, const void* p2)
{
	const VEGATriangulationData::SubPath* vp1 = (VEGATriangulationData::SubPath*)p1;
	const VEGATriangulationData::SubPath* vp2 = (VEGATriangulationData::SubPath*)p2;

	if (vp1 == vp2)
		return 0;

	int comp = (vp1->min_x < vp2->min_x) ? -1 : (vp1->min_x > vp2->min_x);
	if (comp)
		return comp;

	comp = (vp1->min_y < vp2->min_y) ? -1 : (vp1->min_y > vp2->min_y);
	if (comp)
		return comp;

	// Choose the one with the largest area to be considered the smaller one.
	VEGA_FIX area1 = VEGA_ABS(vp1->signedArea);
	VEGA_FIX area2 = VEGA_ABS(vp2->signedArea);
	return (area1 > area2) ? -1 : (area1 < area2);
}

// Gist: Traverse all edges and split the path in sub paths.
// Then merge the sub paths by adding corridors with 0 width
// between them and see if the resulting path is self intersecting.
// If it is not then it can be triangulated right away.
// FIXME: If it is then it means that we need to do more work to remove
// the intersections. This problem is not solved yet.
OP_STATUS VEGATriangulationData::tryToMakeMultipleSubPathsSimple(VEGAPath* source, VEGAPath& target, const bool xorFill)
{
	OP_ASSERT(source);
	OP_ASSERT(source->getCategory() == VEGAPath::COMPLEX);
	OP_ASSERT(!source->cannotBeConvertedToSimple());

	if (!subPathsValid)
		RETURN_IF_ERROR(collectSubPathInfo(source));

	OP_ASSERT(subPathsValid);
	RETURN_IF_ERROR(mergeSubPaths(source, target, xorFill));

	const OP_BOOLEAN si = target.isSelfIntersecting();
	RETURN_IF_ERROR(si);

	if (si == OpBoolean::IS_TRUE)
	{
		// No need to try to make the path simple again if we couldn't do it the first time.
		source->cannotBeConverted = true;
		target.m_category = VEGAPath::COMPLEX;
	}
	else
		target.m_category = VEGAPath::SIMPLE;
	return OpStatus::OK;
}

/*
  Finds the left-most point in path. To break ties, the point with the
  smalles y-value is used.
 */
static inline
void getLeftmostPointInPath(VEGAPath& path, const VEGATriangulationData::SubPath& subPath, unsigned int& idx, VEGA_FIX* point)
{
	point[0] = point[1] = VEGA_INFINITY;
	for (unsigned int cl = subPath.startLine; cl <= subPath.endLine; cl++)
	{
		VEGA_FIX* clline = path.getLine(cl);
		if (clline[VEGALINE_STARTX] < point[0] ||
			(clline[VEGALINE_STARTX] == point[0] && clline[VEGALINE_STARTY] < point[1]))
		{
			point[0] = clline[VEGALINE_STARTX];
			point[1] = clline[VEGALINE_STARTY];
			idx = cl;
		}
	}
}

/*
  Add points in subPath to path, staring with point with index
  startIdx. If reverseDir is true points are added in reverse order.
 */
static inline
OP_STATUS addSubPath(VEGAPath& target,
					 const VEGAPath& source, const VEGATriangulationData::SubPath& subPath,
					 const unsigned int startIdx, const bool reverseDir)
{
	OP_ASSERT(startIdx >= subPath.startLine &&
			  startIdx <= subPath.endLine);

	VEGA_FIX* line = source.getLine(startIdx);
	RETURN_IF_ERROR(target.lineTo(line[VEGALINE_STARTX], line[VEGALINE_STARTY]));

	int il = startIdx;
	if (reverseDir)
	{
		do
		{
			il = subPath.decIdx(il);
			line = source.getLine(il);
			RETURN_IF_ERROR(target.lineTo(line[VEGALINE_STARTX], line[VEGALINE_STARTY]));
		} while (il != (int)startIdx);
	}
	else
	{
		do
		{
			line = source.getLine(il);
			RETURN_IF_ERROR(target.lineTo(line[VEGALINE_ENDX], line[VEGALINE_ENDY]));
			il = subPath.incIdx(il);
		} while (il != (int)startIdx);
	}

	return OpStatus::OK;
}

void findCorridorStart(const VEGAPath* source, VEGATriangulationData::SubPath& subPath,
					   const bool reverse,
					   const VEGA_FIX* endPoint,
					   VEGA_FIX* startPoint, unsigned int& idxToStart)
{
	bool inOrder = subPath.addInOrder;
	if (reverse)
		inOrder = !inOrder;

	unsigned int clline = subPath.leftmost_vtx;
	if (!inOrder)
		clline = subPath.decIdx(clline);
	const unsigned int startIdx = clline;

	do
	{
		VEGA_FIX* cl = source->getLine(clline);
		if (cl[VEGALINE_STARTX] > endPoint[VEGALINE_STARTX] &&
			cl[VEGALINE_ENDX] > endPoint[VEGALINE_STARTX])
			; // no point in considering
		else
		{
			VEGA_FIX x = -VEGA_INFINITY;
			VEGA_FIX y = -VEGA_INFINITY; //intersection point
			if (cl[VEGALINE_STARTY] == endPoint[VEGALINE_STARTY] &&
				cl[VEGALINE_STARTX] < endPoint[VEGALINE_STARTX])
			{
				x = cl[VEGALINE_STARTX];
				y = cl[VEGALINE_STARTY];
			}
			else // some extra horizontal length added to avoid intersections at the end points.
				VEGAPath::line_intersection(cl[VEGALINE_STARTX], cl[VEGALINE_STARTY],
											cl[VEGALINE_ENDX], cl[VEGALINE_ENDY],
											MIN(cl[VEGALINE_STARTX], cl[VEGALINE_ENDX]) - VEGA_INTTOFIX(5), endPoint[VEGALINE_STARTY],
											endPoint[VEGALINE_STARTX] + VEGA_INTTOFIX(5), endPoint[VEGALINE_STARTY],
											x, y, false);
			if (x > startPoint[0]) // new intersection point found closer to the corridor end point.
			{
				startPoint[0] = x;
				startPoint[1] = y;
				idxToStart = clline;
			}
		}

		if (inOrder)
			clline = subPath.incIdx(clline);
		else
			clline = subPath.decIdx(clline);
	} while (clline != startIdx);
}

/* Gist: Sort sub paths by leftmost vertex and traverse
 * sub paths left to right, connecting every encountered
 * sub path with its predecessor (now consisting of all sub paths
 * indexed 0 to current-1 after they have been sorted) by a
 * 0-width corridor. All END points considered for new lines
 * are START points of existing lines. */
OP_STATUS VEGATriangulationData::mergeSubPaths(VEGAPath* source, VEGAPath& newPath, const bool xorFill)
{
	OP_ASSERT(subPathsValid);

	// Sort sub paths by leftmost vertex
	op_qsort(subPaths, numSubPaths, sizeof(SubPath), subPathComp);

	// padding added to avoid unnecessary intersections at line
	// segment end points
	VEGA_FIX wallX,tmp;
	source->getBoundingBox(wallX, tmp, tmp, tmp);
	wallX -= VEGA_INTTOFIX(5);

	// Determine corridor start point for each sub-path
	for (unsigned curSubPath = 0; curSubPath < numSubPaths; curSubPath++)
	{
		SubPath* subPath = subPaths+curSubPath;

		if (!subPath->numLines())
			continue;

		// Merge subPath to newPath by adding a horizontal corridor between
		// the leftmost vertex of newPath and a vertex from newPath.
		// The corridor shouldn't add an intersection.

		// end point of corridor (the leftmost vertex).
		unsigned idxToEnd = subPath->leftmost_vtx;
		VEGA_FIX* endPoint = source->getLine(idxToEnd);
		OP_ASSERT(endPoint[VEGALINE_STARTX] < VEGA_INFINITY);

		// We need to know if subPath is enclosed inside of newPath or if they are separate because
		// that determines if subPath should be added clockwise or counterclockwise to bewPath.
		VEGA_FIX x, y; // A point inside rightPath
		getPointInPath(*source, *subPath, idxToEnd, x, y);
		bool endPointInside = false;
		for (unsigned sp = 0; sp < curSubPath; ++sp)
		{
			if (!subPaths[sp].numLines())
				continue;

			if (source->isPointInside(x, y, true, subPaths[sp].startLine, subPaths[sp].endLine))
				endPointInside = !endPointInside;
		}

		subPath->addInOrder = subPath->clockwise();
		if (endPointInside)
		{
			// non-zero fill and same direction makes things tricky
			if (!xorFill && subPath->clockwise())
				return OpStatus::ERR;

			subPath->addInOrder = !subPath->addInOrder;
		}

		// Find start point of corridor by traversing newPath and finding the right most point
		// of its boundary, left of endPoint, that lies on the horizontal line through endPoint.
		VEGA_FIX startPoint[2] = { wallX, endPoint[VEGALINE_STARTY] }; // (x,y) start point of corridor
		unsigned int idxToStart = ~0u;
		for (unsigned sp = 0; sp < curSubPath; ++sp)
		{
			const unsigned n = subPaths[sp].numLines();
			if (!n)
				continue;

			if (subPaths[sp].min_y > endPoint[VEGALINE_STARTY] ||
				subPaths[sp].max_y < endPoint[VEGALINE_STARTY] ||
				subPaths[sp].min_x > endPoint[VEGALINE_STARTX])
				continue;

			findCorridorStart(source, subPaths[sp],
							  endPointInside,
							  endPoint,
							  startPoint, idxToStart);

		}

		OP_ASSERT(startPoint[0] <= endPoint[VEGALINE_STARTX]);
		subPath->corridorStartIdx = idxToStart;
		subPath->corridorStartPoint[0] = startPoint[0];
		subPath->corridorStartPoint[1] = startPoint[1];

		VEGA_FIX startK = VEGA_INTTOFIX(0);
		if (idxToStart != ~0u)
		{
			VEGA_FIX* l = source->getLine(idxToStart);
			startK = VEGA_FIXDIV(startPoint[1]    - l[VEGALINE_STARTY],
								 l[VEGALINE_ENDY] - l[VEGALINE_STARTY]);
		}
		subPath->corridorStartK = startK;
	}

	unsigned sp = 0;
	while (!subPaths[sp].numLines())
		++sp;

	// order the sub-paths for merging
	SubPath orderedSubPaths;
	OP_ASSERT(subPaths[sp].corridorStartIdx == ~0u);
	subPaths[sp].Into(&orderedSubPaths);
	++sp;
	for (; sp < numSubPaths; ++sp)
	{
		if (!subPaths[sp].numLines())
			continue;

		const unsigned startIdx = subPaths[sp].corridorStartIdx;
		if (startIdx == ~0u)
			subPaths[sp].Into(&orderedSubPaths);
		else
		{
			OP_ASSERT(startIdx < source->getNumLines());
			for (unsigned i = sp; i > 0; --i)
			{
				const unsigned pred = i-1;
				if (startIdx >= subPaths[pred].startLine &&
					startIdx <= subPaths[pred].endLine)
				{
					subPaths[pred].AddChild(subPaths+sp);
					break;
				}
			}
		}
		OP_ASSERT(subPaths[sp].InList());
	}

	newPath.prepare(source->getNumLines() + 3*numSubPaths);
	const SubPath* subPath = orderedSubPaths.First();
	RETURN_IF_ERROR(newPath.moveTo(subPath->corridorStartPoint[0],
								   subPath->corridorStartPoint[1]));
	RETURN_IF_ERROR(mergeSubPathsR(source, newPath, subPath));
	orderedSubPaths.clearTree();
	return newPath.close(false);
}

// static
OP_STATUS VEGATriangulationData::mergeSubPathsR(const VEGAPath* source,
												VEGAPath& target,
												const SubPath* subPath)
{
	OP_ASSERT(subPath);
	OP_ASSERT(subPath->numLines());

	unsigned i = subPath->leftmost_vtx;
	VEGA_FIX* l = source->getLine(i);

	RETURN_IF_ERROR(target.lineTo(subPath->corridorStartPoint[0],
								  subPath->corridorStartPoint[1]));
	RETURN_IF_ERROR(target.lineTo(l[VEGALINE_STARTX],
								  l[VEGALINE_STARTY]));

	const SubPath* suc = (const SubPath*)subPath->FirstChild();
	if (subPath->addInOrder)
	{
		do
		{
			while (suc && i == suc->corridorStartIdx) // time to change path?
			{
				RETURN_IF_ERROR(mergeSubPathsR(source, target, suc));
				suc = suc->Suc();
			}

			l = source->getLine(i);
			RETURN_IF_ERROR(target.lineTo(l[VEGALINE_ENDX], l[VEGALINE_ENDY]));

			i = subPath->incIdx(i);
		} while (i != subPath->leftmost_vtx);
	}
	else
	{
		do
		{
			i = subPath->decIdx(i);

			while (suc && i == suc->corridorStartIdx) // time to change path?
			{
				RETURN_IF_ERROR(mergeSubPathsR(source, target, suc));
				suc = suc->Suc();
			}

			l = source->getLine(i);
			RETURN_IF_ERROR(target.lineTo(l[VEGALINE_STARTX], l[VEGALINE_STARTY]));
		} while (i != subPath->leftmost_vtx);
	}
	OP_ASSERT(!suc);

	RETURN_IF_ERROR(target.lineTo(subPath->corridorStartPoint[0],
								  subPath->corridorStartPoint[1]));

	suc = subPath->Suc();
	if (!suc ||
		// side-wise merging at non-top level is dealt with through recursion above
		suc->corridorStartIdx != ~0u)
		return OpStatus::OK;

	return mergeSubPathsR(source, target, suc);
}

OP_STATUS VEGATriangulationData::copy(VEGATriangulationData* other)
{
	OP_ASSERT(other);

	markSubPathInfoInvalid();
	markTriangulationInfoInvalid();

	OP_ASSERT(!subPaths);
	OP_ASSERT(!triangles);

	if (other->trianglesValid)
	{
		if (other->numTriangles)
		{
			OP_ASSERT(other->triangles);
			const size_t numVerts = 3*other->numTriangles;
			triangles = OP_NEWA(unsigned short, numVerts);
			RETURN_OOM_IF_NULL(triangles);
			op_memcpy(triangles, other->triangles, numVerts*sizeof(*triangles));
			numTriangles = other->numTriangles;
		}
		trianglesValid = other->trianglesValid;
	}

	numSubPaths = other->numSubPaths;
	if (other->numSubPaths && other->subPathsValid)
	{
		subPaths = OP_NEWA(SubPath, other->numSubPaths);
		RETURN_OOM_IF_NULL(subPaths);
		for (size_t i = 0; i < other->numSubPaths; i++)
			subPaths[i].copy(other->subPaths[i]);
		subPathsValid = other->subPathsValid;
	}

	return OpStatus::OK;
}

OP_STATUS VEGATriangulationData::triangulate(VEGAPath* path)
{
	OP_ASSERT(path);
	OP_ASSERT(!subPaths);
	OP_ASSERT(!triangles);

	OP_ASSERT(path->getCategory() == VEGAPath::SIMPLE);

#ifdef VEGA_DEBUG_TRIANGULATION_IMAGE_OUTPUT
	// don't allow re-entrant calls to VEGATriangulator::triangulate
	static bool triangulating = false;
	if (triangulating)
		return OpStatus::ERR;
	triangulating = true;
#endif // VEGA_DEBUG_TRIANGULATION_IMAGE_OUTPUT

	unsigned short* tmpTris = NULL;
	int tmpNumTris = 0;
	VEGATriangulator t(*path);
	OP_STATUS status = t.triangulate(tmpTris, tmpNumTris);

#ifdef VEGA_DEBUG_TRIANGULATION_IMAGE_OUTPUT
	triangulating = false;
#endif // VEGA_DEBUG_TRIANGULATION_IMAGE_OUTPUT

	RETURN_IF_ERROR(status);
	triangles = tmpTris;
	numTriangles = tmpNumTris;
	trianglesValid = true;
	return OpStatus::OK;
}

void VEGATriangulationData::getTriangles(unsigned short** tris, int* numTris)
{
	OP_ASSERT(trianglesValid);
	*tris    = triangles;
	*numTris = numTriangles;
}

static inline void consumeWarps(const VEGAPath* path, unsigned& i, const unsigned n)
{
	while (i < n && path->isLineWarp(i))
		++i;
}

static inline void consumeLines(const VEGAPath* path, unsigned& i, const unsigned n)
{
	while (i < n && !path->isLineWarp(i) && (i == 0 || !path->isLineClose(i-1)))
		++i;
}

void VEGATriangulationData::countSubPaths(VEGAPath* path)
{
	// determine number of subpaths
	OP_ASSERT(!numSubPaths);

	unsigned i = 0;
	const unsigned n = path->getNumLines();
	do
	{
		consumeWarps(path, i, n);
		if (i >= n)
			break;
		++i; // don't hang when encountering close not followed by warp
		++numSubPaths;
		consumeLines(path, i, n);
	}
	while (i < n);
}

OP_STATUS VEGATriangulationData::collectSubPathInfo(VEGAPath* path)
{
	OP_ASSERT(path && path->multipleSubPaths);
	OP_ASSERT(!subPaths);
	OP_ASSERT(!triangles);

	countSubPaths(path);
	OP_ASSERT(numSubPaths);

	subPaths = OP_NEWA(SubPath, numSubPaths);
	RETURN_OOM_IF_NULL(subPaths);

	unsigned i = 0;
	const unsigned n = path->getNumLines();
	for (unsigned int sp = 0; sp < numSubPaths; ++sp)
	{
		consumeWarps(path, i, n);
		OP_ASSERT(i < n);
		subPaths[sp].startLine = i;

		VEGA_FIX* l = path->getLine(i);
		subPaths[sp].min_x = subPaths[sp].max_x = l[VEGALINE_STARTX];
		subPaths[sp].min_y = subPaths[sp].max_y = l[VEGALINE_STARTY];
		VEGA_FIX min_y = l[VEGALINE_STARTY];
		subPaths[sp].leftmost_vtx = i;

		++i; // needed to support close not followed by warp

		consumeLines(path, i, n);
		subPaths[sp].endLine = i-1;

		subPaths[sp].signedArea = VEGA_FIXMUL(l[VEGALINE_ENDX] - l[VEGALINE_STARTX],
											  l[VEGALINE_ENDY] + l[VEGALINE_STARTY]);

		for (unsigned p = subPaths[sp].startLine+1; p <= subPaths[sp].endLine; ++p)
		{
			l = path->getLine(p);

			if (l[VEGALINE_STARTX] < subPaths[sp].min_x)
			{
				subPaths[sp].min_x = l[VEGALINE_STARTX];
				subPaths[sp].leftmost_vtx = p;
				min_y = l[VEGALINE_STARTY];
			}
			else if (l[VEGALINE_STARTX] == subPaths[sp].min_x)
			{
				if (l[VEGALINE_STARTY] < min_y)
				{
					subPaths[sp].leftmost_vtx = p;
					min_y = l[VEGALINE_STARTY];
				}
			}

			subPaths[sp].max_x = MAX(subPaths[sp].max_x, l[VEGALINE_STARTX]);

			subPaths[sp].min_y = MIN(subPaths[sp].min_y, l[VEGALINE_STARTY]);
			subPaths[sp].max_y = MAX(subPaths[sp].max_y, l[VEGALINE_STARTY]);

			/* The area of a polygon is calculated by going through all edges
			 * and summarize the result from (b.x - a.x) * (b.y + a.y) where
			 * the edge goes from point a to point b. The result should then
			 * be divided by 2 (which is unneccessary in our case since we are
			 * only interested in the sign of the sum, hence the "twice"). */
			subPaths[sp].signedArea +=
				VEGA_FIXMUL(l[VEGALINE_ENDX] - l[VEGALINE_STARTX],
							l[VEGALINE_ENDY] + l[VEGALINE_STARTY]);
		}
	}

	subPathsValid = true;
	return OpStatus::OK;
}

void VEGATriangulationData::markSubPathInfoInvalid()
{
	OP_DELETEA(subPaths);
	subPaths = NULL;
	numSubPaths = 0;
	subPathsValid = false;
}

void VEGATriangulationData::markTriangulationInfoInvalid()
{
	OP_DELETEA(triangles);
	triangles = NULL;
	numTriangles = 0;
	trianglesValid = false;
}

static inline
unsigned int getOrderedIdx(unsigned int idx, unsigned int offset, unsigned int count)
{
	int orderedIdx = idx - offset;
	if (orderedIdx < 0)
		orderedIdx += count;
	OP_ASSERT(orderedIdx >= 0 && (unsigned int)orderedIdx < count);
	return (unsigned int)orderedIdx;
}

void VEGATriangulationData::SubPath::AddChild(SubPath* subPath)
{
	const unsigned int subPathIdx = getOrderedIdx(subPath->corridorStartIdx,
												  leftmost_vtx,
												  numLines());

	for (SubPath* s = FirstChild(); s; s = s->Suc())
	{
		const unsigned int candIdx = getOrderedIdx(s->corridorStartIdx,
												   leftmost_vtx,
												   numLines());

		if (addInOrder ?
			(subPathIdx < candIdx) :
			(subPathIdx > candIdx))
		{
			subPath->Precede(s);
			return;
		}

		if (subPath->corridorStartIdx == s->corridorStartIdx &&
			(addInOrder ?
			 subPath->corridorStartK <= s->corridorStartK :
			 subPath->corridorStartK >= s->corridorStartK))
		{
			subPath->Precede(s);
			return;
		}
	}

	subPath->Under(this);
}

#endif // VEGA_SUPPORT && VEGA_3DDEVICE
