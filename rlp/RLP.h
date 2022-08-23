/*
    Copyright (C) 2018-2019 SKALE Labs

    This file is part of skale-consensus.

    skale-consensus is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as published
    by the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    skale-consensus is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with skale-consensus.  If not, see <https://www.gnu.org/licenses/>.

    @file RLP.h
    @author Stan Kladko
    @date 2018
*/


#pragma once

//#include "Exceptions.h"
#include "vector_ref.h"
#include <array>
//#include <exception>
//#include <iomanip>
#include <iosfwd>
#include <vector>

class RLP;

// Numeric types.
using bigint = boost::multiprecision::number< boost::multiprecision::cpp_int_backend<> >;
using u64 = boost::multiprecision::number< boost::multiprecision::cpp_int_backend< 64, 64,
        boost::multiprecision::unsigned_magnitude, boost::multiprecision::unchecked, void > >;
using u256 = boost::multiprecision::number< boost::multiprecision::cpp_int_backend< 256, 256,
        boost::multiprecision::unsigned_magnitude, boost::multiprecision::unchecked, void > >;
using u160 = boost::multiprecision::number< boost::multiprecision::cpp_int_backend< 160, 160,
        boost::multiprecision::unsigned_magnitude, boost::multiprecision::unchecked, void > >;


// String types.
using strings = vector< string >;



template<class _T>
struct intTraits {
    static const unsigned maxSize = sizeof(_T);
};
template<>
struct intTraits<u160> {
    static const unsigned maxSize = 20;
};
template<>
struct intTraits<u256> {
    static const unsigned maxSize = 32;
};
template<>
struct intTraits<bigint> {
    static const unsigned maxSize = ~(unsigned) 0;
};

static const uint8_t c_rlpMaxLengthBytes = 8;
static const uint8_t c_rlpDataImmLenStart = 0x80;
static const uint8_t c_rlpListStart = 0xc0;

static const uint8_t c_rlpDataImmLenCount =
        c_rlpListStart - c_rlpDataImmLenStart - c_rlpMaxLengthBytes;
static const uint8_t c_rlpDataIndLenZero = c_rlpDataImmLenStart + c_rlpDataImmLenCount - 1;
static const uint8_t c_rlpListImmLenCount = 256 - c_rlpListStart - c_rlpMaxLengthBytes;
static const uint8_t c_rlpListIndLenZero = c_rlpListStart + c_rlpListImmLenCount - 1;

template<class T>
struct Converter {
    static T convert(RLP const &, int) { BOOST_THROW_EXCEPTION(InvalidArgumentException("BadCast", "")); }
};



/// Converts a big-endian byte-stream represented on a templated collection to a templated integer
/// value.
/// @a _In will typically be either string or bytes.
/// @a T will typically by unsigned, u160, u256 or bigint.
template < class T, class _In >
inline T fromBigEndian( _In const& _bytes ) {
    T ret = ( T ) 0;
    for ( auto i : _bytes )
        ret = ( T )(
                ( ret << 8 ) | ( uint8_t )( typename make_unsigned< decltype( i ) >::type ) i );
    return ret;
}


class RLP {
public:
    /// Conversion flags
    enum {
        AllowNonCanon = 1,
        ThrowOnFail = 4,
        FailIfTooBig = 8,
        FailIfTooSmall = 16,
        Strict = ThrowOnFail | FailIfTooBig,
        VeryStrict = ThrowOnFail | FailIfTooBig | FailIfTooSmall,
        LaissezFaire = AllowNonCanon
    };


    /// Construct a null node.
    RLP() {}

    /// Construct a node of value given in the bytes.
    explicit RLP(vector_ref<uint8_t const> _d, int _s = VeryStrict);

    /// Construct a node of value given in the bytes.
    explicit RLP(vector<uint8_t> const &_d, int _s = VeryStrict) : RLP(&_d, _s) {}

    /// Construct a node to read RLP data in the bytes given.
    RLP(uint8_t const *_b, unsigned _s, int _st = VeryStrict)
            : RLP(vector_ref<uint8_t const>(_b, _s), _st) {}

