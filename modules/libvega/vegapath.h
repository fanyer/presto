/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef VEGAPATH_H
#define VEGAPATH_H

#ifdef VEGA_SUPPORT
#include "modules/libvega/vegafixpoint.h"

class VEGATransform;
struct VEGAPrimitive;

#define VEGALINE_STARTX 0
#define VEGALINE_STARTY 1
#define VEGALINE_ENDX 2
#define VEGALINE_ENDY 3

#define VEGA_LINE_ATTR_NOT_VISIBLE 1
#define VEGA_LINE_ATTR_WARP 2
#define VEGA_LINE_ATTR_CLOSE 4

/** Line caps are used for the end of lines in outlined shapes. */
typedef enum{VEGA_LINECAP_BUTT, VEGA_LINECAP_ROUND, VEGA_LINECAP_SQUARE} VEGALineCap;
/** Line joins are used for the joinings of lines when outlining shapes. */
typedef enum{VEGA_LINEJOIN_MITER, VEGA_LINEJOIN_ROUND, VEGA_LINEJOIN_BEVEL} VEGALineJoin;

/** The main path class which stores a shape which can be rendered using the
 * VEGARenderer. */
class VEGAPath
{
public:
	VEGAPath();
	~VEGAPath();

	/** Clone this path, but cut away the parts that fall outside
	  * [0; 1] after transform has been applied. New vertices will
	  * be introduced as necessary.
	  * NOTE: it's only valid to call this function on convex paths.
	  * @constrained (out) the constrained path
	  * @param transform the transform to apply to the vertices
	  * @param on_x if true, constrain x
	  * @param on_y if true, constrain y
	  * @return OpStatus::OK on success, OpStatus::ERR_NO_MEMORY on OOM */
	OP_STATUS buildConstrainedPath(VEGAPath& constrained, const VEGATransform& transform, bool on_x, bool on_y);
	/** Clone this path, but cut away the parts falling outside
	  * boundary after transform has been applied. on_x, boundary and
	  * upper together describe a cut-off plane. Everything falling
	  * outside this plane will be removed, and new vertices will be
	  * introduced on the intersection points.
	  * NOTE: it's only valid to call this function on convex paths.
	  * @constrained (out) the constrained path
	  * @param transform the transform to apply to the vertices
	  * @param on_x true if constrain is on x (everything falling to
	  * one side of a vertical plane is removed)
	  * @param boundary the position of the cut-off plane
	  * @param upper true if the boundary is the upper limit (values
	  * above boundary will be cut off)
	  * @return OpStatus::OK on success, OpStatus::ERR_NO_MEMORY on OOM */
	OP_STATUS constrain(VEGAPath& constrained, const VEGATransform& transform, bool on_x, VEGA_FIX boundary, bool upper);

	/** Copy a path.
	  * @param other the path to copy.
	  * @returns OpStatus::OK on success, OpStatus::ERR_NO_MEMORY on OOM. */
	OP_STATUS copy(const VEGAPath *other);

	/** Prepare a path with a specific number of lines. Used to avoid
	  * allocating more than once.
	  * @param numLines the number of lines to prepare for.
	  * @returns OpStatus::OK on success, OpStatus::ERR_NO_MEMORY on OOM. */
	OP_STATUS prepare(unsigned int numLines);

	/** Move to a position. Will always start a new subpath, if a new
	  * subpath is not desired, use warpTo instead. This will also close
	  * the current sub-path with a non-stroked line.
	  * @param x, y the coordinate to move to.
	  * @returns OpStatus::OK on success, OpStatus::ERR_NO_MEMORY on OOM. */
	OP_STATUS moveTo(VEGA_FIX x, VEGA_FIX y);

	/** Draw a line to a position.
	  * @param x, y the coordinate to draw a line to.
	  * @returns OpStatus::OK on success, OpStatus::ERR_NO_MEMORY on OOM. */
	OP_STATUS lineTo(VEGA_FIX x, VEGA_FIX y);

