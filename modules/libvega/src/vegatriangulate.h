/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef VEGA_TRIANGULATE_H
#define VEGA_TRIANGULATE_H

#include "modules/libvega/vegafixpoint.h"
#include "modules/libvega/vegapath.h"
#include "modules/libvega/src/vegasweepline.h"
#include "modules/util/tree.h"

/** This class represents a trapezoid in the path. A trapezoid
 * consists of an upper and a lower edge. Trapezoids are linked into
 * trapezoid chains, which are y-monotone polygons representing parts
 * of the path to triangulate. It also maintains the concept of a
 * splitter, which is the vertex that caused the creation of the
 * trapezoid. This is only set for trapezoids where the splitter is
 * part of the active chain. The splitter is used to introduce edges
 * when encountering reflex vertices. */
class VEGATrapezoid : public ListElement<VEGATrapezoid>
{
public:
	VEGATrapezoid() : splitter(NULL), upper(NULL), lower(NULL) {}

	VEGATrapezoid& operator=(const VEGATrapezoid& o)
	{
		splitter = o.splitter;
		upper    = o.upper;
		lower    = o.lower;
		return *this;
	}

#ifdef _DEBUG
	unsigned short idx;
#endif // _DEBUG

	VEGASLVertex* splitter; ///< Vertex that caused creation of this trapezoid.
	VEGASLEdge* upper;      ///< The upper line of this trapezoid.
	VEGASLEdge* lower;      ///< The lower line of this trapezoid.
};

/** A sweep-line implementation for triangulating non-complex paths.
 *
 * Inspired by:
 *
 * http://www.personal.kent.edu/~rmuhamma/Compgeometry/MyCG/PolyPart/polyPartition.htm
 *
 * Gist: Build trapezoids, connect them into y-monotone polygons
 * (inserting extra edges at reflex vertices), triangulate each
 * y-monotone polygon.
 *
 * HACKS! In order to allow duplicate edges and vertices, the
 * following hacks are present in VEGATriangulator:
 *
 * - buildTrapezoids: Identifying splitter through loop, and leaving
 *   it null if no splitter can be found.
 *
 * - buildTrapezoids: Using rightmost vertex if trapezoid when
 *   dealing with misseed left reflex vertices (arguably not a hack,
 *   there's not much else to do).
 *
 * - buildTrapezoids: Removing all edges ending on visited vertex and
 *   dups before adding edges starting on vertex.
 *
 * - determineVertexOrder: Has code to deal with duplicate vertices,
 *   which probably isn't needed.
 *
 * - triangulate: Discards empty trapezoid runs (needed for paths
 *   with "empty" parts, eg "linked rects" selftest). */
class VEGATriangulator : public VEGASweepLine
{
public:
	VEGATriangulator(VEGAPath& path);
	~VEGATriangulator();

	/** Triangulate path.
	 * @param tris (out) Indices to the vertices of the triangles (three per triangle).
	 * @param numTris (out) The number of triangles.
	 * @return
	 * OpStatus::OK if the path was successfully triangulated,
	 * OpStatus::ERR_NO_MEMORY on OOM,
	 * OpStatus::ERR if the path could not be triangulated (indicating a bug). */
	OP_STATUS triangulate(unsigned short*& tris, int& numTris);

private:
	OP_STATUS init();

	VEGATrapezoid* findChain(VEGATrapezoid* t);
	bool linkTrapezoid(VEGATrapezoid* t, VEGATrapezoid* chain_end = NULL);
	/** Split the trapezoid toSplit into two trapezoid by introducing a
	 * new edge. toSplit will maintain its upper edge and get the
	 * newly introduced edge as its lower edge. The added trapezoid
	 * will get the newly introduced edge as its upper edge, toSplit's
	 * old lower edge as its lower edge and toSplit's splitter as its
	 * splitter. toSplit is removed from any current chain and
	 * re-added, after which the new trapezoid is added.
	 * @param toSplit The trapezoid to split.
	 * @param splitStart Left-top vertex of edge to add.
	 * @param splitEnd Right-bottom vertex of edge to add.
	 * @return true if trapezoid was split and a new edge
	 * introduced, false if the edge to add already existed. */
	bool splitReflex(VEGATrapezoid* toSplit,
	                 VEGASLVertex* splitStart, VEGASLVertex* splitEnd);

