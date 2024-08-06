#include <wsong/ipc/shmpool.hpp>
#include <mutex>
#include <filesystem>

namespace wsong {
namespace ipc {

namespace fs = std::filesystem;

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
 * @brief Get the chunktable file path
 */
#define WS_SHM_POOL_GROUP_CHUNKTABLE(group) \
    WS_SHM_POOL_GROUP_META(group) "/chunktable"
/**
 * @brief Get the buddy system file path
 */
#define WS_SHM_POOL_GROUP_BUDDIES(group) \
    WS_SHM_POOL_GROUP_META(group) "/buddies"


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
public:
    ShmPoolImpl(const uint64_t capacity) {
        // TODO:
    }

    WS_DLL_PRIVATE static void initialize(const std::string& group) {
        std::lock_guard<std::mutex> guard(init_lock);
        if (ShmPoolImpl::group.empty()) {
            std::error_code errcode;
            // 1. Test if folder exists already, throw exception if it does.
            fs::path group_dir(WS_SHM_POOL_GROUP_META(group));
            if (fs::exists(group_dir,errcode)) {
                throw ws_invalid_argument_exp(
                        std::string("Group:") + group + "'s metadata folder(" + 
                        WS_SHM_POOL_GROUP_META(group) + ") has existed already");
            } else if (errcode) {
                throw ws_invalid_argument_exp(
                        std::string("Unable to check Group:") + group + "'s metadata folder(" +
                        WS_SHM_POOL_GROUP_META(group) + "). Error:" + errcode.message());
            }
            // 2. Create folders and initialize the data structures
            if (!fs::create_directory(group_dir,errcode)) {
                throw ws_invalid_argument_exp(
                        std::string("Failed to create folder:") + group + "'s metadata folder(" +
                        WS_SHM_POOL_GROUP_META(group) + "). Error:" + errcode.message());
            }
            // 3. Create chunktable and buddy system files. 
            // TODO:
            // 4. set up group name.
            ShmPoolImpl::group = group;
        } else {
            throw ws_reinitialization_exp("ShmPoolImpl has been initialized already.");
        }
    }

    WS_DLL_PRIVATE static void uninitialize() {
        // TODO: unmap all and delete the shared folder
    }
};

} // namespace ipc
} // namespace wsong
