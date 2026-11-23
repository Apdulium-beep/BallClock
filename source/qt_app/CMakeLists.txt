cmake_minimum_required(VERSION 3.19)
project(BallClockController LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(MSVC)
    add_compile_options(/utf-8)
endif()

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets Network)

qt_standard_project_setup()
qt_add_library(clocktransport STATIC clocktransport.cpp clocktransport.h)
target_link_libraries(clocktransport PUBLIC Qt6::Core)

set(FASTLED_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../firmware/.pio/libdeps/d1_mini/FastLED/src")
set(REFERENCE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../firmware/src")
set(ENGINE_DLL "${CMAKE_CURRENT_BINARY_DIR}/clockengine.dll")
add_custom_command(OUTPUT "${ENGINE_DLL}"
    COMMAND ${CMAKE_COMMAND} -E env "PATH=C:/Qt/Tools/mingw1310_64/bin\;$ENV{PATH}" C:/Qt/Tools/mingw1310_64/bin/g++.exe -std=c++17 -O2 -flto -fwhole-program -shared -static -ffunction-sections -fdata-sections -Wl,--gc-sections,--exclude-all-symbols
        -DFASTLED_STUB_IMPL -DFASTLED_NO_PINMAP -DHAS_HARDWARE_PIN_SUPPORT -DFASTLED_HAS_ENGINE_EVENTS=0 -DSKETCH_HAS_LARGE_MEMORY=0 -DFL_PLATFORM_HAS_TINY_MEMORY=1 -DFL_PLATFORM_HAS_HUGE_MEMORY=0 -DFL_PLATFORM_HAS_LARGE_MEMORY=0
        -I${CMAKE_CURRENT_SOURCE_DIR}/native -I${REFERENCE_ROOT} -I${FASTLED_ROOT}
        ${CMAKE_CURRENT_SOURCE_DIR}/native/clockengine.cpp ${CMAKE_CURRENT_SOURCE_DIR}/native/fastled_subset.cpp
        ${REFERENCE_ROOT}/render.cpp ${REFERENCE_ROOT}/geometry.cpp
        -o "${ENGINE_DLL}"
    DEPENDS native/clockengine.cpp native/fastled_subset.cpp native/clockengine.h native/Arduino.h
        ${REFERENCE_ROOT}/render.cpp ${REFERENCE_ROOT}/render.h ${REFERENCE_ROOT}/settings.h ${REFERENCE_ROOT}/gradient.h ${REFERENCE_ROOT}/artcodec.h
        ${REFERENCE_ROOT}/font_diag.h ${REFERENCE_ROOT}/font_4x5.h ${REFERENCE_ROOT}/geometry.cpp ${REFERENCE_ROOT}/geometry.h
    VERBATIM)
add_custom_target(clockengine ALL DEPENDS "${ENGINE_DLL}")

qt_add_executable(BallClockController
    WIN32 MACOSX_BUNDLE
    main.cpp
    mainwindow.cpp
    mainwindow.h
    ledmatrixwidget.h
    ledmatrixwidget.cpp
    usbclock.h
    usbclock.cpp
    pcrenderer.cpp
    pcrenderer.h
    wificlock.cpp
    wificlock.h
    ../../firmware/src/geometry.cpp
)

target_link_libraries(BallClockController
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Network
)
target_include_directories(BallClockController PRIVATE ../../firmware/src)
add_dependencies(BallClockController clockengine)

install(TARGETS BallClockController
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET BallClockController
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})

# Explicit read-only hardware check: usb_probe.exe COM5 (sends only '?').
qt_add_executable(usb_probe EXCLUDE_FROM_ALL tests/usb_probe.cpp usbclock.cpp usbclock.h)
target_include_directories(usb_probe PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(usb_probe PRIVATE Qt6::Core)

qt_add_executable(protocol_test EXCLUDE_FROM_ALL tests/protocol_test.cpp usbclock.cpp usbclock.h)
target_include_directories(protocol_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(protocol_test PRIVATE Qt6::Core)
add_executable(geometry_test EXCLUDE_FROM_ALL ../../firmware/test/geo_test.cpp ../../firmware/src/geometry.cpp)
target_include_directories(geometry_test PRIVATE ../../firmware/src)
enable_testing()
add_test(NAME usb_protocol COMMAND protocol_test)
add_test(NAME firmware_geometry COMMAND geometry_test)
qt_add_executable(display_test EXCLUDE_FROM_ALL tests/display_test.cpp ledmatrixwidget.cpp ledmatrixwidget.h ../../firmware/src/geometry.cpp)
target_include_directories(display_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ../../firmware/src)
target_link_libraries(display_test PRIVATE Qt6::Widgets)
target_sources(display_test PRIVATE pcrenderer.cpp)
add_dependencies(display_test clockengine)
add_test(NAME exact_rgb_display COMMAND display_test)
set_tests_properties(exact_rgb_display PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
qt_add_executable(settings_ui_test EXCLUDE_FROM_ALL tests/settings_ui_test.cpp mainwindow.cpp mainwindow.h usbclock.cpp usbclock.h ledmatrixwidget.cpp ledmatrixwidget.h ../../firmware/src/geometry.cpp)
target_include_directories(settings_ui_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ../../firmware/src)
target_link_libraries(settings_ui_test PRIVATE Qt6::Widgets)
target_sources(settings_ui_test PRIVATE wificlock.cpp wificlock.h)
target_link_libraries(settings_ui_test PRIVATE Qt6::Network)
target_sources(settings_ui_test PRIVATE pcrenderer.cpp)
add_dependencies(settings_ui_test clockengine)
add_test(NAME read_settings_without_overwrite COMMAND settings_ui_test)
set_tests_properties(read_settings_without_overwrite PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
qt_add_executable(usb_live_test EXCLUDE_FROM_ALL tests/usb_live_test.cpp mainwindow.cpp mainwindow.h usbclock.cpp usbclock.h ledmatrixwidget.cpp ledmatrixwidget.h ../../firmware/src/geometry.cpp)
target_include_directories(usb_live_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ../../firmware/src)
target_link_libraries(usb_live_test PRIVATE Qt6::Widgets)
target_sources(usb_live_test PRIVATE wificlock.cpp wificlock.h)
target_link_libraries(usb_live_test PRIVATE Qt6::Network)
target_sources(usb_live_test PRIVATE pcrenderer.cpp)
add_dependencies(usb_live_test clockengine)
qt_add_executable(engine_test EXCLUDE_FROM_ALL tests/engine_test.cpp pcrenderer.cpp ../../firmware/src/geometry.cpp)
target_include_directories(engine_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ../../firmware/src)
target_link_libraries(engine_test PRIVATE Qt6::Core)
add_dependencies(engine_test clockengine)
add_test(NAME offline_pc_renderer COMMAND engine_test)
foreach(target BallClockController usb_probe protocol_test settings_ui_test usb_live_test)
    target_link_libraries(${target} PRIVATE clocktransport)
endforeach()
qt_add_executable(wifi_transport_test EXCLUDE_FROM_ALL tests/wifi_transport_test.cpp wificlock.cpp wificlock.h)
target_include_directories(wifi_transport_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(wifi_transport_test PRIVATE clocktransport Qt6::Network)
add_test(NAME wifi_transport COMMAND wifi_transport_test)
add_executable(firmware_api_test EXCLUDE_FROM_ALL tests/firmware_api_test.cpp ../../firmware/src/settings.cpp)
target_sources(firmware_api_test PRIVATE ../../firmware/src/art.cpp)
target_include_directories(firmware_api_test PRIVATE ../../firmware/test/host ../../firmware/src)
add_test(NAME firmware_api_validation COMMAND firmware_api_test)
qt_add_executable(wifi_ui_test EXCLUDE_FROM_ALL tests/wifi_ui_test.cpp mainwindow.cpp mainwindow.h usbclock.cpp usbclock.h
    wificlock.cpp wificlock.h ledmatrixwidget.cpp ledmatrixwidget.h pcrenderer.cpp ../../firmware/src/geometry.cpp)
target_include_directories(wifi_ui_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ../../firmware/src)
target_link_libraries(wifi_ui_test PRIVATE clocktransport Qt6::Widgets Qt6::Network)
add_dependencies(wifi_ui_test clockengine)
add_test(NAME wifi_ui_and_usb_switch COMMAND wifi_ui_test)
set_tests_properties(wifi_ui_and_usb_switch PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
add_executable(network_mode_test EXCLUDE_FROM_ALL tests/network_mode_test.cpp ../../firmware/src/net.cpp)
target_include_directories(network_mode_test PRIVATE ../../firmware/test/host ../../firmware/src)
add_test(NAME direct_ap_and_router_modes COMMAND network_mode_test)
find_program(NODE_EXECUTABLE node)
if(NODE_EXECUTABLE)
    add_test(NAME phone_page_save_and_time COMMAND "${NODE_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/phone_page_test.js")
endif()
