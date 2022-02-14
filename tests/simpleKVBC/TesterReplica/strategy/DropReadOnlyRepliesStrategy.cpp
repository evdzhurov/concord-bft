// Concord
//
// Copyright (c) 2018-2021 VMware, Inc. All Rights Reserved.
//
// This product is licensed to you under the Apache 2.0 license (the "License").
// You may not use this product except in compliance with the Apache 2.0
// License.
//
// This product may include a number of subcomponents with separate copyright
// notices and license terms. Your use of these subcomponents is subject to the
// terms and conditions of the subcomponent's license, as noted in the LICENSE
// file.

#include "DropReadOnlyRepliesStrategy.hpp"
#include "StrategyUtils.hpp"
#include "bftengine/ClientMsgs.hpp"
#include "messages/ClientReplyMsg.hpp"

namespace concord::kvbc::strategy {

using bftEngine::impl::MessageBase;
using bftEngine::ClientRequestMsgHeader;

std::string concord::kvbc::strategy::DropReadOnlyRepliesStrategy::getStrategyName() {
  return CLASSNAME(DropReadOnlyRepliesStrategy);
}
uint16_t concord::kvbc::strategy::DropReadOnlyRepliesStrategy::getMessageCode() { return MsgCode::ClientReply; }

bool concord::kvbc::strategy::DropReadOnlyRepliesStrategy::dropMessage(const MessageBase& msg, NodeNum) {
  const auto& replyMsg = static_cast<const ClientReplyMsg&>(msg);
  return replyMsg.isReadOnly();
}

}  // namespace concord::kvbc::strategy