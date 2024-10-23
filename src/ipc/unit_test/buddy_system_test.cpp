#include <buddy_system.hpp>
#include <iostream>

using namespace wsong::ipc;

void print_tree(int64_t* tree,uint32_t level) {
    for (uint32_t i=0;i<=(1u<<level);i++) {
        std::cout << " [" << i << "]" << tree[i];
    }
    std::cout << std::endl;
}

int main(int argc, char** argv) {
    const uint64_t  capacity    = 1ull<<23;
    const uint64_t  unit_size   = 1ull<<20;
    const uint64_t  tree_size   = BuddySystem::calc_tree_size(capacity,unit_size);
    int64_t* tree = reinterpret_cast<int64_t*>(malloc(tree_size));

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

    std::cout << "1 - Initialized:" << std::endl;
    print_tree(tree,4);

    uint64_t ofst_1MB = bs.allocate(1ull<<20);
    std::cout << "2 - Allocated 1 MB@" << ofst_1MB << std::endl;
    print_tree(tree,4);

    uint64_t ofst_100 = bs.allocate(100);
    std::cout << "3 - Allocated 100B@" << ofst_100 << std::endl;
    print_tree(tree,4);

    uint64_t ofst_1048577 = bs.allocate(1048577);
    std::cout << "4 - Allocated 100B@" << ofst_1048577 << std::endl;
    print_tree(tree,4);

    uint64_t ofst_2MB= bs.allocate(2097152);
    std::cout << "5 - Allocated 2 MB@" << ofst_2MB << std::endl;
    print_tree(tree,4);

    try { 
        std::cout << "6 - Allocated 3 MB." << std::endl;
        bs.allocate(3<<20);
        print_tree(tree,4);
    } catch (wsong::ws_system_error_exp &ex) {
        std::cout << "Failed-OOM." << std::endl;
    }

    std::cout << "7 - Free 100@" << ofst_100 << std::endl;
    bs.free(ofst_100);
    print_tree(tree,4);

    std::cout << "8 - Free 1MB@" << ofst_1MB << std::endl;
    bs.free(ofst_1MB);
    print_tree(tree,4);

    try {
    std::cout << "9 - Free 1MB@" << ofst_1MB << " again" << std::endl;
    bs.free(ofst_1MB);
    print_tree(tree,4);
    } catch (wsong::ws_invalid_argument_exp& ex) {
        std::cout << "Failed-not allocated." << std::endl;
    }

    std::cout << "10 - Free 1048577@" << ofst_1048577 << std::endl;
    bs.free(ofst_1048577);
    print_tree(tree,4);

    std::cout << "11 - Free 2MB@" << ofst_2MB << std::endl;
    bs.free(ofst_2MB);
    print_tree(tree,4);

    free(reinterpret_cast<void*>(tree));

    std::cout << "Test finished successfully." << std::endl;
    return 0;
};
