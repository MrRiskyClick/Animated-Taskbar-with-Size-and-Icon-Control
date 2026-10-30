// Read-only, independently compiled cross-language interoperability probe.
// Exit 0 means lease observation is fresh, NOT permission to mutate Windows.
#include "src/core/lease_v1.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

int main(int argc, char** argv) {
    if (argc != 3) return 2;
    const std::string trial = argv[2];
    std::ifstream input(argv[1], std::ios::binary);
    if (!input) return 3;
    std::string bytes(std::istreambuf_iterator<char>{input}, {});
    animated_taskbar::LeaseV1 lease;
    const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    if (now < 0 || !animated_taskbar::ParseLeaseV1(bytes, lease) ||
        !animated_taskbar::LeaseV1IsFresh(
            lease, trial, static_cast<std::uint64_t>(now))) return 3;
    std::cout << "Fresh read-only lease (not authorization).\n";
    return 0;
}
