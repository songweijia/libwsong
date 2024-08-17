#include <wsong/ipc/shmpool.hpp>
#include <mutex>
#include <filesystem>

#include "shmpool_internals.hpp"
#include "buddy_system.hpp"

namespace wsong {
namespace ipc {

namespace fs = std::filesystem;

ShmPool::ShmPool() {}
ShmPool::~ShmPool() {}

/**
 * @class   ShmPoolImpl
 * @brief   The class implements the API defined in ShmPool
 */
class ShmPoolImpl : public ShmPool {
private:
    static std::mutex   init_lock;
    static std::string  group;
    /**
     * @brief The offset of pool inside the virtual address space for shared memory pools.
     */
    uint64_t offset;
    /**
     * @brief The capacity of this shared memory pool.
     */
    uint64_t capacity;
public:
    ShmPoolImpl(const uint64_t cap): ShmPool(), capacity(cap) {
        offset = BuddySystem::get()->allocate(cap);
    }

    virtual ~ShmPoolImpl() {
        BuddySystem::get()->free(offset,capacity);
    }

    WS_DLL_PRIVATE static void create_group(const std::string& group) {
        std::error_code errcode;
        // 1. Test if folder exists already, throw exception if it does.
        fs::path group_dir(get_shm_pool_group_dir(group));
        if (fs::exists(group_dir,errcode)) {
            throw ws_invalid_argument_exp(
                    std::string("Group:") + group + "'s metadata folder(" + 
                    get_shm_pool_group_dir(group) + ") has existed already." +
                    "If this is a leftover from a previously crashed application, " +
                    "you can try delete it manually and restart.");
        } else if (errcode) {
            throw ws_invalid_argument_exp(
                    std::string("Unable to check Group:") + group + "'s metadata folder(" +
                    get_shm_pool_group_dir(group) + "). Error:" + errcode.message());
        }
        // 2. Create folders and initialize the data structures
        if (!fs::create_directory(group_dir,errcode)) {
            throw ws_invalid_argument_exp(
                    std::string("Failed to create folder:") + group + "'s metadata folder(" +
                    get_shm_pool_group_dir(group) + "). Error:" + errcode.message());
        }
        // 3. Create the buddy system.
        BuddySystem::create_buddy_system(group,WS_SHM_POOL_VA_SIZE,WS_MIN_SHM_POOL_SIZE);
        
        // 4. set up group name.
        ShmPoolImpl::group = group;
    }

    WS_DLL_PRIVATE static void initialize(const std::string& group) {
        std::lock_guard<std::mutex> lock(init_lock);
        if (ShmPoolImpl::group.empty()) {
            ShmPoolImpl::group = group;
        } else {
            throw ws_reinitialization_exp("ShmPoolImpl has been initialized already.");
        }
    }
    
    WS_DLL_PRIVATE static void remove_group(const std::string& group) {
        if (!group.empty()) {
            BuddySystem::remove_buddy_system(group);
            fs::remove_all(fs::path(get_shm_pool_group_dir(group)));
        }
    }
};

std::mutex   ShmPoolImpl::init_lock;
std::string  ShmPoolImpl::group;

void ShmPool::create_group(const std::string& group) {
    ShmPoolImpl::create_group(group);
}

void ShmPool::remove_group(const std::string& group) {
    ShmPoolImpl::remove_group(group);
}

void ShmPool::initialize(const std::string& group) {
    ShmPoolImpl::initialize(group);
}

ShmPool* ShmPool::create(const uint64_t capacity) {
    // TODO:
    return nullptr;
}

} // namespace ipc
} // namespace wsong
