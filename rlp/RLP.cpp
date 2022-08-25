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


#include "SkaleCommon.h"
#include "Log.h"

#include "RLP.h"

using namespace std;
vector<uint8_t> RLPNull = rlp("");
vector<uint8_t> RLPEmptyList = rlpList();


namespace {

/// Determine bytes required to encode the given integer value. @returns 0 if @a _i is zero.
    template<class T>
    inline unsigned bytesRequired(T _i) {
        static_assert(is_same<bigint, T>::value || !numeric_limits<T>::is_signed,
                      "only unsigned types or bigint supported");  // bigint does not carry sign bit on shift
        unsigned i = 0;
        for (; _i != 0; ++i, _i >>= 8) {
        }
        return i;
    }


    string constructRLPSizeErrorInfo(size_t _actualSize, size_t _dataSize) {
        stringstream s;
        s << "Actual size: " << _actualSize << ", data size: " << _dataSize;
        return s.str();
    }

}  // namespace

RLP::RLP(vector_ref<uint8_t const> _d, int _s) : m_data(_d) {
    if ((_s & FailIfTooBig) && actualSize() < _d.size()) {
        CHECK_STATE2(!(_s & ThrowOnFail), constructRLPSizeErrorInfo(actualSize(), _d.size()));
        m_data.reset();
    }
    if ((_s & FailIfTooSmall) && actualSize() > _d.size()) {
        CHECK_STATE2(!(_s & ThrowOnFail), constructRLPSizeErrorInfo(actualSize(), _d.size()));
        m_data.reset();
    }
}

RLP::iterator &RLP::iterator::operator++() {
    if (m_remaining) {
        m_currentItem.retarget(m_currentItem.next().data(), m_remaining);
        m_currentItem = m_currentItem.cropped(0, sizeAsEncoded(m_currentItem));
        m_remaining -= min<size_t>(m_remaining, m_currentItem.size());
    } else
        m_currentItem.retarget(m_currentItem.next().data(), 0);
    return *this;
}

RLP::iterator::iterator(RLP const &_parent, bool _begin) {
    if (_begin && _parent.isList()) {
        auto pl = _parent.payload();
        m_currentItem = pl.cropped(0, sizeAsEncoded(pl));
        m_remaining = pl.size() - m_currentItem.size();
    } else {
        m_currentItem = _parent.data().cropped(_parent.data().size());
        m_remaining = 0;
    }
}

RLP RLP::operator[](size_t _i) const {
    if (_i < m_lastIndex) {
        m_lastEnd = sizeAsEncoded(payload());
        m_lastItem = payload().cropped(0, m_lastEnd);
        m_lastIndex = 0;
    }
    for (; m_lastIndex < _i && m_lastItem.size(); ++m_lastIndex) {
        m_lastItem = payload().cropped(m_lastEnd);
        m_lastItem = m_lastItem.cropped(0, sizeAsEncoded(m_lastItem));
        m_lastEnd += m_lastItem.size();
    }
    return RLP(m_lastItem, ThrowOnFail | FailIfTooSmall);
}

vector<RLP> RLP::toList(int _flags) const {
    vector<RLP> ret;
    if (!isList()) {
        CHECK_STATE2(!(_flags & ThrowOnFail), "BadCast");
        return ret;
    }
    for (auto const &i: *this)
        ret.push_back(i);
    return ret;
}

size_t RLP::actualSize() const {
    if (isNull())
        return 0;
    if (isSingleByte())
        return 1;
    if (isData() || isList())
        return payloadOffset() + length();
    return 0;
}

void RLP::requireGood() const {
    CHECK_STATE2(!(isNull()), "BadRLP");
    uint8_t n = m_data[0];
    if (n != RLP_DATA_IMM_LEN_START + 1)
        return;
    CHECK_STATE2(m_data.size() >= 2, "BadRLP");
    CHECK_STATE2(m_data[1] >= RLP_DATA_IMM_LEN_START, "BadRLP");
}

bool RLP::isInt() const {
    if (isNull())
        return false;
    requireGood();
    uint8_t n = m_data[0];
    if (n < RLP_DATA_IMM_LEN_START)
        return !!n;
    else if (n == RLP_DATA_IMM_LEN_START)
        return true;
    else if (n <= RLP_DATA_IND_LEN_ZERo) {
        CHECK_STATE2(!(m_data.size() <= 1), "BadRLP");
        return m_data[1] != 0;
    } else if (n < RLP_LIST_START) {
        CHECK_STATE2(!(m_data.size() <= size_t(1 + n - RLP_DATA_IND_LEN_ZERo)), "BadRLP");
        return m_data[1 + n - RLP_DATA_IND_LEN_ZERo] != 0;
    } else
        return false;
    return false;
}

