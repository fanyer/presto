/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef VEGA_SWEEP_LINE_H
#define VEGA_SWEEP_LINE_H

#include "modules/libvega/vegafixpoint.h"
#include "modules/libvega/vegapath.h"

#ifdef _DEBUG
# include "modules/libvega/src/vegasweepline_debug.h"
#endif // _DEBUG

struct VEGASLVertex;

/** This class represents an edge in the path. It contains pointers to
 * the start and end vertices and a couple of utility functions.
 *
 * HACKS! In order to allow duplicate edges and vertices, the
 * following hacks are present in VEGASLEdge:
 *
 * - above: Determining aboveness for identical edges is based on
 *   (right-wise) successors.
 *
 * - getAngleDelta: In order to avoid hangs some break-conditions
 *   have been added to path-traversal code. */
class VEGASLEdge : public ListElement<VEGASLEdge>
{
public:
#ifdef _DEBUG
	unsigned short idx;
#endif // _DEBUG

	VEGASLVertex* lt; ///< left-or-top vertex
	VEGASLVertex* rb; ///< right-or-bottom vertex

	VEGASLVertex* first;  ///< path-wise start vertex of this edge
	VEGASLVertex* second; ///< path-wise end vertex of this edge

	/// Initialize edge from vertex from to vertex to.
	void init(VEGASLVertex* from, VEGASLVertex* to);
	/// @return true if this line is above the line o
	bool above(const VEGASLEdge& o) const;
	/// @return true if this edge intersects o
	bool intersects(const VEGASLEdge& o) const;

	/// see static getAngleDelta
	float getAngleDelta(const VEGASLEdge& o) const;

	/** Compute difference in angle between two path segments. l1 and
	 * l2 are assumed to be joined insomuch as that at least one
	 * vertex of one edge is on the other edge. The path segments are
	 * traversed until there's a difference in angle between them,
	 * measured by picking a pivot point. See implementation for
	 * details.
	 * @param l1 Edge to traverse until there's a difference in angle.
	 * @param l2 Edge to traverse until there's a difference in angle.
	 * @param l1Dir true if moving from first to second, false otherwise.
	 * @param l2Dir true if moving from first to second, false otherwise. */
	static float getAngleDelta(const VEGASLVertex* pivot,
	                            const VEGASLEdge* l1, const VEGASLEdge* l2,
	                            bool l1Dir, bool l2Dir);

	/** Check for intersection between two line segments. l1 and l2
	 * represent line segments and should contain four values each:
	 * [sx, sy, ex, ey]. If one vertex of one edge is on the other
	 * edge this function returns false, but uses *edgenum (if not
	 * NULL) to indicate which endpoint was on the other edge: 1 for
	 * startpoint of l1, 2 for endpoint of l1, 3 for startpoint of l2,
	 * 4 forendpoint of l2. If this is not the case *edgenum will be
	 * set to 0 (if non-NULL). */
	static bool intersects(const double* l1, const double* l2, unsigned* edgenum = NULL);

	/// @return true if endpoint of one edge is startpoint of the other
	bool connectsTo(const VEGASLEdge& o) const
	{
		return (first == o.second || second == o.first);
	}

	/// @return true if both edges have identical start- and endpoint
	bool identicalTo(const VEGASLEdge& o) const;

	/** get path-wise first point (reversed if dir is false) */
	const VEGASLVertex* firstPoint (const bool dir = true) const { return dir ? first : second; }
	/** get path-wise second point (reversed if dir is false) */
	const VEGASLVertex* secondPoint(const bool dir = true) const { return dir ? second : first; }
	/** get path-wise next edge (reversed if dir is false) */
	const VEGASLEdge* nextEdge(const bool dir = true) const;
};

/** Ordered list of edges. Added edges are assmed to be overlapping
 * along y, and are ordered from highest to lowest. */
class VEGASLVerticalEdges : public List<VEGASLEdge>
{
public:
	~VEGASLVerticalEdges() { RemoveAll(); }
	/// Insert l into head, so that head will remain ordered on y.
	void insert(VEGASLEdge* l);

