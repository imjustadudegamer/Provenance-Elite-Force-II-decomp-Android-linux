
#pragma once

typedef enum {
    selectsound,
    rejectsound,
    changesound
} invsound_t;

struct refEntity_s;

void CL_Draw3DModel(
    float             x,
    float             y,
    float             w,
    float             h,
    qhandle_t         model,
    vec3_t            origin,
    vec3_t            rotateoffset,
    vec3_t            offset,
    vec3_t            angle,
    const str        &anim,
    int               a11,
    struct refEntity_s *a12,
    float             fov_x,
    float             fov_y
);

qboolean UI_CloseInventory(void);

