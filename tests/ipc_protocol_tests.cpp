#include "native/ipc_protocol.hpp"
#include <cassert>
#include <iostream>
#include <string>
int main() {
    using namespace atb_ipc;
    Secret zero{}, converted{};
    auto hx = HexEncode(zero);
    assert(HexDecode(std::string(hx.data(), hx.size()), converted) && converted == zero);
    assert(!HexDecode("f", converted));
    assert(!HexDecode(std::string(64, 'Z'), converted));
    Secret all{};
    all.fill(255);
    auto allHx = HexEncode(all);
    assert(HexDecode(std::string(allHx.data(), allHx.size()), converted) && converted == all);
    std::array<std::uint8_t,32> nonce{};
    std::array<std::uint8_t,16> trial{};
    nonce[0] = 1; trial[1] = 3;
    const auto c = MakeChallenge(nonce, trial, 1000);
    assert(ValidateChallenge(c, 1000));
    assert(ValidateChallenge(c, 5999));
    assert(!ValidateChallenge(c, 6001));
    assert(!ValidateChallenge(c, 999));
    auto altered = c;
    altered[0] = 0;
    assert(!ValidateChallenge(altered, 1010));
    auto response = MakeResponse(all);
    assert(CheckResponse(response, all));
    response[9] ^= 1;
    assert(!CheckResponse(response, all));
    std::cout << "Stage-1e IPC wire contract passed (no Win32 effects)\n";
}
