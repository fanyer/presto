/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: s; c-basic-offset: 2 -*-
**
** Copyright (C) 2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "../includes/ipcconfig.h"
#include "../includes/ipcimpl.h"
#include <string.h>
#include <errno.h>

using namespace opera_update_checker::ipc;
using namespace opera_update_checker::status;

namespace opera_update_checker
{
  namespace ipc
  {

   /* static */ Channel* Channel::Create(bool is_server, PidType id, ChannelMode mode, ChannelFlowDirection direction)
   {
      OAUCChannelImpl* this_channel = OAUC_NEW(OAUCChannelImpl, ());
      if (!this_channel)
        return NULL;

      this_channel->is_server_ = is_server;
      this_channel->mode_ = mode;
      this_channel->direction_ = direction;

      sprintf(this_channel->name_in_, CHANNEL_NAME_IN, id);
      sprintf(this_channel->name_out_, CHANNEL_NAME_OUT, id);

      if (is_server)
      {
        //Create fifo files
        unlink(this_channel->name_in_);
        if (mkfifo(this_channel->name_in_, S_IRWXU) <0) //server in
        {
          return NULLPTR;
        }

        unlink(this_channel->name_out_);
        if (mkfifo(this_channel->name_out_, S_IRWXU) <0) //server out
        {
          return NULLPTR;
        }
      }

      return this_channel;
    }

    /* static */ void Channel::Destroy(Channel* channel)
    {
      OAUC_DELETE(channel);
    }
  }
}

OAUCChannelImpl::OAUCChannelImpl()
  : is_server_(false)
  , is_connected_(false)
  , mode_(Channel::CHANNEL_MODE_READ)
  , direction_(Channel::FROM_SERVER_TO_CLIENT)
  , pipe_in_(0)
  , pipe_out_(0)
{
  memset(name_in_, 0, MAX_CHANNEL_NAME * sizeof(char));
  memset(name_out_, 0, MAX_CHANNEL_NAME * sizeof(char));
}

OAUCChannelImpl::~OAUCChannelImpl()
{
  Disconnect();
  CleanUpAll();
}

void OAUCChannelImpl::CleanUpAll()
{
  if (pipe_in_ > 0)
    close(pipe_in_);
  if (pipe_out_ > 0)
    close(pipe_out_);

  pipe_in_ = 0;
  pipe_out_ = 0;

  if (is_server_ == true)
  {
    unlink(this->name_in_);
    unlink(this->name_out_);
  }
}

/* virtual */ Status OAUCChannelImpl::Connect(const OAUCTime& timeout)
{
  int open_flags = O_RDONLY | O_NONBLOCK;

  if (pipe_in_ == 0)
  {
    pipe_in_ = open(is_server_ ? name_in_ : name_out_, open_flags);
  }

  if (pipe_in_ <= 0)
  {
    return StatusCode::FAILED;
  }

  is_connected_ = true;
  return StatusCode::OK;
}

/* virtual */ void OAUCChannelImpl::Disconnect()
{
  CleanUpAll();
}

/* virtual */ Status OAUCChannelImpl::SendMessage(const opera_update_checker::ipc::Message& message, const OAUCTime& timeout)
{
  int status = StatusCode::OK;

  if (pipe_out_ == 0)
  {
    OAUCTime time_elapsed = 0;
    for (;;)
    {
      pipe_out_ = open(is_server_ ? name_out_ : name_in_, O_WRONLY | O_NONBLOCK);
      if (pipe_out_ < 0)
      {
        if (time_elapsed >= timeout)
        {
          return StatusCode::TIMEOUT;
        }
        time_elapsed += 10;
        usleep(10000); //wait 10ms
      }
      else
      {
        break;
      }
    }
  }

  if (pipe_out_ <= 0)
  {
    pipe_out_ = 0;
    return StatusCode::FAILED;
  }

  fd_set read_fds, write_fds, except_fds;
  FD_ZERO(&read_fds);
  FD_ZERO(&write_fds);
  FD_ZERO(&except_fds);
  FD_SET(pipe_out_, &write_fds);

  struct timeval write_timeout;
  write_timeout.tv_sec = timeout / 1000;
  write_timeout.tv_usec = timeout - (write_timeout.tv_sec * 1000);

  long size = sizeof(message)+message.data_len;
  long data_remaining = size;
  char *buf = OAUC_NEWA(char, size);
  char *msg_start = buf;
  memcpy(buf, &message, sizeof(message));
  memcpy(buf+sizeof(message), message.data, message.data_len);

  while (data_remaining > 0)
  {
    long data_len = data_remaining > OAUC_PIPE_SIZE ? OAUC_PIPE_SIZE : data_remaining;
    if (select(pipe_out_ + 1, &read_fds, &write_fds, &except_fds, &write_timeout) == 1)
    {
      ssize_t bytes_written = write(pipe_out_, buf, data_len);
      if (bytes_written < 0 && errno != EAGAIN)
      {
          status = StatusCode::FAILED;
          break;
      }
      else if (bytes_written > 0)
      {
        data_remaining -= bytes_written;
        buf += bytes_written;
      }
    }
    else
    {
      status = StatusCode::TIMEOUT;
      break;
    }
  }

  if (msg_start != NULL)
  {
    OAUC_DELETEA(msg_start);
  }

  return status;
}

