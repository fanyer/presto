/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/


/** @mainpage Layout module
 *
 * This is the auto-generated API documentation for the layout module.
 * For more information about the module, see the module's <a
 * href="http://wiki.palace.opera.no/developerwiki/index.php/Modules/layout">Wiki
 * page</a>.
 *
 * @section Overview
 *
 * The layout module contains the functionality for finding the computed css values
 * for each HTML_Element in the document based on the specified css values selected in
 * the style module, create the layout box tree for the logical document, and do the
 * actual layout. It also contains the traversal code for traversing the document tree
 * for painting, text-selection, hover-detection, etc.
 *
 * @section api Public API
 *
 * There is not a well-defined public API for the layout module. Its classes are
 * tightly coupled with the HTML_Element from logdoc as each visible HTML_Element
 * has a layout box associated with it. Typically Boxes and Containers are accessed
 * from a diversity of modules from the old doc complex (doc module on core-1), and
 * also static casts are made to access functionality in specialized Boxes.
 *
 */
