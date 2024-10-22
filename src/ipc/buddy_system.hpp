#pragma once
/**
 * @file    buddy_system.hpp
 * @brief   The buddy system header file.
 * The APIs is NOT thread-safe.
 */
#include <wsong/common.h>
#include <wsong/exceptions.hpp>
#include <cinttypes>
#include <functional>
#include <string>
#include <cstdlib>
#include <utility>

namespace wsong {
namespace ipc {

/**
 * @brief Function of this type will return a raw pointer to the memory to store the
 * buddy system tree, given the size in bytes.
 */
using buddy_system_tree_loader_t = std::function<void*(int64_t)>;

/**
 * @fn inline bool is_power_of_two(T)
 * @brief Test if an integer number is power of two.
 * @tparam      T   The type of the integer
 * @param[in]   x   The value to be tested.
 * @return  True if 'x' is power of two, otherwise false.
 */
template<typename T>
inline bool is_power_of_two(T x) {
    return (x & (x-1)) == 0 && x != 0;
}

/**
 * @fn inline T nearest_power_of_two(T)
 * @brief Round up an integer number to the nearest power of two.
 * @tparam      T   The type of the integer.
 * @param[in]   x   The value to be tested.
 * @return  The value that round up. 
 * @throws  Exception if overflow.
 */
template <typename T>
inline T nearest_power_of_two(T x) {
    if (!is_power_of_two(x)) {
        int lz = __builtin_clz(x);
        if (lz == 0) {
            throw ws_invalid_argument_exp("Overflow at finding the next power of two of value" +
                std::to_string(x));
        }
        x = 1;
        x = x << (sizeof(T)*8 - lz);
    }
    return x;
}

/**
 * @class   BuddySystem buddy_system.hpp    "buddy_system.hpp"
 * @brief   The budy system API
 */
class BuddySystem {
private:
    /**
     * @brief The capacity of the buddy system.
     */
    const uint64_t capacity;
    /**
     * @brief The unit size of the buddy system.
     */
    const uint64_t unit_size;
    /**
     * @brief The number of levels of the binary tree.
     */
    const uint32_t total_level;
    /**
     * @brief The buddy pointer.
     */
    int64_t* buddies_ptr;
    /**
     * @brief The flag mask
     */
    enum FlagMask{
        /** If I own the tree or not */
        TREE_OWNER =    0x1
    };
    const uint32_t flags;

    /**
     * @fn uint32_t BuddySystem::allocate_buddy(uint32_t level, const size_t& size);
     * @brief Allocate a node from the buddy system
     * The buddy system is managed in a complete binary tree as follows:
     *    level 1               1       -+
     *                         / \       |
     *    level 2             2   3      +-> total level = 3
     *                       / \ / \     |
     *    level 3           4  5 6  7   -+
     * The buddy system is organized in an array of type int64_t:
     * buddies_ptr->[<1>,<2>,<3>,...,<7>], <n> has four possibile values:
     * 0    - The node is free and not splited.
     * -1   - The node is splited, and there are some free buddies among its descendants.
     * -2   - the node is splited, and there is not a free buddy among its descendants.
     * N    - the node is used by N bytes, where N must be a positive number.
     *
     * @param[in]   level           The requested node at the corresponding level, in [1, total_level]
     * @param[in]   size            The size of the data
     *
     * @return      the node id in [1,2^total_level) on success, or 0 if allocation fails.
     * @throw       Exception on invalid arguemnt
     */
    WS_DLL_PRIVATE uint32_t allocate_buddy(uint32_t level, const size_t& size);

    /**
     * @fn uint32_t BuddySystem::internal_allocate_buddy(uint32_t level, uint32_t cur, const size_t& size);
     * @brief allocate a buddy at a specific `level` from tree node `cur` or its descendants.
     *
     * @param[in]   level   The level of the buddy we want to allocate.
     * @param[in]   cur     Current tree node number.
     * @param[in]   size    Size of the data will be stored.
     *
     * @return  The node number of allocated buddy, or 0 for failure.
     */
    WS_DLL_PRIVATE uint32_t internal_allocate_buddy(uint32_t level, uint32_t cur, const size_t& size);

    /**
     * @fn bool BuddySystem::internal_is_free(uint32_t cur, const uint64_t offset, const size_t size);
     * @brief test if the range [offset,offset+size), which MUST be inside of tree node `cur`, is free or not.
     * 
     * @param[in]   cur     The tree node to test.
     * @param[in]   offset  The offset of the range.
     * @param[in]   size    The size of the range.
     *
     * @return True for free, otherwise false.
     */
    WS_DLL_PRIVATE bool internal_is_free(uint32_t cur, const uint64_t offset, const size_t size);

