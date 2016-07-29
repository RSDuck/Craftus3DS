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

extern const Direction Direction_Opposite[];
extern const int DirectionToPosition[][3];

#endif  // !DIRECTION_H_INCLUDED