	/** Warp to a position. Same as moving to, but a new subpath is
	  * not started. There will not be a line, only the current position
	  * will be updated.
	  * @param x, y the coordinate to warp to.
	  * @returns OpStatus::OK on success, OpStatus::ERR_NO_MEMORY on OOM. */
	OP_STATUS warpTo(VEGA_FIX x, VEGA_FIX y);

	/** Draw a cubic bezier curve to a point.
	  * @param x, y the end coordinate of the bezier.
	  * @param c1x, c1y the first control point.
	  * @param c2x, c2y the second control point.
	  * @param flatness the flatness of the curve (maximum deviation).
	  * @returns OpStatus::OK on success, OpStatus::ERR_NO_MEMORY on OOM. */
	OP_STATUS cubicBezierTo(VEGA_FIX x, VEGA_FIX y, VEGA_FIX c1x, VEGA_FIX c1y, VEGA_FIX c2x, VEGA_FIX c2y, VEGA_FIX flatness);

	/** Draw a quadratic bezier curve to a point.
	  * @param x, y the end coordinate of the bezier.
	  * @param cx, cy the control point.
	  * @param flatness the flatness of the curve (maximum deviation).
	  * @returns OpStatus::OK on success, OpStatus::ERR_NO_MEMORY on OOM. */
	OP_STATUS quadraticBezierTo(VEGA_FIX x, VEGA_FIX y, VEGA_FIX cx, VEGA_FIX cy, VEGA_FIX flatness);

	/** Draw an arc to a coordinate.
	  * @param x, y the coordinate where the arc should end.
	  * @param rx, ry the radius of the ellipse.
	  * @param xrot the rotation of the axes used to specify radius. Specified in degrees.
	  * @param large the large flag. If true the arc should be >180 degrees.
	  * @param sweep the sweep flag. If true the arc should go through negative angles.
	  * @param flatness the flatness of the arc (maximum deviation).
	  * @returns OpStatus::OK on success, OpStatus::ERR_NO_MEMORY on OOM. */
	OP_STATUS arcTo(VEGA_FIX x, VEGA_FIX y, VEGA_FIX rx, VEGA_FIX ry, VEGA_FIX xrot, bool large, bool sweep, VEGA_FIX flatness);

	/** Append a 'corner' consisting of two quadratic beziers to the path.
	  * In its current form, this is mostly a convenience helper - the
	  * caller has to do the actual calculations.
	  * @param cx1, cy1 control point of the first bezier.
	  * @param cx2, cy2 end coordinate of the first bezier.
	  * @param cx3, cy3 control point of the second bezier.
	  * @param ex, ey end coordinate of the second bezier.
	  * @param flatness the flatness of the curves (maximum deviation).
	  * @returns OpStatus::OK on success, OpStatus::ERR_NO_MEMORY on OOM. */
	OP_STATUS appendCorner(VEGA_FIX cx1, VEGA_FIX cy1, VEGA_FIX cx2, VEGA_FIX cy2,
						   VEGA_FIX cx3, VEGA_FIX cy3, VEGA_FIX ex, VEGA_FIX ey,
						   VEGA_FIX flatness)
	{
		RETURN_IF_ERROR(quadraticBezierTo(cx2, cy2, cx1, cy1, flatness));
		return quadraticBezierTo(ex, ey, cx3, cy3, flatness);
	}

	/** Append a predefined type of geometry to the path.
	 * @param prim A description of the primitive to append. */
	OP_STATUS appendPrimitive(VEGAPrimitive* prim);

	/** Close the current sub-path, either with a visible or invisible line.
	  * Visible lines will be stroked, invisible will not.
	  * @param visible specifies if the closing line should be visible.
	  * @returns OpStatus::OK on success, OpStatus::ERR_NO_MEMORY on OOM. */
	OP_STATUS close(bool visible);

