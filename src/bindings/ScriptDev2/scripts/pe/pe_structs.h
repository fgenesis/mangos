#ifndef __PE_STRUCTS_
#define __PE_STRUCTS_

#include <list>
#include "Util.h"

char *GetRaceName(uint8);

inline uint32 URandomize(uint32 min, uint32 max)
{
    return urand(min, max);
    //return (min + ( rand() % (max - min + 1)));
}

class TemporarySummon;

class Emitter
{
public:
    Emitter(WorldObject *wo, uint32 entry, float speed, uint32 density, float maxradius, uint32 staytime, TempSummonType summtype = TEMPSUMMON_CORPSE_TIMED_DESPAWN)
    // tyId: TYPEID of WorldObject
    // entry: entry, what else?!
    // speed: emitters grow speed
    // density: rel. amount of emitters spawned
    {
        _parent = wo;
        _speed = speed;
        _objects = 0;
        _density = density;
        _maxradius = maxradius;
        _createtime = getMSTime();
        _done = false;
        _deletable = false;
        _entry = entry;
        _staytime = staytime;
        _speedfactor = uint32((_speed / _density) * 1000.0f);
        _overlaptime = 0;
        _lastspawntime = _createtime;
        _summontype = summtype;
        sourceo = wo->GetOrientation();
    }
    ~Emitter()
    {
        for(std::list<Emitter*>::iterator it = children.begin(); it != children.end(); it++)
        {
            delete *it;
        }
    }


    void SetSource(float x, float y, float z, float o)
    {
        sx = x;
        sy = y;
        sz = z;
        so = o;
        for(std::list<Emitter*>::iterator it = children.begin(); it != children.end(); it++)
        {
            (*it)->SetSource(x,y,z,o);
        }
    }
    
    bool IsDone(void) { return _done; }
    bool IsDeletable(void) { return _deletable; }

    void UnsummonOld(void);
        
    std::list<Emitter*> children;
    float sx,sy,sz,so,sourceo;
    uint32 _nexttime, _entry, _createtime, _staytime, _lastspawntime, _overlaptime;
    uint32 _objects, _density;
    float _speed;
    WorldObject *_parent;
    float _maxradius;
    bool _done, _deletable;
    uint32 _speedfactor;
    TempSummonType _summontype;

    virtual void Update(uint32) = 0;
};

// speed: how long it should take until all are spawned
// density: amount to be spawned
class RandomEmitter : public Emitter
{
public:
    RandomEmitter(WorldObject *wo, uint32 entry, float speed, uint32 density, float maxradius, uint32 staytime, TempSummonType summtype = TEMPSUMMON_TIMED_OR_CORPSE_DESPAWN)
        : Emitter(wo,entry,speed,density,maxradius,staytime,summtype) {}
    virtual void Update(uint32);
};

// speed: how long it should take until the "beam" reached end of radius (=all spawned)
// density: amount to be spawned
class BeamEmitter : public Emitter
{
public:
    BeamEmitter(WorldObject *wo, uint32 entry, float speed, uint32 density, float maxradius, uint32 staytime, TempSummonType summtype = TEMPSUMMON_TIMED_OR_CORPSE_DESPAWN)
        : Emitter(wo,entry,speed,density,maxradius,staytime,summtype) {}
    virtual void Update(uint32);
    void SetTarget(float x, float y);
    void SetOrientation(float o);
    float basex,basey;
};

class TripleBeamEmitter : public BeamEmitter
{
public:
    TripleBeamEmitter(WorldObject *wo, uint32 entry, float speed, uint32 density, float maxradius, uint32 staytime, TempSummonType summtype = TEMPSUMMON_TIMED_OR_CORPSE_DESPAWN)
        : BeamEmitter(wo,entry,speed,density,maxradius,staytime,summtype) {}
    void CreateChildBeams(float);
};

class OutwardCircleEmitter : public Emitter
{
public:
    OutwardCircleEmitter(WorldObject *wo, uint32 entry, float speed, uint32 density, float maxradius, uint32 staytime, TempSummonType summtype = TEMPSUMMON_TIMED_OR_CORPSE_DESPAWN)
        : Emitter(wo,entry,speed,density,maxradius,staytime,summtype) {}
    void Update(uint32);
    void CreateChildBeams(uint32 amount, float circlefactor, bool rnd);
    float _circlefactor;
};

// to this emitter, density means every <density> ms spawn new NPC, and <speed> the circling speed
class OrbitingEmitter : public Emitter
{
public:
    OrbitingEmitter(WorldObject *wo, uint32 entry, float speed, uint32 density, float maxradius, uint32 staytime, TempSummonType summtype = TEMPSUMMON_TIMED_OR_CORPSE_DESPAWN)
        : Emitter(wo,entry,speed,density,maxradius,staytime,summtype)
    {
        _radius = maxradius;
    }
    virtual void Update(uint32);

    float _radius;
};

// this takes _maxradius as base raduis
class RandomRadiusOrbitingEmitter : public OrbitingEmitter
{
public:
    RandomRadiusOrbitingEmitter(WorldObject *wo, uint32 entry, float speed, uint32 density, float maxradius, uint32 staytime, float radiusoffs, float radiusoffsfactor,  TempSummonType summtype = TEMPSUMMON_TIMED_OR_CORPSE_DESPAWN)
        : OrbitingEmitter(wo,entry,speed,density,maxradius,staytime,summtype)
    {
        _radiusoffs = radiusoffs;
        _radiusoffsfactor = radiusoffsfactor;
    }

    float _radiusoffs;
    float _radiusoffsfactor;

    virtual void Update(uint32);
};









#endif