/* virtual */ const opera_update_checker::ipc::Message* OAUCChannelImpl::GetMessage(const OAUCTime& timeout)
{
  int status = StatusCode::OK;
  opera_update_checker::ipc::Message *msg = NULLPTR;

  fd_set read_fds, write_fds, except_fds;
  FD_ZERO(&read_fds);
  FD_ZERO(&write_fds);
  FD_ZERO(&except_fds);
  FD_SET(pipe_in_, &read_fds);
  struct timeval read_timeout;
  read_timeout.tv_sec = timeout / 1000;
  read_timeout.tv_usec = timeout - (read_timeout.tv_sec * 1000);

  if (select(pipe_in_ + 1, &read_fds, &write_fds, &except_fds, &read_timeout) == 1)
  {
    msg = OAUC_NEW(opera_update_checker::ipc::Message, ());
    memset(msg, 0, sizeof(opera_update_checker::ipc::Message));
    ssize_t bytes_read = read(pipe_in_, (void*)msg, sizeof(opera_update_checker::ipc::Message));
    if (bytes_read > 0)
    {
      //read message data
      if (bytes_read == sizeof(opera_update_checker::ipc::Message))
      {
        if (msg->data_len < MAX_MESSAGE_PAYLOAD_SIZE)
        {
          msg->data = OAUC_NEWA(char, msg->data_len);
          msg->owns_data = true;

          FD_ZERO(&read_fds);
          FD_ZERO(&write_fds);
          FD_ZERO(&except_fds);
          FD_SET(pipe_in_, &read_fds);

          bool first = true;
          long data_remaining = msg->data_len + sizeof(opera_update_checker::ipc::Message);
          long offset = 0;
          while (data_remaining > 0)
          {
            if (select(pipe_in_ + 1, &read_fds, &write_fds, &except_fds, &read_timeout) == 1)
            {
              if (first == true)
              {
                data_remaining -= sizeof(opera_update_checker::ipc::Message);
              }
              long data_len = data_remaining > OAUC_PIPE_SIZE ? OAUC_PIPE_SIZE : data_remaining;
              bytes_read = read(pipe_in_, msg->data + offset, data_len);
              if (bytes_read < 0 && errno != EAGAIN)
              {
                status = StatusCode::FAILED;
                break;
              }
              else
              {
                first = false;
                offset += bytes_read;
                data_remaining -= bytes_read;
                if (bytes_read == 0) //end of file
                {
                  break;
                }
              }
            }
            else
            {
              status = StatusCode::TIMEOUT;
              break;
            }
          }
        }
        else
        {
          status = StatusCode::FAILED;
        }
      }
      else
      {
        status = StatusCode::FAILED;
      }
    }
  }
  else
  {
    status = StatusCode::TIMEOUT;
  }

  if (status == StatusCode::OK)
  {
    return msg;
  }
  else
  {
    if (msg != NULL)
    {
      OAUC_DELETE(msg);
      msg = NULLPTR;
    }
    return NULLPTR;
  }
}

/*static */
PidType opera_update_checker::ipc::GetCurrentProcessId() {
  return getpid();
}
