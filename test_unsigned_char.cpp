// The varint decoder must not depend on the signedness of char. This test is built with
// -funsigned-char, which is how char already behaves on some targets, aarch64 Linux among them.

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

#include "rear_coded_array.separate_headers.hpp"

static_assert(std::is_unsigned_v<char>, "build this test with -funsigned-char");

int main() {
    // A rear length of 200 takes two bytes, so the loop in decode_int runs as well as its exit.
    std::vector<std::string> keys{std::string(200, 'a'), "b"};
    RearCodedArray array(keys.begin(), keys.end(), 1024);

    char out[201];
    for (size_t i = 0; i < keys.size(); ++i) {
        array.access(i, out);
        if (keys[i] != out)
            throw std::runtime_error("access mismatch at " + std::to_string(i));
    }
    if (array.rank("bz") != keys.size())
        throw std::runtime_error("wrong rank");
    return 0;
}
