# Pinned publishedPartner/prototype ScreenPlay interfaces. Actual firmware
# services, mappability and pixel-format support must be checked at runtime.
include_guard(GLOBAL)
foreach(pair IN ITEMS "SurfaceManager:surfacemanager"
    "SurfaceUpdate:surfaceupdateclient")
  string(REPLACE ":" ";" fields "${pair}")
  list(GET fields 0 name)
  list(GET fields 1 dll)
  set(proxy "${SYMBIAN_SDK_PREFIX}/proxies/${dll}/${dll}.dso")
  if(EXISTS "${proxy}" AND NOT TARGET Symbian::${name})
    add_library(SymbianScreenPlay${name} INTERFACE)
    add_library(Symbian::${name} ALIAS SymbianScreenPlay${name})
    target_link_libraries(SymbianScreenPlay${name} INTERFACE
      SymbianNativeAbi Symbian::EUser "${proxy}")
    set_target_properties(SymbianScreenPlay${name} PROPERTIES
      SYMBIAN_SCREENPLAY_API "${name}"
      SYMBIAN_SCREENPLAY_FIRMWARE_COMPATIBILITY "unknown")
  endif()
endforeach()