size_t RLP::length() const {
    if (isNull())
        return 0;
    requireGood();
    size_t ret = 0;
    uint8_t const n = m_data[0];
    if (n < RLP_DATA_IMM_LEN_START)
        return 1;
    else if (n <= RLP_DATA_IND_LEN_ZERo)
        return n - RLP_DATA_IMM_LEN_START;
    else if (n < RLP_LIST_START) {
        CHECK_STATE2(!(m_data.size() <= size_t(n - RLP_DATA_IND_LEN_ZERo)), "BadRLP");
        if (m_data.size() > 1) {
            CHECK_STATE2(!(m_data[1] == 0), "BadRLP");
        }
        unsigned lengthSize = n - RLP_DATA_IND_LEN_ZERo;
        // We did not check, but would most probably not fit in our memory.
        CHECK_STATE2 (!(lengthSize > sizeof(ret)), "UndersizeRLP");
        // No leading zeroes.
        CHECK_STATE2 (m_data[1], "BadRLP");
        for (unsigned i = 0; i < lengthSize; ++i)
            ret = (ret << 8) | m_data[i + 1];
        // Must be greater than the limit.
        CHECK_STATE2 (!(ret < RLP_LIST_START - RLP_DATA_IMM_LEN_START - RLP_MAX_LENGTH_BYTES), "BadRLP");
    } else if (n <= RLP_LIST_IND_LEN_ZERO)
        return n - RLP_LIST_START;
    else {
        unsigned lengthSize = n - RLP_LIST_IND_LEN_ZERO;
        CHECK_STATE2 (!(m_data.size() <= lengthSize), "BadRLP");
        if (m_data.size() > 1) {
            CHECK_STATE2 (!(m_data[1] == 0), "BadRLP");
        }
        CHECK_STATE2 (!(lengthSize > sizeof(ret)), "UndersizeRLP");
        CHECK_STATE2 (m_data[1], "BadRLP");
        for (unsigned i = 0; i < lengthSize; ++i)
            ret = (ret << 8) | m_data[i + 1];
        CHECK_STATE2 (!(ret < 0x100 - RLP_LIST_START - RLP_MAX_LENGTH_BYTES), "BadRLP");
    }
    // We have to be able to add payloadOffset to length without overflow.
    // This rejects roughly 4GB-sized vector<RLP> on some platforms.
    CHECK_STATE2 (!(ret >= numeric_limits<size_t>::max() - 0x100), "UndersizeRLP");
    return ret;
}

size_t RLP::items() const {
    if (isList()) {
        vector_ref<uint8_t const> d = payload();
        size_t i = 0;
        for (; d.size(); ++i)
            d = d.cropped(sizeAsEncoded(d));
        return i;
    }
    return 0;
}

RLPOutputStream & RLPOutputStream::appendRaw(vector_ref<uint8_t const> _s, size_t _itemCount) {
    m_out.insert(m_out.end(), _s.begin(), _s.end());
    noteAppended(_itemCount);
    return *this;
}

void RLPOutputStream::noteAppended(size_t _itemCount) {
    if (!_itemCount)
        return;
    //	cdebug << "noteAppended(" << _itemCount << ")";
    while (m_listStack.size()) {
        CHECK_STATE2(!(m_listStack.back().first < _itemCount), "RLP  :itemCount too large");
        m_listStack.back().first -= _itemCount;
        if (m_listStack.back().first)
            break;
        else {
            auto p = m_listStack.back().second;
            m_listStack.pop_back();
            size_t s = m_out.size() - p;  // list size
            auto brs = bytesRequired(s);
            unsigned encodeSize = s < RLP_LIST_IMM_LEN_COUNT ? 1 : (1 + brs);
            auto os = m_out.size();
            m_out.resize(os + encodeSize);
            memmove(m_out.data() + p + encodeSize, m_out.data() + p, os - p);
            if (s < RLP_LIST_IMM_LEN_COUNT)
                m_out[p] = (uint8_t) (RLP_LIST_START + s);
            else if (RLP_LIST_IND_LEN_ZERO + brs <= 0xff) {
                m_out[p] = (uint8_t) (RLP_LIST_IND_LEN_ZERO + brs);
                uint8_t *b = &(m_out[p + brs]);
                for (; s; s >>= 8)
                    *(b--) = (uint8_t) s;
            } else
                BOOST_THROW_EXCEPTION(InvalidStateException("RLP itemCount too large for RLP",
                                                            __CLASS_NAME__));
        }
        _itemCount = 1;  // for all following iterations, we've effectively appended a single item
        // only since we completed a list.
    }
}

