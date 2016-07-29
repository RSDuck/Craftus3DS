#include "craftus/world/direction.h"

const Direction Direction_Opposite[] = {Direction_Back, Direction_Front, Direction_Left, Direction_Right, Direction_Bottom, Direction_Top};

const int DirectionToPosition[][3] = {{0, 0, 1}, {0, 0, -1}, {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}};
