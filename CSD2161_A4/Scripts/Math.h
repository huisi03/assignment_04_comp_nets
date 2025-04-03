#ifndef MATH
#define MATH // header guards

#include <cmath>

float degToRad(float angle);

inline AEVec2 operator+(const AEVec2& lhs, const AEVec2& rhs)
{
    return { lhs.x + rhs.x, lhs.y + rhs.y };
}

inline AEVec2 operator-(const AEVec2& lhs, const AEVec2& rhs)
{
    return { lhs.x - rhs.x, lhs.y - rhs.y };
}

inline AEVec2 operator*(const AEVec2& vec, float scalar)
{
    return { vec.x * scalar, vec.y * scalar };
}

inline AEVec2 operator*(float scalar, const AEVec2& vec)
{
    return vec * scalar;
}

inline AEVec2& operator+=(AEVec2& lhs, const AEVec2& rhs)
{
    lhs.x += rhs.x;
    lhs.y += rhs.y;
    return lhs;
}

inline AEVec2& operator-=(AEVec2& lhs, const AEVec2& rhs)
{
    lhs.x -= rhs.x;
    lhs.y -= rhs.y;
    return lhs;
}

#endif
