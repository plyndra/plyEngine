#include "plyEngine/engine.hpp"

int main() {
    if (ply::init() != 0) {
        return 1;
    }
    ply::shutdown();
    return 0;
}
