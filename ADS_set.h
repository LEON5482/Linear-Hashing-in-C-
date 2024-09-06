#ifndef ADS_SET_H
#define ADS_SET_H

#include <functional>
#include <stdexcept>
#include <iostream>
#include <algorithm>

template <typename Key, size_t N = 3>
class ADS_set {
public:
    class iterator;
    using value_type = Key;
    using key_type = Key;
    using reference = value_type &;
    using const_reference = const value_type &;
    using size_type = size_t;
    using difference_type = std::ptrdiff_t;
    using const_iterator = iterator;
    using key_compare = std::less<key_type>;
    using key_equal = std::equal_to<key_type>;
    using hasher = std::hash<key_type>;

private:
    size_type bucketSize = N;
    struct Element {
        key_type key;
        bool used{false};
    };

    struct Bucket {
        Element* data;
        Bucket* overflow_bucket;

        Bucket() : data(new Element[N]), overflow_bucket(nullptr) {}

        ~Bucket(){
            delete overflow_bucket;
            delete[] data;
        }
    };

    Bucket** table{nullptr};
    size_type tableSize = 2;
    size_type nextToSplit = 0;
    size_type d = 1;
    bool rehashing = false;
    size_type current_size = 0;

    size_type least_sig(const key_type& key) const {
        size_type index = hasher{}(key) & ((1 << d) - 1);
        if (index < nextToSplit) {
            index = hasher{}(key) & ((1 << (d + 1)) - 1);
        }
        return index;
    }

    bool contains(const key_type& key) const {
        size_type index = least_sig(key);
        Bucket* bucket = table[index];
        while (bucket) {
            for (size_type i = 0; i < bucketSize; ++i) {
                if (bucket->data[i].used && key_equal{}(bucket->data[i].key, key)) {
                    return true;
                }
            }
            bucket = bucket->overflow_bucket;
        }
        return false;
    }

    std::tuple<bool, size_type, Bucket*> contains(const key_type& key, const size_type& index) const {
        Bucket* bucket = table[index];
        while (bucket) {
            for (size_type i = 0; i < bucketSize; ++i) {
                if (bucket->data[i].used && key_equal{}(bucket->data[i].key, key)) {
                    return {true, i, bucket};
                }
            }
            bucket = bucket->overflow_bucket;
        }
        return {false, 0, bucket};
    }

    void add_to_chain(Bucket*& bucket, const key_type& key) {
        while (bucket->overflow_bucket != nullptr) {
            bucket = bucket->overflow_bucket;
        }

        Bucket* new_bucket = new Bucket;
        new_bucket->data[0] = {key, true};
        bucket->overflow_bucket = new_bucket;
    }

    void set_d() {
        size_type lastCycle = 0;
        while (true) {
            size_type fake_d = 1 << lastCycle;
            if (fake_d > tableSize) {
                d = --lastCycle;
                nextToSplit = tableSize - (1 << lastCycle);
                break;
            } else if (fake_d == tableSize) {
                d = lastCycle;
                nextToSplit = 0;
                break;
            }
            ++lastCycle;
        }
    }

    void rehash() {
        if (rehashing) return;
        rehashing = true;
        Bucket* oldBucket = table[nextToSplit];
        table[nextToSplit] = nullptr;

        nextToSplit++;
        size_type f = 1 << d;
        if (nextToSplit >= f) {
            nextToSplit = 0;
            d++;
            Bucket** newTable = new Bucket*[static_cast<size_t>(1<<(d+1))];
            for (size_type i = 0; i < tableSize; ++i) {
                newTable[i] = table[i];
            }

            delete[] table;
            table = newTable;
        }
        table[tableSize] = new Bucket;
        tableSize++;
        Bucket* to_delete = oldBucket;
        while (oldBucket) {
            for (size_type i = 0; i < bucketSize; ++i) {
                if (oldBucket->data[i].used) {
                    size_type newIndex = least_sig(oldBucket->data[i].key);
                    insert_internal(newIndex, oldBucket->data[i].key, table);
                }
            }
            
            oldBucket = oldBucket->overflow_bucket;
            
        }
        delete to_delete;
        rehashing = false;
    }

    void insert_internal(size_type index, const key_type& key, Bucket** table_ref) {
        if (!table_ref[index]) {
            table_ref[index] = new Bucket;
        }
        Bucket* bucket = table_ref[index];
        while (bucket) {
            for (size_type i = 0; i < bucketSize; ++i) {
                if (!bucket->data[i].used) {
                    bucket->data[i] = {key, true};
                    return;
                }
            }
            if (!bucket->overflow_bucket) {
                add_to_chain(bucket, key);
                if (!rehashing) {
                    rehash();
                }
                return;
            }
            bucket = bucket->overflow_bucket;
        }
    }

public:
    ADS_set() : table{new Bucket*[4]} {
        for (size_type i = 0; i < tableSize; i++) {
            table[i] = new Bucket;
        }
    }

