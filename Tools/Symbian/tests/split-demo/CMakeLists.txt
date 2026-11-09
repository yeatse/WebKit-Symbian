# A small AVKON application to try split-e32.py on a phone without building
# WebKit: see README.md. One build per variant:
#   -DDEMO_NAME=splitdemoa -DDEMO_UID3=0xE1A5C1A0 -DDEMO_CAPTION="Split A"
cmake_minimum_required(VERSION 3.28)
project(split_demo LANGUAGES C CXX ASM)
include(SymbianApp)

set(DEMO_NAME splitdemo CACHE STRING "Executable name")
set(DEMO_UID3 0xE1A5C1A0 CACHE STRING "UID3 (unprotected range)")
set(DEMO_CAPTION "Split demo" CACHE STRING "Menu caption")

# Link order sets the image layout: demo_first at the start, demo_last at the end.
set(sources demo_first.cpp DemoApp.cpp demo_bulk.cpp demo_last.cpp)
symbian_add_executable(${DEMO_NAME} SOURCES ${sources})
target_compile_features(${DEMO_NAME} PRIVATE cxx_std_20)
target_compile_definitions(${DEMO_NAME} PRIVATE
  DEMO_UID3=${DEMO_UID3} "DEMO_NAME_TEXT=\"${DEMO_NAME}\"" __wchar_t=__symbian_wchar_t)
symbian_enable_native_leaves(${DEMO_NAME} SOURCES ${sources})
target_link_libraries(${DEMO_NAME} PRIVATE SymbianNativeAbi Symbian::EUser)
target_include_directories(${DEMO_NAME} SYSTEM PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../browser/include")
target_compile_options(${DEMO_NAME} PRIVATE -fms-extensions -Wno-microsoft-extra-qualification)
# The browser's system DLLs, so the imports look like the browser's.
foreach (dll avkon cone eikcore eikcoctl eikctl eikdlg apparc apgrfx bafl efsrv fbscli bitgdi gdi ws32 etext form esock insock commondialogs connmon estor hal imageconversion)
  target_link_libraries(${DEMO_NAME} PRIVATE "${SYMBIAN_SDK_PREFIX}/proxies/${dll}/${dll}.dso")
endforeach ()
target_link_libraries(${DEMO_NAME} PRIVATE Symbian::Streams
  "${SYMBIAN_SDK_PREFIX}/proxies/euser/euser.dso"
  "${SYMBIAN_SDK_PREFIX}/proxies/libdl/libdl.dso")
target_link_options(${DEMO_NAME} PRIVATE -znocopyreloc -znotext --icf=all --pic-veneer)
# The SDK replaces the layout once the target is complete; put the demo's back.
function(demo_use_image_layout target)
  get_target_property(script ${target} SYMBIAN_IMAGE_LAYOUT)
  get_target_property(options ${target} LINK_OPTIONS)
  set(layout "${CMAKE_CURRENT_SOURCE_DIR}/demo_image.ld")
  list(TRANSFORM options REPLACE "^${script}$" "${layout}")
  set_target_properties(${target} PROPERTIES LINK_OPTIONS "${options}"
    LINK_DEPENDS "${layout}" SYMBIAN_IMAGE_LAYOUT "${layout}")
endfunction()
cmake_language(DEFER CALL demo_use_image_layout ${DEMO_NAME})
symbian_publish_executable(${DEMO_NAME} UID3 ${DEMO_UID3} CAPABILITIES 0x2000)

foreach (kind reg loc)
  set(rss "${CMAKE_CURRENT_BINARY_DIR}/${DEMO_NAME}_${kind}.rss")
  configure_file(splitdemo_${kind}.rss.in "${rss}" @ONLY)
  set(rpp "${CMAKE_CURRENT_BINARY_DIR}/${DEMO_NAME}_${kind}.rpp")
  set(rsc "${CMAKE_CURRENT_BINARY_DIR}/e32/${DEMO_NAME}_${kind}.rsc")
  add_custom_command(OUTPUT "${rsc}"
    COMMAND "${CMAKE_C_COMPILER}" -E -P -x c
      -I "${SYMBIAN_SDK_PREFIX}/include/platform"
      -I "${SYMBIAN_SDK_PREFIX}/include/native" "${rss}" -o "${rpp}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/e32"
    COMMAND "${SYMBIAN_SDK_PREFIX}/bin/rcomp" -u "-s${rpp}" "-o${rsc}" "-h${CMAKE_CURRENT_BINARY_DIR}/${DEMO_NAME}_${kind}.rsg"
    DEPENDS "${rss}" VERBATIM)
  list(APPEND resources "${rsc}")
endforeach ()
add_custom_target(demo_resources ALL DEPENDS ${resources})
add_dependencies(${DEMO_NAME} demo_resources)
