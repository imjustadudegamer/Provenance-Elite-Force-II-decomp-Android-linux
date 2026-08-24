
#ifndef __ACTORSTRATEGIES_H__
#define __ACTORSTRATEGIES_H__


class Actor;
class Entity;
class Archiver;

typedef SafePtr<Actor> ActorThinkActorPtr;

class ActorThink
{
public:
    virtual ~ActorThink() {}

    virtual void DoArchive(Archiver& arc);
    virtual void Think(Actor&) = 0;
    virtual bool isSimple(void) { return false; }

    virtual void ActorStateUpdate(Actor& actor);  
    virtual void CheckGround(Actor& actor);       
    virtual void TryDrown(Actor& actor);          
    virtual void InanimateObject(Actor& actor);   
    virtual void DoMove(Actor& actor);            

    virtual void ProcessBehaviors(Actor& actor);    
    virtual void UpdateBossHealth(Actor& actor);    
};

class DefaultThink : public ActorThink
{
public:
    virtual void Think(Actor& actor) override;  
};

class SimplifiedThink : public ActorThink
{
public:
    SimplifiedThink(Actor *actor);
    ~SimplifiedThink(void);

    virtual bool isSimple(void) override { return true; }

    virtual void DoArchive(Archiver& arc) override;  
    virtual void DoMove(Actor& actor) override;      
    virtual void Think(Actor& actor) override;       

protected:

private:
    const SimplifiedThink& operator=(const SimplifiedThink&);

    ActorThinkActorPtr _actor;
    int                _previousContents;




};

#endif 