    ADS_set(std::initializer_list<key_type> ilist) : ADS_set() {
        for (const auto& key : ilist) {
            insert(key);
        }
    }

    template<typename InputIt>
    ADS_set(InputIt first, InputIt last) : ADS_set() {
        while (first != last) {
            insert(*first++);
        }
    }

    ADS_set(const ADS_set &other) : ADS_set() {
        for (size_type i = 0; i < other.tableSize; ++i) {
            Bucket* bucket = other.table[i];
            while (bucket) {
                for (size_type j = 0; j < bucketSize; ++j) {
                    if (bucket->data[j].used) {
                        insert(bucket->data[j].key);
                    }
                }
                bucket = bucket->overflow_bucket;
            }
        }
        set_d();
    }

    ~ADS_set() {
        for (size_type i = 0; i < tableSize; ++i) {
            delete table[i];
        }
        delete[] table;
    }

    ADS_set &operator=(const ADS_set &other){
        if (this == &other) return *this;

        clear();

        for (size_type i = 0; i < other.tableSize; ++i) {
            Bucket* bucket = other.table[i];
            while (bucket) {
                for (size_type j = 0; j < bucketSize; ++j) {
                    if (bucket->data[j].used) {
                        insert(bucket->data[j].key);
                    }
                }
                bucket = bucket->overflow_bucket;
            }
        }

        set_d();

        return *this;
    }

    ADS_set& operator=(std::initializer_list<key_type> ilist) {
        clear();
        for (const auto& key : ilist) {
            insert(key);
        }
        set_d();
        return *this;
    }

    const_iterator begin() const {
        return const_iterator(table, tableSize, bucketSize);
    }

    const_iterator end() const {
        return const_iterator(table, tableSize, bucketSize, tableSize, 0, nullptr);
    }

    std::pair<iterator, bool> insert(const key_type& key) {
        size_type index = least_sig(key);

        auto cont = contains(key, index);

        if (!(std::get<0>(cont))) {
            
            insert_internal(index, key, table);
            ++current_size;
    index = least_sig(key);
    Bucket* bucket = table[index];

    while (bucket) {
        for (size_type i = 0; i < bucketSize; ++i) {
            if (bucket->data[i].used && key_equal{}(bucket->data[i].key, key)) {
                return {iterator(table, tableSize, bucketSize, index, i, bucket), true};
            }
        }
        bucket = bucket->overflow_bucket;
    }
        } else {

            return {iterator(table, tableSize, bucketSize, index, std::get<1>(cont), std::get<2>(cont)), false};
            
        }
        return {end(), false};
    }

    void dump(std::ostream& o = std::cerr) const {
        o << "\n\nbuckets: " << std::endl;
        for (size_type i = 0; i < tableSize; ++i) {
            o << "Bucket " << i << ": ";
            Bucket* bucket = table[i];
            while (bucket) {
                for (size_type j = 0; j < bucketSize; ++j) {
                    if (bucket->data[j].used) {
                        o << bucket->data[j].key << " ";
                    }
                }
                bucket = bucket->overflow_bucket;
                if (bucket) {
                    o << " -> ";
                }
            }
            o << std::endl;
        }
        o << "d: " << d << "\nNextToSplit: " << nextToSplit << "\nsize: " << size() << "\ntableSize: " << tableSize << std::endl;
    }

    void insert(std::initializer_list<key_type> ilist) {
        for (const auto& key : ilist) {
            insert(key);
        }
    }

    template<typename InputIt>
    void insert(InputIt first, InputIt last) {
        while (first != last) {
            insert(*first++);
        }
    }

    size_type size() const {
        return current_size;
    }

    bool empty() const {
        return size() == 0;
    }

    void clear() {
        for (size_t i = 0; i < tableSize; ++i) {
                    delete table[i];
        }
        delete[] table;
        
        tableSize = 2;
        nextToSplit = 0;
        d = 1;
        current_size = 0;
        table = new Bucket*[4];
        for (size_t i = 0; i < tableSize; ++i) {
            table[i] = new Bucket(); 
        }
    }