    /// Construct a node to read RLP data in the string.
    explicit RLP(string const &_s, int _st = VeryStrict)
            : RLP(vector_ref<uint8_t const>((uint8_t const *) _s.data(), _s.size()), _st) {}

    /// The bare data of the RLP.
    vector_ref<uint8_t const> data() const { return m_data; }

    /// @returns true if the RLP is non-null.
    explicit operator bool() const { return !isNull(); }

    /// No value.
    bool isNull() const { return m_data.size() == 0; }

    /// Contains a zero-length string or zero-length list.
    bool isEmpty() const {
        return !isNull() && (m_data[0] == c_rlpDataImmLenStart || m_data[0] == c_rlpListStart);
    }

    /// String value.
    bool isData() const { return !isNull() && m_data[0] < c_rlpListStart; }

    /// List value.
    bool isList() const { return !isNull() && m_data[0] >= c_rlpListStart; }

    /// Integer value. Must not have a leading zero.
    bool isInt() const;

    /// @returns the number of items in the list, or zero if it isn't a list.
    size_t itemCount() const { return isList() ? items() : 0; }

    size_t itemCountStrict() const {
        CHECK_STATE2(isList(), "BadCast");
        return items();
    }

    /// @returns the number of bytes in the data, or zero if it isn't data.
    size_t size() const { return isData() ? length() : 0; }

    size_t sizeStrict() const {
        CHECK_STATE2(!isData(), "BadCast");
        return length();
    }

    /// Converts to string. @returns the empty string if not a string.
    string toString(int _flags = LaissezFaire) const {
        if (!isData()) {
            CHECK_STATE2(!(_flags & ThrowOnFail), "BadCast");
            return string();
        }
        return payload().cropped(0, length()).toString();
    }

    /// Equality operators; does best-effort conversion and checks for equality.
    bool operator==(char const *_s) const { return isData() && toString() == _s; }

    bool operator!=(char const *_s) const { return isData() && toString() != _s; }

    bool operator==(string const &_s) const { return isData() && toString() == _s; }

    bool operator!=(string const &_s) const { return isData() && toString() != _s; }

    bool operator==(unsigned const &_i) const { return isInt() && toInt<unsigned>() == _i; }

    bool operator!=(unsigned const &_i) const { return isInt() && toInt<unsigned>() != _i; }

    bool operator==(u256 const &_i) const { return isInt() && toInt<u256>() == _i; }

    bool operator!=(u256 const &_i) const { return isInt() && toInt<u256>() != _i; }

    /// Converts to int of type given; if isData(), decodes as big-endian bytestream. @returns 0 if
    /// not an int or data.
    template<class _T = unsigned>
    _T toInt(int _flags = Strict) const {
        requireGood();
        if ((!isInt()) || isList() || isNull()) {
            CHECK_STATE2(!(_flags & ThrowOnFail), "BadCast");
            return 0;
        }

        auto p = payload();
        if (p.size() > intTraits<_T>::maxSize && (_flags & FailIfTooBig)) {
            CHECK_STATE2 (!(_flags & ThrowOnFail), "BadCast");
            return 0;
        }

        return fromBigEndian<_T>(p);
    }

    bool operator==(bigint const &_i) const { return isInt() && toInt<bigint>() == _i; }

    bool operator!=(bigint const &_i) const { return isInt() && toInt<bigint>() != _i; }

    /// Subscript operator.
    /// @returns the list item @a _i if isList() and @a _i < listItems(), or RLP() otherwise.
    /// @note if used to access items in ascending order, this is efficient.
    RLP operator[](size_t _i) const;

    using element_type = RLP;

    /// @brief Iterator class for iterating through items of RLP list.
    class iterator {
        friend class RLP;

    public:
        using value_type = RLP;
        using element_type = RLP;

        iterator &operator++();

        iterator operator++(int) {
            auto ret = *this;
            operator++();
            return ret;
        }

        RLP operator*() const { return RLP(m_currentItem); }

        bool operator==(iterator const &_cmp) const {
            return m_currentItem == _cmp.m_currentItem;
        }

        bool operator!=(iterator const &_cmp) const { return !operator==(_cmp); }

