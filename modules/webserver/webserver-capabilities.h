/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  2004
 *
 * Web server capabilities
 * Lars T Hansen
 */

// Web server module is capable of disabling the multipart parser and using the parser from the MIME module
#ifndef WEBSERVER_CAPABILITIES_H
#define WEBSERVER_CAPABILITIES_H


#define WEBSERVER_CAP_NO_MULTIPART_PARSER
#define WEBSERVER_CAP_CONTROL_FILE_ACCESS
#define WEBSERVER_CAP_HAS_ISPROXIED_FUNCTION
#define WEBSERVER_CAP_HAS_ISLOCAL_FUNCTION
#define WEBSERVER_CAP_STATS_TRANSFER
#define WEBSERVER_CAP_STATS_NUM_USERS

// SetVisibleRobots(), SetVisibleASD(), SetVisibleUPNP(),
// IsVisibleRobots(), IsVisibleASD() and IsVisibleUPNP()
// are present in WebSubServer
#define WEBSERVER_CAP_SET_VISIBILITY

// WEBSERVER_LISTEN_UPNP_DISCOVERY is available
#define WEBSERVER_CAP_LISTEN_UPNP_DISCOVERY

// WEBSERVER_LISTEN_ROBOTS_TXT is available
#define WEBSERVER_CAP_LISTEN_ROBOTS_TXT

// The WebServer support "Admin", so MatchServer() and MatchServerAdmin() are available, and request to admin from other devices will be redirected
#define WEBSERVER_CAP_ADMIN_SUPPORT

// WebServer::IsListening() is supported
#define WEBSERVER_CAP_IS_LISTENING

#endif // WEBSERVER_CAPABILITIES_H
