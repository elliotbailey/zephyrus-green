#ifndef FERRY_H_
#define FERRY_H_

#include <stdbool.h>

struct coordinates {
    double lat;
    double lon;
};

struct ferry {
    int mmsi;
    bool near_terminal;
    struct coordinates coords;
};

extern struct ferry tracked_ferries[5];

struct ferry *get_ferry_by_mmsi(int mmsi);
void track_new_ferry(struct ferry ferry);
bool is_ferry_near_coords(struct ferry ferry, struct coordinates coords);
double distance_between_coords(struct coordinates coords1, struct coordinates coords2);

#endif