    private:
        iterator() {}

        iterator(RLP const &_parent, bool _begin);

        size_t m_remaining = 0;
        vector_ref<uint8_t const> m_currentItem;
    };

    /// @brief Iterator into beginning of sub-item list (valid only if we are a list).
    iterator begin() const { return iterator(*this, true); }

    /// @brief Iterator into end of sub-item list (valid only if we are a list).
    iterator end() const { return iterator(*this, false); }

    template<class T>
    inline T convert(int _flags) const;


    template<class T, class U>
    explicit operator pair<T, U>() const {
        return toPair<T, U>();
    }

    template<class T>
    explicit operator vector<T>() const {
        return toVector<T>();
    }

    template<class T>
    explicit operator set<T>() const {
        return toSet<T>();
    }

    template<class T, size_t N>
    explicit operator array<T, N>() const {
        return toArray<T, N>();
    }

    /// Converts to bytearray. @returns the empty byte array if not a string.
    vector<uint8_t> toBytes(int _flags = LaissezFaire) const {
        if (!isData()) {
            CHECK_STATE2 (!(_flags & ThrowOnFail), "BadCast");
            return vector<uint8_t>();
        }
        return vector<uint8_t>(payload().data(), payload().data() + length());
    }

    /// Converts to bytearray. @returns the empty byte array if not a string.
    vector_ref<uint8_t const> toByteArray(int _flags = LaissezFaire) const {
        if (!isData()) {
            CHECK_STATE2 (!(_flags & ThrowOnFail), "BadCast");
            return vector_ref<uint8_t const>();
        }
        return payload().cropped(0, length());
    }



    /// Converts to string. @throws BadCast if not a string.
    string toStringStrict() const { return toString(Strict); }

    template<class T>
    vector<T> toVector(int _flags = LaissezFaire) const {
        vector<T> ret;
        if (isList()) {
            ret.reserve(itemCount());
            for (auto const &i: *this)
                ret.push_back(i.convert<T>(_flags));
        } else {
            CHECK_STATE2 (!(_flags & ThrowOnFail), "BadCast");
        }
        return ret;
    }

    template<class T>
    set<T> toSet(int _flags = LaissezFaire) const {
        set<T> ret;
        if (isList())
            for (auto const &i: *this)
                ret.insert(i.convert<T>(_flags));
        else {
            CHECK_STATE2 (!(_flags & ThrowOnFail), "BadCast");
        }
        return ret;
    }

    template<class T>
    unordered_set<T> toUnorderedSet(int _flags = LaissezFaire) const {
        unordered_set<T> ret;
        if (isList())
            for (auto const &i: *this)
                ret.insert(i.convert<T>(_flags));
        else {
            CHECK_STATE2 (!(_flags & ThrowOnFail) , "BadCast");
        }
        return ret;
    }

    template<class T, class U>
    pair<T, U> toPair(int _flags = Strict) const {
        pair<T, U> ret;
        if (itemCountStrict() != 2) {
            CHECK_STATE2 (!(_flags & ThrowOnFail), "BadCast");
            return ret;
        }
        ret.first = (*this)[0].convert<T>(_flags);
        ret.second = (*this)[1].convert<U>(_flags);
        return ret;
    }

    template<class T, size_t N>
    array<T, N> toArray(int _flags = LaissezFaire) const {
        if (itemCountStrict() != N) {
            CHECK_STATE2 (!(_flags & ThrowOnFail),"BadCast");
            return array<T, N>();
        }
        array<T, N> ret;
        for (size_t i = 0; i < N; ++i)
            ret[i] = operator[](i).convert<T>(_flags);
        return ret;
    }



    int64_t toPositiveInt64(int _flags = Strict) const {
        int64_t i = toInt<int64_t>(_flags);
        CHECK_STATE2 (!((_flags & ThrowOnFail) && i < 0), "BadCast");
        return i;
    }

