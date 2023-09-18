#pragma once

#include <algorithm>
#include <cstdlib>
#include <future>
#include <map>
#include <numeric>
#include <random>
#include <string>
#include <vector>
#include <mutex>

using namespace std::string_literals;

template <typename Key, typename Value>
class ConcurrentMap {
public:
    static_assert(std::is_integral_v<Key>, "ConcurrentMap supports only integer keys");

    struct Access {
        std::lock_guard<std::mutex> lock;
        Value& ref_to_value;
    };

    explicit ConcurrentMap(std::size_t bucket_count) : maps_(std::vector<SafeMap>(bucket_count)), size_(bucket_count) {}

    Access operator[](const Key& key) {
    return {std::lock_guard(maps_[key % size_].m_), maps_[key % size_].map_[key]};
    }

    void erase(Key key) {
        std::size_t map_num = key % size_;
        std::lock_guard(maps_[map_num].m_);
        maps_[map_num].map_.erase(key);
    } 

    std::map<Key, Value> BuildOrdinaryMap() {
        std::map<Key, Value> result;
        for (auto& safe_map : maps_) {
            std::lock_guard lock(safe_map.m_);
            for (auto& k_v : safe_map.map_) {
                result.insert(k_v);
            }
        }
        return result;
    }

private:
    struct SafeMap {
        std::mutex m_;
        std::map<Key, Value> map_;
    };
    std::vector<SafeMap> maps_;
    std::size_t size_;
};