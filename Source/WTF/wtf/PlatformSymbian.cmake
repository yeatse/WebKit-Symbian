list(APPEND WTF_SOURCES
    generic/MainThreadGeneric.cpp
    generic/MemoryFootprintGeneric.cpp
    generic/RunLoopGeneric.cpp
    generic/WorkQueueGeneric.cpp

    posix/FileHandlePOSIX.cpp
    posix/FileSystemPOSIX.cpp
    posix/MappedFileDataPOSIX.cpp
    posix/ThreadingPOSIX.cpp

    symbian/CPUTimeSymbian.cpp
    symbian/FileSystemSymbian.cpp
    symbian/FileSystemSymbianNative.cpp
    symbian/MainThreadProfile.cpp
    symbian/LibcxxSupport.cpp
    symbian/MemoryPressureHandlerSymbian.cpp
    symbian/MemoryPressureMonitorSymbianNative.cpp
    symbian/OSAllocatorSymbian.cpp
    symbian/SymbianSystem.cpp

    text/unix/TextBreakIteratorInternalICUUnix.cpp

    unix/LanguageUnix.cpp
    unix/LoggingUnix.cpp
    unix/UniStdExtrasUnix.cpp
)

list(APPEND WTF_PUBLIC_HEADERS
    symbian/SymbianSystem.h
    symbian/MainThreadProfile.h
    symbian/MemoryPressureHandlerSymbian.h
)

list(APPEND WTF_LIBRARIES
    Threads::Threads
    "${SYMBIAN_SDK_PREFIX}/lib/armv6/mbedtls/lib/libmbedcrypto.a"
)
