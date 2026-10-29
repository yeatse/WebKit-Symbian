# CMake's FindICU creates independent imported targets. With static archives,
# libicuuc needs the data archive after it; the shared-library order is not
# sufficient. Apply these dependencies after WebKit has found the ICU targets.
include_guard(GLOBAL)

function(arm11_static_icu_dependencies)
    foreach(component i18n uc data)
        if(NOT TARGET ICU::${component})
            message(FATAL_ERROR "ARM11 QEMU requires the static ICU ${component} target")
        endif()
    endforeach()
    set_property(TARGET ICU::i18n APPEND PROPERTY INTERFACE_LINK_LIBRARIES ICU::uc)
    set_property(TARGET ICU::uc APPEND PROPERTY INTERFACE_LINK_LIBRARIES ICU::data)
endfunction()

cmake_language(DEFER CALL arm11_static_icu_dependencies)
