/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef CONTENT_GENERATOR_H
#define CONTENT_GENERATOR_H

#include "modules/util/simset.h"

class HTML_Element;
class HLDocProfile;
class Content;

/** Content generator for CSS generated content (CSS property 'content').

	This class is used when handling a CSS 'content' property. First, call
	Reset() (since this object is re-used between 'content' properties).

	Then, for each component in the 'content' property, call an appropriate
	Add*() method (AddString() for regular text, for instance). For each call
	to Add*(), a ContentGenerator::Content object will be created, unless the
	content can be appended to the previously created ContentGenerator::Content
	object.

	When everything has been added, call GetContent(). Iterate through the
	ContentGenerator::Content objects using Link::Suc(). For each, call
	CreateElement(). */

class ContentGenerator
{
public:

	class Content : public Link
	{
	public:

		Content();
		~Content();

		BOOL AddString(const uni_char* string);

		void Reset();

		OP_STATUS CreateElement(HTML_Element*& element, HLDocProfile* hld_profile) const;

		BOOL			is_text;
#ifdef SKIN_SUPPORT
		BOOL			is_skin;
#endif // SKIN_SUPPORT
		uni_char*		content_buf;
		int				content_buf_size;
	};

	~ContentGenerator();

	/** Reset the content generator.

		Should be called when starting to generate content for a 'content'
		property. */

	void			Reset();

	/** Get the first ContentGenerator::Content object in the list.

		The list of object can then be iterated using Link::Suc(). */

	const Content*	GetContent();

	/** Add a string. */

	BOOL			AddString(const uni_char* string);

	/** Add a URL (e.g. an image, HTML document, whatever).

		This is the method to call when handling a url() part of a 'content'
		property. */

	BOOL			AddURL(const uni_char* url);

	/** Add a line break.

		This is just a convenience method that will call AddString() in its
		implementation. */

	BOOL			AddLineBreak();

#ifdef SKIN_SUPPORT
	/** Add a skin element.

		This is the method to call when handling a -o-skin() part of a
		'content' property. */

	BOOL			AddSkinElement(const uni_char* skin_elm);
#endif // SKIN_SUPPORT

private:

	Head content_list;
};

#endif // CONTENT_GENERATOR_H
