/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#ifndef SVG_TIME_PARSER_H
#define SVG_TIME_PARSER_H

#ifdef SVG_SUPPORT

#include "modules/svg/src/SVGTimeValue.h"
#include "modules/svg/src/parser/svgtokenizer.h"

class SVGVector;

class SVGTimeParser
{
public:
    OP_STATUS ParseTimeList(const uni_char *input_string, unsigned str_len, SVGVector *vector);
    OP_STATUS ParseAnimationTime(const uni_char *input_string, unsigned str_len, SVG_ANIMATION_TIME &clock_value);

private:
    BOOL GetNextTimeValue(SVGTimeObject *&time_value);

    void ParseEventValue(SVGTimeObject *&time_value, const uni_char *id_value, unsigned id_value_len);
    void ParseAccessKeyValue(SVGTimeObject *&time_value);
    void ParseOffsetValue(SVGTimeObject *&time_value);

    SVG_ANIMATION_TIME GetOffset(BOOL optional, BOOL optional_sign);
    void FindIdReference(const uni_char *& name, unsigned &name_len);
    unsigned ScanEventType();
    SVG_ANIMATION_TIME ParseAnimationTime();
    double GetOptionalFraction();
    SVG_ANIMATION_TIME GetOptionalMetric();
    SVG_ANIMATION_TIME GetDigits();
	BOOL ScanUnicode(unsigned& uc);

    OP_STATUS status;
    SVGVector *vector;
    SVGTokenizer tokenizer;
};

#endif // SVG_SUPPORT
#endif // !SVG_TIME_PARSER_H
