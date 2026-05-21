cmake_minimum_required(VERSION 3.19)
project(mac-collector LANGUAGES CXX)

set(CMAKE_PREFIX_PATH "$ENV{HOME}/Qt/6.11.0/gcc_64/lib/cmake" CACHE PATH "Qt6 CMake config directory")

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets Network Multimedia)

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

    src/capture.cpp
    src/db.cpp
    src/mac.cpp
    src/main.cpp
    src/parser.cpp
    src/ui.cpp

    resources.qrc
    setup.sh
    src/api_client.cpp
    include/api_client.h
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

# 실행파일 위치 고정: 프로젝트 루트/bin
set(FIXED_OUTPUT_DIR "${CMAKE_SOURCE_DIR}/bin")
set_target_properties(mac-collector PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${FIXED_OUTPUT_DIR}"
)

install(TARGETS mac-collector
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET mac-collector
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})