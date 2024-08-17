#include "buddy_system.hpp"
#include <wsong/ipc/shmpool.hpp>
#include <wsong/exceptions.hpp>
#include <sys/stat.h>
#include <sys/file.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <cinttypes>
#include <vector>
#include <utility>
#include <fstream>
#include <filesystem>

#if __has_include(<format>)
#include <format>
#endif

#include "shmpool_internals.hpp"

#define IS_POWER_OF_TWO(x)  ((x&((x)-1) == 0) && (x!=0))

namespace wsong {
namespace ipc {

std::unique_ptr<BuddySystem> BuddySystem::singleton;

BuddySystem::BuddySystem(
        const std::string& group,
        const uint64_t va_cap,
        const uint64_t min_pool_cap):
    group_name(group),va_capacity(va_cap),min_pool_capacity(min_pool_cap),
    fd(-1),buddies_ptr(reinterpret_cast<uint8_t*>(MAP_FAILED)) {
    // Test arguments
    if (group_name.size() == 0) {
        throw ws_invalid_argument_exp("BuddySystem constructor got empty group name.");
    }
    if (!IS_POWER_OF_TWO(va_cap)) {
        throw ws_invalid_argument_exp(
#if __has_include(<format>)
                std::format("BuddySystem constructor got invalid virtual address cap:{:x}", va_cap)
#else
                std::string("BuddySystem constructor got invalid virtual address cap:") + 
                    std::to_string(va_cap)
#endif
                );
    }
    if (!IS_POWER_OF_TWO(min_pool_cap)) {
        throw ws_invalid_argument_exp(
#if __has_include(<format>)
                std::format("BuddySystem constructor got invalid minimal pool capacity:{:x}", min_pool_cap)
#else
                std::string("BuddySystem constructor got invalid minimal pool capacity:") +
                    std::to_string(min_pool_cap)
#endif
                );
    }
    if (min_pool_cap > va_cap) {
        throw ws_invalid_argument_exp(
#if __has_include(<format>)
                std::format("BuddySystem constructor: min_pool_cap:{:x} is greater than va_cap:{:x}.",
                    min_pool_cap,va_cap)
#else
                std::string("BuddySystem constructor: min_pool_cap:)" + 
                    std::to_string(min_pool_cap) + " is greater than va_cap: " +
                    std::to_string(va_cap) + ".")
#endif
                );
    }

    // open the file
    fd = open(WS_SHM_POOL_GROUP_BUDDIES(this->group_name),O_RDWR);
    if (fd == -1) {
        throw ws_system_error_exp(
#if __has_include(<format>)
                std::format("Failed to open the buddy system file:{}, error:{}.",
                    WS_SHM_POOL_GROUP_BUDDIES(this->group_name),strerror(errno)
#else
                std::string("Failed to open the buddy system file:") +
                    WS_SHM_POOL_GROUP_BUDDIES(this->group_name) + 
                    ", error:" + strerror(errno)
#endif
                );
    }
    struct stat sbuf;
    if (fstat(fd,&sbuf) == -1) {
        close(fd);
        fd = -1;
        throw ws_system_error_exp("Failed to fstat buddy system file.");
    }

    if (static_cast<off_t>((va_cap/min_pool_cap)<<1) != sbuf.st_size) {
        close(fd);
        fd = -1;
        throw ws_invalid_argument_exp(
#if __has_include(<format>)
                std::format("Buddy system has invalid size:{:x}, expecting {:x}.",
                    sbuf.st_size,va_cap/min_pool_cap)
#else
                std::string("Buddy system has invalid size:") + std::to_string(sbuf.st_size) +
                ", expecting " + std::to_string(va_cap/min_pool_cap)
#endif
                );
    }
    // mmap it.
    buddies_ptr = reinterpret_cast<uint8_t*>(mmap(nullptr,sbuf.st_size,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0));
    if (buddies_ptr == MAP_FAILED) {
        close(fd);
        fd = -1;
        throw ws_invalid_argument_exp(
#if __has_include(<format>)
                std::format("Failed to map the buddy system file, error:{}.", strerror(errno))
#else
                std::string("Failed to map the buddy sytem file, error:") + strerror(errno)
#endif
                );
    }
}

BuddySystem::~BuddySystem() {
    if (buddies_ptr != MAP_FAILED) {
        munmap(reinterpret_cast<void*>(buddies_ptr),va_capacity/min_pool_capacity);
    }
    if (fd != -1) {
        close (fd);
    }
}

WS_DLL_PRIVATE uint32_t internal_allocate_buddy(uint8_t* buddies_ptr, uint32_t level, uint32_t cur = 1);

/**
 * @fn uint32_t allocate_buddy(uint8_t* buddies_ptr, uint32_t total_level, uint32_t level);
 * @brief Allocate a node from the buddy system
 * The buddy system is managed in a complete binary tree as follows:
 *    level 1               1       -+
 *                         / \       |
 *    level 2             2   3      +-> total level = 3
 *                       / \ / \     |
 *    level 3           4  5 6  7   -+
 * The buddy system is organized in a byte array:
 * buddies_ptr->[<1>,<2>,<3>,...,<7>], <n> has three possibilities
 * 0 - the node is free and not splited at all.
 * 1 - the node is splited, but not there are some free space.
 * 2 - the node is full.
 * @param[in]   buddies_ptr     Pointer to the buddy system binary tree
 * @param[in]   total_level     The number of total levels in the binary tree
 * @param[in]   level           The requested node at the corresponding level, in [1, total_level]
 * @return      the node id in [1,2^total_level) on success, or 0 if allocation fails.
 * @throw       Exception on invalid arguemnt
 */
WS_DLL_PRIVATE uint32_t allocate_buddy(uint8_t* buddies_ptr, uint32_t total_level, uint32_t level) {
    if (level < 1 || level > total_level) {
        throw ws_invalid_argument_exp(
#if __has_include(<format>)
                std::format("Requested level:{}, is out of range [1,{}].", level, total_level)
#else
                std::string("Requested level:") + std::to_string(level) + 
                ", is out of range [1," + std::to_string(total_level) + "]."
#endif
                );
    }
    return internal_allocate_buddy(buddies_ptr,level,1);
}

#define BUDDY_STATE_IDLE    (0)
#define BUDDY_STATE_SPLT    (1)
#define BUDDY_STATE_FULL    (2)
#define LEVEL_OF(nid)   (32 - __builtin_clz(nid))

uint32_t internal_allocate_buddy(uint8_t* buddies_ptr, uint32_t level, uint32_t cur) {
    uint32_t cur_level = LEVEL_OF(cur);
    if (cur_level > level) {
        throw ws_invalid_argument_exp(
#if __has_include(<format>)
                std::format("Requested level:{} is higher than the level of the current node:{}.",level,cur)
#else
                std::string("Requested level:") + std::to_string(level) +
                " is higher than the level of the current node:" + std::to_string(cur)
#endif
                );
    } else if (cur_level == level) {
        if (buddies_ptr[cur] == BUDDY_STATE_IDLE) {
            // found a buddy, set it to full and return
            buddies_ptr[cur] = BUDDY_STATE_FULL;
            return cur;
        } else {
            // not found - running OOM
            return 0;
        }
    } else {
        // going down
        switch(buddies_ptr[cur]) {
        case BUDDY_STATE_IDLE:
            // split
            buddies_ptr[cur] = BUDDY_STATE_SPLT;
            return internal_allocate_buddy(buddies_ptr,level,cur<<1);
        case BUDDY_STATE_SPLT:
            // try left
            {
                uint32_t l = cur<<1;
                uint32_t r = l + 1;
                uint32_t ret = internal_allocate_buddy(buddies_ptr,level,l);
                if (ret == 0) {
                    ret = internal_allocate_buddy(buddies_ptr,level,r);
                }
                if (ret > 0 && buddies_ptr[l] == BUDDY_STATE_FULL && buddies_ptr[r] == BUDDY_STATE_FULL) {
                    buddies_ptr[cur] = BUDDY_STATE_FULL;
                }
                return ret;
            }
        case BUDDY_STATE_FULL:
        default:
            // not found - running OOM
            return 0;
        }
    }
}

/**
 * @fn void free_buddy(uint8_t* buddies_ptr, uint32_t total_level, uint32_t node_number)
 * @brief free a buddy represented by the node number in the buddy system binary tree.
 * @param[in]   buddies_ptr     Pointer to the buddy system binary tree.
 * @param[in]   total_level     The number of total levels in the binary tree.
 * @param[in]   node_number     The node number to free.
 * @throw       Exception on invalid arguemnt
 */
WS_DLL_PRIVATE void free_buddy(uint8_t* buddies_ptr, uint32_t total_level, uint32_t node_number) {
    if (node_number >= (1U<<total_level)) {
        throw ws_invalid_argument_exp(
#if __has_include(<format>)
                std::format("Node to free:{} is out of range. Expected range [1,{}).",
                    node_number,1U<<total_level)
#else
                std::string("Node to free:") + std::to_string(node_number) +
                "is out of range. Expected range [1," + std::to_string(total_level) + ")."
#endif
                );
    }
    if (buddies_ptr[node_number] != BUDDY_STATE_FULL) {
        throw ws_invalid_argument_exp(
#if __has_include(<format>)
                std::format("The state of node to free:{} is in STATE({}). Expecting STATE({}).",
                    node_number, buddies_ptr[node_number],BUDDY_STATE_FULL)
#else
                std::string("The state of node to free:") + std::to_string(node_number) +
                "is in STATE(" + std::to_string(buddies_ptr[node_number]) +
                "). Expecting STATE(" + std::to_string(BUDDY_STATE_FULL) + ")."
#endif
                );
    }
    if (LEVEL_OF(node_number) < total_level &&
        buddies_ptr[node_number<<1] != BUDDY_STATE_IDLE && buddies_ptr[(node_number<<1) + 1]) {
        throw ws_invalid_argument_exp(
#if __has_include(<format>)
                std::format("Node to free:{} is in Full state but not allocated as a whole.",node_number)
#else
                std::string("Node to free:") + std::to_string(node_number) +
                " is in Full state but not allocated as a whole"
#endif
                );
    }
    
    buddies_ptr[node_number] = BUDDY_STATE_IDLE;
    int parent = (node_number>>1);
    while (parent > 0) {
        if (buddies_ptr[parent<<1] == BUDDY_STATE_IDLE &&
            buddies_ptr[(parent<<1) + 1] == BUDDY_STATE_IDLE) {
            // turn to idle
            buddies_ptr[parent] = BUDDY_STATE_IDLE;
        } else if (buddies_ptr[parent] == BUDDY_STATE_FULL) {
            // turn to splt
            buddies_ptr[parent] = BUDDY_STATE_SPLT;
        } else {
            // stay splt, no need to continue.
            break;
        }
        parent = parent >> 1;
    }
}

uint64_t BuddySystem::allocate(
        const uint64_t pool_size) {
    uint64_t vaddr = 0ULL;
    // STEP 0: validate pool_size
    if (pool_size < this->min_pool_capacity ||
        pool_size > this->va_capacity ||
        (pool_size & (pool_size-1))) {
        throw ws_invalid_argument_exp(
#if __has_include(<format>)
                std::format("Invalid requested pool size:{:x}, expecting a power of two value in [{:x},{:x}]",
                    pool_size,this->min_pool_capacity,this->va_capacity)
#else
                std::string("Invalid requested pool size:") + std::to_string(pool_size) +
                ", expecting a power of two value in range [" + std::to_string(min_pool_capacity) + "," +
                std::to_string(va_capacity) + "]"
#endif
                );
    }

    // STEP 1: applying pthread lock.
    std::lock_guard<std::mutex> lock(buddies_mutex);
    
    // STEP 2: lock the buddy file
    if (flock(fd,LOCK_EX) == -1) {
        throw ws_system_error_exp(
#if __has_include(<format>)
                std::format("Failed to apply file lock on buddy system file:{}, error: {}.",
                    WS_SHM_POOL_GROUP_BUDDIES(group_name),strerror(errno))
#else
                std::string("Failed to apply file lock on buddy system file:") + 
                WS_SHM_POOL_GROUP_BUDDIES(group_name) + ", error:" + strerror(errno)
#endif
                );
    }

    // STEP 3: allocate the space
    const uint32_t total_level      = __builtin_ctzl(va_capacity/min_pool_capacity) + 1;
    const uint32_t requested_level  = __builtin_ctzl(this->va_capacity/pool_size) + 1;
    try {
        uint32_t node = allocate_buddy(this->buddies_ptr,total_level,requested_level);
        vaddr = (node - (1U<<(requested_level-1)))*(this->va_capacity>>(requested_level-1));
    } catch (ws_exp& ex) {
        flock(fd,LOCK_UN);
        throw ex;
    }

    // STEP 4: unlock the buddy file
    if (flock(fd,LOCK_UN) == -1) {
        throw ws_system_error_exp(
#if __has_include(<format>)
                std::format("Failed to unlock buddy system file:{}, error: {}.",
                    WS_SHM_POOL_GROUP_BUDDIES(group_name),strerror(errno))
#else
                std::string("Failed to unlock buddy system file:") + 
                WS_SHM_POOL_GROUP_BUDDIES(group_name) + ", error:" + strerror(errno)
#endif
                );
    }

    return vaddr;
}

void BuddySystem::free(const uint64_t pool_offset, const uint64_t pool_size) {
    // STEP 0: validate arguments
    if (pool_size < this->min_pool_capacity ||
        pool_size > this->va_capacity ||
        (pool_size & (pool_size-1))) {
        throw ws_invalid_argument_exp(
#if __has_include(<format>)
                std::format("Invalid requested pool size:{:x}, expecting a power of two value in [{:x},{:x}]",
                    pool_size,this->min_pool_capacity,this->va_capacity)
#else
                std::string("Invalid requested pool size:") + std::to_string(pool_size) +
                ", expecting a power of two value in range [" + std::to_string(min_pool_capacity) + "," +
                std::to_string(va_capacity) + "]"
#endif
                );
    }
    
    if (pool_offset % pool_size) {
        throw ws_invalid_argument_exp(
#if __has_include(<format>)
                std::format("Requested pool_offset:{:x} does not align with pool_size:{:x}.",
                    pool_offset,pool_size)
#else
                std::string("Requested pool_offset:") + std::to_string(pool_offset) +
                " does not align with pool_size:" + std::to_string(pool_size)
#endif
                );
    }

    // STEP 1: applying pthread lock.
    std::lock_guard<std::mutex> lock(buddies_mutex);
    
    // STEP 2: lock the buddy file
    if (flock(fd,LOCK_EX) == -1) {
        throw ws_system_error_exp(
#if __has_include(<format>)
                std::format("Failed to apply file lock on buddy system file:{}, error: {}.",
                    WS_SHM_POOL_GROUP_BUDDIES(group_name),strerror(errno))
#else
                std::string("Failed to apply file lock on buddy system file:") + 
                WS_SHM_POOL_GROUP_BUDDIES(group_name) + ", error:" + strerror(errno)
#endif
                );
    }

    // STEP 3: free the space
    const uint32_t total_level  = __builtin_ctzl(va_capacity/min_pool_capacity) + 1;
    const uint32_t node_number  = (va_capacity + pool_offset) / pool_size;
    try {
        free_buddy(this->buddies_ptr,total_level,node_number);
    } catch (ws_exp& ex) {
        flock(fd,LOCK_UN);
        throw ex;
    }

    // STEP 4: unlock the buddy file
    if (flock(fd,LOCK_UN) == -1) {
        throw ws_system_error_exp(
#if __has_include(<format>)
                std::format("Failed to unlock buddy system file:{}, error: {}.",
                    WS_SHM_POOL_GROUP_BUDDIES(group_name),strerror(errno))
#else
                std::string("Failed to unlock buddy system file:") + 
                WS_SHM_POOL_GROUP_BUDDIES(group_name) + ", error:" + strerror(errno)
#endif
                );
    }
}

std::pair<uint64_t,uint64_t> BuddySystem::query(const uint64_t va_offset) {
    // STEP 0: get the virtual node number
    uint32_t vnode = (va_capacity + va_offset) / min_pool_capacity;

    // STEP 1: applying pthread lock.
    std::lock_guard<std::mutex> lock(buddies_mutex);
    
    // STEP 2: read-lock the buddy file
    if (flock(fd,LOCK_SH) == -1) {
        throw ws_system_error_exp(
#if __has_include(<format>)
                std::format("Failed to apply shared file lock on buddy system file:{}, error: {}.",
                    WS_SHM_POOL_GROUP_BUDDIES(group_name),strerror(errno))
#else
                std::string("Failed to apply shared file lock on buddy system file:") + 
                WS_SHM_POOL_GROUP_BUDDIES(group_name) + ", error:" + strerror(errno)
#endif
                );
    }

    // STEP 3: find the pool
    while (this->buddies_ptr[vnode]!=BUDDY_STATE_FULL) {
        vnode = (vnode>>1);
    }

    // STEP 4: unlock the buddy file
    if (flock(fd,LOCK_UN) == -1) {
        throw ws_system_error_exp(
#if __has_include(<format>)
                std::format("Failed to unlock buddy system file:{}, error: {}.",
                    WS_SHM_POOL_GROUP_BUDDIES(group_name),strerror(errno))
#else
                std::string("Failed to unlock buddy system file:") + 
                WS_SHM_POOL_GROUP_BUDDIES(group_name) + ", error:" + strerror(errno)
#endif
                );
    }

    // STEP 5: calculate the pool size and offset
    uint64_t pool_size      = va_capacity/(1U<<(LEVEL_OF(vnode) - 1));
    uint64_t pool_offset    = vnode * pool_size - va_capacity;

    return {pool_offset,pool_size};
}

void BuddySystem::initialize_buddy_system(const std::string& group,
        const uint64_t va_cap, const uint64_t min_pool_cap) {
    singleton = std::make_unique<BuddySystem>(group,va_cap,min_pool_cap);
}

void BuddySystem::create_buddy_system(const std::string& group,
        const uint64_t va_cap, const uint64_t min_pool_cap) {
    // STEP 1: Test arguments
    if (group.size() == 0) {
        throw ws_invalid_argument_exp("BuddySystem constructor got empty group name.");
    }
    if (!IS_POWER_OF_TWO(va_cap)) {
        throw ws_invalid_argument_exp(
#if __has_include(<format>)
                std::format("BuddySystem constructor got invalid virtual address cap:{:x}", va_cap)
#else
                std::string("BuddySystem constructor got invalid virtual address cap:") + 
                    std::to_string(va_cap)
#endif
                );
    }
    if (!IS_POWER_OF_TWO(min_pool_cap)) {
        throw ws_invalid_argument_exp(
#if __has_include(<format>)
                std::format("BuddySystem constructor got invalid minimal pool capacity:{:x}", min_pool_cap)
#else
                std::string("BuddySystem constructor got invalid minimal pool capacity:") +
                    std::to_string(min_pool_cap)
#endif
                );
    }
    if (min_pool_cap > va_cap) {
        throw ws_invalid_argument_exp(
#if __has_include(<format>)
                std::format("BuddySystem constructor: min_pool_cap:{:x} is greater than va_cap:{:x}.",
                    min_pool_cap,va_cap)
#else
                std::string("BuddySystem constructor: min_pool_cap:)" + 
                    std::to_string(min_pool_cap) + " is greater than va_cap: " +
                    std::to_string(va_cap) + ".")
#endif
                );
    }

    // STEP 2: create file.
    std::ofstream bfile(WS_SHM_POOL_GROUP_BUDDIES(group));
    for (uint32_t i=0;i<((va_cap/min_pool_cap)<<1);i++)
        bfile.put(BUDDY_STATE_IDLE);
    bfile.close();
}

void BuddySystem::remove_buddy_system() {
    if (singleton) {
        std::string group = singleton->group_name;
        singleton.reset();
        std::filesystem::remove(std::filesystem::path(WS_SHM_POOL_GROUP_BUDDIES(group)));
    }
}

}
}
