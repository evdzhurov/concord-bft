cmake_minimum_required (VERSION 3.2)
project(skvbc_replica VERSION 0.1.0.0 LANGUAGES CXX)

set(byzantine_strategy_src
	strategy/StrategyUtils.cpp
    strategy/DropPrePreparesNoViewChangeStrategy.cpp
	strategy/DropReadOnlyRepliesStrategy.cpp
	strategy/MangledPreProcessResultMsgStrategy.cpp
	strategy/ShufflePrePrepareMsgStrategy.cpp)

add_executable(skvbc_replica main.cpp
		 internalCommandsHandler.cpp
		 setup.cpp
		 ${concord_bft_tools_SOURCE_DIR}/KeyfileIOUtils.cpp
		 WrapCommunication.cpp
		 ${byzantine_strategy_src})

if(${USE_COMM_PLAIN_TCP})
	target_compile_definitions(skvbc_replica PUBLIC USE_COMM_PLAIN_TCP)
endif()

if(${USE_COMM_TLS_TCP})
	target_compile_definitions(skvbc_replica PUBLIC USE_COMM_TLS_TCP)
endif()

if(BUILD_ROCKSDB_STORAGE)
	target_compile_definitions(skvbc_replica PUBLIC "USE_ROCKSDB=1")
endif()

target_link_libraries(skvbc_replica PUBLIC kvbc corebft threshsign util test_config_lib stdc++fs skvbc_messages_cmf)

target_include_directories(skvbc_replica PUBLIC ..)
target_include_directories(skvbc_replica PUBLIC ../..)
target_include_directories(skvbc_replica PUBLIC ${libkvbc_SOURCE_DIR}/include)
target_include_directories(skvbc_replica PRIVATE ${bftengine_SOURCE_DIR}/src/preprocessor)



