// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (C) 2012 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#ifdef POSIX_AUTOUPDATECHECKER_IMPLEMENTATION
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <cstring>
#include <ctime>
#include <algorithm>
#include "platforms/posix/autoupdate_checker/impl/ipcimpl.h"
#include "platforms/posix/autoupdate_checker/impl/buffer.h"

namespace oaucipc = opera_update_checker::ipc;
using oaucipc::Channel;
using oaucipc::ChannelImpl;
using oaucipc::MessageType;
using opera_update_checker::status::Status;
using opera_update_checker::status::StatusCode;

namespace {
/* These names will be suffixed by the process id of the server. This process
 * id will be given to the client as a command line parameter upon launch. */
const char* s2c_pipe = "/tmp/oaucipc-s2c-";
const char* c2s_pipe = "/tmp/oaucipc-c2s-";

struct Header {
  uint32_t data_len;
  oaucipc::MessageType message_type;
};

int TimedOpen(const char* pipe_name, int oflag, OAUCTime* timeout) {
  while (!*timeout <= 0) {
    int pipe = open(pipe_name, oflag|O_NONBLOCK);
    if (pipe == -1 && errno == ENXIO) {
      usleep(1000);  // wait for 1ms and try again
      *timeout -= 1;
    } else {
      // We've either connected or found an error that we can't wait through
      return pipe;
    }
  }
  return -1;
}

const ssize_t OPERATION_TIMED_OUT = -2;
ssize_t TimedWrite(int pipefd,
                   const void* data,
                   size_t data_len,
                   OAUCTime* timeout) {
  size_t bytes_sent = 0;
  while (!*timeout <= 0 && bytes_sent < data_len) {
    ssize_t write_results =
        write(pipefd,
              reinterpret_cast<const char*>(data) + bytes_sent,
              data_len - bytes_sent);
    if (write_results == -1 && errno == EAGAIN) {
      // Pipe is busy, didn't send anything
      usleep(1000);  // wait for 1ms and try again
      *timeout -= 1;
    } else if (write_results == -1) {
      // Unrecoverable error
      return write_results;
    } else {
      bytes_sent += write_results;
    }
  }
  return *timeout <= 0 ? OPERATION_TIMED_OUT : bytes_sent;
}

ssize_t TimedRead(int pipefd,
                   void* data,
                   size_t data_len,
                   OAUCTime* timeout) {
  size_t bytes_read = 0;
  while (!*timeout <= 0 && bytes_read < data_len) {
    ssize_t read_results = read(pipefd,
                                reinterpret_cast<char*>(data) + bytes_read,
                                data_len - bytes_read);
    if (read_results == -1 && errno == EAGAIN) {
      // Pipe is busy, didn't read anything
      usleep(1000);  // wait for 1ms and try again
      *timeout -= 1;
    } else if (read_results == -1) {
      // Unrecoverable error
      return read_results;
    } else {
      bytes_read += read_results;
    }
  }
  return *timeout <= 0 ? OPERATION_TIMED_OUT : bytes_read;
}
}  // anonymous namespace

ChannelImpl::ChannelImpl(bool is_server, PidType id)
  : is_server_(is_server),
    id_(id),
    send_pipe_(-1),
    receive_pipe_(-1) {
  memset(s2c_pipe_name_, '\0', MAX_NAME_LENGTH);
  memset(c2s_pipe_name_, '\0', MAX_NAME_LENGTH);
}

ChannelImpl::~ChannelImpl() {
  Unlink();
}

bool ChannelImpl::Init() {
  bool success = 0 < snprintf(
        s2c_pipe_name_,
        MAX_NAME_LENGTH,
        "%s%x",
        s2c_pipe,
        id_);
  success &= 0 < snprintf(
        c2s_pipe_name_,
        MAX_NAME_LENGTH,
        "%s%x",
        c2s_pipe,
        id_);
  if (is_server_) {
    success &= mkfifo(s2c_pipe_name_, S_IRWXU) == 0;
    success &= mkfifo(c2s_pipe_name_, S_IRWXU) == 0;
  }
  return success;
}

