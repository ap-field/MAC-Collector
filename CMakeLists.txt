cmake_minimum_required(VERSION 3.19)
project(mac-collector LANGUAGES CXX)

set(CMAKE_PREFIX_PATH "/home/kali/Qt/6.11.0/gcc_64/lib/cmake")

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets Network Multimedia)

qt_standard_project_setup()
qt_add_executable(mac-collector
    WIN32 MACOSX_BUNDLE

    include/capture.h
    include/collectorwindow.h
    include/db.h
    include/dot11hdr.h
    include/mac.h
    include/parser.h
    include/radiotaphdr.h
    include/ui.h
    include/api_client.h
    include/vendor_lookup.h

    src/capture.cpp
    src/collectorwindow.cpp
    src/db.cpp
    src/mac.cpp
    src/main.cpp
    src/parser.cpp
    src/ui.cpp
    src/api_client.cpp
    src/vendor_lookup.cpp

    resources.qrc
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
)

# # setcap 절대경로로 지정
# add_custom_command(TARGET mac-collector POST_BUILD
#     COMMAND /usr/sbin/setcap cap_net_raw,cap_net_admin=eip $<TARGET_FILE:mac-collector>
#     COMMENT "Setting packet capture capability..."
# )

install(TARGETS mac-collector
    BUNDLE  DESTINATION
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET mac-collector
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})