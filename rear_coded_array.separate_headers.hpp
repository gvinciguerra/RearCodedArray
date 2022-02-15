//
// Created by Giorgio Vinciguerra on 12/07/21.
//

#pragma once

#include <algorithm>
#include <cassert>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

size_t compute_lcp(const char *a, const char *b) {
    size_t i = 0;
    while (a[i] != '\0' && a[i] == b[i])
        ++i;
    return i;
}

size_t compute_lcp(std::string_view a, std::string_view b) {
    return std::mismatch(a.begin(), a.end(), b.begin(), b.end()).first - a.begin();
}

class RearCodedArray {
    class HeaderIterator;
    class BlockInfo;

    std::string data;
    std::string headers;
    std::vector<BlockInfo> info;
    size_t n;
    char *buffer;

public:

    template<typename InputIt>
    RearCodedArray(InputIt first, InputIt last, size_t block_bytes) : n(0) {
        data.reserve(1 << 22);
        headers.reserve(1 << 20);
        size_t input_bytes = 0;
        size_t max_length = 0;
        size_t max_lcp = 0;
        size_t sum_lcp = 0;
        size_t sum_length = 0;
        std::string prev;

        for (n = 0; first != last; ++n, ++first) {
            if (*first <= prev)
                throw std::invalid_argument("data is not sorted");

            auto lcp = compute_lcp(prev, *first);
            max_lcp = std::max(max_lcp, lcp);
            max_length = std::max(max_length, first->length());
            sum_lcp += lcp;
            sum_length += first->length();
            input_bytes += first->length() + 1;

            auto current_block_bytes = n == 0 ? size_t(-1) : data.size() - info.back().data_pointer;
            if (current_block_bytes >= block_bytes) {
                info.emplace_back(n, data.size(), headers.size());
                headers.append(*first);
                headers.push_back('\0');
                prev = *first;
                continue;
            }

            auto rear_length = prev.length() - lcp;
            encode_int(rear_length, data);
            data.append(*first, lcp);
            data.push_back('\0');
            prev = *first;
        }

        headers.append('\0', sizeof(uint64_t));
        info.emplace_back(n, data.size(), headers.size());
        info.shrink_to_fit();
        data.shrink_to_fit();
        headers.shrink_to_fit();
        buffer = new char[max_length]();

        size_t max_hdr_lcp = 0;
        size_t sum_hdr_lcp = 0;
        for (auto it = headers_begin() + 1; it != headers_end(); ++it) {
            auto lcp = compute_lcp(*it, *(it - 1));
            max_hdr_lcp = std::max(max_hdr_lcp, lcp);
            sum_hdr_lcp += lcp;
        }

        std::cout << "Input bytes             " << input_bytes << std::endl
                  << "Input avg length        " << sum_length / double(n) << std::endl
                  << "Input avg LCP           " << sum_lcp / double(n) << ", max " << max_lcp << std::endl
                  << "RC block_bytes          " << block_bytes << std::endl
                  << "RC bytes                " << size_in_bytes() << std::endl
                  << "RC blocks               " << blocks_count() << std::endl
                  << "RC headers avg LCP      " << sum_hdr_lcp / double(blocks_count())
                  << ", max " << max_hdr_lcp << std::endl
                  << "Avg strings per block   " << n / blocks_count() << std::endl;
    }

    ~RearCodedArray() { delete buffer; }

    size_t blocks_count() const { return info.size() - 1; }

    size_t size_in_bytes() const {
        return data.size() * sizeof(data[0]) + headers.size() * sizeof(headers[0])
            + info.size() * sizeof(info[0])
            + sizeof(*this);
    }

