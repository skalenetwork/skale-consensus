#pragma once
#include <boost/container/small_vector.hpp>

template< typename T >
using small_vector = typename boost::container::small_vector< T, 1 >;