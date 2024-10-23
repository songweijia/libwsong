#include <wsong/ipc/shmpool.hpp>
#include <mutex>
#include <filesystem>
#include <errno.h>
#include <string.h>
#include <jemalloc/jemalloc.h>

#include "shmpool_metadata.hpp"
#include "buddy_system.hpp"



namespace wsong {
namespace ipc {

namespace fs = std::filesystem;

ShmPool::ShmPool() {}
ShmPool::~ShmPool() {}


void*   shmpool_extent_alloc(extent_hooks_t*,void*,size_t,size_t,bool*,bool*,unsigned);
bool    shmpool_extent_dalloc(extent_hooks_t*,void*,size_t,bool,unsigned);
void    shmpool_extent_destroy(extent_hooks_t*,void*,size_t,bool,unsigned);
bool    shmpool_extent_commit(extent_hooks_t*,void*,size_t,size_t,size_t,unsigned);
bool    shmpool_extent_decommit(extent_hooks_t*,void*,size_t,size_t,size_t,unsigned);
bool    shmpool_extent_purge_lazy(extent_hooks_t*,void*,size_t,size_t,size_t,unsigned);
bool    shmpool_extent_purge_forced(extent_hooks_t*,void*,size_t,size_t,size_t,unsigned);
bool    shmpool_extent_split(extent_hooks_t*,void*,size_t,size_t,size_t,bool,unsigned);
bool    shmpool_extent_merge(extent_hooks_t*,void*,size_t,void*,size_t,bool,unsigned);


/**
 * @class   ShmPoolImpl
 * @brief   The class implements the API defined in ShmPool
 */
class ShmPoolImpl : public ShmPool {
private:
    /**
     * @brief the lock to prevent concurrent lock.
     */
    static std::mutex   init_lock;
    /**
     * @brief global group name
     */
    static std::string  group;
    /**
     * @brief The offset of pool inside the virtual address space for shared memory pools.
     */
    const uint64_t offset;
    /**
     * @brief The capacity of this shared memory pool.
     */
    const uint64_t capacity;
    /**
     * @brief The arena index of this share memory pool.
     */
    unsigned arena_index;

    /**
     * @fn void ShmPoolImpl::register_arena()
     * @brief Register the jemalloc arena using offset and capacity setting.
     */
    void register_arena() {
        extent_hooks_t hooks = {
            .alloc          =   shmpool_extent_alloc,
            .dalloc         =   shmpool_extent_dalloc,
            .destroy        =   shmpool_extent_destroy,
            .commit         =   shmpool_extent_commit,
            .decommit       =   shmpool_extent_decommit,
            .purge_lazy     =   shmpool_extent_purge_lazy,
            .purge_forced   =   shmpool_extent_purge_forced,
            .split          =   shmpool_extent_split,
            .merge          =   shmpool_extent_merge
        };

       
        size_t oldlen = sizeof(arena_index);
        if (ws_mallctl("arenas.create",
                reinterpret_cast<void*>(&arena_index),&oldlen,
                reinterpret_cast<void*>(&hooks),sizeof(hooks))) {
            throw ws_system_error_exp(
                std::string("arenas.create failed with error:") + 
                strerror(errno));
        }
    }

    /**
     * @fn void ShmPoolImpl::unregister_arena()
     * @brief Unregister the arena.
     */
    void unregister_arena() {
        if (arena_index == UINT_MAX) return;

        char mallctl_ns[64];
        sprintf(mallctl_ns,"arena.%u.reset",arena_index);
        if (ws_mallctl(mallctl_ns,nullptr,nullptr,nullptr,0)) {
            throw ws_system_error_exp(
                std::string("mallctl_ns")+ " failed with error:" +
                strerror(errno));
        }
    }

public:
    ShmPoolImpl(const uint64_t cap): 
        ShmPool(),
        offset(VAW::get()->allocate(cap)),
        capacity(cap),
        arena_index(UINT_MAX) {
        register_arena();
    }

    virtual ~ShmPoolImpl() {
        unregister_arena();
        VAW::get()->free(offset);
    }

    virtual uint64_t get_capacity() override {
        return this->capacity;
    }

    virtual uint64_t get_offset() override {
        return this->offset;
    }

    virtual uint64_t get_vaddr() override {
        return this->offset + WS_SHM_POOL_VA_START;
    }

    virtual void* malloc(size_t size) override {
        // TODO:
        return nullptr;
    }

    virtual void free(void* ptr) override {
        // TODO:
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
        // 3. Create the virtual address window.
        VAW::create(group);
        
        // 4. set up group name.
        ShmPoolImpl::group = group;
    }

    WS_DLL_PRIVATE static void remove_group(const std::string& group) {
        if (!group.empty()) {
            VAW::remove(group);
            fs::remove_all(fs::path(get_shm_pool_group_dir(group)));
        }
    }

    WS_DLL_PRIVATE static void initialize(const std::string& group) {
        std::lock_guard<std::mutex> lock(init_lock);
        if (ShmPoolImpl::group.empty()) {
            ShmPoolImpl::group = group;
            VAW::initialize(group);
        } else {
            throw ws_reinitialization_exp("ShmPoolImpl has been initialized already.");
        }
    }

    WS_DLL_PRIVATE static void uninitialize() {
        VAW::uninitialize();
    }
    
    WS_DLL_PRIVATE static std::unique_ptr<ShmPool> create(const uint64_t capacity) {
        return std::unique_ptr<ShmPool>(dynamic_cast<ShmPool*>(new ShmPoolImpl(capacity)));
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

void ShmPool::uninitialize() {
    ShmPoolImpl::uninitialize();
}

std::unique_ptr<ShmPool> ShmPool::create(const uint64_t capacity) {
    return ShmPoolImpl::create(capacity);
}

/** TODO: Implementing them: **/

void*   shmpool_extent_alloc(extent_hooks_t*,void*,size_t,size_t,bool*,bool*,unsigned) {
    return nullptr;
}

bool    shmpool_extent_dalloc(extent_hooks_t*,void*,size_t,bool,unsigned) {
    return false;
}

void    shmpool_extent_destroy(extent_hooks_t*,void*,size_t,bool,unsigned) {
}

bool    shmpool_extent_commit(extent_hooks_t*,void*,size_t,size_t,size_t,unsigned) {
    return false;
}

bool    shmpool_extent_decommit(extent_hooks_t*,void*,size_t,size_t,size_t,unsigned) {
    return false;
}

bool    shmpool_extent_purge_lazy(extent_hooks_t*,void*,size_t,size_t,size_t,unsigned) {
    return false;
}

bool    shmpool_extent_purge_forced(extent_hooks_t*,void*,size_t,size_t,size_t,unsigned) {
    return false;
}

bool    shmpool_extent_split(extent_hooks_t*,void*,size_t,size_t,size_t,bool,unsigned) {
    return false;
}

bool    shmpool_extent_merge(extent_hooks_t*,void*,size_t,void*,size_t,bool,unsigned) {
    return false;
}

/******************************/

} // namespace ipc
} // namespace wsong