RLPOutputStream &RLPOutputStream::appendList(size_t _items) {
    //	cdebug << "appendList(" << _items << ")";
    if (_items)
        m_listStack.push_back(make_pair(_items, m_out.size()));
    else
        appendList(vector<uint8_t>());
    return *this;
}

RLPOutputStream &RLPOutputStream::appendList(vector_ref<uint8_t const> _rlp) {
    if (_rlp.size() < RLP_LIST_IMM_LEN_COUNT)
        m_out.push_back((uint8_t) (_rlp.size() + RLP_LIST_START));
    else
        pushCount(_rlp.size(), RLP_LIST_IND_LEN_ZERO);
    appendRaw(_rlp, 1);
    return *this;
}

RLPOutputStream &RLPOutputStream::append(vector_ref<uint8_t const> _s, bool _compact) {
    size_t s = _s.size();
    uint8_t const *d = _s.data();
    if (_compact)
        for (size_t i = 0; i < _s.size() && !*d; ++i, --s, ++d) {
        }

    if (s == 1 && *d < RLP_DATA_IMM_LEN_START)
        m_out.push_back(*d);
    else {
        if (s < RLP_DATA_IMM_LEN_COUNT)
            m_out.push_back((uint8_t) (s + RLP_DATA_IMM_LEN_START));
        else
            pushCount(s, RLP_DATA_IND_LEN_ZERo);
        appendRaw(vector_ref<uint8_t const>(d, s), 0);
    }
    noteAppended();
    return *this;
}

RLPOutputStream &RLPOutputStream::append(bigint _i) {
    if (!_i)
        m_out.push_back(RLP_DATA_IMM_LEN_START);
    else if (_i < RLP_DATA_IMM_LEN_START)
        m_out.push_back((uint8_t) _i);
    else {
        unsigned br = bytesRequired(_i);
        if (br < RLP_DATA_IMM_LEN_COUNT)
            m_out.push_back((uint8_t) (br + RLP_DATA_IMM_LEN_START));
        else {
            auto brbr = bytesRequired(br);
            CHECK_STATE2(!(RLP_DATA_IND_LEN_ZERo + brbr > 0xff), "RLP exception Number too large for RLP");
            m_out.push_back((uint8_t) (RLP_DATA_IND_LEN_ZERo + brbr));
            pushInt(br, brbr);
        }
        pushInt(_i, br);
    }
    noteAppended();
    return *this;
}

void RLPOutputStream::pushCount(size_t _count, uint8_t _base) {
    auto br = bytesRequired(_count);
    CHECK_STATE2(int( br ) +_base <= 0xff, "Count too large for RLP");
    m_out.push_back((uint8_t) (br + _base));  // max 8 bytes.
    pushInt(_count, br);
}

string escaped( string const& _s, bool _all ) {
    static const map< char, char > prettyEscapes{ { '\r', 'r' }, { '\n', 'n' }, { '\t', 't' },
                                                  { '\v', 'v' } };
    string ret;
    ret.reserve( _s.size() + 2 );
    ret.push_back( '"' );
    for ( auto i : _s )
        if ( i == '"' && !_all )
            ret += "\\\"";
        else if ( i == '\\' && !_all )
            ret += "\\\\";
        else if ( prettyEscapes.count( i ) && !_all ) {
            ret += '\\';
            ret += prettyEscapes.find( i )->second;
        } else if ( i < ' ' || _all ) {
            ret += "\\x";
            ret.push_back( "0123456789abcdef"[( uint8_t ) i / 16] );
            ret.push_back( "0123456789abcdef"[( uint8_t ) i % 16] );
        } else
            ret.push_back( i );
    ret.push_back( '"' );
    return ret;
}

static void streamOut(ostream &_out, RLP const &_d, unsigned _depth = 0) {
    if (_depth > 64)
        _out << "<max-depth-reached>";
    else if (_d.isNull())
        _out << "null";
    else if (_d.isInt())
        _out << showbase << hex << nouppercase
             << _d.toInt<bigint>(RLP::LaissezFaire) << dec;
    else if (_d.isData())
        _out << escaped(_d.toString(), false);
    else if (_d.isList()) {
        _out << "[";
        int j = 0;
        for (auto i: _d) {
            _out << (j++ ? ", " : " ");
            streamOut(_out, i, _depth + 1);
        }
        _out << " ]";
    }
}

ostream &operator<<(ostream &_out, RLP const &_d) {
    streamOut(_out, _d);
    return _out;
}
