add_library(memory3 STATIC
    FillingMemoryMgr.cpp
    PinnedMemoryMgr.cpp
    RawMemoryBlock.cpp
    TypedMemoryBlock.cpp
    MemoryBlockItf.cpp
    PrimordialMemoryMgr.cpp
    SharedMemoryMgr.cpp
    UniqueRawMemoryBlock.cpp
    MemoryManagerItf.cpp
    QuotaMemoryMgr.cpp
    TrackingMemoryManagerItf.cpp
)

add_subdirectory(test)