	/// Add edge if vertex is the start (left-top) of edge.
	void addIfStart(VEGASLEdge* edge, VEGASLVertex* vertex)
	{
		if (edge->lt == vertex)
			insert(edge);
	}

	/// Remove edge if vertex is the end (right-bottom) of edge.
	void removeIfEnd(VEGASLEdge* edge, VEGASLVertex* vertex)
	{
		if (edge->rb == vertex)
		{
			OP_ASSERT(edge->InList());
			edge->Out();
		}
	}
};

static inline
int floatComp(const float a, const float b) { return (a < b) ? -1 : ((a > b) ? 1 : 0); }

/** This class represent a vertex in the path. It contains pointers to
 * the edges it is part of, along with some utility functions. */
struct VEGASLVertex
{
	unsigned short idx; ///< index of this vertex
	float x; ///< x coord of this vertex
	float y; ///< y coord of this vertex

	// NOTE: Only available until call to buildTrapezoids has returned.
	VEGASLEdge* startOf; ///< Edge that this vertex is the start of.
	VEGASLEdge* endOf;   ///< Edge that this vertex is the end of.

	bool reflex_handled; ///< true if this vertex has been used to introduce a reflex edge.

	/** @return
	 * -1 if this vertex is left of or above o,
	 *  0 if they are equal,
	 *  1 if this vertex is right of or below o. */
	int compare(const VEGASLVertex& o) const { return (x == o.x) ? floatComp(y, o.y) : floatComp(x, o.x); }
	/// @return true if this vertex is left of o.
	bool leftOf(const VEGASLVertex& o) const { return compare(o) == -1; }
	/// @return true if this vertex is right of o.
	bool rightOf(const VEGASLVertex& o) const { return compare(o) == 1; }

	// The end points of this vertex' two edges are both to the
	// right of this vertex (ie < with this vertex as middle).
	bool isRightReflex()
	{
		if (reflex_handled)
			return false;
		const VEGASLVertex& v1 = *startOf->second;
		const VEGASLVertex& v2 = *endOf->first;
		return this->leftOf(v1) && this->leftOf(v2);
	}
	// The end points of this vertex' two edges are both to the
	// left of this vertex (ie > with this vertex as middle).
	bool isLeftReflex()
	{
		if (reflex_handled)
			return false;
		const VEGASLVertex& v1 = *startOf->second;
		const VEGASLVertex& v2 = *endOf->first;
		return this->rightOf(v1) && this->rightOf(v2);
	}

	/// Compute the squared distance from this vertex to o.
	float squareDistanceTo(const VEGASLVertex& o) const
	{
		const float dx = o.x - x;
		const float dy = o.y - y;
		return dx*dx + dy*dy;
	}
};

inline bool VEGASLEdge::identicalTo(const VEGASLEdge& o) const
{
	return (!lt->compare(*o.lt) && !rb->compare(*o.rb));
}

inline const VEGASLEdge* VEGASLEdge::nextEdge(const bool dir/* = true*/) const
{
	return dir ? second->startOf : first->endOf;
}

/** Utility class for implementing sweepline-algorithms. This class
 * creates and holds an internal representation of a VEGAPath in the
 * form of a set of VEGASLVertex:es and VEGASLEdge:s. Duplicate
 * vertices and intermediate vertices on line segments will be
 * pruned. The vertices are ordered primarily on x, secondarily on y
 * (if accessed using m_order). */
class VEGASweepLine
{
public:
	VEGASweepLine(VEGAPath& path);
	virtual ~VEGASweepLine();

