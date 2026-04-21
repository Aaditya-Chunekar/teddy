#include "editor.hpp"
#include <iostream>

int main(int argc, char* argv[]) {
    try {
        Editor ed;
        if (argc >= 2) ed.open(argv[1]);
        ed.run();
    } catch (const std::exception& e) {
        std::cerr << "teddy: " << e.what() << '\n';
        return 1;
    }
}
