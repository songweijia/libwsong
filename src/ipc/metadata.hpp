#pragma once
/**
 * @file    metadata.hpp
 * @brief   The metadata of shared memory pools.
 *
 * This file contains the API for the metadata operation which falls in two parts:
 * 1)   The per-group buddy system in ramdisk
 * 2)   The per-group mapping from offset to 2MB IPC SHM regions.
 */

#include <wsong/common.h>
#include <wsong/exceptions.hpp>

#include <cinttypes>
#include <memory>
#include <string>
#include <mutex>

#include "buddy_system.hpp"

namespace wsong {
namespace ipc {

/**
 * @brief The metadata home folder in ramdisk
 */
#define WS_SHM_POOL_META_HOME   "/dev/shm/"
/**
 * @brief The prefix of the metadata folder.
 * All metadata under group G will be put in "/dev/shm/group_G/"
 */
#define WS_SHM_POOL_META_PREFIX "group_"
#define xstr(s) str(s)
#define str(s)  #s

/**
 * @fn std::string get_shm_pool_group_folder(const std::string& group);
 * @brief Get the meta data folder of a shared memory group
 * @param[in]   group   The group name
 * @return  The ramdisk directory for the corresponding group.
 */
inline std::string get_shm_pool_group_dir(const std::string& group) {
    return std::string(WS_SHM_POOL_META_HOME) + WS_SHM_POOL_META_PREFIX + group;
}

/**
 * @fn std::string get_shm_pool_group_buddies(const std::string& group);
 * @brief Get the buddies's file of a shared memory group
 * @param[in]   group   The group name
 * @return  The buddies file for the corresponding group.
 */
inline std::string get_shm_pool_group_buddies(const std::string& group) {
    return get_shm_pool_group_dir(group) + "/buddies";
}

/**
 * @brief The minimum capacity of a shared memory pool. - 4GB
 */
#define WS_SHM_POOL_MIN_CAPACITY (0x100000000LLU)
/**
 * @class   VAW  metadata.hpp "metadata.hpp"
 * @brief   The API to manipulate the virtual address window(VAW).
 */
class VAW {
private:
    /**
     * @brief   The group name
     */
    const std::string   group_name;

    /**
     * @brief   The buddy system managing the pools
     */
    std::unique_ptr<BuddySystem>    buddies;

    /**
     * @brief   The file descriptor (ramdisk buddy file)
     */
    int fd;

    /**
     * @brief   The multithreading mutex - for concurrent control among the threads in current process, NOT among
     *          processes.
     */
    mutable std::mutex  buddies_mutex;

    /**
     * @brief   The global singleton.
     */
    static std::unique_ptr<VAW>     singleton;

public:
    /**
     * @fn VAW::VAW(const std::string& group);
     * @brief   The constructor
     * @param[in]   group       Name of the process group
     */
    WS_DLL_PRIVATE VAW(const std::string&  group);

    /**
     * @fn uint64_t VAW::allocate(const size_t pool_size);
     * @brief   Allocate a shared memory pool with size `pool_size`, which must be power of two.
     *
     * @param[in]   pool_size   The size of the memory pool, which must be power of two.
     *
     * @return      The offset of the shared memory pool in the virtual address space.
     * @throws      Exception will be thrown on failure.
     */
    WS_DLL_PRIVATE uint64_t allocate(const size_t pool_size);

    /**
     * @fn void VAW::free(const uint64_t pool_offset);
     * @brief   Free a shared memory pool @ offset `pool_offset`, which will be checked against the allocated pool
     *          in the buddy system. Exception will be thrown if such a pool offset does not match any allocated pool.
     * @param[in]   pool_offset     The offset of the shared memory pool to be freed.
     *
     * @throws      Exception will be thrown on failure.
     */
    WS_DLL_PRIVATE void free(const uint64_t pool_offset);

    /**
     * @fn std::pair<uint64_t,size_t> VAW::query(const uint64_t va_offset);
     * @brief   Query for the shared memory pool contains a corresponding virtual address, in the form of offset
     *          relative to the start virtual address of the VAW.
     *
     * @param[in]   va_offset   The offset of the virtual address.
     *
     * @return      A pair with the first element be the offset of the shared memory pool, and the second be the size
     *              of it.
     * @throws      Exception will be thrown on failure.
     */
    WS_DLL_PRIVATE std::pair<uint64_t, size_t> query(const int64_t va_offset);

    /**
     * @fn VAW::~VAW();
     * @brief   The destructor.
     */
    WS_DLL_PRIVATE virtual ~VAW();

    /**
     * @fn void VAW::initialize(const std::string& group);
     * @brief   Initialize the process as a member of `group`. It will NOT create the global state on ramdisk if not
     *          exist. Use `create` for that. Important: this call is NOT thread safe. It must be called ONLY once.
     * @param[in]   group       The name of the process group.
     *
     * @throws      Exception will be thrown on failure.
     */
    WS_DLL_PRIVATE static void initialize(const std::string& group);

    /**
     * @fn void VAW::uinitialize();
     * @brief   Uninitialize the process as a member of `group`. Again, this call is NOT thread safe.
     * @throws      Exception will be thrown on failure.
     */
    WS_DLL_PRIVATE static void uninitialize();

    /**
     * @fn void VAW::create(std::string& group);
     * @brief   Create the global medadata of a process group.
     *
     * @param[in]   group       The name of the process group.
     *
     * @throws      Exception will be thrown on failure.
     */
    WS_DLL_PRIVATE static void create(const std::string& group);

    /**
     * @fn void VAW::remove(const std::string& group)
     * @brief   Remove the global metadata of a process group
     *
     * @param[in]   group       The name of the process group.
     *
     * @throws      Exception will be thrown on failure.
     */
    WS_DLL_PRIVATE static void remove(const std::string& group);

    /**
     * @fn VAW* get();
     * @brief   Get the VAW singleton.
     * @return  The pointer to the VAW singleton
     * @throw   Exception will be thrown on failure.
     */
    WS_DLL_PRIVATE static VAW* get();
};

}
}
