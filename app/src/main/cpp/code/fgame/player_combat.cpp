

#include "player.h"
#include "weaputils.h"

Q_UNUSED_VAR static Entity *FindClosestEntityInRadius(Vector origin, Vector forward, float fov, float maxdist)
{
    float    dist, dot;
    float    fovdot = cos(fov * 0.5 * M_PI / 180.0);
    Entity  *ent;
    Entity  *bestent  = NULL;
    int      bestdist = 999999;
    qboolean valid_entity;

    ent = findradius(NULL, origin, maxdist);

    while (ent) {
        valid_entity = false;

        if (ent->flags & FL_AUTOAIM) {
            valid_entity = true;
        }

        if (valid_entity) {
            Vector delta = (ent->centroid) - origin;

            dist = delta.length();

            if (dist < bestdist) {
                delta.normalize();

                dot = DotProduct(forward, delta);

                if (dot > fovdot) {
                    trace_t trace;
                    trace = G_Trace(
                        origin, vec_zero, vec_zero, ent->centroid, NULL, MASK_OPAQUE, false, "FindClosestEntityInRadius"
                    );

                    if ((trace.ent && trace.entityNum == ent->entnum) || (trace.fraction == 1)) {
                        bestent  = ent;
                        bestdist = dist;
                    }
                }
            }
        }
        ent = findradius(ent, origin, maxdist);
    }
    return bestent;
}



