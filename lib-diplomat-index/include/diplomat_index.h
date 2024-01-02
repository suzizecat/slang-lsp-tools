#pragma once

#include <vector>
#include <cstdint>

#include <unordered_map>
#include <filesystem>
#include <memory>

namespace diplomat
{

    template<typename IndexedType, typename Key>
    class DiplomatIndex
    {
        std::unordered_map<Key, std::unique_ptr<std::unordered_map<unsigned int, std::vector<IndexedType*> > > > _index;

        bool _valid;

        public:
            void add_element(Key key, unsigned int line, IndexedType* val);
            void refresh_index(Key key);
            void refresh_all_indexes();

    };


    /**
     * @brief Add an element to the global index
     * 
     * @tparam IT Item type
     * @tparam K Key type
     * @param key Key of the index (e.q. file designator)
     * @param line Line of the artefact
     * @param val Artefact to save
     */
    template<typename IT, typename K>
    void DiplomatIndex<IT,K>::add_element(K key, unsigned int line, IT* val)
    {
        if(! _index.contains(key))
            _index[key] =  std::make_unique< std::unordered_map<unsigned int, std::vector<IndexedType*> >();
        _index[key].
    }


    /**
     * @brief Refresh a single index, without reloading others. 
     * 
     * @tparam IT 
     * @tparam K 
     * @param key 
     */
    template<typename IT, typename K>
    void DiplomatIndex<IT,K>::refresh_index(K key)
    {
        _indexes.erase(key);
        if(_data_holder.contains(key))
            _indexes[key] = std::make_unique<IndexType_t>(_data_holder[key].start(),_data_holder[key].end())           
    }

    /**
     * @brief Clear and recompute all indexes
     * 
     * @tparam IT Item type
     * @tparam K Key type
     */
    template<typename IT, typename K>
    void DiplomatIndex<IT,K>::refresh_all_indexes()
    {
        _indexes.clear();

        for(const auto& [file, items] : _data_holder )
        {
            refresh_index(file);
        }

        _valid = true;
    }


};