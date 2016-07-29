#ifndef BOX_H_INCLUDED
#define BOX_H_INCLUDED

#include <stdbool.h>

typedef struct {
	int startX, startY;
	int endX, endY;
} Box;

inline bool Box_Intersect(Box a, Box b) {
	return a.startX < b.endX && b.startX < a.endX && a.startY < b.endY && b.startY < a.endY;
}

inline bool Box_IsPointInside(Box a, int x, int y) {
	return x >= a.startX && x < a.endX && y >= a.startY && y < a.endY;
}

#endif  // !BOX_H_INCLUDED