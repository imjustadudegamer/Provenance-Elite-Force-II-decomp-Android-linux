

#pragma once

#include "g_local.h"
#include "../corepp/class.h"
#include "../corepp/container.h"
class PathNode;
#ifndef EF2_PATHNODEPTR_DEFINED
#define EF2_PATHNODEPTR_DEFINED
typedef SafePtr<PathNode> PathNodePtr;
#endif

class Path : public Class
{
private:
    Container<PathNodePtr> pathlist;
    Container<float>       distanceToNextNode;
    Container<Vector>      dirToNextNode;
    float                  pathlength;
    PathNodePtr            from;
    PathNodePtr            to;
    int                    nextnode;

    void _UpdateNodeValues(int const nodeNumber);
    void _UpdatePathLength(void);

public:
    CLASS_PROTOTYPE(Path);

    Path();
    Path(int numnodes);

    void      Clear(void);
    void      Reset(void);
    void      AddNode(PathNode *node);
    void      InsertNode(PathNode *node, int const insertionPoint);
    void      RemoveNode(PathNode *node);
    PathNode *GetNode(int num);
    int       GetNodeIndex(PathNode *node);
    PathNode *PreviousNode(void);
    PathNode *NextNode(void);
    PathNode *NextNode(const PathNode *node);
    Vector    ClosestPointOnPath(const Vector& pos);
    float     DistanceAlongPath(const Vector& pos);
    Vector    PointAtDistance(float dist);
    PathNode *NextNode(float dist);
    void      DrawPath(float r, float g, float b, float time);
    int       NumNodes(void);
    float     Length(void);
    PathNode *Start(void);
    PathNode *End(void);

    virtual void Archive(Archiver& arc) override;
};

typedef SafePtr<Path> PathPtr;
