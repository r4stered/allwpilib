project(filterdesigner)

include(CompileWarnings)
include(GenResources)
include(LinkMacOSGUI)

configure_file(src/main/generate/WPILibVersion.cpp.in WPILibVersion.cpp)
generate_resources(src/main/native/resources generated/main/cpp FILTERDESIGNER filterdesigner filterdesigner_resources_src)

file(GLOB filterdesigner_src src/main/native/cpp/*.cpp ${CMAKE_CURRENT_BINARY_DIR}/WPILibVersion.cpp)

if(WIN32)
    set(filterdesigner_rc src/main/native/win/filterdesigner.rc)
elseif(APPLE)
    set(MACOSX_BUNDLE_ICON_FILE filterdesigner.icns)
    set(APP_ICON_MACOSX src/main/native/mac/filterdesigner.icns)
    set_source_files_properties(${APP_ICON_MACOSX} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
endif()

add_executable(
    filterdesigner
    ${filterdesigner_src}
    ${filterdesigner_resources_src}
    ${filterdesigner_rc}
    ${APP_ICON_MACOSX}
)
wpilib_link_macos_gui(filterdesigner)
target_link_libraries(filterdesigner libglass datalog wpiutil)

if(WIN32)
    set_target_properties(filterdesigner PROPERTIES WIN32_EXECUTABLE YES)
elseif(APPLE)
    set_target_properties(filterdesigner PROPERTIES MACOSX_BUNDLE YES OUTPUT_NAME "FilterDesigner")
endif()
