// Run this under AddressSanitizer, the only thing here that sees these two faults: the decoding
// buffer must hold the longest key and its NUL, and it must be freed the way it was allocated.

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

// Both headers define the same names. The headers they include are above, so their guards keep them
// out of the namespaces.
namespace headers_in_blocks {
#include "rear_coded_array.hpp"
}
namespace separate_headers {
#include "rear_coded_array.separate_headers.hpp"
}

template<typename Array>
void check_longest_key() {
    std::vector<std::string> keys{"a", "abc"};
    Array array(keys.begin(), keys.end(), 1024);
    // A query that extends the longest key makes rank compare the byte after that key.
    if (array.rank("abcd") != keys.size())
        throw std::runtime_error("wrong rank");
}

int main() {
    check_longest_key<headers_in_blocks::RearCodedArray>();
    check_longest_key<separate_headers::RearCodedArray>();
    return 0;
}
