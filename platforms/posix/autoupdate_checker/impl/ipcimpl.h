// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (C) 2012 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#ifndef PLATFORMS_POSIX_AUTOUPDATE_CHECKER_IMPL_IPCIMPL_H_
#define PLATFORMS_POSIX_AUTOUPDATE_CHECKER_IMPL_IPCIMPL_H_
#ifdef POSIX_AUTOUPDATECHECKER_IMPLEMENTATION
#include "adjunct/autoupdate/autoupdate_checker/adaptation_layer/ipc.h"

namespace opera_update_checker {
namespace ipc {

/** This implementation uses two named pipes - one for sending, one for
 * receiving.*/
class ChannelImpl : public Channel {
 public:
  /**
   * @param is_server Decides which of the two pipes becomes the send pipe
   * and which gets to be the receive pipe. It's irrelevant which process gets
   * to be the server, either can be launched first and create the pipe if it's
   * not yet there.
   * @param name Will be suffixed with "s2c" and "c2s" strings to create names
   * for the server-to-client pipe and client-to-server pipe. Must be an
   * accessible name, ex. "oauc_pipes_", not "/oauc_pipes_" as the root of
   * the filesystem will usually not be writable by the user.
   */
  ChannelImpl(bool is_server, PidType id);
  virtual ~ChannelImpl();

  /** Call before using. Channel::Create() will do this for you.
   * @returns Whether the object is initialized and ready to use.
   */
  bool Init();
  virtual status::Status Connect(const OAUCTime& timeout);
  virtual bool IsConnected();
  virtual void Disconnect();
  virtual status::Status SendMessage(const Message& message,
                                     const OAUCTime& timeout);
  virtual const Message* GetMessage(const OAUCTime& timeout);

 private:
  ChannelImpl(const ChannelImpl& rhs);
  ChannelImpl& operator=(const ChannelImpl& rhs);
  void Unlink();
  bool is_server_;
  PidType id_;
  static const size_t MAX_NAME_LENGTH = 128;
  char s2c_pipe_name_[MAX_NAME_LENGTH];
  char c2s_pipe_name_[MAX_NAME_LENGTH];
  int send_pipe_;
  int receive_pipe_;
};
}  // namespace ipc
}  // namespace opera_update_checker

#endif  // POSIX_AUTOUPDATECHECKER_IMPLEMENTATION
#endif  // PLATFORMS_POSIX_AUTOUPDATE_CHECKER_IMPL_IPCIMPL_H_