    char *access(size_t i, char *out) const {
        auto block = block_containing_position(i);
        auto out_ptr = stpcpy(out, headers.data() + info[block].header_pointer);
        auto data_ptr = data.data() + info[block].data_pointer;
        for (int j = 1; j <= i - info[block].count; ++j) {
            auto rear_length = decode_int(data_ptr);
            out_ptr -= rear_length;
            auto tmp = stpcpy(out_ptr, data_ptr);
            data_ptr += tmp - out_ptr + 1;
            out_ptr = tmp;
        }
        return out_ptr + 1;
    }

    size_t rank(std::string_view s) const {
        auto block = block_containing_string(s);
        return info[block].count + block_rank(s, block);
    }

    size_t rank(std::string_view s, size_t block) const {
        return info[block].count + block_rank(s, block);
    }

    HeaderIterator headers_begin() const { return {headers.data(), 0, info.data()}; }
    HeaderIterator headers_end() const { return {headers.data(), blocks_count(), info.data()}; }

private:

    size_t block_containing_position(size_t i) const {
        auto it = std::upper_bound(info.begin(), info.end(), i, [](auto &a, auto &b) { return a < b.count; });
        return std::distance(info.begin(), std::prev(it));
    }

    static std::pair<int, size_t> strcmp_lcp(const char *s1, size_t len1, const char *s2) {
        size_t i = 0;
        auto w1 = (const uint64_t *) s1;
        auto w2 = (const uint64_t *) s2;
        while (i + 8 < len1 && *w1++ == *w2++)
            i += 8;

        while (true) {
            unsigned char u1 = s1[i];
            unsigned char u2 = s2[i];
            if (u1 != u2)
                return {u1 - u2, i};
            if (i == len1)
                return {0, i};
            ++i;
        }
    }

    static size_t lcp64(const char *s1, size_t len1, const char *s2) {
        size_t i = 0;
        auto w1 = (const uint64_t *) s1;
        auto w2 = (const uint64_t *) s2;
        while (i + 8 < len1 && *w1++ == *w2++)
            i += 8;
        while (i != len1 && s1[i] == s2[i])
            ++i;
        return i;
    }

    size_t block_containing_string(std::string_view s) const {
        size_t lo = 0;
        size_t hi = blocks_count();
        size_t count = hi - lo;
        size_t llcp = 0;
        size_t rlcp = 0;
        while (count > 0) {
            auto step = count / 2;
            auto i = lo + step;
            __builtin_prefetch(headers.data() + info[lo + step / 2].header_pointer);
            __builtin_prefetch(headers.data() + info[lo + step + step / 2].header_pointer);
            auto min_lcp = std::min(llcp, rlcp);
            auto[cmp_result, lcp] = strcmp_lcp(s.data() + min_lcp, s.length() - min_lcp,
                                               headers.data() + info[i].header_pointer + min_lcp);
            lcp += min_lcp;
            if (cmp_result >= 0) {
                llcp = lcp;
                lo = i + 1;
                count -= step + 1;
            } else {
                rlcp = lcp;
                count = step;
            }
        }
        return lo - (lo != 0);
    }

    size_t block_rank(std::string_view pattern, size_t block) const {
        auto header_ptr = headers.data() + info[block].header_pointer;
        auto pattern_lcp = lcp64(pattern.data(), pattern.length(), header_ptr); // LCP b/w current string and pattern
        if (pattern[pattern_lcp] < header_ptr[pattern_lcp])
            return 0;

        auto data_ptr = data.data() + info[block].data_pointer;
        auto strings_in_block = info[block + 1].count - info[block].count;
        auto curr_length = pattern_lcp + strlen(header_ptr + pattern_lcp); // Length of the current string
        //for (auto i = 0; i <= block_bytes; i += 64)
        //    __builtin_prefetch(data_ptr + i);

        for (int j = 1; j < strings_in_block; ++j) {
            auto suffix_to_remove = decode_int(data_ptr);
            auto prev_string_lcp = curr_length - suffix_to_remove; // LCP b/w curr and previous string in the block
            if (prev_string_lcp < pattern_lcp)
                return j;

            if (prev_string_lcp == pattern_lcp) {
                auto lcp = lcp64(pattern.data() + prev_string_lcp, pattern.length() - prev_string_lcp, data_ptr);
                pattern_lcp += lcp;
                if (pattern[pattern_lcp] < data_ptr[lcp])
                    return j;
            }

            auto suffix_len = std::strlen(data_ptr);
            data_ptr += suffix_len + 1;
            curr_length = curr_length - suffix_to_remove + suffix_len;
        }

        return strings_in_block;
    }

