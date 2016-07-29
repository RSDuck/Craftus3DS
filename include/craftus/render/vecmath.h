#ifndef VECMATH_H_INCLUDED
#define VECMATH_H_INCLUDED

#include <citro3d.h>

inline C3D_FVec FVec_Add(C3D_FVec a, C3D_FVec b) { return (C3D_FVec){a.w + b.w, a.z + b.z, a.y + b.y, a.x + b.x}; }

inline C3D_FVec FVec_Sub(C3D_FVec a, C3D_FVec b) { return (C3D_FVec){a.w - b.w, a.z - b.z, a.y - b.y, a.x - b.x}; }

inline C3D_FVec FVec_Mul(C3D_FVec a, C3D_FVec b) { return (C3D_FVec){a.w * b.w, a.z * b.z, a.y * b.y, a.x * b.x}; }

inline C3D_FVec FVec_Div(C3D_FVec a, C3D_FVec b) { return (C3D_FVec){a.w / b.w, a.z / b.z, a.y / b.y, a.x / b.x}; }

inline C3D_FVec FVec_Cross(C3D_FVec a, C3D_FVec b) { return (C3D_FVec){1.f, a.x * b.y - a.y * b.x, a.z * b.x - a.x * b.z, a.y * b.z - a.z * b.y}; }

#endif  // !VECMATH_H_INCLUDED