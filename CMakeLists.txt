cmake_minimum_required(VERSION 3.19)
project(mac-collector LANGUAGES CXX)

find_package(Qt6 6.4 REQUIRED COMPONENTS Core Widgets Network Multimedia)

qt_standard_project_setup()
qt_add_executable(mac-collector
    WIN32 MACOSX_BUNDLE

    include/capture.h
    include/db.h
    include/dot11hdr.h
    include/mac.h
    include/parser.h
    include/radiotaphdr.h
    include/ui.h
    include/api_client.h
    include/app.h
    include/channel_hopper.h
    include/net.h

    src/capture.cpp
    src/db.cpp
    src/mac.cpp
    src/main.cpp
    src/parser.cpp
    src/ui.cpp
    src/app.cpp
    src/api_client.cpp
    src/channel_hopper.cpp
    src/net.cpp

    resources.qrc
    setup.sh
    run.sh
)

qt_add_resources(mac-collector "resources"
    PREFIX "/audio"
    BASE "${CMAKE_SOURCE_DIR}/audio"
    FILES
        "${CMAKE_SOURCE_DIR}/audio/scan_guide.wav"
        "${CMAKE_SOURCE_DIR}/audio/mac_detected.wav"
        "${CMAKE_SOURCE_DIR}/audio/input_guide.wav"
        "${CMAKE_SOURCE_DIR}/audio/register_complete.wav"
        "${CMAKE_SOURCE_DIR}/audio/duplicate_notice.wav"
        "${CMAKE_SOURCE_DIR}/audio/update_guide.wav"
        "${CMAKE_SOURCE_DIR}/audio/update_complete.wav"
)

target_include_directories(mac-collector PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(mac-collector
    PRIVATE
        Qt6::Core
        Qt6::Widgets
        Qt6::Network
        Qt6::Multimedia
        pcap
        sqlite3
        glog
)

# 실행파일 위치 고정: 프로젝트 루트/bin
set(FIXED_OUTPUT_DIR "${CMAKE_SOURCE_DIR}/bin")
set_target_properties(mac-collector PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${FIXED_OUTPUT_DIR}"
)

# setcap은 root 권한이 필요하므로 POST_BUILD 자동 실행 불가.
# Qt Creator: Projects → Build Steps에 "set-caps" 타겟 추가하거나
# 터미널에서: cmake --build build --target set-caps
add_custom_target(set-caps
    COMMAND sudo setcap cap_net_raw,cap_net_admin=eip "${FIXED_OUTPUT_DIR}/mac-collector"
    COMMENT "Applying cap_net_raw,cap_net_admin capabilities to binary..."
    VERBATIM
)
add_dependencies(set-caps mac-collector)

install(TARGETS mac-collector
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)