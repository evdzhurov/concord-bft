#include "gtest/gtest.h"
#include "SeqNumInfo.hpp"
#include "InternalReplicaApi.hpp"
#include "ReplicaConfig.hpp"
#include "helper.hpp"

using namespace std;
using namespace bftEngine;
using namespace bftEngine::impl;

namespace {

class InternalReplicaApiMock : public InternalReplicaApi {
 public:
  InternalReplicaApiMock(const ReplicaConfig& config, const ReplicasInfo& info)
      : replicaConfig_{config}, replicasInfo_{info} {}
  virtual ~InternalReplicaApiMock() override {}

  virtual const ReplicasInfo& getReplicasInfo() const override { return replicasInfo_; }
  virtual bool isValidClient(NodeIdType clientId) const override { return false; /* ToDo--EDJ*/ }
  virtual bool isIdOfReplica(NodeIdType id) const override { return false; /* ToDo--EDJ */ }
  virtual const std::set<ReplicaId>& getIdsOfPeerReplicas() const override { return replicasInfo_.idsOfPeerReplicas(); }
  virtual ViewNum getCurrentView() const override { return currentView_; }
  virtual ReplicaId currentPrimary() const override { return currentPrimary_; }
  virtual bool isCurrentPrimary() const override { return false; /* ToDo--EDJ */ }
  virtual bool currentViewIsActive() const override { return false; /* ToDo--EDJ*/ }
  virtual bool isReplyAlreadySentToClient(NodeIdType clientId, ReqId reqSeqNum) const override {
    return false; /* ToDo--EDJ*/
  }
  virtual bool isClientRequestInProcess(NodeIdType clientId, ReqId reqSeqNum) const override {
    return false; /* ToDo--EDJ*/
  }
  virtual SeqNum getPrimaryLastUsedSeqNum() const override { return SeqNum{0}; /* ToDo--EDJ*/ }
  virtual uint64_t getRequestsInQueue() const override { return 0; /* ToDo--EDJ*/ }
  virtual SeqNum getLastExecutedSeqNum() const override { return SeqNum{0}; /* ToDo--EDJ*/ }
  virtual std::pair<PrePrepareMsg*, bool> buildPrePrepareMessage() override { return std::make_pair(nullptr, false); }
  virtual bool tryToSendPrePrepareMsg(bool batchingLogic) override { return false; }
  virtual std::pair<PrePrepareMsg*, bool> buildPrePrepareMsgBatchByRequestsNum(uint32_t requiredRequestsNum) override {
    return std::make_pair(nullptr, false);
  }
  virtual std::pair<PrePrepareMsg*, bool> buildPrePrepareMsgBatchByOverallSize(
      uint32_t requiredBatchSizeInBytes) override {
    return std::make_pair(nullptr, false);
  }

  virtual IncomingMsgsStorage& getIncomingMsgsStorage() override { return dummyStorage_; }
  virtual concord::util::SimpleThreadPool& getInternalThreadPool() override { return simpleThreadPool_; }

  virtual bool isCollectingState() const override { return false; /* ToDo--EDJ */ }

  virtual const ReplicaConfig& getReplicaConfig() const override { return replicaConfig_; }

 private:
  const ReplicaConfig& replicaConfig_;
  const ReplicasInfo& replicasInfo_;
  ViewNum currentView_ = ViewNum{0};
  ReplicaId currentPrimary_ = ReplicaId{0};

  class IncomingMsgsStorageDummy : public IncomingMsgsStorage {
    virtual void start() override {}
    virtual void stop() override {}
    virtual bool isRunning() const override { return false; /*ToDo--EDJ*/ };

    virtual bool pushExternalMsg(std::unique_ptr<MessageBase> msg) override { return false; /*ToDo--EDJ*/ }
    virtual bool pushExternalMsg(std::unique_ptr<MessageBase> msg, IncomingMsgsStorage::Callback onMsgPopped) override {
      return false; /*ToDo--EDJ*/
    }
    virtual bool pushExternalMsgRaw(char* msg, size_t size) override { return false; /*ToDo--EDJ*/ }
    virtual bool pushExternalMsgRaw(char* msg, size_t size, IncomingMsgsStorage::Callback onMsgPopped) override {
      return false; /*ToDo--EDJ*/
    }

    virtual void pushInternalMsg(InternalMessage&& msg) override { /*ToDo--EDJ*/
    }
  };

  IncomingMsgsStorageDummy dummyStorage_;
  concord::util::SimpleThreadPool simpleThreadPool_;
};

TEST(seqNumInfo_test, basic_tests) {
  // Default construction and destruction
  { SeqNumInfo info; }

  // Create InternalReplicaApiMock
  const ReplicaConfig& config = createReplicaConfig();
  ReplicasInfo replicasInfo(config, false, false);

  InternalReplicaApiMock replicaApiMock(config, replicasInfo);

  // Construct and destroy with context
  {
    SeqNumInfo info;
    SeqNumInfo::init(info, static_cast<InternalReplicaApi*>(&replicaApiMock));
    SeqNumInfo::reset(info);
  }
}
}  // namespace

int main(int argc, char* argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
