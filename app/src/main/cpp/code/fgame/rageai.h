
#ifndef __RAGEAI_H__
#define __RAGEAI_H__


class Actor;
class Entity;
class Archiver;
class FuzzyEngine;

void FillBehaviorPackageList(void);
void ClearBehaviorPackageList(void);

class Strategos
{
public:
    Strategos() : _sightBasedHate(0.0f), _nextEvaluateTime(0.0f), _evaluateInterval(0.0f) {}
    Strategos(Actor *act) : _sightBasedHate(0.0f), _nextEvaluateTime(0.0f), _evaluateInterval(0.0f) {}
    virtual ~Strategos();

    virtual void Evaluate() {}
    virtual void Attack(Entity *enemy) {}
    virtual void DoArchive(Archiver& arc, Actor *act);
    void         Archive(Archiver& arc);
    virtual void SetBehaviorPackage(const str &packageName) {}

    virtual void NotifySightStatusChanged(Entity *enemy, qboolean canSee) {}

    virtual void NotifyDamageChanged(Entity *enemy, float damage) {}

    virtual float GetCheckYawMin() { return 0; }
    virtual float GetCheckYawMax() { return 0; }
    virtual float GetCheckInConeDistMax() { return 0; }
    virtual void  SetCheckInConeDistMax(float distance) {}

    void  SetEvaluateInterval(float interval) { _evaluateInterval = interval; }
    float GetEvaluateInterval() { return _evaluateInterval; }

    void  SetNextEvaluateTime(float time) { _nextEvaluateTime = time; }
    float GetNextEvaluateTime() { return _nextEvaluateTime; }

    void  SetSightBasedHate(float hate) { _sightBasedHate = hate; }
    float GetSightBasedHate() { return _sightBasedHate; }

private:
    float _sightBasedHate;
    float _nextEvaluateTime;
    float _evaluateInterval;
};

class DefaultStrategos : public Strategos
{
public:
    DefaultStrategos();
    DefaultStrategos(Actor *act);
    ~DefaultStrategos();

    void Evaluate();
    void NotifySightStatusChanged(Entity *enemy, qboolean canSee);
    void NotifyDamageChanged(Entity *enemy, float damage);
    void SetBehaviorPackage(const str &packageName);

    float GetCheckYawMin();
    float GetCheckYawMax();
    float GetCheckInConeDistMax();
    void  SetCheckInConeDistMax(float distance);

    void Attack(Entity *enemy);
    void DoArchive(Archiver &arc, Actor *actor);
     void Archive(Archiver &arc);

private:
    void _EvaluateEnemies();
    void _EvaluatePackages();
    void _EvaluateWorld();
    void _CheckForInTheWay();
    void _CheckForInConeOfFire();

    Actor *act;
    float  _checkYawMin;
    float  _checkYawMax;
    float  _checkInConeDistMax;
};


class PackageManager
{
public:
    PackageManager();
    PackageManager(Actor *actor);
    ~PackageManager();

    void RegisterPackage(const str& packageName);
    void UnregisterPackage(const str& packageName);

    void  EvaluatePackages(FuzzyEngine *fEngine);
    int   GetHighestScoringPackage();
    int   GetPackageIndex(const str& packageName);
    int   GetCurrentFVarIndex();
    float GetCurrentFVarLastExecuteTime();
    void  SetLastExecutionTime(int packageIndex);
    void  UpdateCurrentPackageIndex(int packageIndex);
    str   GetCurrentPackageName();


    void DoArchive(Archiver& arc, Actor* act);

private:
    Actor                             *act;
    Container<BehaviorPackageEntry_t>  _BehaviorPackages;

    int   _currentFVarIndex;
    float _currentFVarLastExecuteTime;
    int   _currentPackageIndex;
};


class Personality
{
public:
    Personality();
    Personality(Actor *actor);
    ~Personality();

    void SetBehaviorTendency(const str& packageName, float tendency);
    void SetTendency(const str& tendencyName, float tendencyValue);

    void  SetAggressiveness(float aggressiveness);
    float GetAggressiveness();

    void  SetTalkiness(float talkiness);
    float GetTalkiness();

    float GetTendency(const str& tendencyName);

    virtual void Archive(Archiver& arc);
    void DoArchive(Archiver& arc, Actor* act);

    qboolean WantsToExecuteCurrentPackage(float interval);
    qboolean ExecutedPackageInLastTimeFrame(float interval);

protected:
    qboolean _wantsToExecutePackage(PackageTendency_t *tendency);
    float _clampValue(float value);

private:
    float _aggressiveness;
    float _talkiness;

    float _anger;
    float _fear;

    Container<PackageTendency_t> _PackageTendencies;
    Container<Tendency_t>        _Tendencies;

    Actor *act;
};

#endif 
