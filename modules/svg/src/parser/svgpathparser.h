/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#ifndef SVG_PATH_PARSER_H
#define SVG_PATH_PARSER_H

#ifdef SVG_SUPPORT

#include "modules/svg/src/OpBpath.h"
#include "modules/svg/src/parser/svgtokenizer.h"

class SVGPathParser
{
public:
    /**
     * Parse a string by extracting path segment from it and adding
     * them to the OpBpath
     */
    OP_STATUS ParsePath(const uni_char *input_string, unsigned str_len, OpBpath *path);

private:
    BOOL GetNextSegment(SVGPathSeg &seg, SVGPathSeg::SVGPathSegType prev_path_seg_type);
    BOOL GetNextCmdType(SVGPathSeg::SVGPathSegType &path_seg_type, BOOL& implicit);
    void AddSegmentToPath(OpBpath *path, const SVGPathSeg& seg);
    BOOL GetArcFlag();

	double NextPathNumber();

	SVGTokenizer tokenizer;
    OpBpath *path;
    OP_STATUS status;
};

#endif // SVG_SUPPORT
#endif // !SVG_PATH_PARSER_H


