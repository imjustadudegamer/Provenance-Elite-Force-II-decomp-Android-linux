

#include "cg_local.h"
#include "cg_commands.h"



void CG_ResetPlayerEntity(centity_t *cent)
{
    cent->errorTime    = -99999; 
    cent->extrapolated = qfalse;

    VectorCopy(cent->currentState.origin, cent->lerpOrigin);
    VectorCopy(cent->lerpOrigin, cent->rawOrigin);

    VectorCopy(cent->currentState.angles, cent->lerpAngles);
    VectorCopy(cent->lerpAngles, cent->rawAngles);

    if (cent->currentState.number == cg.snap->ps.clientNum) {
        VectorCopy(cent->lerpOrigin, cg.currentViewPos);
        VectorCopy(cent->lerpAngles, cg.currentViewAngles);

        cg.viewInterpTime      = -1;
        cg.viewInterpBlendLeft = 0.0f;
        cg.viewInterpCamera    = 0;
    }

    cent->splash_still_count = 0;
}

void CG_Player(centity_t *cent)
{
}
