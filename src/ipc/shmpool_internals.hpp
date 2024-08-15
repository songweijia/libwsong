#pragma once

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
 * @brief Get the meta data folder of a shared memory group
 */
#define WS_SHM_POOL_GROUP_META(group) \
    WS_SHM_POOL_META_HOME \
    WS_SHM_POOL_META_PREFIX \
    xstr(group)
/**
 * @brief Get the buddy system file path
 */
#define WS_SHM_POOL_GROUP_BUDDIES(group) \
    WS_SHM_POOL_GROUP_META(group) "/buddies"
/**
 * @brief The minimum capacity of a shared memory pool. - 4GB
 */
#define WS_SHM_POOL_MIN_CAPACITY (0x100000000LLU)
