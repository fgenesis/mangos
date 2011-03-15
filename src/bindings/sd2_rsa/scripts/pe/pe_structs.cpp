#include "precompiled.h"
#include "pe_base.h"
#include "pe_structs.h"


char *GetRaceName(uint8 race)
{
    switch (race)
    {
    case 1: return "Human";
    case 2: return "Orc";
    case 3: return "Dwarf";
    case 4: return "Nightelf";
    case 5: return "Undead";
    case 6: return "Tauren";
    case 7: return "Gnome";
    case 8: return "Troll";
    case 10: return "Blood Elf";
    case 11: return "Draenei";
    default: return "[ERROR]";
    }
}

#define RAD_FIX(x) ( (x)>(2*M_PI) ? ((x)-(2*M_PI)) : ( ((x)<0) ? ((x)+(2*M_PI)) : (x) ) )


void RandomEmitter::Update(uint32 diff)
{
    uint32 newtime = WorldTimer::tickTime();
    
    if(_lastspawntime + _staytime < newtime)
    {
        _deletable = true;
        return;
    }
    if(_objects > _density)
    {
        _done = true;
        return;
    }

    uint32 workdiff;
    for(workdiff = diff + _overlaptime; workdiff > _speedfactor && _objects <= _density; workdiff -= _speedfactor)
    {
        float x = (sin(float((rand()%100000) / 100.0f)) * _maxradius) + sx;
        float y = (cos(float((rand()%100000) / 100.0f)) * _maxradius) + sy;

        float z = sz;
        _lastspawntime += _speedfactor;

        Creature *npc = _parent->SummonCreatureCustom(_entry,x,y,z,0,_summontype,_staytime);
        if(npc)
        {
            npc->FallGround();
        }
        _objects++;
    }
    _overlaptime = workdiff;

    for(std::list<Emitter*>::iterator it = children.begin(); it != children.end(); it++)
    {
        (*it)->Update(diff);
    }
}

void BeamEmitter::SetTarget(float x, float y)
{
    SetOrientation(_parent->GetAngle(x,y));
}

void BeamEmitter::SetOrientation(float o)
{
    so = o;
    basex = cos(o);
    basey = sin(o);
}

void BeamEmitter::Update(uint32 diff)
{
    if(_deletable)
        return;

    uint32 newtime = WorldTimer::tickTime();
    float _radfact = _maxradius / _speed; // speed dependant radius factor
    float radius = _radfact * ((newtime - _createtime) / 1000.0f); // current radius
    if(_lastspawntime + _staytime < newtime)
    {
        _deletable = true;
        return;
    }

    if(_done)
        return;

    // doesnt really matter here that they are spawned backwards.. since it happens in the same time nobody will see
    uint32 workdiff;
    for(workdiff = diff + _overlaptime; workdiff >= _speedfactor && _objects <= _density; workdiff -= _speedfactor)
    {
        radius = _radfact * ((_lastspawntime - _createtime + workdiff) / 1000.0f);
        if(radius > _maxradius)
            continue;
        float x = (basex * radius) + sx;
        float y = (basey * radius) + sy;

        float z = sz;
        _lastspawntime += _speedfactor;
        Creature *npc = _parent->SummonCreatureCustom(_entry,x,y,z,0,_summontype,_staytime);
        if(npc)
        {
            npc->FallGround();
        }
        _objects++;
    }
    _overlaptime = workdiff;

    for(std::list<Emitter*>::iterator it = children.begin(); it != children.end(); it++)
    {
        (*it)->Update(diff);
    }

    if(/*radius > _maxradius ||*/ _objects > _density)
        _done = true;
}

void TripleBeamEmitter::CreateChildBeams(float ang)
{
    BeamEmitter *emit;

    emit = new BeamEmitter(_parent,_entry,_speed,_density,_maxradius,_staytime,_summontype);
    emit->SetOrientation(so+ang);
    children.push_back(emit);

    emit = new BeamEmitter(_parent,_entry,_speed,_density,_maxradius,_staytime,_summontype);
    emit->SetOrientation(so-ang);
    children.push_back(emit);
}

void OutwardCircleEmitter::Update(uint32 diff)
{
    if(children.size())
    {
        _done = (*children.begin())->IsDone();
        _deletable = (*children.begin())->IsDeletable();
    }

    float factor = float((2*M_PI) / children.size());
    float addfactor;
    uint32 i; // beam index

    uint32 workdiff;
    for(workdiff = diff + _overlaptime; workdiff > _speedfactor; workdiff -= _speedfactor)
    {
        i = 0;
        for(std::list<Emitter*>::iterator it = children.begin(); it != children.end(); it++)
        {
            addfactor = _circlefactor * ((_lastspawntime - _createtime + workdiff) / 1000.0f);
            float baseo = sourceo + (factor * i);
            BeamEmitter *beam = (BeamEmitter*)(*it);
            //float newo = beam->so + addfactor;
            //newo = fmod(newo,float(2+M_PI));
            //beam->SetOrientation(newo);
            beam->SetOrientation(baseo + addfactor);
            beam->Update(_speedfactor);

            i++;
        }
        _lastspawntime += _speedfactor;
    }
    _overlaptime = workdiff;
}

void OutwardCircleEmitter::CreateChildBeams(uint32 amount, float circlefactor, bool rnd)
{
    _circlefactor = circlefactor;
    float factor = float((2*M_PI) / amount);
    float o;
    for(uint32 i = 0; i < amount; i++)
    {
        BeamEmitter *beam = new BeamEmitter(_parent,_entry,_speed,_density,_maxradius,_staytime, _summontype);
        if(!rnd)
            o = _parent->GetOrientation() + (factor * i);
        else
            o = fmod(rand()/1000.0f,float(2*M_PI));
        beam->SetOrientation(o);
        children.push_back(beam);
    }
}

void OrbitingEmitter::Update(uint32 diff)
{
    if(_deletable)
        return;

    uint32 workdiff;
    for(workdiff = diff + _overlaptime; workdiff > _density; workdiff -= _density)
    {
        float xoffs = sin(_lastspawntime / _speed) * _radius;
        float yoffs = cos(_lastspawntime / _speed) * _radius;
        float o = 0; // TODO: formula here!
        
        Creature *npc = _parent->SummonCreatureCustom(_entry,sx + xoffs,sy + yoffs,sz,o,_summontype,_staytime);
        if(npc)
        {
            npc->FallGround();
        }
        _objects++;

        _lastspawntime += _density;
    }
    _overlaptime = workdiff;

    if(_createtime + _staytime < _lastspawntime)
        _deletable = true;
}

void RandomRadiusOrbitingEmitter::Update(uint32 diff)
{
    if(_deletable)
        return;

    uint32 workdiff;
    for(workdiff = diff + _overlaptime; workdiff > _density; workdiff -= _density)
    {
        _radius = _maxradius + (_radiusoffs * sin(_lastspawntime / _radiusoffsfactor));
        OrbitingEmitter::Update(_density);
    }
    _overlaptime = workdiff;
}