Status ChannelImpl::Connect(const OAUCTime& timeout) {
  OAUCTime time_left = timeout;
  int c2s_pipe =
      TimedOpen(c2s_pipe_name_, is_server_ ? O_RDONLY : O_WRONLY, &time_left);
  int s2c_pipe =
      TimedOpen(s2c_pipe_name_, is_server_ ? O_WRONLY : O_RDONLY, &time_left);
  if (is_server_) {
    send_pipe_ = s2c_pipe;
    receive_pipe_ = c2s_pipe;
  } else {
    receive_pipe_ = s2c_pipe;
    send_pipe_ = c2s_pipe;
  }

  if (IsConnected()) {
      return StatusCode::OK;
  } else {
    Disconnect();
    if (errno == ENOSPC)
      return StatusCode::OOM;
    return StatusCode::FAILED;
  }
}

bool ChannelImpl::IsConnected() {
  return send_pipe_ != -1 && receive_pipe_ != -1;
}

void ChannelImpl::Disconnect() {
  Unlink();
}

Status ChannelImpl::SendMessage(const Message& message,
                                const OAUCTime& timeout) {
  if (!IsConnected()) {
    return StatusCode::FAILED;
  }
  OAUCTime time_left = timeout;

  Header header;
  header.data_len = message.data_len;
  header.message_type = message.type;
  ssize_t header_bytes_written =
      TimedWrite(send_pipe_, &header, sizeof(header), &time_left);
  if (header_bytes_written == OPERATION_TIMED_OUT)
    return StatusCode::TIMEOUT;
  if (header_bytes_written != sizeof(header))
    return StatusCode::FAILED;
  ssize_t data_bytes_written =
      TimedWrite(send_pipe_, message.data, message.data_len, &time_left);
  if (data_bytes_written == OPERATION_TIMED_OUT)
    return StatusCode::TIMEOUT;
  if (data_bytes_written != message.data_len)
    return StatusCode::FAILED;

  return StatusCode::OK;
}

const oaucipc::Message* ChannelImpl::GetMessage(const OAUCTime& timeout) {
  if (!IsConnected()) {
    return NULLPTR;
  }
  OAUCTime time_left = timeout;
  Header header;
  ssize_t header_bytes_read =
      TimedRead(receive_pipe_, &header, sizeof(header), &time_left);
  if (header_bytes_read != sizeof(header))
    return NULLPTR;
  Buffer<char> receive_buffer;
  if (!receive_buffer.Allocate(header.data_len))
    return NULLPTR;
  ssize_t data_bytes_read = TimedRead(receive_pipe_,
                                      receive_buffer.Pointer(),
                                      receive_buffer.Size(),
                                      &time_left);
  if (data_bytes_read != header.data_len)
    return NULLPTR;
  oaucipc::Message* msg = OAUC_NEW(oaucipc::Message, ());
  if (!msg)
    return NULLPTR;
  msg->data = receive_buffer.Release();
  msg->data_len = header.data_len;
  msg->type = header.message_type;
  msg->owns_data = true;
  return msg;
}

void ChannelImpl::Unlink() {
  close(receive_pipe_);
  close(send_pipe_);
  receive_pipe_ = -1;
  send_pipe_ = -1;
  if (is_server_) {
    unlink(c2s_pipe_name_);
    unlink(s2c_pipe_name_);
  }
}

/*static */
Channel* Channel::Create(bool is_server,
                         PidType id,
                         ChannelMode  /*mode*/,
                         ChannelFlowDirection  /*flow_direction*/) {
  ChannelImpl* obj = OAUC_NEW(ChannelImpl, (is_server, id));
  if (obj && obj->Init())
    return obj;
  OAUC_DELETE(obj);
  return NULLPTR;
}

/*static */
void Channel::Destroy(Channel* channel) {
  OAUC_DELETE(channel);
}

PidType oaucipc::GetCurrentProcessId() {
  return getpid();
}
#endif  // POSIX_AUTOUPDATECHECKER_IMPLEMENTATION
