// Minimal headless stand-in for SFML 3 (sf::Vector2 and sf::Rect only).
// Used ONLY by the tests ("make test"): the engine in Game/ needs nothing else from SFML,
// so scratch/test_*.cpp + Game/scr/*.cpp build and run on machines without SFML.
// The game itself (make / make run) always uses the real SFML headers.
#pragma once
#include <algorithm>

namespace sf {

template <typename T>
struct Vector2 {
    T x{};
    T y{};
    constexpr Vector2() = default;
    constexpr Vector2(T X, T Y) : x(X), y(Y) {}
    template <typename U>
    constexpr explicit Vector2(const Vector2<U>& o) : x(static_cast<T>(o.x)), y(static_cast<T>(o.y)) {}
};

template <typename T> constexpr Vector2<T> operator+(const Vector2<T>& a, const Vector2<T>& b) { return {a.x + b.x, a.y + b.y}; }
template <typename T> constexpr Vector2<T> operator-(const Vector2<T>& a, const Vector2<T>& b) { return {a.x - b.x, a.y - b.y}; }
template <typename T> constexpr Vector2<T> operator-(const Vector2<T>& a) { return {-a.x, -a.y}; }
template <typename T> constexpr Vector2<T> operator*(const Vector2<T>& a, T s) { return {a.x * s, a.y * s}; }
template <typename T> constexpr Vector2<T> operator*(T s, const Vector2<T>& a) { return {a.x * s, a.y * s}; }
template <typename T> constexpr Vector2<T> operator/(const Vector2<T>& a, T s) { return {a.x / s, a.y / s}; }
template <typename T> Vector2<T>& operator+=(Vector2<T>& a, const Vector2<T>& b) { a.x += b.x; a.y += b.y; return a; }
template <typename T> Vector2<T>& operator-=(Vector2<T>& a, const Vector2<T>& b) { a.x -= b.x; a.y -= b.y; return a; }
template <typename T> constexpr bool operator==(const Vector2<T>& a, const Vector2<T>& b) { return a.x == b.x && a.y == b.y; }
template <typename T> constexpr bool operator!=(const Vector2<T>& a, const Vector2<T>& b) { return !(a == b); }

using Vector2f = Vector2<float>;
using Vector2i = Vector2<int>;
using Vector2u = Vector2<unsigned int>;

// SFML 3 style rectangle: position + size, contains() excludes the right/bottom edge
template <typename T>
struct Rect {
    Vector2<T> position{};
    Vector2<T> size{};
    constexpr Rect() = default;
    constexpr Rect(const Vector2<T>& p, const Vector2<T>& s) : position(p), size(s) {}
    bool contains(const Vector2<T>& p) const {
        const T minX = std::min(position.x, static_cast<T>(position.x + size.x));
        const T maxX = std::max(position.x, static_cast<T>(position.x + size.x));
        const T minY = std::min(position.y, static_cast<T>(position.y + size.y));
        const T maxY = std::max(position.y, static_cast<T>(position.y + size.y));
        return p.x >= minX && p.x < maxX && p.y >= minY && p.y < maxY;
    }
    constexpr Vector2<T> getCenter() const { return {position.x + size.x / 2, position.y + size.y / 2}; }
};

using FloatRect = Rect<float>;
using IntRect = Rect<int>;

struct Color {
    unsigned char r{0};
    unsigned char g{0};
    unsigned char b{0};
    unsigned char a{255};
    constexpr Color() = default;
    constexpr Color(unsigned char red, unsigned char green, unsigned char blue, unsigned char alpha = 255)
        : r(red), g(green), b(blue), a(alpha) {}
    constexpr bool operator==(const Color& o) const { return r == o.r && g == o.g && b == o.b && a == o.a; }
    constexpr bool operator!=(const Color& o) const { return !(*this == o); }
    static const Color Red;
    static const Color Green;
    static const Color Yellow;
    static const Color White;
    static const Color Transparent;
};

inline const Color Color::Red{255, 0, 0, 255};
inline const Color Color::Green{0, 255, 0, 255};
inline const Color Color::Yellow{255, 255, 0, 255};
inline const Color Color::White{255, 255, 255, 255};
inline const Color Color::Transparent{0, 0, 0, 0};

} // namespace sf

