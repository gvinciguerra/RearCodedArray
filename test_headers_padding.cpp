// Run this under AddressSanitizer, the only thing here that sees this fault. strcmp_lcp and lcp64
// read a header a word at a time, so the last header needs padding after it.

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "rear_coded_array.separate_headers.hpp"

int main() {
    // A block_bytes of 0 makes every key a header, so the second key ends the headers area. The first
    // key and its NUL take 24 bytes because ASan misses an unaligned read that starts in a valid word.
    for (size_t length = 1; length <= 16; ++length) {
        std::vector<std::string> keys{std::string(23, 'a'), std::string(length, 'z')};
        RearCodedArray array(keys.begin(), keys.end(), 0);
        if (array.rank(std::string(64, 'z')) != keys.size())
            throw std::runtime_error("wrong rank at length " + std::to_string(length));
    }
    return 0;
}
