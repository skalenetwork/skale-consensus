/*
 * Author: Alexander Ponomarev
 * Author: Stan Kladko
 *
 * This is cache that cleans the last recently used items
 * until both the size in total number of elements and the size
 * in bytes are satisfied
 */

#ifndef LRUORDEREDCACHE_MEMORY_CONSTRAINED_HPP_INCLUDED
#define	LRUORDEREDCACHE_MEMORY_CONSTRAINED_HPP_INCLUDED

#include <map>
#include <list>
#include <string>
#include <cstddef>
#include <stdexcept>
#include <any>
#include <memory>

#define CACHE_READ_LOCK( _M_ ) std::shared_lock< std::shared_mutex > _read_lock_( _M_ );
#define CACHE_WRITE_LOCK( _M_ ) std::unique_lock< std::shared_mutex > _write_lock_( _M_ );

namespace cache {

    constexpr size_t MAX_VALUE_SIZE = 1024 * 1024 * 1024; // 1 GiGABYTE

    template<typename key_t, typename value_t>
    class lru_ordered_memory_constrained_cache {

        std::shared_mutex m;

    public:
        typedef typename std::pair<key_t, value_t> key_value_pair_t;
        typedef typename std::list<key_value_pair_t>::iterator list_iterator_t;

        lru_ordered_memory_constrained_cache(size_t max_size, size_t max_bytes) :
                _max_size(max_size), _max_bytes(max_bytes) {
        }

        void put(const key_t& key, const value_t& value, size_t value_size) {
            if (value_size > MAX_VALUE_SIZE) {
                throw std::length_error( "Item size too large:" + std::to_string(value_size));
            }

            CACHE_WRITE_LOCK(m)

            auto it = _cache_items_map.find(key);
            _cache_items_list.push_front(key_value_pair_t(key, value));
            if (it != _cache_items_map.end()) {
                auto item_size = _cache_items_sizes_map.at(key);
                if (item_size > _total_bytes) {
                    throw std::underflow_error("Item size more than total bytes" +
                                                to_string(item_size) + ":" + to_string(_total_bytes));
                }
                _total_bytes -= item_size;
                _cache_items_list.erase(it->second);
                _cache_items_sizes_map.erase(key);
                _cache_items_map.erase(it);
            }
            _cache_items_map[key] = _cache_items_list.begin();
            _cache_items_sizes_map[key] = value_size;
            _total_bytes += value_size;

            while (_cache_items_map.size() > _max_size || _total_bytes > _max_bytes) {
                auto last = _cache_items_list.end();
                last--;
                auto item_size = _cache_items_map.at(last->first);
                if ( item_size > _total_bytes ) {
                    throw std::underflow_error( "Item size more than total bytes" +
                                                to_string( item_size ) + ":" +
                                                to_string( _total_bytes ) );
                }
                _total_bytes -= item_size;
                _cache_items_sizes_map.erase(last->first);
                _cache_items_map.erase(last->first);
                _cache_items_list.pop_back();
            }
        }

        const value_t& get(const key_t& key) {

            CACHE_WRITE_LOCK(m);

            auto it = _cache_items_map.find(key);
            if (it == _cache_items_map.end()) {
                throw std::range_error("There is no such key in cache");
            } else {
                _cache_items_list.splice(_cache_items_list.begin(), _cache_items_list, it->second);
                return it->second->second;
            }
        }

        std::any getIfExists(const key_t& key) {

            CACHE_WRITE_LOCK(m);

            auto it = _cache_items_map.find(key);
            if (it == _cache_items_map.end()) {
                return std::any();
            } else {
                _cache_items_list.splice(_cache_items_list.begin(), _cache_items_list, it->second);
                return it->second->second;
            }
        }


        bool exists(const key_t& key)  {

            CACHE_READ_LOCK(m);

            return _cache_items_map.find(key) != _cache_items_map.end();
        }

        size_t size()  {
            CACHE_READ_LOCK(m);
            return _cache_items_map.size();
        }

        size_t total_bytes()  {
            return _total_bytes;
        }

    private:
        std::list<key_value_pair_t> _cache_items_list;
        std::map<key_t, list_iterator_t> _cache_items_map;
        std::map<key_t, size_t> _cache_items_sizes_map;
        size_t _max_size;
        size_t _max_bytes;
        std::atomic<size_t> _total_bytes = 0;
    };

} // namespace cache

#endif
