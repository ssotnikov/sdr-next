qt_add_executable(sdr_app main.cpp controller.cpp controller.hpp spectrum_item.cpp spectrum_item.hpp engine.cpp engine.hpp)
set_target_properties(sdr_app PROPERTIES WIN32_EXECUTABLE TRUE)
if(WIN32)
  enable_language(RC)
  configure_file(app.rc.in "${CMAKE_CURRENT_BINARY_DIR}/sdr_app.rc" @ONLY)
  set_source_files_properties("${CMAKE_CURRENT_BINARY_DIR}/sdr_app.rc" PROPERTIES
    OBJECT_DEPENDS "${PROJECT_SOURCE_DIR}/assets/icon/sdrnext.ico")
  target_sources(sdr_app PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/sdr_app.rc")
endif()

set(sdr_icon_files)
foreach(size IN ITEMS 16 20 24 32 40 48 64 96 128 256 512)
  list(APPEND sdr_icon_files "${PROJECT_SOURCE_DIR}/assets/icon/png/sdrnext-${size}.png")
endforeach()
qt_add_resources(sdr_app application_icons
  PREFIX "/icons"
  BASE "${PROJECT_SOURCE_DIR}/assets/icon/png"
  FILES ${sdr_icon_files})
target_link_libraries(sdr_app PRIVATE sdr_core Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2 Qt6::Multimedia)
qt_add_qml_module(sdr_app URI SDR VERSION 1.0 QML_FILES Main.qml)
if(MSVC)
  target_compile_options(sdr_app PRIVATE /utf-8 /W4)
endif()
install(TARGETS sdr_app RUNTIME DESTINATION bin)
