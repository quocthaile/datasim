// Fault-injection process only. Never used as evidence of numerical correctness.
#include <fstream>
#include <iostream>
#include <string>
#include <chrono>
#include <thread>
int main(int argc, char** argv) {
    if (argc>1 && std::string(argv[1])=="--echo") {
        for (int i=2;i<argc;++i) std::cout << argv[i] << '\n';
        std::ofstream("cwd-marker.txt") << "created in child workspace";
        return 0;
    }
    if (argc>1 && std::string(argv[1])=="--hang") {
        std::cout << "started timeout fixture\n" << std::flush;
        std::this_thread::sleep_for(std::chrono::milliseconds(800));
        std::ofstream("must-not-exist.txt") << "escaped timeout";
        return 0;
    }
    if (argc == 3 && std::string(argv[2]) == "--version") {
        std::cout << "ngspice-test-fixture (not a simulator)\n"; return 0;
    }
    std::string mode; std::ifstream("mode.txt") >> mode;
    if (mode == "exit") return 9;
    if (mode == "missing") return 0;
    if (mode == "warning") std::cout << "Warning: injected warning\n";
    std::ofstream out("samples.dat");
    if (mode == "empty") return 0;
    if (mode == "signal") { out << "time v(vin)\n0 0\n"; return 0; }
    out << "time v(vin) v(vout)\n";
    if (mode == "malformed") out << "0 0 broken\n";
    else if (mode == "nonfinite") out << "0 0 inf\n";
    else out << "0 0 0\n";
    return 0;
}
