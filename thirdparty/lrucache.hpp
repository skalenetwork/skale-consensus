/*
 * File:   lrucache.hpp
 * Author: Alexander Ponomarev
 *
 * Created on June 20, 2013, 5:09 PM
 */


#ifndef _LRUCACHE_HPP_INCLUDED_
#define	_LRUCACHE_HPP_INCLUDED_

#include <any>

#include <unordered_map>
#include <list>
#include <string>
#include <cstddef>
#include <stdexcept>
#include "SkaleCommon.h"

/*
 * Synchronized LRU cache
 */

namespace cache {


    template<typename key_t, typename value_t>
    class lru_cache {

        std::shared_mutex m;

    public:
        typedef typename std::pair<key_t, value_t> key_value_pair_t;
        typedef typename std::list<key_value_pair_t>::iterator list_iterator_t;

        lru_cache(size_t max_size) :
                _max_size(max_size) {
        }


        bool putIfDoesNotExist(const key_t& key, const value_t& value) {
            WRITE_LOCK(m)
            if (!existsUnsafe(key)) {
                putUnsafe( key, value );
                return true;
            } else {
                return false;
            }
        }

        void put(const key_t& key, const value_t& value) {
            WRITE_LOCK(m)
            putUnsafe(key, value);
        }


        const value_t& get(const key_t& key) {

            WRITE_LOCK(m);

            auto it = _cache_items_map.find(key);
            if (it == _cache_items_map.end()) {
                throw std::range_error("There is no such key in cache");
            } else {
                _cache_items_list.splice(_cache_items_list.begin(), _cache_items_list, it->second);
                return it->second->second;
            }
        }


        const std::any getIfExists(const key_t& key) {

            WRITE_LOCK(m);

            auto it = _cache_items_map.find(key);
            if (it == _cache_items_map.end()) {
                return std::any();
            } else {
                _cache_items_list.splice(_cache_items_list.begin(), _cache_items_list, it->second);
                return it->second->second;
            }
        }


        bool exists(const key_t& key)  {
            READ_LOCK(m);
            return existsUnsafe(key);
        }


        size_t size()  {
            READ_LOCK(m);
            return _cache_items_map.size();
        }

    private:
        std::list<key_value_pair_t> _cache_items_list;
        std::unordered_map<key_t, list_iterator_t> _cache_items_map;
        size_t _max_size;

        bool existsUnsafe(const key_t& key)  {
            return _cache_items_map.find(key) != _cache_items_map.end();
        }

        void putUnsafe(const key_t& key, const value_t& value) {

            auto it = _cache_items_map.find(key);
            _cache_items_list.push_front(key_value_pair_t(key, value));
            if (it != _cache_items_map.end()) {
                _cache_items_list.erase(it->second);
                _cache_items_map.erase(it);
            }
            _cache_items_map[key] = _cache_items_list.begin();

            if (_cache_items_map.size() > _max_size) {
                auto last = _cache_items_list.end();
                last--;
                _cache_items_map.erase(last->first);
                _cache_items_list.pop_back();
            }
        }

    };

} // namespace cache

#endif	/* _LRUCACHE_HPP_INCLUDED_ */
