// Concord
//
// Copyright (c) 2018-2019 VMware, Inc. All Rights Reserved.
//
// This product is licensed to you under the Apache 2.0 license (the "License").
// You may not use this product except in compliance with the Apache 2.0
// License.
//
// This product may include a number of subcomponents with separate copyright
// notices and license terms. Your use of these subcomponents is subject to the
// terms and conditions of the sub-component's license, as noted in the LICENSE
// file.

#include "IncomingMsgsStorageImp.hpp"
#include "messages/InternalMessage.hpp"
#include "Logger.hpp"
#include <future>

using std::queue;
using namespace std::chrono;
using namespace concord::diagnostics;

namespace bftEngine::impl {

IncomingMsgsStorageImp::IncomingMsgsStorageImp(const std::shared_ptr<MsgHandlersRegistrator>& msgHandlersPtr,
                                               std::chrono::milliseconds msgWaitTimeout,
                                               uint16_t replicaId)
    : IncomingMsgsStorage(),
      msgHandlers_(msgHandlersPtr),
      msgWaitTimeout_(msgWaitTimeout),
      take_lock_recorder_(histograms_.take_lock),
      wait_for_cv_recorder_(histograms_.wait_for_cv) {
  replicaId_ = replicaId;
  ptrProtectedQueueForExternalMessages_ = new queue<MessageWithCallback>();
  ptrProtectedQueueForInternalMessages_ = new queue<InternalMessage>();
  lastOverflowWarning_ = MinTime;
  ptrThreadLocalQueueForExternalMessages_ = new queue<MessageWithCallback>();
  ptrThreadLocalQueueForInternalMessages_ = new queue<InternalMessage>();
}

IncomingMsgsStorageImp::~IncomingMsgsStorageImp() {
  delete ptrProtectedQueueForExternalMessages_;
  delete ptrProtectedQueueForInternalMessages_;
  delete ptrThreadLocalQueueForExternalMessages_;
  delete ptrThreadLocalQueueForInternalMessages_;
}

void IncomingMsgsStorageImp::start() {
  if (!dispatcherThread_.joinable()) {
    std::future<void> futureObj = signalStarted_.get_future();
    dispatcherThread_ = std::thread([=] { dispatchMessages(signalStarted_); });
    // Wait until thread starts
    futureObj.get();
  };
}

void IncomingMsgsStorageImp::stop() {
  if (dispatcherThread_.joinable()) {
    stopped_ = true;
    dispatcherThread_.join();
    LOG_INFO(GL, "Dispatching thread stopped");
  }
}

bool IncomingMsgsStorageImp::pushExternalMsg(std::unique_ptr<MessageBase> msg) {
  return pushExternalMsg(std::move(msg), Callback{});
}

// can be called by any thread
bool IncomingMsgsStorageImp::pushExternalMsg(std::unique_ptr<MessageBase> msg, Callback onMsgPopped) {
  MsgCode::Type type = static_cast<MsgCode::Type>(msg->type());
  LOG_TRACE(MSGS, type);
  std::unique_lock<std::mutex> mlock(lock_);
  if (ptrProtectedQueueForExternalMessages_->size() >= maxNumberOfPendingExternalMsgs_) {
    Time now = getMonotonicTime();
    auto msg_type = static_cast<MsgCode::Type>(msg->type());
    if ((now - lastOverflowWarning_) > (milliseconds(minTimeBetweenOverflowWarningsMilli_))) {
      LOG_WARN(GL, "Queue Full. Dropping some msgs." << KVLOG(maxNumberOfPendingExternalMsgs_, msg_type));
      lastOverflowWarning_ = now;
    }
    dropped_msgs++;
    return false;
  }
  histograms_.dropped_msgs_in_a_row->record(dropped_msgs);
  dropped_msgs = 0;
  ptrProtectedQueueForExternalMessages_->push(std::make_pair(std::move(msg), std::move(onMsgPopped)));
  condVar_.notify_one();
  return true;
}

bool IncomingMsgsStorageImp::pushExternalMsgRaw(char* msg, size_t size) {
  return pushExternalMsgRaw(msg, size, Callback{});
}

bool IncomingMsgsStorageImp::pushExternalMsgRaw(char* msg, size_t size, Callback onMsgPopped) {
  size_t actualSize = 0;
  MessageBase* mb = MessageBase::deserializeMsg(msg, size, actualSize);
  return pushExternalMsg(std::unique_ptr<MessageBase>(mb), std::move(onMsgPopped));
}

// can be called by any thread
void IncomingMsgsStorageImp::pushInternalMsg(InternalMessage&& msg) {
  std::unique_lock<std::mutex> mlock(lock_);
  ptrProtectedQueueForInternalMessages_->push(std::move(msg));
  condVar_.notify_one();
}

// should only be called by the dispatching thread
IncomingMsg IncomingMsgsStorageImp::getMsgForProcessing() {
  auto msg = popThreadLocal();
  if (msg.tag != IncomingMsg::INVALID) return msg;
  {
    take_lock_recorder_.start();
    std::unique_lock<std::mutex> mlock(lock_);
    take_lock_recorder_.end();
    {
      if (ptrProtectedQueueForExternalMessages_->empty() && ptrProtectedQueueForInternalMessages_->empty()) {
        LOG_TRACE(MSGS, "Waiting for condition variable");
        wait_for_cv_recorder_.start();
        condVar_.wait_for(mlock, msgWaitTimeout_);
        wait_for_cv_recorder_.end();
      }

      // no new message
      if (ptrProtectedQueueForExternalMessages_->empty() && ptrProtectedQueueForInternalMessages_->empty()) {
        LOG_DEBUG(MSGS, "No pending messages");
        return IncomingMsg();
      }

      // swap queues
      auto t1 = ptrThreadLocalQueueForExternalMessages_;
      ptrThreadLocalQueueForExternalMessages_ = ptrProtectedQueueForExternalMessages_;
      ptrProtectedQueueForExternalMessages_ = t1;
      histograms_.external_queue_len_at_swap->record(ptrThreadLocalQueueForExternalMessages_->size());

      auto* t2 = ptrThreadLocalQueueForInternalMessages_;
      ptrThreadLocalQueueForInternalMessages_ = ptrProtectedQueueForInternalMessages_;
      ptrProtectedQueueForInternalMessages_ = t2;
      histograms_.internal_queue_len_at_swap->record(ptrThreadLocalQueueForInternalMessages_->size());
    }
  }
  return popThreadLocal();
}

IncomingMsg IncomingMsgsStorageImp::popThreadLocal() {
  if (!ptrThreadLocalQueueForInternalMessages_->empty()) {
    auto msg = IncomingMsg{std::move(ptrThreadLocalQueueForInternalMessages_->front())};
    ptrThreadLocalQueueForInternalMessages_->pop();
    return msg;
  } else if (!ptrThreadLocalQueueForExternalMessages_->empty()) {
    auto& item = ptrThreadLocalQueueForExternalMessages_->front();
    if (item.second) {
      item.second();
    }
    auto msg = IncomingMsg{std::move(item.first)};
    ptrThreadLocalQueueForExternalMessages_->pop();
    return msg;
  } else {
    return IncomingMsg{};
  }
}

void IncomingMsgsStorageImp::dispatchMessages(std::promise<void>& signalStarted) {
  signalStarted.set_value();
  MDC_PUT(MDC_REPLICA_ID_KEY, std::to_string(replicaId_));
  MDC_PUT(MDC_THREAD_KEY, "message-processing");
  try {
    while (!stopped_) {
      auto msg = getMsgForProcessing();
      {
        TimeRecorder scoped_timer(*histograms_.evaluate_timers);
        timers_.evaluate();
      }

      MsgHandlerCallback msgHandlerCallback = nullptr;
      switch (msg.tag) {
        case IncomingMsg::INVALID:
          LOG_TRACE(GL, "Invalid message - ignore");
          break;
        case IncomingMsg::EXTERNAL: {
          MsgCode::Type type = static_cast<MsgCode::Type>(msg.external->type());
          LOG_TRACE(MSGS, type);
          msgHandlerCallback = msgHandlers_->getCallback(msg.external->type());
          if (msgHandlerCallback) {
            msgHandlerCallback(std::move(msg.external));
          } else {
            LOG_WARN(GL,
                     "Received unknown external message"
                         << KVLOG(msg.external->type(), msg.external->senderId(), msg.external->size()));
          }
        } break;
        case IncomingMsg::INTERNAL:
          msgHandlers_->handleInternalMsg(std::move(msg.internal));
      };
    }
  } catch (const std::exception& e) {
    LOG_FATAL(GL, "Exception: " << e.what() << "exiting ...");
    std::terminate();
  }
}

}  // namespace bftEngine::impl
