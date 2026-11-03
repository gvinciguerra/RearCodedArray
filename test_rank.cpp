// The constructor requires keys sorted as std::string sorts them, by unsigned bytes. rank must order
// bytes the same way, so the alphabet here straddles the sign bit of a char.

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

// Every string of at most max_length bytes over the alphabet, the empty one included, sorted.
std::vector<std::string> strings_up_to(size_t max_length) {
    const std::string alphabet = "az\x7f\x80\xc3\xff";
    std::vector<std::string> result{""};
    size_t begin = 0;
    for (size_t length = 1; length <= max_length; ++length) {
        auto end = result.size();
        for (auto i = begin; i < end; ++i)
            for (auto c: alphabet)
                result.push_back(result[i] + c);
        begin = end;
    }
    std::sort(result.begin(), result.end());
    return result;
}

template<typename Array>
size_t wrong_ranks(const std::vector<std::string> &keys, const std::vector<std::string> &queries, size_t block_bytes) {
    Array array(keys.begin(), keys.end(), block_bytes);
    size_t wrong = 0;
    for (auto &query: queries) {
        size_t expected = std::upper_bound(keys.begin(), keys.end(), query) - keys.begin();
        if (array.rank(query) != expected)
            ++wrong;
    }
    return wrong;
}

int main() {
    auto queries = strings_up_to(4);
    auto keys = strings_up_to(3);
    keys.erase(keys.begin()); // The constructor rejects the empty string

    size_t wrong = 0;
    for (size_t block_bytes: {8, 64, 4096}) {
        auto in_blocks = wrong_ranks<headers_in_blocks::RearCodedArray>(keys, queries, block_bytes);
        auto separate = wrong_ranks<separate_headers::RearCodedArray>(keys, queries, block_bytes);
        std::cerr << "block_bytes " << block_bytes << ": wrong ranks of " << queries.size() << " queries: "
                  << in_blocks << " with headers in the blocks, " << separate << " with separate headers" << std::endl;
        wrong += in_blocks + separate;
    }
    return wrong != 0;
}
