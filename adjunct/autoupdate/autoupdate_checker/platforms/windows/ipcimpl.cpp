/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: s; c-basic-offset: 2 -*-
**
** Copyright (C) 2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "adjunct/autoupdate/autoupdate_checker/platforms/windows/includes/ipcimpl.h"
#include <stdio.h>

using namespace opera_update_checker::ipc;
using namespace opera_update_checker::status;

namespace
{
  DWORD TranslateDirection(Channel::ChannelFlowDirection dir)
  {
    switch(dir)
    {
      case Channel::BIDIRECTIONAL:
        return PIPE_ACCESS_DUPLEX;
      case Channel::FROM_CLIENT_TO_SERVER:
        return PIPE_ACCESS_INBOUND;
      default:
        OAUC_ASSERT(!"Unknown direction");
        // follow through
      case Channel::FROM_SERVER_TO_CLIENT:
        return PIPE_ACCESS_OUTBOUND;
    }
  }

  DWORD TranslateMode(Channel::ChannelMode mode)
  {
    switch(mode)
    {
      case Channel::CHANNEL_MODE_WRITE:
         return GENERIC_WRITE;
      case Channel::CHANNEL_MODE_READ_WRITE:
        return GENERIC_READ | GENERIC_WRITE;
      default:
        OAUC_ASSERT(!"Unknown mode");
        // fall through
      case Channel::CHANNEL_MODE_READ:
        return GENERIC_READ;
    }
  }

  inline bool IS_VALID_HANDLE(HANDLE h)
  {
    return h != INVALID_HANDLE_VALUE;
  }
  const unsigned MESSAGE_TYPE_SIZE = sizeof(int);
  const unsigned MESSAGE_HEADER_SIZE = MESSAGE_TYPE_SIZE;
  const unsigned MIN_MESSAGE_SIZE = MESSAGE_HEADER_SIZE;
  const unsigned MAX_MESSAGE_SIZE = MIN_MESSAGE_SIZE + MAX_MESSAGE_PAYLOAD_SIZE;
}

namespace opera_update_checker { namespace ipc {

/* static */ Channel* Channel::Create(bool is_server, PidType id, ChannelMode mode, ChannelFlowDirection direction)
{
  OAUCChannelImpl* this_channel = OAUC_NEW(OAUCChannelImpl, ());
  if (!this_channel)
    return NULLPTR;
  this_channel->is_server_ = is_server;
  this_channel->mode_ = mode;
  this_channel->direction_ = direction;
  sprintf(this_channel->name_, CHANNEL_NAME_TEMPLATE, id);

  if (is_server)
  {
    this_channel->sec_attribs_.bInheritHandle = TRUE;
    this_channel->pipe_handle_ = CreateNamedPipeA(this_channel->name_,
                                                FILE_FLAG_OVERLAPPED | TranslateDirection(direction),
                                                PIPE_TYPE_MESSAGE | PIPE_READMODE_BYTE,
                                                MAX_CHANNELS,
                                                MAX_MESSAGE_SIZE,
                                                MAX_MESSAGE_SIZE,
                                                0,
                                                &this_channel->sec_attribs_);
    if (!IS_VALID_HANDLE(this_channel->pipe_handle_))
    {
      OAUC_DELETE(this_channel);
      return NULLPTR;
    }
  }

  return this_channel;
}

/* static */ void Channel::Destroy(Channel* channel)
{
  OAUC_DELETE(channel);
}

} }

void OAUCChannelImpl::CleanUpAll()
{
  if (IS_VALID_HANDLE(pipe_handle_))
  {
    CancelIo(pipe_handle_);
    CloseHandle(pipe_handle_);
    pipe_handle_ = INVALID_HANDLE_VALUE;
  }
}

