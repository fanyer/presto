/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2002 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef XMLPARSER_XMLCONFIG_H
#define XMLPARSER_XMLCONFIG_H

#ifdef XML_VALIDATING
# undef XML_VALIDATING_ELEMENT_DECLARED
# define XML_VALIDATING_ELEMENT_DECLARED
  /** Signal validation errors for undeclared elements. */

# undef XML_VALIDATING_ATTRIBUTE_DECLARED
# define XML_VALIDATING_ATTRIBUTE_DECLARED
  /** Signal validation errors for undeclared attributes. */

# undef XML_VALIDATING_ENTITY_DECLARED
# define XML_VALIDATING_ENTITY_DECLARED
  /** Signal validation errors for references to undeclared entites,
      otherwise ignore such references. */

# undef XML_VALIDATING_ELEMENT_CONTENT
# define XML_VALIDATING_ELEMENT_CONTENT
  /** Signal validation errors if an element's content does not match the
      declared element content specification. */

# undef XML_VALIDATING_ID_ATTRIBUTES
# define XML_VALIDATING_ID_ATTRIBUTES
  /** Signal validation errors if an ID attribute's value is not unique or
      does not match the 'Name' production, or if an IDREF attribute's value
      does not match the value of an ID attribute or does not match the 'Name'
      production or if an IDREFS attribute's value does not match the values
      of ID attributes or does not match the 'Names' production. */

# undef XML_VALIDATING_PE_IN_MARKUP_DECL
# define XML_VALIDATING_PE_IN_MARKUP_DECL
  /** Signal validation errors for markup declarations that begin and end in
      different entity expansions. */

# undef XML_VALIDATING_PE_IN_GROUP
# define XML_VALIDATING_PE_IN_GROUP
  /** Signal validation errors for content groups that begin and end in
      different entity expansions. */
#endif // XML_VALIDATING

#undef XML_SUPPORT_EXTERNAL_ENTITIES
#define XML_SUPPORT_EXTERNAL_ENTITIES
/** Only required if validating, but currently used even when not validating
    when loading an XHTML document to load a simple DTD that defines the
    latin-1 character entities. */

#undef XML_ATTRIBUTE_DEFAULT_VALUES
#define XML_ATTRIBUTE_DEFAULT_VALUES

#if defined XML_SUPPORT_EXTERNAL_ENTITIES
# define XML_SUPPORT_CONDITIONAL_SECTIONS
  /** Only allowed in the external doctype subset, which is only read when
      validating. */
#endif // XML_VALIDATING && XML_SUPPORT_EXTERNAL_ENTITIES

#ifdef XML_VALIDATING_ELEMENT_CONTENT
# define XML_COMPATIBILITY_CONTENT_MODEL_DETERMINISTIC
#endif // XML_VALIDATING_ELEMENT_CONTENT

#if defined XML_VALIDATING_ELEMENT_DECLARED || defined XML_VALIDATING_ELEMENT_CONTENT || defined XML_ATTRIBUTE_DEFAULT_VALUES
# define XML_STORE_ELEMENTS
#endif // XML_VALIDATING_ELEMENT_DECLARED || XML_VALIDATING_ELEMENT_CONTENT

#if defined XML_VALIDATING || defined XML_ATTRIBUTE_DEFAULT_VALUES
# define XML_STORE_ATTRIBUTES
#endif // XML_VALIDATING || XML_ATTRIBUTE_DEFAULT_VALUES

#define XML_NORMALIZE_LINE_BREAKS
#define XML_STYLESHEET
#define XML_CHECK_NAMESPACE_WELL_FORMED

#if defined _DEBUG && defined XML_VALIDATING
# define XML_DUMP_DOCTYPE
#endif // _DEBUG && XML_VALIDATING

#endif // XMLPARSER_XMLCONFIG_H