    size_type erase(const key_type &key){
        if(!contains(key)){
            return 0;
        }

        size_type index = least_sig(key);
        Bucket* bucket = table[index];
        bool deleted = false;
        while(bucket){
            for(size_type i = 0; i < bucketSize; i++){
                if(bucket->data[i].used && key_equal{}(bucket->data[i].key, key)){
                    bucket->data[i].used = false;
                    deleted = true;
                }
                if(deleted) break;
            }
            if(deleted) break;
            bucket = bucket->overflow_bucket;
        }

        --current_size;
        return 1;
    }

iterator find(const key_type &key) const {
    size_type index = least_sig(key);
    Bucket* bucket = table[index];
    size_type i = 0;

    while (bucket) {
        for (i = 0; i < bucketSize; i++) {
            if (bucket->data[i].used && key_equal{}(bucket->data[i].key, key)) {
                //std::cout << "Found key: " << key << " at index: " << index << ", bucketIndex: " << i << std::endl;
                return iterator(table, tableSize, bucketSize, index, i, bucket);
            }
        }
        bucket = bucket->overflow_bucket;
    }
    
    //std::cout << "Key: " << key << " not found, returning end()" << std::endl;
    return end();
}

    void swap(ADS_set &other) {
        std::swap(table, other.table);
        std::swap(tableSize, other.tableSize);
        std::swap(nextToSplit, other.nextToSplit);
        std::swap(d, other.d);
        std::swap(rehashing, other.rehashing);
        std::swap(current_size, other.current_size);
    }

    size_type count(const key_type& key) const {
        return contains(key) ? 1 : 0;
    }

};


template <typename Key, size_t N>
class ADS_set<Key, N>::iterator {
    Bucket** table;
    size_type tableSize;
    size_type bucketSize;
    size_type tableIndex;
    size_type bucketIndex;
    Bucket* currentBucket;

public:
    using value_type = Key;
    using reference = const value_type &;
    using pointer = const value_type *;
    using iterator_category = std::forward_iterator_tag;
    using difference_type = std::ptrdiff_t;

    explicit iterator(Bucket** table = nullptr, size_type tableSize = 0, size_type bucketSize = 0, size_type tableIndex = 0, size_type bucketIndex = 0, Bucket* currentBucket = nullptr)
        : table(table), tableSize(tableSize), bucketSize(bucketSize), tableIndex(tableIndex), bucketIndex(bucketIndex), currentBucket(currentBucket) {
        if (currentBucket == nullptr) {
            currentBucket = table[tableIndex];
        }
        moveToNextValid();
    }

    reference operator*() const {
        return currentBucket->data[bucketIndex].key;
    }

    pointer operator->() const {
        return &currentBucket->data[bucketIndex].key;
    }

    iterator& operator++() {
        ++bucketIndex;
        moveToNextValid();
        return *this;
    }

    iterator operator++(int) {
        iterator tmp = *this;
        ++(*this);
        return tmp;
    }

    friend bool operator==(const iterator &lhs, const iterator &rhs) {
        return lhs.table == rhs.table &&
               lhs.tableIndex == rhs.tableIndex &&
               lhs.bucketIndex == rhs.bucketIndex &&
               lhs.currentBucket == rhs.currentBucket;
    }

    friend bool operator!=(const iterator &lhs, const iterator &rhs) {
        return !(lhs == rhs);
    }

private:
    void moveToNextValid() {
        while (tableIndex < tableSize) {
            if (currentBucket == nullptr) {
                currentBucket = table[tableIndex];
            }
            while (currentBucket) {
                while (bucketIndex < bucketSize) {
                    if (currentBucket->data[bucketIndex].used) {
                        //std::cout << std::endl << currentBucket->data[bucketIndex].key << std::endl;
                        return;
                    }
                    ++bucketIndex;
                }
                currentBucket = currentBucket->overflow_bucket;
                bucketIndex = 0;
            }
            ++tableIndex;
            currentBucket = (tableIndex < tableSize) ? table[tableIndex] : nullptr;
        }
    }

};

    template <typename Key, size_t N>
    bool operator==(const ADS_set<Key, N>& lhs, const ADS_set<Key, N>& rhs) {
        if (lhs.size() != rhs.size()) {
            return false;
        }

        for (const auto& element : lhs) {
            if (rhs.find(element) == rhs.end()) {
             return false;
            }
        }

        return true;
    }

    template <typename Key, size_t N>
    bool operator!=(const ADS_set<Key, N>& lhs, const ADS_set<Key, N>& rhs) {
        return !(lhs == rhs);
    }

template <typename Key, size_t N>
void swap(ADS_set<Key,N> &lhs, ADS_set<Key,N> &rhs) noexcept {
    lhs.swap(rhs);
}

#endif // ADS_SET_H