#include "app/CommandLineOptions.h"
#include "rendering/Renderer.h"
#include <exception>
#include <iostream>

int main(int argc, char** argv) {
    try {
        rendering::Renderer renderer(app::CommandLineOptions::parse(argc, argv));
        return renderer.run();
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }
}
