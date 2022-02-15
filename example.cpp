#include <chrono>
#include <fstream>
#include <iostream>
#include <random>
#include <string>

#include "rear_coded_array.separate_headers.hpp"

template<typename F, class V>
size_t query_ns(F f, const V &queries) {
    using timer = std::chrono::high_resolution_clock;
    auto start = timer::now();
    auto cnt = 0;
    for (auto &q: queries)
        cnt += f(q);
    auto stop = timer::now();
    [[maybe_unused]] volatile auto tmp = cnt;
    return std::chrono::duration_cast<std::chrono::nanoseconds>(stop - start).count() / queries.size();
}

std::vector<std::string> read_strings(const std::string &path, size_t limit = -1) {
    auto previous_value = std::ios::sync_with_stdio(false);
    std::vector<std::string> result;
    std::ifstream in(path.c_str());
    std::string str;
    while (std::getline(in, str) && limit-- > 0)
        result.push_back(str);
    std::ios::sync_with_stdio(previous_value);
    return result;
}

int main() {
    std::vector<std::string> data = read_strings("/usr/share/dict/words");
    std::cout << "Read " << data.size() << " lines" << std::endl;
    std::sort(data.begin(), data.end());

    for (auto block_size: {32, 128, 512, 2048}) {
        std::cout << std::string(79, '=') << std::endl;
        RearCodedArray rca(data.begin(), data.end(), block_size);

        // TEST ACCESS AND RANK
        char buffer[1024];
        for (auto i = 0; i < data.size(); ++i) {
            rca.access(i, buffer);
            if (std::string(buffer) != data[i])
                throw std::runtime_error("Mismatch at " + std::to_string(i));
            if (rca.rank(data[i]) != i + 1)
                throw std::runtime_error("Rank mismatch at " + std::to_string(i));
        }

        // MEASURE RANK TIME
        std::vector<std::string> queries;
        std::mt19937 gen;
        std::sample(data.begin(), data.end(), std::back_inserter(queries), 1000000, gen);
        std::shuffle(queries.begin(), queries.end(), gen);
        std::cout << "Rank time (ns)          " << query_ns([&](auto &s) { return rca.rank(s); }, queries) << std::endl;
    }

    return 0;
}
