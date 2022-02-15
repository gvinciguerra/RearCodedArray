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

    std::string data;
    std::vector<size_t> pointers; // TODO: Interleave pointers and counts
    std::vector<uint32_t> counts;
    size_t n;
    char *buffer;

public:

    template<typename InputIt>
    RearCodedArray(InputIt first, InputIt last, size_t block_bytes) : n(0) {
        data.reserve(1 << 20);
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

            auto current_block_bytes = n == 0 ? std::numeric_limits<size_t>::max() : data.size() - pointers.back();
            if (current_block_bytes >= block_bytes) {
                pointers.push_back(data.size());
                data.append(*first);
                data.push_back('\0');
                counts.push_back(n);
                prev = *first;
                continue;
            }

            auto rear_length = prev.length() - lcp;
            encode_int(rear_length, data);
            data.append(*first, lcp);
            data.push_back('\0');
            prev = *first;
        }

        counts.push_back(n);
        data.shrink_to_fit();
        pointers.shrink_to_fit();
        counts.shrink_to_fit();
        buffer = new char[max_length]();

        size_t max_hdr_lcp = 0;
        size_t sum_hdr_lcp = 0;
        for (size_t i = 1; i < pointers.size(); ++i) {
            auto lcp = compute_lcp(data.data() + pointers[i], data.data() + pointers[i - 1]);
            max_hdr_lcp = std::max(max_hdr_lcp, lcp);
            sum_hdr_lcp += lcp;
        }

        std::cout << "Input bytes             " << input_bytes << std::endl
                  << "Input avg length        " << sum_length / double(n) << std::endl
                  << "Input avg LCP           " << sum_lcp / double(n) << ", max " << max_lcp << std::endl
                  << "RC block_bytes          " << block_bytes << std::endl
                  << "RC bytes                " << size_in_bytes() << std::endl
                  << "RC blocks               " << pointers.size() << std::endl
                  << "RC headers avg LCP      " << sum_hdr_lcp / double(pointers.size())
                  << ", max " << max_hdr_lcp << std::endl
                  << "Avg strings per block   " << n / pointers.size() << std::endl;
    }

    ~RearCodedArray() { delete buffer; }

    size_t size_in_bytes() const {
        return data.size() * sizeof(data[0]) + pointers.size() * sizeof(pointers[0]) + sizeof(*this)
            + counts.size() * sizeof(counts[0]);
    }

    char *access(size_t i, char *out) const {
        auto block = block_containing_position(i);
        auto data_ptr = data.data() + pointers[block];
        auto out_ptr = stpcpy(out, data_ptr);
        data_ptr += out_ptr - out + 1;
        for (int j = 1; j <= i - counts[block]; ++j) {
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
        return counts[block] + block_rank(s, block);
    }

    size_t rank(std::string_view s, size_t block) const {
        return counts[block] + block_rank(s, block);
    }

    HeaderIterator headers_begin() const { return {data.data(), 0, pointers.data()}; }
    HeaderIterator headers_end() const { return {data.data(), pointers.size(), pointers.data()}; }

private:

    size_t block_containing_position(size_t i) const {
        auto it = std::prev(std::upper_bound(counts.begin(), counts.end(), i));
        return std::distance(counts.begin(), it);
    }

    size_t block_containing_string(std::string_view s) const {
        size_t lo = 0;
        size_t hi = pointers.size();
        size_t count = hi - lo;
        while (count > 0) {
            auto step = count / 2;
            auto i = lo + step;
            if (std::strcmp(s.data(), data.data() + pointers[i]) >= 0) {
                lo = i + 1;
                count -= step + 1;
            } else
                count = step;
        }
        return lo - (lo != 0);
    }

    size_t block_rank(std::string_view pattern, size_t block) const {
        assert(block < pointers.size());
        auto data_ptr = data.data() + pointers[block];
        auto buffer_end = stpcpy(buffer, data_ptr); // points to the \0 of the decoded string
        data_ptr += buffer_end - buffer + 1;
        auto pattern_lcp = compute_lcp(pattern.data(), buffer); // LCP b/w current string and pattern

        if (pattern[pattern_lcp] < buffer[pattern_lcp])
            return 0;

        auto strings_in_block = counts[block + 1] - counts[block];
        for (int j = 1; j < strings_in_block; ++j) {
            auto rear_length = decode_int(data_ptr);
            auto prev_string_lcp = buffer_end - buffer - rear_length; // LCP b/w curr and previous string in the block
            if (prev_string_lcp < pattern_lcp)
                return j;

            buffer_end -= rear_length;
            auto tmp = stpcpy(buffer_end, data_ptr);
            data_ptr += tmp - buffer_end + 1;
            buffer_end = tmp;
            if (prev_string_lcp == pattern_lcp) {
                pattern_lcp += compute_lcp(pattern.data() + prev_string_lcp, buffer + prev_string_lcp);
                if (pattern[pattern_lcp] < buffer[pattern_lcp])
                    return j;
            }
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

    static size_t decode_int(char const *&in) {
        size_t result = 0;
        uint8_t shift = 0;
        while (!(*in & 128)) {
            result |= (*in & 127) << shift;
            ++in;
            shift += 7;
        }
        result |= (*in & 127) << shift;
        ++in;
        return result;
    }

    class HeaderIterator {
        const char *ptr;
        size_t block;
        const size_t *pointers;

    public:
        using iterator_category = std::random_access_iterator_tag;
        using value_type = const char *;
        using difference_type = std::make_signed_t<size_t>;
        using pointer = value_type *;
        using reference = const value_type &;

        HeaderIterator(const char *ptr, size_t block, const size_t *pointers)
            : ptr(ptr), block(block), pointers(pointers) {}

        value_type operator*() const { return ptr + pointers[block]; }

        value_type operator[](difference_type off) const { return ptr + pointers[block + off]; }

        HeaderIterator &operator++() {
            ++block;
            return *this;
        }

        HeaderIterator operator++(int) {
            auto b = block;
            ++*this;
            return HeaderIterator(ptr, b, pointers);
        }

        HeaderIterator &operator--() {
            --block;
            return *this;
        }

        HeaderIterator operator--(int) {
            auto b = block;
            --*this;
            return HeaderIterator(ptr, b, pointers);
        }

        HeaderIterator &operator+=(difference_type off) {
            block += off;
            return *this;
        }

        HeaderIterator operator+(difference_type off) const { return HeaderIterator(ptr, block + off, pointers); }

        HeaderIterator &operator-=(difference_type off) {
            block -= off;
            return *this;
        }

        HeaderIterator operator-(difference_type off) const { return HeaderIterator(ptr, block - off, pointers); }

        difference_type operator-(const HeaderIterator &right) const { return difference_type(block - right.block); }

        bool operator==(const HeaderIterator &r) const { return block == r.block; }
        bool operator!=(const HeaderIterator &r) const { return block != r.block; }
        bool operator<(const HeaderIterator &r) const { return block < r.block; }
        bool operator<=(const HeaderIterator &r) const { return block <= r.block; }
        bool operator>(const HeaderIterator &r) const { return block > r.block; }
        bool operator>=(const HeaderIterator &r) const { return block >= r.block; }
    };
};