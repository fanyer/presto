/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef MODULES_SPATIAL_NAVIGATION_DOCUMENTATION_MAINPAGE_H
#define MODULES_SPATIAL_NAVIGATION_DOCUMENTATION_MAINPAGE_H

/** @mainpage spatial_navigation module
Spatial navigation is used for navigating through links and forms elements 
(and in the future possibly other elements as well). All these navigable 
elements will be referred to as links, except when it obviously is not. 

Spatial navigation roughly consists of three parts: 
- Handling of spatial navigation. This includes visualization, keeping track 
  of active link and frame and overall handling.  This is handled by the
  OpSnHandler class.
- Link and frame structure. This part traverses the layout tree(s) and figures
  out where to navigate (or scroll) next.  This is handled by the 
  SnTraversalObject class.
- The algorithm. Uses the link and frame structure to choose the next link, 
  based on active link and frame, direction and the placement of the other links.
  This is handled by the OpSnAlgorithm class.
  
 @see http://wiki.palace.opera.no/developerwiki/index.php/Modules/spatial_navigation
*/

#endif // !MODULES_HARDCORE_DOCUMENTATION_MAINPAGE_H