	/** Remove the last line of the path. Useful if using the path has caused it
	  * to be closed and you want to change it back to the original state.
	  * This function can only remove lines from the current subpath.
	  * @returns true if a line was removed, false otherwise. */
	bool removeLastLine();

	/** Get the number of lines in the path. */
	unsigned int getNumLines() const { return numLines; }

	/** Get the line with the specified number. */
	VEGA_FIX *getLine(unsigned int lnum) const;

	/** Get the line with the specified number if it is not a warp line.
	  * If the line is a warp line, NULL will be returned instead. */
	VEGA_FIX *getNonWarpLine(unsigned int lnum) const;

	/** Get the length of the line with the specified number. */
	VEGA_FIX getLineLength(unsigned int lnum);

	/** Get the bounding box of the path. */
	void getBoundingBox(VEGA_FIX& x, VEGA_FIX& y, VEGA_FIX& w, VEGA_FIX& h);

	/** Transform all vertices in the path (including intersections). */
	void transform(VEGATransform *trans);

	/** Determine if the current sub-path is closed. */
	bool isClosed() const;

	/** Check if a line is visible and should be stroked. */
	bool isLineVisible(unsigned int lnum) const;

	/** Check if a line is a warp (not visible and not included in fills). */
	bool isLineWarp(unsigned int lnum) const;

	/** Check if a line is a close (end of path or sub-path). */
	bool isLineClose(unsigned int lnum) const;

	/** Check if a point is inside the path.
	  * @param x, y the point to check.
	  * @param xorFill specifies if xor or zero/set fill will be used to
	  *  render this path.
	  * @returns true if the point is inside, false otherwise. */
	bool isPointInside(VEGA_FIX x, VEGA_FIX y, bool xorFill);
	/** Check if a point is inside a part of the path.
	  * @param x, y the point to check.
	  * @param xorFill specifies if xor or zero/set fill will be used to
	  *  render this path.
	  * @param startIdx start of the range to check
	  * @param endIdx end of the range to check (inclusive)
	  * @returns true if the point is inside, false otherwise. */
	bool isPointInside(VEGA_FIX x, VEGA_FIX y, bool xorFill,
					   unsigned startIdx, unsigned endIdx);

	/** Check if the path intersects the given rectangle / box
	 * @param minx,miny,maxx,maxy points defining a rectangle
	 * @param xorFill specifies if xor or zero/set fill will be used to
	 *  render this path.
	 * @return true if the path intersects the rectangle, false otherwise */
	bool intersects(VEGA_FIX minx, VEGA_FIX miny, VEGA_FIX maxx, VEGA_FIX maxy, bool xorFill);

	/** Check if the path is enclosed by the given rectangle
	 * @param minx,miny,maxx,maxy points defining a rectangle
	 * @return true if the path is fully enclosed, false otherwise */
	bool isEnclosed(VEGA_FIX minx, VEGA_FIX miny, VEGA_FIX maxx, VEGA_FIX maxy);

	/** Check if the path has multiple sub-paths. */
	bool hasMultipleSubPaths() const { return multipleSubPaths; }

	/** Get the triangles for this path. Triangulates if needed. */
	OP_STATUS getTriangles(unsigned short** tris, int* numTri);
	bool isTriangulated() const;

	/** Create an outline from a path. The path should be dashed before this.
	  * The outlined version of the path will keep the line join, cap, width and miter limit.
	  * @param outline the outline version of the path.
	  * @param flatness the flatness to use for round joins and caps.
	  * @param firstLine the line to start to outline at (used for sub-paths, should be zero most of the time.
	  * @returns OpStatus::OK on success, OpStatus::ERR_NO_MEMORY on OOM. */
	OP_STATUS createOutline(VEGAPath *outline, VEGA_FIX flatness, int firstLine = 0);