    /**
     * @fn void BuddySystem::free_buddy(uint32_t node);
     * @brief Free a buddy represented by the binary tree node number.
     *
     * @param[in]   node_number     The number of the tree node to be freed.
     *
     * @throw   Exception on invalid arguments.
     */
    WS_DLL_PRIVATE void free_buddy(uint32_t node);

    /**
     * @fn bool BuddySystem::is_tree_owner();
     *
     * @brief Test if I own the memory of the binary tree.
     *
     * @return True/False
     */
     WS_DLL_PRIVATE bool is_tree_owner();

public:
    /**
     * @fn BuddySystem::BuddySystem (const uint32_t capacity_exp, const uint32_t unit_exp, const bool init_flag, const buddy_system_tree_loader_t& loader);
     * @brief   The constructor
     *
     * @param[in]   capacity_exp    The 2's exponent of the total capacity of the buddy system.
     * @param[in]   unit_exp        The 2's exponent of the unit size of the buddy system.
     * @param[in]   init_flag       The data is loaded from an existing buddy system therefore no initialization is needed.
     * @param[in]   loader          The allocator for the backup memory space of the buddy system.
     */
    WS_DLL_PRIVATE BuddySystem (
        const uint32_t capacity_exp,
        const uint32_t unit_exp, const bool init_flag,
        const buddy_system_tree_loader_t& loader);

    /**
     * @fn BuddySystem::BuddySystem (const uint32_t capacity_exp, const uint32_t uint_exp);
     * @brief   The constructor
     * @param[in]   capacity_exp    The 2's exponent of the total capacity of the buddy system.
     * @param[in]   unit_exp        The 2's exponent of the unit size of the buddy system.
     */
    WS_DLL_PRIVATE BuddySystem (
        const uint32_t capacity_exp,
        const uint32_t uint_exp);

    /**
     * @fn uint64_t BuddySystem::allocate(const size_t size);
     * @brief   Allocate an object or memory block of a given size from the buddy system.
     *
     * @param[in]   size            The size of the object/memory block
     *
     * @return  The offset of the allocated object/memory block in the buddy system.
     */
    WS_DLL_PRIVATE uint64_t allocate(const size_t size);

    /**
     * @fn void BuddySystem::free(const uint64_t offset);
     * @brief   Free an object/memory block allocated from the buddy system.
     *
     * @param[in]   offset          The offset of the object in the buddy system.
     *
     * @throw       Exception on failure
     */
    WS_DLL_PRIVATE void free(const uint64_t offset);

    /**
     *  @fn bool BuddySystem::is_free(const uint64_t offset, const size_t size);
     *  @brief  Test if a range is free.
     *
     *  @param[in]  offset          The starting offset of this range.
     *  @param[in]  size            The size of the range.
     *
     *  @return     True if there is no overlapping object/memory block, otherwise false.
     *  @throw      Exception on failure.
     */
    WS_DLL_PRIVATE bool is_free(const uint64_t offset, const size_t size);

    /**
     * @fn std::pair<uint64_t,size_t> BuddySystem::query(const uint64_t offset);
     * @brief   Finding the buddy containing a corresponding offset.
     *
     * @param[in]   offset          The offset of the object in the buddy system
     *
     * @return      A pair with the first element being the offset of the buddy, and the second the size of it.
     * @throw       Exception on failure. 
     */
    WS_DLL_PRIVATE std::pair<uint64_t,size_t> query(const uint64_t offset);

    /**
     *  @fn BuddySystem::~BuddySystem();
     *  @brief  The destructor
     */
    WS_DLL_PRIVATE virtual ~BuddySystem ();

    /**
     * @fn uint64_t BuddySystem::get_capacity();
     * @brief   The getter for attribute capacity.
     */
    WS_DLL_PRIVATE uint64_t get_capacity();

    /**
     * @fn uint64_t BuddySystem::get_unit_size();
     * @brief   The getter for attribute unit_size.
     */
    WS_DLL_PRIVATE uint64_t get_unit_size();

    /**
     * @fn uint64_t BuddySystem::get_tree_size();
     * @brief   Calculate the size of tree storage in memory.
     *
     * @param[in]   capacity        The capacity of the buddy system.
     * @param[in]   unit_size       The unit size of the buddy system.
     *
     * @return      The size of the binary tree used to store the buddy system.
     */
    static inline uint64_t calc_tree_size(uint64_t capacity, uint64_t unit_size) {
        return static_cast<uint64_t>(capacity/unit_size*sizeof(int64_t))<<1;
    }
};

}
}
