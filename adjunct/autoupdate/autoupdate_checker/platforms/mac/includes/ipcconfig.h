/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: s; c-basic-offset: 2 -*-
**
** Copyright (C) 2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#ifndef IPC_CONF_H
# define IPC_CONF_H

# ifndef CHANNEL_NAME_IN
#  define CHANNEL_NAME_IN ("/tmp/oauc_server_in%d")
# endif // CHANNEL_NAME_IN

# ifndef CHANNEL_NAME_OUT
#  define CHANNEL_NAME_OUT ("/tmp/oauc_server_out%d")
# endif // CHANNEL_NAME_OUT

# ifndef MAX_CHANNEL_NAME
#  define MAX_CHANNEL_NAME (256)
# endif // MAX_CHANNEL_NAME

# ifndef MAX_MESSAGE_PAYLOAD_SIZE
#  define MAX_MESSAGE_PAYLOAD_SIZE ((512) * (1024)) // 0.5MB
# endif // MAX_MESSAGE_PAYLOAD_SIZE

#endif // IPC_CONF_H
