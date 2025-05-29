#include "ferry.h"
#include "math.h"
#include <stdbool.h>

#define PI 3.14159265358979323846

struct ferry tracked_ferries[5];
int num_ferries = 0;

static double deg2rad(double deg) {
    return deg * (PI / 180.0);
}

double haversine_distance(double lat1, double lon1,
                          double lat2, double lon2) {
    const double R = 6371000.0;  // Earth radius in meters

    double phi1 = deg2rad(lat1);
    double phi2 = deg2rad(lat2);
    double deltaphi = deg2rad(lat2 - lat1);
    double deltalambda = deg2rad(lon2 - lon1);

    double a = sin(deltaphi/2) * sin(deltaphi/2)
             + cos(phi1) * cos(phi2)
             * sin(deltalambda/2) * sin(deltalambda/2);
    double c = 2 * atan2(sqrt(a), sqrt(1 - a));

    return R * c;
}

double distance_between_coords(struct coordinates coords1, struct coordinates coords2) {
    return haversine_distance(coords1.lat, coords1.lon, coords2.lat, coords2.lon);
}

bool is_ferry_near_coords(struct ferry ferry, struct coordinates coords) {

    return distance_between_coords(ferry.coords, coords) < 100;

}

struct ferry *get_ferry_by_mmsi(int mmsi) {
    for (int i = 0; i < 5; i++) {
        if (tracked_ferries[i].mmsi == mmsi) {
            return &tracked_ferries[i];
        }
    }
    return NULL;
}

void track_new_ferry(struct ferry ferry) {
    if (num_ferries == 4) return;
    num_ferries++;
    tracked_ferries[num_ferries] = ferry;
}