#ifndef DIRECTION_H_INCLUDED
#define DIRECTION_H_INCLUDED

typedef enum {
	Direction_Front = 0,
	Direction_Back,
	Direction_Right,
	Direction_Left,
	Direction_Top,
	Direction_Bottom,
	Directions_Count,
} Direction;

typedef enum { Axis_X, Axis_Y, Axis_Z } Axis;

extern const Direction Direction_Opposite[];
extern const int DirectionToPosition[][3];
extern const Axis DirectionToAxis[];

#endif  // !DIRECTION_H_INCLUDED