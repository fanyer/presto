// -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
//
// Copyright (C) Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.

#ifndef SOURCE_POSITION_ATTR_H_
#define SOURCE_POSITION_ATTR_H_

#include "modules/logdoc/complex_attr.h"

/**
 * Class which stores a position in a file as a line number + character position in line
 * pair (1-based position).
 * Intended to be used for script and style element, and will then record the position
 * where the first text child of those elements start, rather than where the element
 * itself starts.
 */

class SourcePositionAttr : public ComplexAttr
{
public:
	SourcePositionAttr() : line_number(0), line_position(0) {}

	/**
	 * @param line  Line number of position to store (1 is first line)
	 * @param pos   Character position in line to store (1 is first position)
	 */
	SourcePositionAttr(unsigned line, unsigned pos) : line_number(line), line_position(pos) {}
	SourcePositionAttr(const SourcePositionAttr* other) { line_number = other->line_number; line_position = other->line_position; }

	/// @see ComplexAttr::CreateCopy(ComplexAttr **copy_to)
	virtual OP_STATUS	CreateCopy(ComplexAttr **copy_to) { *copy_to = OP_NEW(SourcePositionAttr, (this)); return *copy_to ? OpStatus::OK : OpStatus::ERR_NO_MEMORY; }
	/// @see ComplexAttr::MoveToOtherDocument(FramesDocument*, FramesDocument*)
	virtual BOOL 		MoveToOtherDocument(FramesDocument*, FramesDocument*) { return FALSE; }
	/// @see ComplexAttr::Equals(ComplexAttr *other);
	virtual BOOL 		Equals(ComplexAttr *other)
	{
		OP_ASSERT(other->IsA(ComplexAttr::T_SOURCE_ATTR));
		SourcePositionAttr *src_pos_attr = static_cast<SourcePositionAttr*>(other);
		return line_number == src_pos_attr->line_number && line_position == src_pos_attr->line_position;
	}

	/// @return   Line number of stored position (1 is first line)
	unsigned			GetLineNumber() { return line_number; }
	/// @return   Character position in line of stored position (1 is first charater)
	unsigned			GetLinePosition() { return line_position; }
private:
	unsigned			line_number;
	unsigned			line_position;
};

#endif /* SOURCE_POSITION_ATTR_H_ */