	/** Initialize internal state. Creates storage for and initializes
	 * m_vertices and m_edges. Any pointless vertices will be pruned.
	 * @return OpStatus::OK on success and OpStatus::ERR_NO_MEMORY on OOM. */
	virtual OP_STATUS init();

# ifdef VEGA_DEBUG_SWEEP_LINE_IMAGE_OUTPUT
	OP_STATUS initDebugRT();
	UINT32 DebugGetColor(unsigned int i, unsigned char alpha = 0xff);
	VEGA_FIX DebugMapX(float v) { return VEGA_FLTTOFIX(m_px + ((v-m_bx) / m_bw) * (m_dw - 2*m_px)); }
	VEGA_FIX DebugMapY(float v) { return VEGA_FLTTOFIX(m_py + ((v-m_by) / m_bh) * (m_dh - 2*m_py)); }
	void DebugDrawOutline(bool lines = true, bool verts = true, bool nums = true,
	                      unsigned int from = 0, unsigned int to = 0);
	void DebugDumpPathSegment(unsigned int from, unsigned int to, const char* fn);
	void DebugDumpRT(const char* name, unsigned idx = 0);
# endif // VEGA_DEBUG_SWEEP_LINE_IMAGE_OUTPUT

protected:
	OP_STATUS initVertices();
	OP_STATUS initEdges();

	/** p and i are lines in the path for the previous (after pruning)
	 * and current vertex. If isRedundant returns true the current
	 * vertex will be discarded.
	 * @param p Line for the previous vertex (after pruning).
	 * @param v Line for the current vertex.
	 * @return true if current vertex should be discarded, false otherwise. */
	virtual bool isRedundant(const VEGA_FIX* p, const VEGA_FIX* v) const;

	VEGAPath& m_path;

	/** Number of lines in path. */
	const unsigned int m_numLines;

	/** Number of unique vertices/edges. This will differ from the
	 * number of lines when pointless vertices are encountered, which
	 * can happen after eg transforms have been applied. */
	unsigned int m_numUnique;

	/** Size of m_edges. Default value is 0, in which case it will be
	 * set to m_numLines from initEdges. This allows inheriting
	 * classes to request storage for more than m_numLines edges
	 * (used eg to store edges introduced due to reflex vertices in
	 * VEGATriangulator). */
	unsigned int m_maxLines;

	/** The vertices of the path. */
	VEGASLVertex*   m_vertices;
	/** The edges of the path. */
	VEGASLEdge*     m_edges;

	/** Indices to vertices ordered primarily on x, secondarily on y.
	 * NOTE: Range of m_order is [0; m_numUnique[ -
	 * [m_numUnique; m_numLines[ contains indices to pruned vertices. */
	unsigned short* m_order;

# ifdef VEGA_DEBUG_SWEEP_LINE_IMAGE_OUTPUT
	class VEGARenderer*     m_renderer;
	class VEGARenderTarget* m_renderTarget;
	class OpBitmap*         m_bitmap;

	float m_bx, m_by, m_bw, m_bh; ///< bounding box
	unsigned m_dw, m_dh, m_px, m_py; ///< output image size and padding
# endif // VEGA_DEBUG_SWEEP_LINE_IMAGE_OUTPUT
};

/** A sweep-line approach for detecting self-intersection. Based on
 * the Bentley-Ottmann algorithm.
 *
 * HACKS! In order to allow duplicate edges and vertices, the
 * following hacks are present in VEGASelfIntersect:
 *
 * - isSelfIntersecting: Traversing up and down in Q to cope with
 *   duplicate edges. */
class VEGASelfIntersect : public VEGASweepLine
{
public:
	VEGASelfIntersect(VEGAPath& path) : VEGASweepLine(path) {}

	/** @return
	 *   OpStatus::ERR_NO_MEMORY on OOM,
	 *   OpBoolean::IS_TRUE if path is self-intersecting,
	 *   OpBoolean::IS_FALSE if path is not self-intersecting. */
	OP_BOOLEAN isSelfIntersecting();

#ifdef VEGA_DEBUG_SELF_INTERSECT_IMAGE_OUTPUT_OUTLINE
	void DebugDumpOutline();
#endif // VEGA_DEBUG_SELF_INTERSECT_IMAGE_OUTPUT_OUTLINE
};

#endif  // !VEGA_SWEEP_LINE_H
