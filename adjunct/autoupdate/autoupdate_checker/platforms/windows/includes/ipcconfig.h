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

# ifndef MAX_CHANNELS
#  define MAX_CHANNELS (1)
# endif // MAX_CHANNELS

# ifndef CHANNEL_NAME_TEMPLATE
#  define CHANNEL_NAME_TEMPLATE ("\\\\.\\pipe\\oauc_pipe%d")
# endif // CHANNEL_NAME_TEMPLATE

# ifndef MAX_CHANNEL_NAME
#  define MAX_CHANNEL_NAME (sizeof(CHANNEL_NAME_TEMPLATE) - 2 / sizeof('a') + 11)
# endif // MAX_CHANNEL_NAME

# ifndef MAX_MESSAGE_PAYLOAD_SIZE
#  define MAX_MESSAGE_PAYLOAD_SIZE ((512) * (1024)) // 512 kB
# endif // MAX_MESSAGE_PAYLOAD_SIZE

#endif // IPC_CONF_H