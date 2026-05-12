cmake_minimum_required(VERSION 3.19)
project(mac-collector LANGUAGES CXX)          # ✅ 1. project() 이름 변경

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets)

find_package(QT NAMES Qt6 Qt5 COMPONENTS Widgets REQUIRED)
find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Widgets REQUIRED)

qt_standard_project_setup()
qt_add_executable(mac-collector               # ✅ 2. 실행 파일(타겟) 이름 변경
    WIN32 MACOSX_BUNDLE

    include/capture.h include/collectorwindow.h include/db.h include/dot11hdr.h include/mac.h include/parser.h include/radiotaphdr.h include/ui.h
    src/capture.cpp src/collectorwindow.cpp src/db.cpp src/mac.cpp src/main.cpp src/parser.cpp src/ui.cpp
)
target_include_directories(mac-collector PRIVATE   # ✅ 3. 타겟 이름 참조 변경
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(mac-collector           # ✅ 3. 타겟 이름 참조 변경
    PRIVATE
        Qt::Core
        Qt::Widgets
        pcap
        sqlite3
)

install(TARGETS mac-collector                 # ✅ 3. 타겟 이름 참조 변경
    BUNDLE  DESTINATION
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET mac-collector                      # ✅ 3. 타겟 이름 참조 변경
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})