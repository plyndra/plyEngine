#include "plyEngine/engine.hpp"

#include <iostream>

namespace ply {

int init() {
    std::cout << "plyEngine " << kEngineVersion << " initialized\n";
    return 0;
}

void shutdown() {}

}  // namespace ply
