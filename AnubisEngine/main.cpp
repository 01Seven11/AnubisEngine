#include "AnubisEngine.h"

#include <iostream>
#include <stdexcept>
#include <cstdlib>

int main(int argc, char* argv[]) {
    AnubisEngine engine;

    try {
        engine.run();
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}