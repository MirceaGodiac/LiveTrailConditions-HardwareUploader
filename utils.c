#include "utils.h"

int clamp_int(int value, int min_value, int max_value)
{
    if (value < min_value)
    {
        return min_value;
    }
    if (value > max_value)
    {
        return max_value;
    }
    return value;
}

int map_int(int x, int in_min, int in_max, int out_min, int out_max)
{
    if (in_max == in_min)
    {
        return out_min;
    }
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}