OAUCChannelImpl::OAUCChannelImpl()
  : is_server_(false)
  , is_connected_(false)
  , mode_(CHANNEL_MODE_READ)
  , direction_(FROM_SERVER_TO_CLIENT)
  , pipe_handle_(INVALID_HANDLE_VALUE)
{
  memset(name_, 0, sizeof(name_));
  memset(&sec_attribs_, 0, sizeof(sec_attribs_));
}

OAUCChannelImpl::~OAUCChannelImpl()
{
   Disconnect();
}

/* virtual */ Status OAUCChannelImpl::Connect(const OAUCTime& timeout)
{
  if (is_server_)
  {
    OVERLAPPED connection_overlapped_;
    ZeroMemory(&connection_overlapped_, sizeof(connection_overlapped_));
    connection_overlapped_.hEvent = CreateEvent(&sec_attribs_, TRUE, FALSE, NULL);
    if (!IS_VALID_HANDLE(connection_overlapped_.hEvent))
      return StatusCode::FAILED;

    // Disconnect first just in case any client was connected to the pipe
    // and closed its end (the server must disconnect its end before connecting again
    // in such case).
    DisconnectNamedPipe(pipe_handle_);
    BOOL retval = ConnectNamedPipe(pipe_handle_, &connection_overlapped_);
    if (!retval)
    {
      DWORD error = GetLastError();
      if (error != ERROR_PIPE_CONNECTED)
      {
        Status code = StatusCode::FAILED;
        if (error == ERROR_IO_PENDING) // Block anyway as this is what this API is supposed to do.
        {
          DWORD win_timeout = ((timeout == WAIT_INFINITELY) ? INFINITE : ((timeout == RETURN_IMMEDIATELY) ? 0 : timeout));
          DWORD result = WaitForSingleObject(connection_overlapped_.hEvent, win_timeout);
          if (result == WAIT_TIMEOUT)
            code = StatusCode::TIMEOUT;
          else if (result == WAIT_FAILED || result == WAIT_ABANDONED /* Should not happen for event. If it does return FAILED. */)
            code = StatusCode::FAILED;
          else
            code = StatusCode::OK;
        }
        CancelIo(pipe_handle_);
        CloseHandle(connection_overlapped_.hEvent);
        connection_overlapped_.hEvent = INVALID_HANDLE_VALUE;
        is_connected_ = code == StatusCode::OK;
        return code;
      }
    }
  }
  else
  {
    DWORD win_timeout = ((timeout == WAIT_INFINITELY) ? NMPWAIT_WAIT_FOREVER : ((timeout == RETURN_IMMEDIATELY) ? NMPWAIT_USE_DEFAULT_WAIT : timeout));
    if (!WaitNamedPipeA(name_, win_timeout))
      return StatusCode::TIMEOUT;

    pipe_handle_ = CreateFileA(name_, TranslateMode(mode_), 0, &sec_attribs_, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
    if (!IS_VALID_HANDLE(pipe_handle_))
      return StatusCode::FAILED;
  }

  is_connected_ = true;
  return StatusCode::OK;
}

/* virtual */ void OAUCChannelImpl::Disconnect()
{
  if (IsConnected() && is_server_)
    DisconnectNamedPipe(pipe_handle_);
  CleanUpAll();
}

/* virtual */ Status OAUCChannelImpl::SendMessage(const opera_update_checker::ipc::Message& message, const OAUCTime& timeout)
{
  if (!IsConnected())
    return StatusCode::FAILED;

  if (message.data_len > MAX_MESSAGE_PAYLOAD_SIZE)
    return StatusCode::INVALID_PARAM;

  char data[MAX_MESSAGE_SIZE];

  // Put the type.
  memcpy_s(data, MIN_MESSAGE_SIZE + message.data_len, &message.type, MESSAGE_TYPE_SIZE);

  // Now the message itself.
  memcpy_s(data + MESSAGE_TYPE_SIZE, message.data_len, message.data, message.data_len);

  OVERLAPPED overlapped;
  ZeroMemory(&overlapped, sizeof(overlapped));
  overlapped.hEvent = CreateEvent(&sec_attribs_, TRUE, FALSE, NULL);
  if (!IS_VALID_HANDLE(overlapped.hEvent))
    return StatusCode::FAILED;

  if (!WriteFile(pipe_handle_, data, MIN_MESSAGE_SIZE + message.data_len, NULL, &overlapped))
  {
    if (GetLastError() != ERROR_IO_PENDING)
      return StatusCode::FAILED;
  }

  DWORD win_timeout = ((timeout == WAIT_INFINITELY) ? INFINITE : ((timeout == RETURN_IMMEDIATELY) ? 0 : timeout));
  DWORD result = WaitForSingleObject(overlapped.hEvent, win_timeout);
  Status status = StatusCode::OK;
  if (result == WAIT_TIMEOUT)
    status = StatusCode::TIMEOUT;
  else if (result == WAIT_FAILED || result == WAIT_ABANDONED /* Should not happen for event. If it does return FAILED. */)
    status = StatusCode::FAILED;

  CloseHandle(overlapped.hEvent);
  return status;
}

namespace
{
  class MessageCleaner {
    opera_update_checker::ipc::Message* message_;
  public:
    MessageCleaner(opera_update_checker::ipc::Message* message) : message_(message) {
    };

    opera_update_checker::ipc::Message* Release() {
      opera_update_checker::ipc::Message* to_return = message_;
      message_ = NULLPTR;
      return to_return;
    }

    opera_update_checker::ipc::Message* Get() {
      return message_;
    }

    ~MessageCleaner() {
      OAUC_DELETE(message_);
    }
  };
}

/* virtual */ const opera_update_checker::ipc::Message* OAUCChannelImpl::GetMessage(const OAUCTime& timeout)
{
  if (!IsConnected())
    return NULLPTR;

  opera_update_checker::ipc::Message* message = OAUC_NEW(opera_update_checker::ipc::Message, ());
  if (!message)
    return NULLPTR;

  MessageCleaner auto_message(message);

  OVERLAPPED overlapped;
  ZeroMemory(&overlapped, sizeof(overlapped));
  overlapped.hEvent = CreateEvent(&sec_attribs_, TRUE, FALSE, NULL);
  if (!IS_VALID_HANDLE(overlapped.hEvent))
    return NULLPTR;

  char data[MAX_MESSAGE_SIZE];
  if (!ReadFile(pipe_handle_, data, MAX_MESSAGE_SIZE, &message->data_len, &overlapped))
  {
    DWORD err = GetLastError();
    if (err != ERROR_IO_PENDING)
    {
      CloseHandle(overlapped.hEvent);
      return NULLPTR;
    }
  }

  DWORD win_timeout = ((timeout == WAIT_INFINITELY) ? INFINITE : ((timeout == RETURN_IMMEDIATELY) ? 0 : timeout));
  DWORD result = WaitForSingleObject(overlapped.hEvent, win_timeout);
  Status status = StatusCode::OK;
  if (result == WAIT_TIMEOUT)
    status = StatusCode::TIMEOUT;
  else if (result == WAIT_FAILED || result == WAIT_ABANDONED /* Should not happen for event. If it does return FAILED. */)
    status = StatusCode::FAILED;

  CloseHandle(overlapped.hEvent);

  if (status == StatusCode::OK && message->data_len >= MESSAGE_TYPE_SIZE)
  {
    message->data_len -= MESSAGE_TYPE_SIZE;
    if (message->data_len > 0)
    {
      message->data = OAUC_NEWA(char, message->data_len);
      if (!message->data)
        return NULLPTR;
      message->owns_data = true;
      memcpy_s(message->data, message->data_len, &data[MESSAGE_TYPE_SIZE], message->data_len);
    }
    message->type = static_cast<MessageType>(data[0]);
    return auto_message.Release();
  }

  return NULLPTR;
}

PidType opera_update_checker::ipc::GetCurrentProcessId() {
  return ::GetCurrentProcessId();
}