	/** Dash the current path. Must be called before creating outline.
	  * The dashed version of the path will keep the line join, cap, width and miter limit.
	  * @param dashed the dashed version of the path.
	  * @param dashOffset start offset into the dasharray.
	  * @param numDash the number of dashes in the array.
	  * @param dashArray the length of the dashes.
	  * @param precompPathLength the precomputed pathlength, scale the calculated length to it (negative value means ignore)
	  * @returns OpStatus::OK on success, OpStatus::ERR_NO_MEMORY on OOM. */
	OP_STATUS createDash(VEGAPath* dashed, VEGA_FIX dashOffset, unsigned int numDash, VEGA_FIX *dashArray, VEGA_FIX precompPathLength = VEGA_INTTOFIX(-1));

	/** Set the line caps to use. */
	void setLineCap(VEGALineCap cap){lineCap = cap;}
	/** Set the line joins to use. */
	void setLineJoin(VEGALineJoin join){lineJoin = join;}
	/** Set the current miter limit (only used for miter joins), */
	void setMiterLimit(VEGA_FIX limit){miterLimit = limit;}
	/** Set the current line with (used when creating outlines). */
	void setLineWidth(VEGA_FIX w){lineWidth = w;}

#ifdef VEGA_3DDEVICE
	enum Category
	{
		UNDETERMINED,
		CONVEX,
		SIMPLE,
		COMPLEX
	};
	/** Get the category of the path, determine as needed. Assumes
	 * COMPLEX but doesn't cache result on OOM. */
	Category getCategory()
	{
		if (m_category == UNDETERMINED)
		{
			if (hasMultipleSubPaths())
				m_category = COMPLEX;
			else if (OpStatus::IsError(determineCategory()))
				return COMPLEX;
		}
		OP_ASSERT(m_category != UNDETERMINED);
		return m_category;
	}
	void forceConvex() { m_category = CONVEX; }
	/** Tries to connect sub paths with non intersecting corridors to
	 * make a path that we are able to triangulate.
	 * m_category for the target path is set to SIMPLE if we succeeded in building a
	 * triangulateable path or COMPLEX if we can't do it.
	 * @param target (out) the rebuilt path which of now should only be used
	 * if the try was successful (target.get_category() returns VEGAPath::SIMPLE). */
	OP_STATUS tryToMakeMultipleSubPathsSimple(VEGAPath& target, bool xorFill);
	bool cannotBeConvertedToSimple() const { return cannotBeConverted; }
#endif // VEGA_3DDEVICE

#ifdef VEGA_USE_LINEDATA_POOL
	static void clearPool(); // Uuuu, ugly...
#endif // VEGA_USE_LINEDATA_POOL

#ifdef _DEBUG
	void DebugDumpPathAsTriangulationSelftest(const char* title);
#endif // _DEBUG

	/**
	   Implementation detail, not for general use.
	   Detect intersecion between two lines or line segments.
	   @param x1 x coord of one end point of first line segment
	   @param y1 y coord of one end point of first line segment
	   @param x2 x coord of other end point of first line segment
	   @param y2 y coord of other end point of first line segment
	   @param x3 x coord of one end point of second line segment
	   @param y3 y coord of one end point of second line segment
	   @param x4 x coord of other end point of second line segment
	   @param y4 y coord of other end point of second line segment
	   @param infinite determines whether passed coords are lines or
	   line segments (ie whether to detect intersection points outside
	   endpoints)
	   @param return true if lines/line segments intersect, false otherwise
	 */
	static bool line_intersection(VEGA_FIX x1, VEGA_FIX y1, VEGA_FIX x2, VEGA_FIX y2,
								  VEGA_FIX x3, VEGA_FIX y3, VEGA_FIX x4, VEGA_FIX y4,
								  VEGA_FIX &x, VEGA_FIX &y, bool infinite);

private:
#ifdef VEGA_3DDEVICE
	/** For undetermined paths, determines whether path is a simple
	  * convex polygon, a simple polygon or a complex polygon, and
	  * updates m_category accordingly. */
	OP_STATUS determineCategory();
	/** Determine if path intersects itself. This function is
	  * expensive and doesn't cache results. */
	OP_BOOLEAN isSelfIntersecting();
#endif // VEGA_3DDEVICE

