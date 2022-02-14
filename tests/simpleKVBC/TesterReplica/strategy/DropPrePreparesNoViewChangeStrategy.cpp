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

#include "DropPrePreparesNoViewChangeStrategy.hpp"
#include "StrategyUtils.hpp"
#include "bftengine/ClientMsgs.hpp"
#include "messages/PrePrepareMsg.hpp"
#include "ReplicaConfig.hpp"

namespace concord::kvbc::strategy {

using bftEngine::impl::MessageBase;
using bftEngine::impl::PrePrepareMsg;
using bftEngine::ClientRequestMsgHeader;

std::string concord::kvbc::strategy::DropPrePreparesNoViewChangeStrategy::getStrategyName() {
  return CLASSNAME(DropPrePreparesNoViewChangeStrategy);
}
uint16_t concord::kvbc::strategy::DropPrePreparesNoViewChangeStrategy::getMessageCode() { return MsgCode::PrePrepare; }

bool concord::kvbc::strategy::DropPrePreparesNoViewChangeStrategy::dropMessage(const MessageBase&, NodeNum dest) {
  // Do not send pre-prepares to f - 1 replicas to isolate them from executing requests.
  return dest < bftEngine::ReplicaConfig::instance().getfVal();
}

}  // namespace concord::kvbc::strategy