    static void encode_int(size_t x, uint8_t *&out) {
        while (x > 127) {
            *out++ = uint8_t(x & 127);
            x >>= 7;
        }
        *out++ = uint8_t(x) | 128;
    }

    static void encode_int(size_t x, std::string &out) {
        while (x > 127) {
            out.push_back(uint8_t(x) & 127);
            x >>= 7;
        }
        out.push_back(uint8_t(x) | 128);
    }

    static size_t decode_int(const char *&in) {
        size_t result = 0;
        uint8_t shift = 0;
        while (*in >= 0) {
            result |= size_t(*in) << shift;
            ++in;
            shift += 7;
        }
        result |= size_t(*in & 127) << shift;
        ++in;
        return result;
    }

    #pragma pack(push, 1)
    struct BlockInfo {
        uint32_t count;          ///< Cumulative string count up to this block
        uint32_t data_pointer;   ///< Pointer to the data block
        uint32_t header_pointer; ///< Pointer to the header string

        BlockInfo(uint32_t count, uint32_t data_pointer, uint32_t header_pointer)
            : count(count), data_pointer(data_pointer), header_pointer(header_pointer) {}
    };
    #pragma pack(pop)

    class HeaderIterator {
        const char *headers_ptr;
        size_t block;
        const BlockInfo *block_info_ptr;

    public:
        using iterator_category = std::random_access_iterator_tag;
        using value_type = const char *;
        using difference_type = std::make_signed_t<size_t>;
        using pointer = value_type *;
        using reference = const value_type &;

        HeaderIterator(const char *ptr, size_t block, const BlockInfo *block_info_ptr)
            : headers_ptr(ptr), block(block), block_info_ptr(block_info_ptr) {}

        value_type operator*() const { return headers_ptr + block_info_ptr[block].header_pointer; }

        value_type operator[](difference_type off) const {
            return headers_ptr + block_info_ptr[block + off].header_pointer;
        }

        HeaderIterator &operator++() {
            ++block;
            return *this;
        }

        HeaderIterator operator++(int) {
            auto b = block;
            ++*this;
            return HeaderIterator(headers_ptr, b, block_info_ptr);
        }

        HeaderIterator &operator--() {
            --block;
            return *this;
        }

        HeaderIterator operator--(int) {
            auto b = block;
            --*this;
            return HeaderIterator(headers_ptr, b, block_info_ptr);
        }

        HeaderIterator &operator+=(difference_type off) {
            block += off;
            return *this;
        }

        HeaderIterator operator+(difference_type off) const {
            return HeaderIterator(headers_ptr, block + off, block_info_ptr);
        }

        HeaderIterator &operator-=(difference_type off) {
            block -= off;
            return *this;
        }

        HeaderIterator operator-(difference_type off) const {
            return HeaderIterator(headers_ptr, block - off, block_info_ptr);
        }

        difference_type operator-(const HeaderIterator &right) const { return difference_type(block - right.block); }

        bool operator==(const HeaderIterator &r) const { return block == r.block; }
        bool operator!=(const HeaderIterator &r) const { return block != r.block; }
        bool operator<(const HeaderIterator &r) const { return block < r.block; }
        bool operator<=(const HeaderIterator &r) const { return block <= r.block; }
        bool operator>(const HeaderIterator &r) const { return block > r.block; }
        bool operator>=(const HeaderIterator &r) const { return block >= r.block; }
    };
};


