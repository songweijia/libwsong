#include <buddy_system.hpp>
#include <iostream>

using namespace wsong::ipc;

int main(int argc, char** argv) {
    const uint64_t  capacity    = 1ull<<23;
    const uint64_t  unit_size   = 1ull<<20;
    const uint64_t  tree_size   = BuddySystem::calc_tree_size(capacity,unit_size);
    int64_t* tree = reinterpret_cast<int64_t*>(tree_size);

    if (tree == nullptr) {
        std::cerr << "Failed to allocate memory for buddy system." << std::endl;
        return 1;
    }

    BuddySystem bs(23,20,true,[&tree,&tree_size](uint64_t size){ 
        if (size!=tree_size)
            return static_cast<void*>(nullptr);
        else
            return static_cast<void*>(tree);
        });

    // TODO:


    free(reinterpret_cast<void*>(tree));

    std::cout << "Test finished successfully." << std::endl;
    return 0;
};
