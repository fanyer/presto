/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/


/** @mainpage SVG module
 *
 * This is the auto-generated API documentation for the SVG module.
 *
 * @section api Public API
 *
 * The SVG module's external API consists mainly of two classes:
 * SVGManager and SVGImage. There are other public classes too,
 * SvgProperties and SVG_Lex. These are used in CSS and attribute
 * recognition by the doc modules.
 *
 * The entire external API is declared and defined in header files in
 * the module's root directory. In particular, all header files in the
 * src subdirectory are internal and must not be included by code
 * outside the SVG module. If definitions in internal header files
 * appear to be needed outside of the module, they should be moved to
 * the external API or the external API should be otherwise extended.
 * The exception to this rule is currently
 * modules/svg/SVGLayoutProperties.h which includes internal header
 * files.
 *
 * @section system External requirements
 *
 * The SVG module requires libvega to even build. What version of
 * libvega that is required is noted on the wiki-page. The module also
 * used datastructures and classes from logdoc (HTML_Element), layout
 * (LayoutProperties) and style (?)
 *
 * @section environment The SVG manager
 *
 * The SVG manager (global g_svg_manager) is the central point for
 * handling SVGs. The manager keeps track of all current SVG
 * documents.
 *
 * @subsection event_handling Event handling
 *
 * SVG Events events are very similar to DOM events and uses the
 * DOM_EventType enumeration, which they also extends. These should be
 * sent to the SVG module thourgh the SVGManager::HandleEvent
 * function. This function needs the direct element which to send the
 * event to. In order to know what SVG element to send the event to
 * SVGManager::FindSVGElement can be used. It takes a root element and
 * a set of coordinates and returns the affected element, if any.  The
 * SVGManager::HandleEvent takes as an argument a
 * SVGManager::EventData object, which should be filled in with the
 * event type, any appropriate additional information for that event
 * type and the target element of the event (as noted above).
 *
 * @section context The SVGContext
 *
 * The external interface of the SVGContext is a very simple, only a
 * virtual destructor. They are internally used by the SVG module to
 * store information at each element. Its content is not known outside
 * the SVG module.
 *
 * @section layoutprop The layout properties
 * The layout properties are used by the layout module when cascading
 * style sheets.
 *
 * For more information about the module, see the module's <a
 * href="http://wiki/developerwiki/index.php/Modules/svg">Wiki
 * page</a>.
 *
 */