	/** Create a set of trapezoid runs, where each run makes up one
	 * monotone polygon. */
	OP_STATUS buildTrapezoids();

	/** Extract and order (with respect to x) the vertices for the
	 * monotone polygon defined by trapezoid. the vertex indices are
	 * stored in m_order.
	 * NOTE: This function overwrites the data in m_order.
	 * @param trapezoid The start of the trapezoid run.
	 * @return The number of vertices in the monotone polygon. */
	unsigned int determineVertexOrder(List<VEGATrapezoid>& trapezoids);
	/** Triangulate a monotone polygon.
	 * @param numVerts The number of vertices in the polygon (indices stored in m_order).
	 * @param tris (out) Target buffer, the vertex indices (three per
	 * triangle) will be stored here.
	 * @return The number of triangles written to tris. */
	int triangulateTrapezoidRun(unsigned int numVerts, unsigned short* tris);

	void setUpper(unsigned short v, bool upper) { m_upper[v] = upper; }
	// NOTE: These are valid only for the trapezoid passed to the
	// previous call to determineTrapezoidVertexOrder.
	bool isUpper(unsigned short v) { return m_upper[v]; }
	bool isOpposite(unsigned short p1, unsigned short p2) { return isUpper(p1) != isUpper(p2); }
	bool isConvex(unsigned short p1, unsigned short p2, unsigned short p3);

#ifdef _DEBUG
	void DebugDumpChains(List<VEGATrapezoid>*& chains, unsigned int numChains);
# ifdef VEGA_DEBUG_TRIANGULATION_IMAGE_OUTPUT_TRIANGLE_RUNS
	void DebugDrawTriangles(const unsigned short* vertexIndices, unsigned numTris, unsigned i);
	void DebugDumpTriangleRuns();
# endif // VEGA_DEBUG_TRIANGULATION_IMAGE_OUTPUT_TRIANGLE_RUNS
#endif // _DEBUG

	/** m_upper[vertex->index] is true if vertex is part of the upper
	 * stretch of the current trapezoid. (Could save some memory by
	 * using a bitmask.) */
	bool* m_upper;
	VEGATrapezoid* m_trapezoids;
	VEGATrapezoid* m_reflexTrapezoids; // part of m_trapezoids
	VEGASLEdge* m_reflexEdges;          // part of m_edges
	List<VEGATrapezoid>* m_chains;

	const unsigned int m_maxTrapezoids;
	const unsigned int m_maxReflexTrapezoids;
	const unsigned int m_maxReflexEdges;

	unsigned int m_numTrapezoids;
	unsigned int m_numReflexTrapezoids;
	unsigned int m_numReflexEdges;
	unsigned int m_numChains;
};

class VEGATriangulationData
{
public:
	class SubPath : public Tree
	{
	public:
		void clearTree()
		{
			for (SubPath* t = FirstChild(); t; t = t->Suc())
				t->clearTree();
			RemoveAll();
		}

		const SubPath* First()      const { return (const SubPath*)Tree::First(); }
		const SubPath* Suc()        const { return (const SubPath*)Tree::Suc(); }
		const SubPath* FirstChild() const { return (const SubPath*)Tree::FirstChild(); }
		SubPath* First()                  { return (SubPath*)Tree::First(); }
		SubPath* Suc()                    { return (SubPath*)Tree::Suc(); }
		SubPath* FirstChild()             { return (SubPath*)Tree::FirstChild(); }

