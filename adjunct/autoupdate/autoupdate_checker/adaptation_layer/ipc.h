/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: s; c-basic-offset: 2 -*-
**
** Copyright (C) 2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef OAUC_IPC_H
# define OAUC_IPC_H

#include "adjunct/autoupdate/autoupdate_checker/common/common.h"

namespace opera_update_checker
{
  namespace ipc
  {
    enum MessageType
    {
      AUTOUPDATE_STATUS
    };

    /** The struct describing a message passed between processes. */
    struct Message
    {
      Message() : data(NULLPTR), data_len(0), owns_data(false) {}
      ~Message()
      {
        if (owns_data)
          OAUC_DELETEA(data);
      }
      MessageType type;
      char* data;
      unsigned long data_len;
      bool owns_data;
    };

    class Channel;
    class Channel
    {
    private:
      Channel(Channel&) {}
    protected:
      Channel() {}
      virtual ~Channel() {}
    public:
      /** The mode of this channel. */
      enum ChannelMode
      {
        CHANNEL_MODE_READ = 0x1, /**< The channel may only be read from. */
        CHANNEL_MODE_WRITE = 0x2, /**< The channel may only be written to. */
        CHANNEL_MODE_READ_WRITE = CHANNEL_MODE_READ | CHANNEL_MODE_WRITE /**< The channel may be both read and written. */
      };

       /** The flow direction of this channel. */
      enum ChannelFlowDirection
      {
        FROM_SERVER_TO_CLIENT,
        FROM_CLIENT_TO_SERVER,
        BIDIRECTIONAL
      };

      /** The factory method.
       *
       * @param[in] is_server - If true it's the server part which creates this channel (the client otherwise).
       * @param[in] name - The unique id of the pipe to be used for creating pipe's name.
       * Its to ensure communicating processes have the same unique pipe name.
       * @param[in] mode - The mode of this channel.
       * @param[in] flow_direction - The flow direction of this channel.
       *
       * @see ChannelMode
       * @see ChannelType
       * @see ChannelFlowDirection
       */
      static Channel* Create(bool is_server, PidType id, ChannelMode mode, ChannelFlowDirection flow_direction);

      /** The terminator */
      static void Destroy(Channel* channel);

      /** Connects to the channel synchronously with a timeout.
       *
       * @param[in] timeout - The wait timeout. If it's RETURN_IMMEDIATELY there is no timeout.
       * If it's WAIT_INFINITELY this function blocks.
       *
       * @return StatusCode.
       * @see StatusCode.
       * @see OAUCTime.
       */
      virtual status::Status Connect(const OAUCTime& timeout) = 0;

      /** Returns true if the channel is connected. Otherwise false. */
      virtual bool IsConnected() = 0;

      /** Disconnects from the channel. */
      virtual void Disconnect() = 0;

      /** Sends a message synchronously with a timeout.
       *
       * @param[in] message - The message to be sent.
       * @param[in] timeout - The wait timeout. If it's RETURN_IMMEDIATELY there is no timeout.
       * If it's WAIT_INFINITELY this function blocks.
       *
       * @return StatusCode.
       * @see StatusCode.
       * @see Message.
       * @see OAUCTime.
       */
      virtual status::Status SendMessage(const Message& message, const OAUCTime& timeout) = 0;

      /** Gets a message synchronously with a timeout.
       *
       * @param[in] timeout - The wait timeout. If it's RETURN_IMMEDIATELY there is no timeout.
       * If it's WAIT_INFINITELY this function blocks.
       *
       * @return A pointer to a message or NULL on error. The caller must release the message when no longer needed.
       *
       * @see Message.
       * @see OAUCTime.
       */
      virtual const Message* GetMessage(const OAUCTime& timeout) = 0;
    };  // class Channel

    /** Returns the process Id of the current process, used for finding out
     * a unique name for a channel.
     * PidType is defined in platform-specific platform.h header and is an
     * integer type (ex. pid_t on POSIX, DWORD on Windows).
     */
    PidType GetCurrentProcessId();
  }  // namespace ipc
}

#endif // OAUC_IPC_H
