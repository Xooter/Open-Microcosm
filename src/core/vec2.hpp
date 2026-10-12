#pragma once

#include <cmath>

// Vector 2D propio: la simulación no depende de raylib.
struct Vec2 {
    float x = 0.0f, y = 0.0f;
};

inline Vec2 operator+(Vec2 a, Vec2 b) { return {a.x + b.x, a.y + b.y}; }
inline Vec2 operator-(Vec2 a, Vec2 b) { return {a.x - b.x, a.y - b.y}; }
inline Vec2 operator*(Vec2 a, float s) { return {a.x * s, a.y * s}; }
inline float dot(Vec2 a, Vec2 b) { return a.x * b.x + a.y * b.y; }
inline float cross(Vec2 a, Vec2 b) { return a.x * b.y - a.y * b.x; }
inline float length(Vec2 a) { return std::sqrt(dot(a, a)); }
inline Vec2 rotate(Vec2 v, float c, float s) { return {c * v.x - s * v.y, s * v.x + c * v.y}; }
