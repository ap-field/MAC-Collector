cmake_minimum_required(VERSION 3.16)
project(mac-collector CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(QT NAMES Qt6 Qt5 COMPONENTS Widgets REQUIRED)
find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Widgets REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_check_modules(PCAP    REQUIRED libpcap)
pkg_check_modules(SQLITE3 REQUIRED sqlite3)

add_executable(mac-collector
    src/main.cpp
    src/mac.cpp
    src/parser.cpp
    src/db.cpp
    src/capture.cpp
    src/ui.cpp
)

target_include_directories(mac-collector PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${PCAP_INCLUDE_DIRS}
    ${SQLITE3_INCLUDE_DIRS}
)

target_link_libraries(mac-collector PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    ${PCAP_LIBRARIES}
    ${SQLITE3_LIBRARIES}
)