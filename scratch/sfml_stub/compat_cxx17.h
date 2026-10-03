// Force-included by "make test" (g++ -include). Back-fills std::clamp for old C++17 standard
// libraries (for example MinGW g++ 6.3) that lack it. Does nothing on a complete C++17 library.
#pragma once
#include <algorithm>

#if defined(__GLIBCXX__) && !defined(__cpp_lib_clamp)
namespace std {
template <class T>
constexpr const T& clamp(const T& v, const T& lo, const T& hi) {
    return (v < lo) ? lo : ((hi < v) ? hi : v);
}
} // namespace std
#endif
