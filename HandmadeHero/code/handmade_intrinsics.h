#include "handmade_platform.h"
#ifndef HANDMADE_INTRINSICS_H
#include "math.h"

inline uint32 RoundReal32ToUInt32(real32 Real32)
{
    uint32 Result = (uint32)roundf(Real32);
    return Result;
}

inline int32 RoundReal32ToInt32(real32 Real32)
{
    int32 Result = (int32)roundf(Real32);
    return Result;
}

inline int32 TruncateReal32ToInt32(real32 Real32)
{
    int32 Result = (int32)Real32;
    return Result;
}
// implement math
inline int32 FloorReal32ToInt32(real32 Real32)
{
    int32 Result = (int32)floorf(Real32);
    return Result;
}

inline real32 Sin(real32 Angle) {
    real32 Result = sinf(Angle);
    return Result;
}
inline real32 Cos(real32 Angle) {

    real32 Result =(real32) cosf(Angle);
    return Result;
}
inline real32 Atan2(real32 Y, real32 X) {

    real32 Result = Atan2(Y, X);
    return Result;
}

#define HANDMADE_INTRINSICS_H
#endif
