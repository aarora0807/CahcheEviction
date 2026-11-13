// main.cpp - entry point: parse arguments, run the App, map errors to exit codes.
#include <exception>
#include <iostream>
#include <string>
#include <vector>

#include "Cli.h"

int main(int argc, char** argv) {
    try {
        const std::vector<std::string> args(argv + 1, argv + argc);
        cachesim::App app(cachesim::Cli::parse(args));
        return app.run();
    } catch (const std::invalid_argument& e) {
        std::cerr << "error: " << e.what() << "\n\n";
        cachesim::Cli::printUsage(std::cerr);
        return 2;
    } catch (const std::exception& e) {
        std::cerr << "fatal: " << e.what() << '\n';
        return 1;
    }
}