		void copy(const SubPath& other)
		{
			startLine             = other.startLine;
			endLine               = other.endLine;
			min_x                 = other.min_x;
			max_x                 = other.max_x;
			min_y                 = other.min_y;
			max_y                 = other.max_y;
			leftmost_vtx          = other.leftmost_vtx;
			signedArea            = other.signedArea;
			corridorStartIdx      = other.corridorStartIdx;
			corridorStartPoint[0] = other.corridorStartPoint[0];
			corridorStartPoint[1] = other.corridorStartPoint[1];
			corridorStartK        = other.corridorStartK;
			addInOrder            = other.addInOrder;
		}

		/**
		   add child in order, based on distance from start of line to
		   corridor start point
		 */
		void AddChild(SubPath* subPath);

		unsigned startLine;
		unsigned endLine;
		VEGA_FIX min_x;
		VEGA_FIX max_x;
		VEGA_FIX min_y;
		VEGA_FIX max_y;
		unsigned leftmost_vtx;
		VEGA_FIX signedArea;

		// index in source path of the start of the corridor
		unsigned corridorStartIdx;
		VEGA_FIX corridorStartPoint[2];
		// ratio on line of start point of corridor (0 at start of
		// line corridorStartIdx, 1 at end)
		VEGA_FIX corridorStartK;
		// true if sub-path should be added in order, false if
		// sub-path should be added in reverse order
		bool addInOrder;

		unsigned numLines() const { return endLine < startLine ? 0 : (unsigned)(endLine - startLine + 1); }
		bool clockwise() const { return signedArea < 0; }

		unsigned int incIdx(const unsigned int idx) const
		{
			OP_ASSERT(idx >= startLine &&
					  idx <= endLine);
			return idx == endLine ? startLine : idx+1;
		}
		unsigned int decIdx(const unsigned int idx) const
		{
			OP_ASSERT(idx >= startLine &&
					  idx <= endLine);
			return idx == startLine ? endLine : idx-1;
		}
	};

	VEGATriangulationData()
	: triangles(NULL), numTriangles(0), trianglesValid(false)
	, numSubPaths(0), subPaths(NULL)
	, subPathsValid(false)
	, twiceThePolygonArea(0)
	{}

	~VEGATriangulationData()
	{
		markTriangulationInfoInvalid();
		markSubPathInfoInvalid();
	}

	bool isTriangulated() const { return trianglesValid; }
	OP_STATUS triangulate(VEGAPath* path);
	void getTriangles(unsigned short** tris, int* numTris);

	void markSubPathInfoInvalid();
	void markTriangulationInfoInvalid();

	/** Get twice the area that the (closed) path encloses.
	 * Could be divided by 2 if the actual area is requested.
	 * This only gives the right result for simple (non-intersecting) paths. */
	VEGA_FIX getTwiceThePolygonArea() { return twiceThePolygonArea; }
	OP_STATUS tryToMakeMultipleSubPathsSimple(VEGAPath* source, VEGAPath& target, const bool xorFill);
	OP_STATUS copy(VEGATriangulationData* other);

private:
	OP_STATUS mergeSubPaths(VEGAPath* source, VEGAPath& newPath, const bool xorFill);
	static OP_STATUS mergeSubPathsR(const VEGAPath* source, VEGAPath& target, const SubPath* subPath);
	OP_STATUS collectSubPathInfo(VEGAPath* path);
	void countSubPaths(VEGAPath* path);

	unsigned short* triangles;
	int numTriangles;
	bool trianglesValid;

	unsigned int numSubPaths;
	SubPath* subPaths;
	bool subPathsValid;

	/* When merging sub paths to a single path we need to know
	 * if the sub paths have vertices ordered clock wise or counter
	 * clock wise. The orientation could be determined from the
	 * sign of the polygon area if caluclated as explained below
	 *
	 * A positive value means a ccw-polygon and negative cw.
	 *
	 * The area of a polygon is calculated by going through all edges
	 * and summarize the result from (b.x - a.x) * (b.y + a.y) where
	 * the edge goes from point a to point b. The result should then
	 * be divided by 2 (which is unneccessary in our case since we are
	 * only interested in the sign of the sum, hence the "twice"). This
	 * is calculated when building the path. */
	VEGA_FIX twiceThePolygonArea;
};

#endif // !VEGA_TRIANGULATE_H
