#pragma once

#include <string>

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

}
}
