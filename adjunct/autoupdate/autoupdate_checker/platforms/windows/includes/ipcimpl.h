/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: s; c-basic-offset: 2 -*-
**
** Copyright (C) 2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#ifndef IPC_IMPL_H
# define IPC_IMPL_H

# include "adjunct/autoupdate/autoupdate_checker/common/common.h"
# include "adjunct/autoupdate/autoupdate_checker/adaptation_layer/ipc.h"
# include "adjunct/autoupdate/autoupdate_checker/platforms/windows/includes/ipcconfig.h"

class OAUCChannelImpl : public opera_update_checker::ipc::Channel
{
private:
  bool is_server_;
  bool is_connected_;
  opera_update_checker::ipc::Channel::ChannelMode mode_;
  opera_update_checker::ipc::Channel::ChannelFlowDirection direction_;
  char name_[MAX_CHANNEL_NAME];
  HANDLE pipe_handle_;
  SECURITY_ATTRIBUTES sec_attribs_;
  friend static opera_update_checker::ipc::Channel* opera_update_checker::ipc::Channel::Create(bool is_server,
                                                               PidType id,
                                                               opera_update_checker::ipc::Channel::ChannelMode mode,
                                                               opera_update_checker::ipc::Channel::ChannelFlowDirection flow_direction);
  void CleanUpAll();
public:
  OAUCChannelImpl();
  ~OAUCChannelImpl();
  virtual opera_update_checker::status::Status Connect(const OAUCTime& timeout);
  virtual bool IsConnected() { return is_connected_; }
  virtual void Disconnect();
  virtual opera_update_checker::status::Status SendMessage(const opera_update_checker::ipc::Message& message, const OAUCTime& timeout);
  virtual const opera_update_checker::ipc::Message* GetMessage(const OAUCTime& timeout);
};

#endif // IPC_IMPL_H
