

#include "g_local.h"
#include "entity.h"
#include "trigger.h"
#include "light.h"
#include "scriptmaster.h"

extern Event EV_Light_SetLight;





extern Event EV_FuncBeam_Color;


Event EV_Light_SetColor
(
	"color",
	EV_DEFAULT,
	NULL,
	NULL,
	""
);

Event EV_Light_SetFalloff
(
	"falloff",
	EV_CODEONLY,
	NULL,
	NULL,
	""
);

Event EV_Light_SetRadius (    "falloff",
    EV_CODEONLY,
    NULL,
    NULL,
    "");


Event EV_Light_SpotDir (    "spot_dir",
    EV_CODEONLY,
    NULL,
    NULL,
    "");

Event EV_Light_SpotRadiusByDistance (    "spot_radiusbydistance",
    EV_CODEONLY,
    NULL,
    NULL,
    "");

Event EV_Light_NoEntityLight (    "no_entity_light",
    EV_CODEONLY,
    NULL,
    NULL,
    "");

Event EV_Light_EntityTrace (    "entity_trace",
    EV_CODEONLY,
    NULL,
    NULL,
    "");

Event EV_Light_SpotAngle (    "spot_angle",
    EV_CODEONLY,
    NULL,
    NULL,
    "");

Event EV_Light_SetLight (    "light",
    EV_DEFAULT,
    NULL,
    NULL,
    "Set the intensity of the light");

CLASS_DECLARATION(Entity, Light, "light") {
     {&EV_Light_SetLight, NULL},
     {NULL,                    NULL}
};

Light::Light()
{
     PostEvent(EV_Remove, 0);
}
