#pragma once
/**
 * @file    buddy_system.hpp
 * @brief   The APIs for allocating system-wide shared memory pool.
 * The APIs defined in this file are process-safe: multiple process can call it concurrently.
 */

#include <wsong/common.h>
#include <wsong/exceptions.hpp>
#include <wsong/ipc/shmpool.hpp>

#include <cinttypes>
#include <string>
#include <memory>
#include <mutex>

namespace wsong {
namespace ipc {

/**
 * @class   BuddySystem buddy_system.hpp    "buddy_system.hpp"
 * @brief The buddy system singleton API
 */
class BuddySystem {
private:
    /**
     * @brief   The group name
     */
    const std::string group_name;
    /**
     * @brief   The virtual address space capacity for this group. Must be power of two.
     */
    const uint64_t va_capacity;
    /**
     * @brief   The minimum capacity of a shared memory pool.
     */
    const uint64_t min_pool_capacity;
    /**
     * @brief   File descriptor
     */
    int fd;
    /**
     * @brief   The buddies system pointer, which pointed to an uint8_t array of length = va_capacity/min_pool_capacity.
     */
    uint8_t* buddies_ptr;
    /**
     * @brief   The multithreading mutex
     */
    mutable std::mutex  buddies_mutex;
    /**
     * @brief   The global singleton.
     */
    static std::unique_ptr<BuddySystem> singleton;

public:
    /**
     * @fn BuddySystem::BuddySystem(const std::string& group, const uint64_t va_cap, const uint64_t min_pool_cap);
     * @brief   The constructor.
     */
    WS_DLL_PRIVATE BuddySystem(
            const std::string& group,
            const uint64_t va_cap,
            const uint64_t min_pool_cap);

    /**
     * @fn uint64_t BuddySystem::allocate(const uint64_t pool_size);
     * @brief   Allocate a shared memory pool with size `pool_size`, which must be power of two.
     * @param[in]   pool_size   The size of the memory pool, which must be power of two.
     * @return      The offset of the shared memory pool in the virtual address space.
     * @throws      Exception will be thrown on failure.
     */
    WS_DLL_PRIVATE uint64_t allocate(
            const uint64_t pool_size);

    /**
     * @fn void BuddySystem::free(const uint64_t pool_offset, const uint64_t pool_size);
     * @brief   Free a shared memory pool @ offset `pool_offset`, which will be checked against the allocated pool
     *          in the buddy system. Exception will be thrown if such a pool offset does not match any allocated pool.
     * @param[in]   pool_offset The offset of the shared memory pool to be freed.
     * @param[in]   pool_size   The size of the memory pool, which must be power of two.
     * @throws      Exception will be thrown on failure.
     */
    WS_DLL_PRIVATE void free(
            const uint64_t pool_offset,
            const uint64_t pool_size);

    /**
     * @fn std::pair<uint64_t,uint64_t> BuddySystem::query(const uint64_t va_offset);
     * @brief   Query for the shared memory pool contains a corresponding virtual address, in the form of offset
     *          relative to the start of the shared virtual memory space.
     * @param[in]   va_offset   The offset of the address
     * @return      A pair with the first element be the offset of the shared memory pool, and the second be the size
     *              of it.
     * @throw       Exception on failure.
     */
    WS_DLL_PRIVATE std::pair<uint64_t,uint64_t> query(const uint64_t va_offset);

    /**
     *  @fn BuddySystem::~BuddySystem();
     *  @brief  The destructor.
     */
    WS_DLL_PRIVATE virtual ~BuddySystem();

    /**
     * @fn void BuddySystem::initialize_buddy_system(const std::string& group, const uint64_t va_cap, const uint64_t min_pool_cap);
     * @brief   Initialize a buddy system. It will NOT create the shared file if not exists.
     * Please note that this method is NOT thread-safe.
     * @param[in]   group           The group the current process belongs to.
     * @param[in]   va_cap          The virtual address space capacity of this buddy system.
     * @param[in]   min_pool_cap    The minimal pool capacity of this buddy system.
     */
    WS_DLL_PRIVATE static void initialize_buddy_system(
        const std::string& group,
        const uint64_t va_cap,
        const uint64_t min_pool_cap);
    /**
     * @fn void BuddySystem::uninitialize_buddy_system();
     * @brief uninitialize the buddysystem if it is initialized.
     * Please note that this method is NOT thread-safe.
     */
    WS_DLL_PRIVATE static void uninitialize_buddy_system();

    /**
     * @fn void BuddySystem::create_buddy_system(const std::string& group, const uint64_t va_cap, const uint64_t min_pool_cap);
     * @brief   create a system wide buddy system. If it has already exists, an exception will be thrown.
     * Please note that this method is NOT thread-safe.
     * @param[in]   group           The group the current process belongs to.
     * @param[in]   va_cap          The virtual address space capacity of this buddy system.
     * @param[in]   min_pool_cap    The minimal pool capacity of this buddy system.
     */
    WS_DLL_PRIVATE static void create_buddy_system(
        const std::string& group,
        const uint64_t va_cap,
        const uint64_t min_pool_cap);

    /**
     * @fn  void BuddySystem::remove_buddy_system();
     * @brief   Destroy and clean the buddy system and remove the system wide state.
     * Important: this function is NOT thread-safe. The application is responsible to make sure no alive threads
     * or processes access the buddy system concurrently.
     * @param[in]   group           The group the current process belongs to.
     */
    WS_DLL_PRIVATE static void remove_buddy_system(const std::string& group);

    /**
     * @fn BuddySystem* get();
     * @brief get the Buddy system singleton.
     * @return the 
     * @throw Exception 
     */
    WS_DLL_PRIVATE static BuddySystem* get();
};

}
}
