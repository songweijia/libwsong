#include "metadata.hpp"

#include <wsong/ipc/shmpool.hpp>

#include <cstdlib>
#include <sys/stat.h>
#include <sys/file.h>
#include <fcntl.h>
#include <errno.h>

namespace wsong {
namespace ipc {

std::unique_ptr<VAW>    buddies;

VAW::VAW(const std::string& group) :
    group_name(group), fd(-1) {

    buddies = std::make_unique<BuddySystem>(group,WS_SHM_POOL_VA_SIZE,WS_MIN_SHM_POOL_SIZE,
        [&fd](size_t s){
            // 1 - open a file
            fd = open (get_shm_pool_group_buddies(group_name).c_str(),O_RDWR);

            if (fd == -1) {
                throw ws_system_error_exp(
                std::string(__PRETTY_FUNCTION__ " failed to open the buddy system file:") + 
                get_shm_pool_group_buddies(group_name) + ", error:" + strerror(errno));
            }

            struct stat sbuf;
            if (fstat(fd,&sbuf) == -1) {
                close(fd);
                fd = -1;
                throw ws_system_error_exp(
                    std::string(__PRETTY_FUNCTION__ " failed to fstat buddy system file:") +
                    get_shm_pool_group_buddies(group_name) + ", error:" + strerror(errno));
            }

            if (static_cast<off_t>(s) != sbuf.st_size) {
                close(fd);
                fd = -1;
                throw ws_invalid_argument_exp(
                    std::string(__PRETTY_FUNCTION__ " encounter invalid file size:") + std::to_string(sbuf.st_size) +
                    ", expecting " + std::to_string(s));
            }

            // mmap it.
            void* buddies_ptr = mmap(nullptr,s,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);
            if (buddies_ptr == MAP_FAILED) {
                close(fd);
                fd = -1;
                throw ws_system_error_exp(
                    std::string(__PRETTY_FUNCTION__ " failed to map the buddy system file, error:") + strerror(no));
            }
            return buddies_ptr;
        });
}

uint64_t VAW::allocate(const size_t pool_size) {
    uint64_t vaddr = 0ULL;
    // 1 - validate pool size
    if (!is_power_of_two(pool_size) || 
        pool_size > buddies->get_capacity() ||
        pool_size < buddies->get_unit_size()) {
        throw ws_invalid_argument_exp(
            std::string(__PRETTY_FUNCTION__ " got invalid pool size:") + std::to_string(pool_size) +
            ", expecting a power of two value in range [" + std::to_string(buddies->get_unit_size()) + "," +
            std::to_string(buddies->get_capacity()) + "]");
    }

    // 2 - applying pthread lock.
    std::lock_guard<std::mutex> lock(buddies_mutex);

    // 3 - lock the buddy file.
    if (flock(fd,LOCK_EX) == -1) {
        throw ws_system_error_exp(
            std::string(__PRETTY_FUNCTION__ " failed to apply file lock on buddy system file:") +
                get_shm_pool_group_buddies(group_name) + ", error:" + strerror(errno));
    }

    // 4 - allocate the space
    try {
        vaddr = buddies->allocate(pool_size);
    } catch (ws_exp& ex) {
        flock(fd,LOCK_UN);
        throw ex;
    }

    // 5 - unlock the buddy file
    if (flock(fd,LOCK_UN) == -1) {
        throw ws_system_error_exp (
            std::string(__PRETTY_FUNCTION__ " failed to unlock buddy system file:") +
            get_shm_pool_group_buddies(group_name) + ", error:" + strerror(errno));
    }

    return vaddr;
}

void VAW::free(const uint64_t pool_offset) {
    // 1 - validate pool_offset
    if (pool_offset % buddies->get_unit_size()) {
        throw ws_invalid_argument_exp(
            std::string(__PRETTY_FUNCTION__ " got invalid pool offset:") + std::to_string(pool_offset) +
            ", which is not multiple of unit size:" + std::to_string(buddies->get_unit_size()) + ".");
    }
    if (pool_offset > buddies->get_capacity()) {
        throw ws_invalid_argument_exp(
            std::string(__PRETTY_FUNCTION__ " got invalid pool offset:") + std::to_string(pool_offset) +
            ", which is beyond window capacity:" + std::to_string(buddies->get_capacity()) + ".");
    }

    // 2 - apply pthread lock
    std::lock_guard<std::mutex> lock(buddies_mutex);

    // 3 - lock the buddy file
    if (flock(fd,LOCK_EX) == -1) {
        throw ws_system_error_exp(
            std::string(__PRETTY_FUNCTION__ " failed to apply file lock on buddy system file:") +
                get_shm_pool_group_buddies(group_name) + ", error:" + strerror(errno));
    }

    // 4 - free
    try{
        buddies->free(pool_offset);
    } catch(ws_exp& ex) {
        flock(fd,LOCK_UN);
        throw ex;
    }

    // 5 - unlock the buddy file
    if (flock(fd,LOCK_UN) == -1) {
        throw ws_system_error_exp (
            std::string(__PRETTY_FUNCTION__ " failed to unlock buddy system file:") +
            get_shm_pool_group_buddies(group_name) + ", error:" + strerror(errno));
    }
}

std::pair<uint64_t, size_t> VAW::query(const int64_t va_offset) {
    std::pair<uint64_t,size_t> result;

    // 1 - apply pthread lock
    std::lock_guard<std::mutex> lock(buddies_mutex);

    // 2 - read-lock the buddy file
    if (flock(fd,LOCK_SH) == -1) {
        throw ws_system_error_exp(
            std::string(__PRETTY_FUNCTION__ " failed to apply shared file lock on buddy system file:") +
            get_shm_pool_group_buddies(group_name) + ", error:" + strerror(errno));
    }

    // 3 - find the offset
    try {
        result = buddies->query(va_offset);
    } catch (ws_exp& ex) {
        flock(fd,LOCK_UN);
        throw ex;
    }

    // 4 - unlock the buddy file
    if (flock(fd,LOCK_UN) == -1) {
        throw ws_system_error_exp (
            std::string(__PRETTY_FUNCTION__ " failed to unlock buddy system file:") +
            get_shm_pool_group_buddies(group_name) + ", error:" + strerror(errno));
    }

    return result;
}

VAW::~VAW() {
    // nothing to do.
}

void VAW::initialize(const std::string& group) {
    singleton = std::make_unique<BuddySystem>(group);
}

void VAW::uninitialize() {
    if (singleton) {
        singleton.reset();
    }
}

void VAW::create(const std::string& group) {
    // 1 - get tree size.
    uint64_t tree_size = BuddySystem::calc_tree_size(WS_SHM_POOL_VA_SIZE,WS_MIN_SHM_POOL_SIZE);

    // 2 - create file.
    std::string file_path_str = get_shm_pool_group_buddies(group);
    std::ofstream bfile(file_path_str);
    bfile.close();
    std::filesystem::path path(file_path_str);
    std::filesystem::resize_file(path,tree_size);

    // 3 - initialize the tree file.
    // TODO:
}
