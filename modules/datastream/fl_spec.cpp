/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#include "core/pch.h"

#include "modules/datastream/fl_lib.h"

DataRecord_Spec::DataRecord_Spec()
{
	enable_tag = TRUE;
	idtag_len = 4;
	tag_big_endian = TRUE;
	tag_MSB_detection = TRUE;
	enable_length = TRUE;
	length_len = 4;
	length_big_endian = TRUE;
	numbers_big_endian = TRUE;
}

DataRecord_Spec::DataRecord_Spec(BOOL e_tag, uint16 tag_len, BOOL tag_be, BOOL tag_MSB, BOOL e_len, uint16 len_len, BOOL len_be, BOOL num_be)
{
	enable_tag = e_tag;
	idtag_len = tag_len;
	tag_big_endian = tag_be;
	tag_MSB_detection = tag_MSB;
	enable_length = e_len;
	length_len = len_len;
	length_big_endian = len_be;
	numbers_big_endian = num_be;
}

#ifdef UNUSED_CODE
DataRecord_Spec &DataRecord_Spec::operator =(const DataRecord_Spec &other)
{
	enable_tag = other.enable_tag;
	idtag_len = other.idtag_len;
	tag_big_endian = other.tag_big_endian;
	tag_MSB_detection = other.tag_MSB_detection;
	enable_length = other.enable_length;
	length_len = other.length_len;
	length_big_endian = other.length_big_endian;
	numbers_big_endian = other.numbers_big_endian;

	return *this;
}
#endif