    template<class _N>
    _N toHash(int _flags = Strict) const {
        requireGood();
        auto p = payload();
        auto l = p.size();
        if (!isData() || (l > _N::size && (_flags & FailIfTooBig)) ||
            (l < _N::size && (_flags & FailIfTooSmall))) {
            CHECK_STATE2 (!(_flags & ThrowOnFail),"BadCast");
            return _N();
        }

        _N ret;
        size_t s = min<size_t>(_N::size, l);
        memcpy(ret.data() + _N::size - s, p.data(), s);
        return ret;
    }

    /// Converts to RLPs collection object. Useful if you need random access to sub items or will
    /// iterate over multiple times.
    vector<RLP> toList(int _flags = Strict) const;

    /// @returns the data payload. Valid for all types.
    vector_ref<uint8_t const> payload() const {
        auto l = length();
        CHECK_STATE2 (!(l > m_data.size()),"BadRLP");
        return m_data.cropped(payloadOffset(), l);
    }

    /// @returns the theoretical size of this item as encoded in the data.
    /// @note Under normal circumstances, is equivalent to m_data.size() - use that unless you know
    /// it won't work.
    size_t actualSize() const;

private:
    /// Disable construction from rvalue
    explicit RLP(vector<uint8_t> const &&) {}

    /// Throws if is non-canonical data (i.e. single byte done in two bytes that could be done in
    /// one).
    void requireGood() const;

    /// Single-byte data payload.
    bool isSingleByte() const { return !isNull() && m_data[0] < c_rlpDataImmLenStart; }

    /// @returns the amount of bytes used to encode the length of the data. Valid for all types.
    unsigned lengthSize() const {
        if (isData() && m_data[0] > c_rlpDataIndLenZero)
            return m_data[0] - c_rlpDataIndLenZero;
        if (isList() && m_data[0] > c_rlpListIndLenZero)
            return m_data[0] - c_rlpListIndLenZero;
        return 0;
    }

    /// @returns the size in bytes of the payload, as given by the RLP as opposed to as inferred
    /// from m_data.
    size_t length() const;

    /// @returns the number of bytes into the data that the payload starts.
    size_t payloadOffset() const { return isSingleByte() ? 0 : (1 + lengthSize()); }

    /// @returns the number of data items.
    size_t items() const;

    /// @returns the size encoded into the RLP in @a _data and throws if _data is too short.
    static size_t sizeAsEncoded(vector_ref<uint8_t const> _data) {
        return RLP(_data, ThrowOnFail | FailIfTooSmall).actualSize();
    }

    /// Our byte data.
    vector_ref<uint8_t const> m_data;

    /// The list-indexing cache.
    mutable size_t m_lastIndex = (size_t) -1;
    mutable size_t m_lastEnd = 0;
    mutable vector_ref<uint8_t const> m_lastItem;
};


/**
 * @brief Class for writing to an RLP bytestream.
 */
class RLPStream {
public:
    /// Initializes empty RLPStream.
    RLPStream() {}

    /// Initializes the RLPStream as a list of @a _listItems items.
    explicit RLPStream(size_t _listItems) { appendList(_listItems); }

    ~RLPStream() {}

    /// Append given datum to the byte stream.
    RLPStream &append(unsigned long _s) { return append(bigint(_s)); }

    RLPStream &append(u160 _s) { return append(bigint(_s)); }

    RLPStream &append(u256 _s) { return append(bigint(_s)); }

    RLPStream &append(bigint _s);

    RLPStream &append(vector_ref<uint8_t const> _s, bool _compact = false);

    RLPStream &append(vector<uint8_t> const &_s) { return append(vector_ref<uint8_t const>(&_s)); }

    RLPStream &append(string const &_s) { return append(vector_ref<uint8_t const>(_s)); }

    RLPStream &append(char const *_s) { return append(string(_s)); }


    /// Appends an arbitrary RLP fragment - this *must* be a single item unless @a _itemCount is
    /// given.
    RLPStream &append(RLP const &_rlp, size_t _itemCount = 1) {
        return appendRaw(_rlp.data(), _itemCount);
    }

    /// Appends a sequence of data to the stream as a list.
    template<class _T>
    RLPStream &append(vector<_T> const &_s) {
        return appendVector(_s);
    }

    template<class _T>
    RLPStream &appendVector(vector<_T> const &_s) {
        appendList(_s.size());
        for (auto const &i: _s)
            append(i);
        return *this;
    }

