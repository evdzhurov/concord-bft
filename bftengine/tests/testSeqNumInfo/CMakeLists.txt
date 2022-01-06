find_package(GTest REQUIRED)

add_executable(seqNumInfo_test 
        seqNumInfo_test.cpp 
        ${bftengine_SOURCE_DIR}/tests/messages/helper.cpp
        )

add_test(seqNumInfo_test seqNumInfo_test)

target_include_directories(seqNumInfo_test PRIVATE
        ${bftengine_SOURCE_DIR}/src/bftengine
        ${bftengine_SOURCE_DIR}/include/bftengine
        ${bftengine_SOURCE_DIR}/tests/messages/
        ${threshsign_SOURCE_DIR}/include
        )

target_link_libraries(seqNumInfo_test PUBLIC
        GTest::Main
        corebft
        )