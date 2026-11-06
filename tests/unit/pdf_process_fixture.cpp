#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>
#ifndef _WIN32
#include <sys/types.h>
#include <unistd.h>
#include <fstream>
#endif

int main(int argc, char** argv) {
    if (argc < 2) return 2;
    const std::string mode = argv[1];
    if (mode == "--write" && argc == 3) {
        const auto bytes = static_cast<std::size_t>(std::stoull(argv[2]));
        const std::string block(4096, 'x');
        for (std::size_t left = bytes; left > 0;) {
            const auto count = std::min(left, block.size());
            std::cout.write(block.data(), static_cast<std::streamsize>(count));
            left -= count;
        }
        std::cout.flush();
        return 0;
    }
    if (mode == "--stderr" && argc == 3) {
        const auto bytes = static_cast<std::size_t>(std::stoull(argv[2]));
        const std::string block(4096, 'e');
        for (std::size_t left = bytes; left > 0;) {
            const auto count = std::min(left, block.size());
            std::cerr.write(block.data(), static_cast<std::streamsize>(count));
            left -= count;
        }
        std::cerr.flush();
        return 0;
    }
    if (mode == "--hang") {
        std::this_thread::sleep_for(std::chrono::seconds(10));
        return 0;
    }
#ifndef _WIN32
    if (mode == "--spawn-child" && argc == 3) {
        const pid_t child = fork();
        if (child < 0) return 4;
        if (child == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            std::ofstream marker(argv[2]); marker << "child survived process timeout";
            _exit(0);
        }
        std::this_thread::sleep_for(std::chrono::seconds(10));
        return 0;
    }
#endif
    if (mode == "--exit") return 9;
    return 3;
}
