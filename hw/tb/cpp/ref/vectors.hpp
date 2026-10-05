#pragma once
#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace vec {

    // Convert u16 two's complement bit pattern -> signed value
    inline int32_t sext16(uint32_t v) {
        return int32_t(int16_t(uint16_t(v)));
    }

    // Read whitespace-separated test vectors
    inline std::vector<int32_t> load(const std::string& path) {
        std::ifstream f(path);
        if (!f) throw std::runtime_error("cannot open " + path);
        std::vector<int32_t> out;
        uint32_t v;
        while (f >> v) {
            if (v > 0xFFFF)
                throw std::runtime_error(path + ": value" + std::to_string(v) +
                                         " is not u16");
            out.push_back(sext16(v));
        }
        if (!f.eof())
            throw std::runtime_error(path + ": parse error after " +
                                        std::to_string(out.size()) + " values");
        return out;
    }

    inline std::vector<int32_t> load_sized(const std:: string& path, size_t expected) {
        auto v = load(path);
        if (v.size() != expected)
            throw std::runtime_error(path + ": " + std::to_string(v.size()) +
                                        " values, expected " + std::to_string(expected));
        return v;
    }
} // namespace vec