	/** Get the normal of a line, scaled to half the specified length. FIXME:DOC */
	bool getScaledLineNormal(VEGA_FIX* lineData, VEGA_FIX lineLength, VEGA_FIX dblnormlen, VEGA_FIX &nx, VEGA_FIX &ny);

	/** Get the direction of a line, scaled to half the specified length. FIXME:DOC */
	bool getScaledLineDirection(VEGA_FIX* lineData, VEGA_FIX lineLength, VEGA_FIX dbllen, VEGA_FIX &nx, VEGA_FIX &ny);

	OP_STATUS increaseLineAllocation();

	OP_STATUS do_miter_join(VEGAPath *outline, VEGA_FIX* line, VEGA_FIX* nextline,
							VEGA_FIX dx, VEGA_FIX dy, VEGA_FIX ndx, VEGA_FIX ndy);

	enum { MAX_LINES_PER_SEGMENT_SHIFT = 12 };

#ifdef VEGA_USE_PRECISE_ITERATIVE_FLATTENING
	OP_STATUS parabolicApprox(const struct VEGACubicBezierSegment& in_c, VEGA_FIX flatness);
#else
	OP_STATUS cubicBezierToHelper(VEGA_FIX x, VEGA_FIX y, VEGA_FIX c1x, VEGA_FIX c1y, VEGA_FIX c2x, VEGA_FIX c2y, VEGA_FIX flatness, int rec_count);
	OP_STATUS quadraticBezierToHelper(VEGA_FIX x, VEGA_FIX y, VEGA_FIX cx, VEGA_FIX cy, VEGA_FIX flatness, int rec_count);
#endif // VEGA_USE_PRECISE_ITERATIVE_FLATTENING

	unsigned int lastSubPathStart;
	unsigned int numLines;
	struct LineData
	{
		// each linedata struct holds VEGA_LINE_ALLOCATION_SIZE lines, and the start point
		VEGA_FIX lines[VEGA_LINE_ALLOCATION_SIZE*2+2]; // ARRAY OK 2009-04-24 wonko
		unsigned char lineAttributes[VEGA_LINE_ALLOCATION_SIZE]; // ARRAY OK 2009-04-24 wonko
		VEGA_FIX linelens[VEGA_LINE_ALLOCATION_SIZE]; // ARRAY OK 2009-04-24 wonko
		unsigned int numCachedLineLens;
	};
	LineData** lineData;
	unsigned int allocatedLineBlocks;

#ifdef VEGA_USE_LINEDATA_POOL
	static VEGAPath::LineData* allocLineData();
	static void freeLineData(LineData* linedata);
#else
	static inline LineData* allocLineData() { return OP_NEW(LineData, ()); }
	static inline void freeLineData(LineData* linedata) { OP_DELETE(linedata); }
#endif // VEGA_USE_LINEDATA_POOL

	bool started;
	bool multipleSubPaths;

#if defined(VEGA_3DDEVICE)
	friend class VEGATriangulationData;
	class VEGATriangulationData* triangulationData;
	void markSubPathInfoInvalid();
	void markTriangulationInfoInvalid();
#endif // VEGA_3DDEVICE

	VEGALineJoin lineJoin;
	VEGALineCap lineCap;
	VEGA_FIX miterLimit;
	VEGA_FIX lineWidth;

#ifdef VEGA_3DDEVICE
	// Very thin lines may miss all samples, we spoof them by capping
	// thickness and reduce opacity.
	VEGA_FIX actualLineWidth;
	Category m_category;
	bool cannotBeConverted;
#endif // VEGA_3DDEVICE

	bool m_bbox_valid;
	VEGA_FIX m_bbox_x, m_bbox_y, m_bbox_w, m_bbox_h;
};

#endif // VEGA_SUPPORT
#endif // VEGAPATH_H
