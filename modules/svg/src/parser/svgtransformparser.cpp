/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#include "core/pch.h"

#ifdef SVG_SUPPORT

#include "modules/svg/src/svgpch.h"
#include "modules/svg/src/parser/svgtransformparser.h"
#include "modules/svg/src/SVGVector.h"

OP_STATUS
SVGTransformParser::ParseTransformList(const uni_char *input_string,
									   unsigned str_len, SVGVector *vector)
{
    tokenizer.Reset(input_string, str_len);
    this->vector = vector;
    this->status = OpStatus::OK;

    if (tokenizer.Scan("none"))
    {
		vector->SetIsNone(TRUE);
    }
    else if (tokenizer.Scan("ref"))
    {
		SVGTransform transform;
		transform.SetTransformType(SVGTRANSFORM_REF);
		ParseRefTransform(transform);
		if (OpStatus::IsSuccess(status))
		{
			SVGTransform *allocated_transform = static_cast<SVGTransform *>(transform.Clone());
			if (!allocated_transform ||
				OpStatus::IsMemoryError(vector->Append(allocated_transform)))
			{
				OP_DELETE(allocated_transform);
				status = OpStatus::ERR_NO_MEMORY;
			}
			vector->SetIsRefTransform();
		}
    }
	else
	{
		SVGTransform transform;
		while (GetNextTransform(transform) &&
			   OpStatus::IsSuccess(status))
		{
			SVGTransform *allocated_transform = static_cast<SVGTransform *>(transform.Clone());
			if (!allocated_transform)
				status = OpStatus::ERR_NO_MEMORY;

			if (OpStatus::IsSuccess(status))
			{
				status = vector->Append(allocated_transform);
				if (OpStatus::IsError(status))
				{
					OP_DELETE(allocated_transform);
				}
			}
		}
	}

	return tokenizer.ReturnStatus(status);
}

OP_STATUS
SVGTransformParser::ParseTransformArgument(const uni_char *input_string, unsigned str_len,
										   SVGTransformType transform_type, SVGTransform &transform)
{
    tokenizer.Reset(input_string, str_len);

    this->status = OpStatus::OK;

	ParseArguments(transform_type, transform);

	return tokenizer.ReturnStatus(status);
}

BOOL
SVGTransformParser::GetNextTransform(SVGTransform &transform)
{
    if (OpStatus::IsError(status))
		return FALSE;

    SVGTransformType transform_type;
    if (!GetNextTransformType(transform_type))
		return FALSE;

    if (transform_type == SVGTRANSFORM_UNKNOWN)
    {
		status = OpStatus::ERR;
		return FALSE;
    }

    tokenizer.EatWsp();

    if (!tokenizer.Scan('('))
    {
		status = OpStatus::ERR;
		return FALSE;
    }

	ParseArguments(transform_type, transform);

	if (OpStatus::IsSuccess(status))
	{
		if (!tokenizer.Scan(')'))
		{
			status = OpStatus::ERR;
			return FALSE;
		}
	}

	tokenizer.EatWspSeparatorWsp(',');

	if (OpStatus::IsError(status))
		return FALSE;
	else
		return TRUE;
}

BOOL
SVGTransformParser::GetNextTransformType(SVGTransformType &transform_type)
{
    tokenizer.EatWsp();

    if (tokenizer.IsEnd())
		return FALSE;

    if (tokenizer.Scan("translate"))
    {
		transform_type = SVGTRANSFORM_TRANSLATE;
    }
    else if (tokenizer.Scan("rotate"))
    {
		transform_type = SVGTRANSFORM_ROTATE;
    }
    else if (tokenizer.Scan("scale"))
    {
		transform_type = SVGTRANSFORM_SCALE;
    }
    else if (tokenizer.Scan("matrix"))
    {
		transform_type = SVGTRANSFORM_MATRIX;
    }
    else if (tokenizer.Scan("skewX"))
    {
		transform_type = SVGTRANSFORM_SKEWX;
    }
    else if (tokenizer.Scan("skewY"))
    {
		transform_type = SVGTRANSFORM_SKEWY;
    }
    else
    {
		// Error condition
		transform_type = SVGTRANSFORM_UNKNOWN;
    }

    return TRUE;
}

void
SVGTransformParser::ParseArguments(SVGTransformType transform_type, SVGTransform &transform)
{
    tokenizer.EatWsp();
    double numbers[6];

    int number_idx = 0;
    while(number_idx < 6 && tokenizer.ScanNumber(numbers[number_idx]))
    {
		tokenizer.EatWspSeparatorWsp(',');
		number_idx++;
    }

    transform.SetTransformType(transform_type);
    if (number_idx == 1)
    {
		if (transform_type == SVGTRANSFORM_SKEWX ||
			transform_type == SVGTRANSFORM_SKEWY)
		{
			transform.SetValuesA1(numbers[0]);
		}
		else if (transform_type == SVGTRANSFORM_ROTATE)
		{
			transform.SetValuesA123(numbers[0], 0, 0, TRUE);
		}
		else if (transform_type == SVGTRANSFORM_TRANSLATE)
		{
			transform.SetValuesA12(numbers[0], 0, TRUE);
		}
		else if (transform_type == SVGTRANSFORM_SCALE)
		{
			transform.SetValuesA12(numbers[0], numbers[0], TRUE);
		}
		else
		{
			status = OpStatus::ERR;
		}
    }
    else if (number_idx == 2)
    {
		if (transform_type == SVGTRANSFORM_TRANSLATE ||
			transform_type == SVGTRANSFORM_SCALE)
		{
			transform.SetValuesA12(numbers[0], numbers[1], FALSE);
		}
		else
		{
			status = OpStatus::ERR;
		}
    }
    else if (number_idx == 3)
    {
		if (transform_type == SVGTRANSFORM_ROTATE)
		{
			transform.SetValuesA123(numbers[0], numbers[1], numbers[2], FALSE);
		}
		else
		{
			status = OpStatus::ERR;
		}
    }
    else if (number_idx == 6)
    {
		if (transform_type == SVGTRANSFORM_MATRIX)
		{
			transform.SetMatrix(numbers[0], numbers[1], numbers[2],
								numbers[3], numbers[4], numbers[5]);
		}
		else
		{
			status = OpStatus::ERR;
		}
    }
}

void
SVGTransformParser::ParseRefTransform(SVGTransform &transform)
{
	tokenizer.EatWsp();
	if (!tokenizer.Scan('('))
	{
		status = OpStatus::ERR;
		return;
	}

	tokenizer.EatWsp();
	if (!tokenizer.Scan("svg"))
	{
		status = OpStatus::ERR;
		return;
	}
	tokenizer.EatWspSeparatorWsp(',');

	double numbers[2];
	int number_idx = 0;
	while(number_idx < 2 && tokenizer.ScanNumber(numbers[number_idx]))
	{
		tokenizer.EatWspSeparatorWsp(',');
		number_idx++;
	}

	if (number_idx == 2)
		transform.SetValuesA12(numbers[0], numbers[1], FALSE);

	if ((number_idx != 0 && number_idx != 2) ||
		!tokenizer.Scan(')'))
	{
		status = OpStatus::ERR;
		return;
	}
}

#endif // SVG_SUPPORT
