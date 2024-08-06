#pragma once
/**
 * @file    shmpool.hpp
 * @brief   The API of an allocator for shared memory implementation.
 *
 * This file contains an allocator to manage the shared memory. Noam Benson-Tilsen(nb498@cornell.edu) helped with
 * prototyping and performance evaluation of the Jemalloc wrapper part. 
 *
 * This library allows multiple processors to access the data allocated from the shared memory pool. The data can be
 * either pod or complex object, as long as the data is allocated from the same shared memory pool. A shared memory
 * pool has ONE owner process, which takes care of allocation/deallocation of the memory space, and MANY loaner
 * processes, which can only access the data that is allocated by the owner process. For performance reason.
 *
 * To support multiple applications on a same server, we introduced a top level concept called 'group'. Each shared
 * memory pool belongs to one the group. A process have to specify its group, identifed by a string, on starting. Only
 * the processes, no matter owners or a loaners, in the same group can communicate using the shared memory pools.
 * Please note that we allows multiple shared memory pools with different owner processes in a group.
 *
 * Each shared memory pool is identified by an integer ID, which is the offset in the full shared memory pool range.
 */

#include <sys/ipc.h>
#include <sys/shm.h>
#include <jemalloc/jemalloc.h>
#include <chrono>
#include <memory>
#include <atomic>

#include <wsong/common.h>
#include <wsong/exceptions.hpp>

namespace wsong {
namespace ipc {

/**
 *  @brief  The start of the virtual address space for shared memory pools.
 */
#define WS_SHM_POOL_VA_START    (0x200000000000LLU)
/**
 * @brief   The end of the virtual address space for shared memory pools.
 */
#define WS_SHM_POOL_VA_END      (0x2fffffffffffLLU)
/**
 * @brief   The virtual address space size for shared memory pools
 */
#define WS_SHM_POOL_VA_SIZE     (0x100000000000LLU)
/**
 * @brief   The memory chunk size, which is the minimum managable memory unit, for shared memory pools.
 */
#define WS_SHM_POOL_CHUNK_SIZE  (0x200000)

/**
 * @class   ShmPool shmpool.hpp "wsong/ipc/shmpool.hpp"
 * @brief   This class defines the API of a shared memory pool.
 *
 * This class is used by both the owner and loaner of a shared memory pool. The owner use this class to:
 * - create a shared memory pool;
 * - allocate memory from the pool;
 * - free memory back to the pool;
 * - clean/destroy a memory pool.
 * The loaner just use this class's static API to clear the memory mapping in a shared memory pool to avoid lingering
 * mappings that prevents the removing of the shared memory blocks.
 *
 * Important: it is the APPLICATION's RESPONSIBILITY to make sure all loanders of a shared memory pool cleaning its
 * memory mappings before the owner destroys it. Otherwise, either the destruction will fail or unknown behaviour will
 * occur.
 */
class ShmPool {
public:
    /**
     * @fn ShmPool::ShmPool(uint64_t capacity);
     * @brief   An owner process call this api to create a shared memory 
     */
    WS_DLL_PRIVATE ShmPool();
    /**
     * @fn ShmPool::~ShmPool();
     * @brief   Destructor
     */
    WS_DLL_PUBLIC virtual ~ShmPool();
    /**
     * @fn void* ShmPool::malloc(size_t size)
     * @brief   Memory allocation
     * @param[in]   size    The requested size of the memory
     * @return      The pointer to the allocated memory, or nullptr on failure.
     */
    WS_DLL_PUBLIC virtual void* malloc(size_t size) = 0;
    /**
     * @fn  ShmPool::free(void* ptr)
     * @brief   Free memory allocated from the same shared memory pool.
     * @param[in]   ptr     The pointer to the memory to be freed.
     * @throws  Exception on failure.
     */
    WS_DLL_PUBLIC virtual void free(void* ptr) = 0;
    /**
     * @fn void ShmPool::initialize(const std::string& group);
     * @brief   Initialize the shared memory pool system.
     * @param[in]   group   The name of the group.
     * @throws  Exception on failure
     */
    WS_DLL_PUBLIC static void initialize(const std::string& group);
    /**
     * @fn void ShmPool::uninitialize();
     * @brief   Ininitialize the shared memory pool system.
     * @throws  Exception on failure.
     */
    WS_DLL_PUBLIC static void uninitialize();
    /**
     * @fn ShmPool* create(const uint64_t)
     * @brief   Create a shared memory pool
     * @param[in]   capacity    The capacity of the shared memory pool
     * @return  The pointer to the shared memory pool, the caller is responsible for
     *          destroy it.
     * @throws      Exception on failure.
     */
    WS_DLL_PUBLIC static ShmPool* create(const uint64_t capacity);
    /**
     * @fn void unmap(const uint64_t addr, const uint64_t size)
     * @brief   Unmap the corresponding address range. Please note that all overlapping chunks
     *          will be removed.
     * @param[in]   vaddr       The starting virtual address of the memory range.
     * @param[in]   size        The size of the range to be unmapped.
     * @throws      Exception on failure.
     */
    WS_DLL_PUBLIC static void unmap(const uint64_t vaddr, const uint64_t size);
};

}
}
