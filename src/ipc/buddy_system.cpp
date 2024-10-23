#include "buddy_system.hpp"
#include <wsong/exceptions.hpp>
#include <string>

namespace wsong {
namespace ipc {

/** @brief  Leaf node is free */
#define BUDDY_STATE_IDLE            (0ll)
/** @brief  Inner node is halfway full: there is some space left in its descendant(s). */
#define BUDDY_STATE_SPLT_HALFWAY    (-1ll)
/** @brief  Inner node is full: there is no space left in its descendants. */
#define BUDDY_STATE_SPLT_FULL       (-2ll)
/** @brief  Test if a buddy node is full or not */
#define BUDDY_IS_FULL(s)    ((s > 0) || (s == BUDDY_STATE_SPLT_FULL))
/** @brief  Get the level of a node */
#define LEVEL_OF(nid)           static_cast<uint32_t>(32 - __builtin_clz(static_cast<uint32_t>(nid)))
/** @brief  Get the number of siblings (nodes at the same level), including tree node itself */
#define NUM_SIBLINGS_OF(nid)    static_cast<uint32_t>(nearest_power_of_two(nid+1)>>1)
/** @brief  Get the sibling index (starting from 0, left to right) */
#define SIBLING_INDEX_OF(nid)   static_cast<uint32_t>(nid - NUM_SIBLINGS_OF(nid))
/** @brief  Get the offset of node */
#define OFFSET_OF(nid,cap)      static_cast<uint64_t>(cap/NUM_SIBLINGS_OF(nid)*SIBLING_INDEX_OF(nid))
/** @brief  Get the range covered by a node */
#define RANGE_OF(nid,cap)       static_cast<size_t>(cap/NUM_SIBLINGS_OF(nid))

BuddySystem::BuddySystem(
    const uint32_t capacity_exp,
    const uint32_t unit_exp,
    const bool init_flag,
    const buddy_system_tree_loader_t& loader) :
    capacity(1ull<<capacity_exp), unit_size(1ull<<unit_exp),
    total_level(__builtin_ctzl(capacity/unit_size) + 1), flags(0) {

    if (unit_exp > capacity_exp) {
        throw ws_invalid_argument_exp(
            std::string(__PRETTY_FUNCTION__) +
            ": got invalid capacity/unit size. Capacity exp:" + std::to_string(capacity_exp) +
            " < unit exp:" + std::to_string(unit_exp));
    }

    buddies_ptr = reinterpret_cast<int64_t*>(loader(calc_tree_size(capacity,unit_size)));

    if (buddies_ptr == nullptr) {
        throw ws_system_error_exp(
            std::string(__PRETTY_FUNCTION__) +": binary tree memory loading failed.");
    }

    if (init_flag) {
        buddies_ptr[1] = BUDDY_STATE_IDLE;
    }
}

BuddySystem::BuddySystem(
    uint32_t capacity_exp,
    uint32_t unit_exp) :
    capacity(1ull<<capacity_exp), unit_size(1ull<<unit_exp), 
    total_level(__builtin_ctzl(capacity/unit_size) + 1), flags(TREE_OWNER) {

    if (unit_exp > capacity_exp) {
        throw ws_invalid_argument_exp(
            std::string(__PRETTY_FUNCTION__) +
            ": got invalid capacity/unit size. Capacity exp:" + std::to_string(capacity_exp) +
            " < unit exp:" + std::to_string(unit_exp));
    }

    buddies_ptr = reinterpret_cast<int64_t*>(malloc((calc_tree_size(capacity,unit_size))));

    if (buddies_ptr == nullptr) {
        throw ws_system_error_exp(
            std::string(__PRETTY_FUNCTION__) +": memory allocation failed.");
    }

    buddies_ptr[1] = BUDDY_STATE_IDLE;
}

WS_DLL_PRIVATE uint32_t BuddySystem::allocate_buddy(uint32_t level, const size_t& size) {
    if (level < 1 || level > total_level) {
        throw ws_invalid_argument_exp(
            std::string("Requested level:") + std::to_string(level) + 
            ", is out of range [1," + std::to_string(total_level) + "].");
    }
    return internal_allocate_buddy(level,1,size);
}

WS_DLL_PRIVATE uint32_t BuddySystem::internal_allocate_buddy(uint32_t level, uint32_t cur, const size_t& size) {
    uint32_t cur_level = LEVEL_OF(cur);
    if (cur_level > level) {
        throw ws_invalid_argument_exp(
            std::string("Requested level:") + std::to_string(level) +
            " is higher than the level of the current node:" + std::to_string(cur));
    } else if (cur_level == level) {
        if (buddies_ptr[cur] == BUDDY_STATE_IDLE) {
            // found a buddy, set it to full and return
            buddies_ptr[cur] = size;
            return cur;
        } else {
            // not found - running OOM
            return 0;
        }
    } else {
        // going down
        uint32_t l = cur<<1;
        uint32_t r = l + 1;
        switch(buddies_ptr[cur]) {
        case BUDDY_STATE_IDLE:
            // split and always use the left child
            buddies_ptr[cur]    = BUDDY_STATE_SPLT_HALFWAY;
            buddies_ptr[l]      = BUDDY_STATE_IDLE;
            buddies_ptr[r]      = BUDDY_STATE_IDLE;
            return internal_allocate_buddy(level,l,size);
        case BUDDY_STATE_SPLT_HALFWAY:
            {
                // try left
                uint32_t ret = internal_allocate_buddy(level,l,size);
                // try right
                if (ret == 0) {
                    ret = internal_allocate_buddy(level,r,size);
                }
                if (ret > 0 &&
                    BUDDY_IS_FULL(buddies_ptr[l]) && 
                    BUDDY_IS_FULL(buddies_ptr[r])) { 
                    buddies_ptr[cur] = BUDDY_STATE_SPLT_FULL;
                }
                return ret;
            }
        case BUDDY_STATE_SPLT_FULL:
        default:
            // not found - running OOM
            return 0;
        }
    }
}

uint64_t BuddySystem::allocate(const size_t size) {
    // 1. Round up to nearest power-of-two value.
    size_t bsize = std::max(nearest_power_of_two(size),this->unit_size);

    // 2. Validation
    if (bsize > this->capacity) {
        throw ws_invalid_argument_exp(
            std::string(__PRETTY_FUNCTION__) + " cannot satisfy buddy size:" +
            std::to_string(size) + ", which is greater than capacity:" +
            std::to_string(this->capacity));
    }

    // 3. allocate
    const uint32_t requested_level  = total_level - __builtin_ctzl(bsize/unit_size);
    uint32_t node = allocate_buddy(requested_level,size);

    if (node == 0) {
        throw ws_system_error_exp(__PRETTY_FUNCTION__ + 
            std::string(" runs out of memory."));
    }

    return OFFSET_OF(node,this->capacity);
}

void BuddySystem::free_buddy(uint32_t node_number) {
    if (node_number >= (1U<<total_level)) {
        throw ws_invalid_argument_exp( __PRETTY_FUNCTION__ + 
                std::string(" tries to free node:") + std::to_string(node_number) +
                ", which is out of range. Expected range [1," + std::to_string(total_level) + ").");
    }
    if (buddies_ptr[node_number] <= 0) {
        throw ws_invalid_argument_exp( __PRETTY_FUNCTION__ + 
                std::string(" tries to free node:") + std::to_string(node_number) +
                "in STATE(" + std::to_string(buddies_ptr[node_number]) +
                "). Expecting an allocated leaf node.");
    }
    
    buddies_ptr[node_number] = BUDDY_STATE_IDLE;
    int parent = (node_number>>1);
    while (parent > 0) {
        if (buddies_ptr[parent<<1] == BUDDY_STATE_IDLE &&
            buddies_ptr[(parent<<1) + 1] == BUDDY_STATE_IDLE) {
            // turn to idle
            buddies_ptr[parent] = BUDDY_STATE_IDLE;
        } else if (buddies_ptr[parent] == BUDDY_STATE_SPLT_FULL) {
            // turn to halfway full
            buddies_ptr[parent] = BUDDY_STATE_SPLT_HALFWAY;
        } else {
            // stay splt, no need to continue.
            break;
        }
        parent = parent >> 1;
    }
}

bool BuddySystem::is_tree_owner() {
    return flags&TREE_OWNER;
}

void BuddySystem::free(const uint64_t offset) {
    
    if (offset % this->unit_size) {
        throw ws_invalid_argument_exp( __PRETTY_FUNCTION__ +
            std::string(" requested offset:") + std::to_string(offset) +
            ", which does not align with unit_size:" + std::to_string(unit_size));
    }

    const uint32_t node_number  = (this->capacity + offset) / this->unit_size;

    free_buddy(node_number);
}

bool BuddySystem::internal_is_free(uint32_t cur, const uint64_t offset, const size_t size) {
    if (this->buddies_ptr[cur] == BUDDY_STATE_IDLE) {
        return true;
    } else if (BUDDY_IS_FULL(this->buddies_ptr[cur])) {
        return false;
    } else { // BUDDY_STATE_SPLT_HALFWAY
        uint32_t l = cur << 1;
        uint32_t r = l + 1;
        if (static_cast<uint64_t>(offset + size) <= OFFSET_OF(r,this->capacity)) {
            // check left tree only
            return internal_is_free(l,offset,size);
        } else if (offset >= OFFSET_OF(r,this->capacity)) {
            // check right tree only
            return internal_is_free(r,offset,size);
        } else {
            return internal_is_free(l,offset,OFFSET_OF(r,this->capacity)-offset) &&
                   internal_is_free(r,OFFSET_OF(r,this->capacity),offset+size-OFFSET_OF(r,this->capacity));
        }
    }
}

bool BuddySystem::is_free(const uint64_t offset, const size_t size) {
    if (static_cast<uint64_t>(offset + size) > this->capacity) {
        throw ws_invalid_argument_exp( __PRETTY_FUNCTION__ +
            std::string(" tested a range out of capacity."));
    }
    return internal_is_free(1,offset,size);
}

std::pair<uint64_t,size_t> BuddySystem::query(const uint64_t offset) {

    uint32_t root = 1;
    uint32_t leaf_index = offset / unit_size;
    uint32_t num_leaves = 1u << (total_level - 1);


    while (buddies_ptr[root] == BUDDY_STATE_SPLT_FULL || buddies_ptr[root] == BUDDY_STATE_SPLT_HALFWAY) {
        num_leaves = num_leaves >> 1;
        if (leaf_index < num_leaves) { // take left
            root = root << 1;
        } else { // take right
            root = (root << 1) + 1;
            leaf_index -= num_leaves;
        }
    }

    if (buddies_ptr[root] == BUDDY_STATE_IDLE) {
        throw ws_invalid_argument_exp( __PRETTY_FUNCTION__ +
            std::string(" queries offset:") + std::to_string(offset) +
            ", which is out of any allocated buddies.");
    }

    uint32_t nodes_at_level =   nearest_power_of_two(root + 1)>>1;
    uint64_t buddy_offset   =   (root - nodes_at_level) * this->capacity / nodes_at_level;

    return {buddy_offset,this->buddies_ptr[root]};
}

uint64_t BuddySystem::get_capacity() {
    return this->capacity;
}

uint64_t BuddySystem::get_unit_size() {
    return this->unit_size;
}

BuddySystem::~BuddySystem() {
    if (is_tree_owner()) {
        ::free(reinterpret_cast<void*>(buddies_ptr));
    }
}

}
}
