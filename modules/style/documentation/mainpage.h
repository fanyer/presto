/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/


/** @mainpage Style module
 *
 * This is the auto-generated API documentation for the style module.
 *
 * @section Overview
 *
 * The style module contains the code for loading and parsing CSS stylesheets,
 * including user stylesheets and other local stylesheets. The parsed stylesheets
 * are kept in an internal structure which is used to find the specified CSS values
 * for each HTML_Element.
 *
 * The CSSCollection API provides functionality for retrieving a set of declarations,
 * one for each property, which applies to an HTML_Element based on cascading
 * order and specificity.
 *
 * It also contains the code required by the DOM 2 Style bindings.
 *
 * @section api Public API
 *
 * <ul>
 * <li>The CSS class for loading and representing a single stylesheet.</li>
 * <li>The CSSCollection for selecting property values for HTML_Elements
 * given the collection of stylesheets which apply for the document.</li>
 * <li>CSS_DOMStyleDeclaration, CSS_DOMRule, CSS_DOMStyleSheet, etc. for
 * the interfacing to DOM 2 Style.</li>
 * <li>Various CSS_decl classes to represent declarations returned from
 * selection from the CSSCollection.</li>
 * <li>SaveCSSWithInline from css_save.h.</li>
 * </ul>
 */
