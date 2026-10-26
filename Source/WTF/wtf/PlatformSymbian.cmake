list(APPEND WTF_SOURCES
    generic/MainThreadGeneric.cpp
    generic/MemoryFootprintGeneric.cpp
    generic/MemoryPressureHandlerGeneric.cpp
    generic/RunLoopGeneric.cpp
    generic/WorkQueueGeneric.cpp

    posix/FileSystemPOSIX.cpp
    posix/ThreadingPOSIX.cpp

    symbian/CPUTimeSymbian.cpp
    symbian/LibcxxSupport.cpp
    symbian/OSAllocatorSymbian.cpp
    symbian/SymbianSystem.cpp

    text/unix/TextBreakIteratorInternalICUUnix.cpp

    unix/LanguageUnix.cpp
    unix/UniStdExtrasUnix.cpp
)

list(APPEND WTF_PUBLIC_HEADERS
    symbian/SymbianSystem.h
)

list(APPEND WTF_LIBRARIES
    Threads::Threads
)
