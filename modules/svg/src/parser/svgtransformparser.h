/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#ifndef SVG_TRANSFORM_PARSER
#define SVG_TRANSFORM_PARSER

#ifdef SVG_SUPPORT

#include "modules/svg/src/parser/svgtokenizer.h"
#include "modules/svg/src/SVGTransform.h"

class SVGVector;

class SVGTransformParser
{
public:
    OP_STATUS ParseTransformList(const uni_char *input_string, unsigned str_len, SVGVector *vector);
    OP_STATUS ParseTransformArgument(const uni_char *input_string, unsigned str_len, SVGTransformType type, SVGTransform &transform);

private:
    BOOL GetNextTransform(SVGTransform &transform);
    BOOL GetNextTransformType(SVGTransformType &transform_type);

	void ParseRefTransform(SVGTransform &transform);
	void ParseArguments(SVGTransformType transform_type, SVGTransform &transform);

    SVGTokenizer tokenizer;
    SVGVector *vector;
    OP_STATUS status;
};

#endif // SVG_SUPPORT
#endif // !SVG_TRANSFORM_PARSER
