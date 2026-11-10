cmake_minimum_required(VERSION 3.28)
project(eka_native_media_patches LANGUAGES CXX ASM)
include(SymbianApp)

set(EKA_SOURCE_DIR "" CACHE PATH "Patched EKA2L1 checkout")
if(NOT EXISTS "${EKA_SOURCE_DIR}/src/patch/priv/inc/sv.s")
    message(FATAL_ERROR "Set EKA_SOURCE_DIR to the patched EKA2L1 checkout")
endif()
set(patch_root "${EKA_SOURCE_DIR}/src/patch")
set(priv "${patch_root}/priv")
set(priv_cpp "${priv}/src/AudCommon.cpp" "${priv}/src/HwrmBase.cpp"
    "${priv}/src/HwrmVibra.cpp" "${priv}/src/HwrmLight.cpp" "${priv}/src/Log.cpp")
file(MAKE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/generated")
file(READ "${SYMBIAN_SDK_PREFIX}/cmake/exe_unwind_image.ld" unwind_layout)
# These linker boundaries serve the private EHABI descriptor, not the frozen
# DLL interface. Do not export zero-size NOTYPE symbols through --export-dynamic.
foreach(boundary __ro_base __ro_end __exidx_start __exidx_end)
    string(REPLACE "${boundary} = .;" "HIDDEN(${boundary} = .);"
        unwind_layout "${unwind_layout}")
endforeach()
file(WRITE "${CMAKE_CURRENT_SOURCE_DIR}/generated/media-unwind.ld" "${unwind_layout}")

function(add_native_media_patch target component uid)
    set(component_root "${patch_root}/${component}")
    set(frozen "${component_root}/eabi/${target}.def")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${component_root}/src/dispatch.s" "${frozen}")
    file(READ "${component_root}/src/dispatch.s" dispatch)
    string(REPLACE ".include \"../../../priv/inc/sv.S\""
        ".include \"${priv}/inc/sv.s\"" dispatch "${dispatch}")
    # Keep the original HLE instructions/IDs; supply ELF function metadata.
    string(REGEX REPLACE "([A-Za-z_][A-Za-z0-9_]*):(\n[ \t]+CallHleDispatch[^\n]+)"
        ".type \\1, %function\n\\1:\\2\n.size \\1, .-\\1" dispatch "${dispatch}")
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/${target}-dispatch.S" "${dispatch}")
    file(READ "${frozen}" definition)
    if(component STREQUAL "mediaclientaudio")
        # Modern SDK declarations use TInt, keeping the original ABI width.
        string(REPLACE "22TMdaPriorityPreference" "i" definition "${definition}")
    else()
        # PriorityL's enum reference remains unchanged. Removing the NewL enum
        # also changes the later Itanium substitution index from SB_ to SA_.
        string(REPLACE "Ei22TMdaPriorityPreference" "Eii" definition "${definition}")
        string(REPLACE
            "_ZN19CVideoPlayerUtility4NewLER27MVideoPlayerUtilityObserveri22TMdaPriorityPreferenceR10RWsSessionR15CWsScreenDeviceR11RWindowBaseRK5TRectSB_"
            "_ZN19CVideoPlayerUtility4NewLER27MVideoPlayerUtilityObserveriiR10RWsSessionR15CWsScreenDeviceR11RWindowBaseRK5TRectSA_"
            definition "${definition}")
    endif()
    # Preserve every frozen ordinal and ABSENT slot from the EKA patch DEF.
    set(generated "${CMAKE_CURRENT_SOURCE_DIR}/generated/${target}.def")
    file(WRITE "${generated}" "${definition}")
    symbian_add_dynamic_library(${target}
        STARTUP "${SYMBIAN_SDK_PREFIX}/cmake/freestanding_dll_startup.S"
        SOURCES ${ARGN} ${priv_cpp} "${CMAKE_CURRENT_BINARY_DIR}/${target}-dispatch.S"
            "${CMAKE_CURRENT_SOURCE_DIR}/native-delete.cpp"
            "${SYMBIAN_SDK_PREFIX}/cmake/exception_descriptor.S"
        LINKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/generated/media-unwind.ld"
        UID3 "${uid}" EXPORT_DEFINITION "${generated}")
    target_link_options(${target} PRIVATE
        "--why-extract=${CMAKE_CURRENT_BINARY_DIR}/${target}-extract.txt")
    target_include_directories(${target} PRIVATE "${component_root}/inc" "${priv}/inc")
    target_compile_definitions(${target} PRIVATE EKA2=1 EKA2L1_MODERN_MMF_HEADERS=1)
    target_compile_options(${target} PRIVATE -fms-extensions
        -Wno-microsoft-extra-qualification -marm
        "$<$<COMPILE_LANGUAGE:CXX>:SHELL:-include ${CMAKE_CURRENT_SOURCE_DIR}/native-patch-compat.h>")
    target_link_libraries(${target} PRIVATE
        Symbian::EUser Symbian::FileServer Symbian::MediaClient)
    if(component STREQUAL "mediaclientvideo")
        target_link_libraries(${target} PRIVATE Symbian::WindowServer Symbian::Gdi)
    endif()
    # Never self-import Symbian::Audio or Symbian::Video to hide missing methods.
    symbian_enable_native_leaves(${target} SOURCES ${ARGN} ${priv_cpp})
endfunction()

set(audio "${patch_root}/mediaclientaudio/src")
add_native_media_patch(mediaclientaudio_general mediaclientaudio 0xee000003
    "${audio}/main.cpp" "${audio}/impl.cpp" "${audio}/clip.cpp"
    "${audio}/tone.cpp" "${audio}/recorder.cpp" "${audio}/player.cpp")
add_native_media_patch(mediaclientvideo_v100 mediaclientvideo 0xee000009
    "${patch_root}/mediaclientvideo/src/VideoPlayerBody.cpp")