    template<class _T, size_t S>
    RLPStream &append(array<_T, S> const &_s) {
        appendList(_s.size());
        for (auto const &i: _s)
            append(i);
        return *this;
    }

    template<class _T>
    RLPStream &append(set<_T> const &_s) {
        appendList(_s.size());
        for (auto const &i: _s)
            append(i);
        return *this;
    }

    template<class _T>
    RLPStream &append(unordered_set<_T> const &_s) {
        appendList(_s.size());
        for (auto const &i: _s)
            append(i);
        return *this;
    }

    template<class T, class U>
    RLPStream &append(pair<T, U> const &_s) {
        appendList(2);
        append(_s.first);
        append(_s.second);
        return *this;
    }

    /// Appends a list.
    RLPStream &appendList(size_t _items);

    RLPStream &appendList(vector_ref<uint8_t const> _rlp);

    RLPStream &appendList(vector<uint8_t> const &_rlp) { return appendList(&_rlp); }

    RLPStream &appendList(RLPStream const &_s) { return appendList(&_s.out()); }

    /// Appends raw (pre-serialised) RLP data. Use with caution.
    RLPStream &appendRaw(vector_ref<uint8_t const> _rlp, size_t _itemCount = 1);

    RLPStream &appendRaw(vector<uint8_t> const &_rlp, size_t _itemCount = 1) {
        return appendRaw(&_rlp, _itemCount);
    }

    /// Shift operators for appending data items.
    template<class T>
    RLPStream &operator<<(T _data) {
        return append(_data);
    }

    /// Clear the output stream so far.
    void clear() {
        m_out.clear();
        m_listStack.clear();
    }

    /// Read the byte stream.
    vector<uint8_t> const &out() const {
        CHECK_STATE2 (m_listStack.empty(), "RLPException() listStack is not empty");
        return m_out;
    }

    /// Invalidate the object and steal the output byte stream.
    vector<uint8_t> &&invalidate() {
        CHECK_STATE2(m_listStack.empty(), "RLPException() listStack is not empty");
        return move(m_out);
    }

    /// Swap the contents of the output stream out for some other byte array.
    void swapOut(vector<uint8_t> &_dest) {
        CHECK_STATE2(m_listStack.empty(), "listStack is not empty");
        swap(m_out, _dest);
    }

private:
    void noteAppended(size_t _itemCount = 1);

    /// Push the node-type byte (using @a _base) along with the item count @a _count.
    /// @arg _count is number of characters for strings, data-bytes for ints, or items for lists.
    void pushCount(size_t _count, uint8_t _offset);

    /// Push an integer as a raw big-endian byte-stream.
    template<class _T>
    void pushInt(_T _i, size_t _br) {
        m_out.resize(m_out.size() + _br);
        uint8_t *b = &m_out.back();
        for (; _i; _i >>= 8)
            *(b--) = (uint8_t) _i;
    }

    /// Our output byte stream.
    vector<uint8_t> m_out;

    vector<pair<size_t, size_t> > m_listStack;
};

template<class _T>
void rlpListAux(RLPStream &_out, _T _t) {
    _out << _t;
}

template<class _T, class... _Ts>
void rlpListAux(RLPStream &_out, _T _t, _Ts... _ts) {
    rlpListAux(_out << _t, _ts...);
}

/// Export a single item in RLP format, returning a byte array.
template<class _T>
vector<uint8_t> rlp(_T _t) {
    return (RLPStream() << _t).out();
}

/// Export a list of items in RLP format, returning a byte array.
inline vector<uint8_t> rlpList() {
    return RLPStream(0).out();
}

template<class... _Ts>
vector<uint8_t> rlpList(_Ts... _ts) {
    RLPStream out(sizeof...(_Ts));
    rlpListAux(out, _ts...);
    return out.out();
}

/// The empty string in RLP format.
extern vector<uint8_t> RLPNull;

/// The empty list in RLP format.
extern vector<uint8_t> RLPEmptyList;

/// Human readable version of RLP.
ostream &operator<<(ostream &_out, RLP const &_d);

