// FG: this file is a replacement for some of the structs used in sc_boss_spell_worker.h

#ifndef RSA_EXTRA_DEFINES
#define RSA_EXTRA_DEFINES

struct Locations
{
    float x, y, z;
    int32 id;
};

struct WayPoints
{
    WayPoints(int32 _id, float _x, float _y, float _z)
    {
        id = _id;
        x = _x;
        y = _y;
        z = _z;
    }
    int32 id;
    float x, y, z;
};

#endif
