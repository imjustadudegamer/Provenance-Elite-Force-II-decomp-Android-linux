

#include "cg_commands.h"
#include "cg_footprints.h"
#include "../corepp/tiki.h"

#define PROBE_GET(name, def, flags) cgi.Cvar_Get(name, def, flags)
#define PROBE_PRINT                 cgi.Printf
#include "../qcommon/qa_probe.h"
#define WIDGET_PROBE_GET(name, def, flags) cgi.Cvar_Get(name, def, flags)
#define WIDGET_PROBE_PRINT                 cgi.Printf
#include "../qcommon/qa_widget.h"

int          current_entity_number = -1;
centity_t   *current_centity       = NULL;
float        current_scale         = -1;
int          current_tiki          = -1;
cvar_t      *cg_showemitters;

Vector last_trace_end;
int    widget_last_trace_end_time; 

refEntity_t *current_entity        = NULL;
trace_t lastImpactTrace;

void CacheResource(const char *stuff);

static void autoCacheTiki(int tiki);

ClientGameCommandManager commandManager;

Event EV_Client_StartBlock
(
    "(",
    EV_DEFAULT,
    NULL,
    NULL,
    "Signals the beginning of a block of commands"
);
Event EV_Client_EndBlock
(
    ")",
    EV_DEFAULT,
    NULL,
    NULL,
    "Signals the end of a block of commands"
);
Event EV_Client_EmitterStartOff
(
    "startoff",
    EV_DEFAULT,
    NULL,
    NULL,
    "Signals an emitter to start in the off state (no tempmodels are emitted)"
);
Event EV_Client_SetAlpha
(
    "alpha",
    EV_DEFAULT,
    "f",
    "alpha",
    "Set the alpha of the spawned tempmodel"
);
Event EV_Client_SetDieTouch
(
    "dietouch",
    EV_DEFAULT,
    NULL,
    NULL,
    "Set the spawned tempmodels to die when they touch a solid"
);
Event EV_Client_SetBounceFactor(
    "bouncefactor",
    EV_DEFAULT,
    "f",
    "factor",
    "Set the bounciness of a tempmodel when it hits a solid.\n"
    "A factor > 1 will make the model bounce higher and higher on each hit"
);
Event EV_Client_SetBounceSound(
    "bouncesound",
    EV_DEFAULT,
    "sF",
    "sound [delay]",
    "When bouncing, what sound to play on impact and an option delay (default is 1 second) between playing this sound"
);
Event EV_Client_SetBounceSoundOnce
(
    "bouncesoundonce",
    EV_DEFAULT,
    "s",
    "sound",
    "When bouncing, what sound to play on impact one time"
);
Event EV_Client_SetScale
(
    "scale",
    EV_DEFAULT,
    "f",
    "scale",
    "Set the scale of a spawned tempmodel"
);
Event EV_Client_SetScaleUpDown
(
    "scaleupdown",
    EV_DEFAULT,
    NULL,
    NULL,
    "Set the tempmodel to scale up to scale value and then down."
);
Event EV_Client_SetScaleMin
(
    "scalemin",
    EV_DEFAULT,
    "f",
    "scalemin",
    "Set the minimum scale of a spawned tempmodel"
);
Event EV_Client_SetScaleMax
(
    "scalemax",
    EV_DEFAULT,
    "f",
    "scalemax",
    "Set the maximum scale of a spawned tempmodel"
);
Event EV_Client_SetModel(
    "model",
    EV_DEFAULT,
    "sSSSSS",
    "modelname1 modelname2 modelname3 modelname4 modelname5 modelname6",
    "Set the modelname of the tempmodel.  If more than 1 model is specified, it will\n"
    "be randomly chosen when spawned"
);
Event EV_Client_SetLife
(
    "life",
    EV_DEFAULT,
    "f",
    "life",
    "Set the life (in seconds) of the spawned tempmodel"
);
Event EV_Client_SetColor
(
    "color",
    EV_DEFAULT,
    "fffF",
    "red green blue alpha",
    "Set the color (modulate) of the spawned tempmodel."
);
Event EV_Client_SetVelocity
(
    "velocity",
    EV_DEFAULT,
    "f",
    "forwardVelocity",
    "Set the forward velocity of the spawned tempmodel"
);
Event EV_Client_SetAngularVelocity
(
    "avelocity",
    EV_DEFAULT,
    "fff",
    "yawVel pitchVel rollVel",
    "Set the angular velocity of the spawned tempmodel"
);
Event EV_Client_SetRandomVelocity
(
    "randvel",
    EV_DEFAULT,
    "SfFSfFSfF",
    "[random|crandom|range] xVel [xVel2] [random|crandom|range] yVel [yVel2] [random|crandom|range] zVel [zVel2]",
    "Add a random component to the regular velocity.\n"
    "If random is specified, the component will range from 0 to specified velocity.\n"
    "If crandom is specified, the component will range from -specified to +specified velocity.\n"
    "If range is specified, the component needs two values; it will randomly pick a number in the range\n"
    "from the first number to the first number plus the second number.\n"
    "If no keyword is explicitly specified, then the component will just be added on\n"
    "without randomness.\n"
    "This velocity is applied using the world axis"
);
Event EV_Client_SetRandomVelocityAlongAxis
(
    "randvelaxis",
    EV_DEFAULT,
    "SfFSfFSfF",
    "[random|crandom|range] forwardVel [forwardVel2] [random|crandom|range] rightVel [rightVel2] [random|crandom|rang"
    "e] upVel [upVel2]",
    "Add a random component to the regular velocity.\n"
    "If random is specified, the component will range from 0 to specified velocity.\n"
    "If crandom is specified, the component will range from -specified to +specified velocity.\n"
    "If range is specified, the component needs two values; it will randomly pick a number in the range\n"
    "from the first number to the first number plus the second number.\n"
    "If no keyword is explicitly specified, then the component will just be added on\n"
    "without randomness.\n"
    "This velocity is applied using the parent axis"
);
Event EV_Client_SetAccel
(
    "accel",
    EV_DEFAULT,
    "fffF",
    "xAcc yAcc zAcc delay",
    "Set the acceleration of the spawned tempmodel.\n"
    "This acceleration is applied using the world axis"
);
Event EV_Client_SetCount(
    "count",
    EV_DEFAULT,
    "i",
    "count",
    "Set the number of tempmodels that are spawned\n"
    "This is only used for the originspawn and tagspawn commands,\n"
    "and not for emitters, use spawnrate instead"
);
Event EV_Client_SetFade
(
    "fade",
    EV_DEFAULT,
    NULL,
    NULL,
    "Set the tempmodel to fade out over it's life"
);
Event EV_Client_SetFadeDelay
(
    "fadedelay",
    EV_DEFAULT,
    "f",
    "time",
    "Set the amount of time to delay a fade"
);
Event EV_Client_SetFadeIn
(
    "fadein",
    EV_DEFAULT,
    "f",
    "time",
    "Set the tempmodel to fade in over the specified time"
);
Event EV_Client_SetTwinkle
(
    "twinkle",
    EV_DEFAULT,
    "ffff",
    "mintimeoff maxtimeoff mintimeon maxtimeon",
    "Set the tempmodel to twinkle with the specified settings"
);
Event EV_Client_SetTrail
(
    "trail",
    EV_DEFAULT,
    "sssf",
    "shader startTag endTag life",
    "Set the tempmodel to have a swipe that follows it"
);
Event EV_Client_SetSpawnRate
(
    "spawnrate",
    EV_DEFAULT,
    "f",
    "rate",
    "Set the spawnrate of the emitter (models per second).\n"
    "This is only used for emitters and not for the originspawn and tagspawn commands"
);
Event EV_Client_SetOriginOffset(
    "offset",
    EV_DEFAULT,
    "SfFSfFSfF",
    "[random|crandom|range] offsetX [offsetX2] [random|crandom|range] offsetY [offsetY2] [random|crandom|range] "
    "offsetZ [offsetZ2]",
    "If random is specified, the component will range from 0 to +specified offset.\n"
    "If crandom is specified, the component will range from -specified to +specified offset.\n"
    "If range is specified, the component needs two values; it will randomly pick a number in the range\n"
    "from the first number to the first number plus the second number.\n"
    "If no keyword is explicitly specified, then the component will just be added on\n"
    "without randomness.\n"
    "This offset is applied using the world axis."
);
Event EV_Client_SetScaleRate(
    "scalerate",
    EV_DEFAULT,
    "f",
    "rate",
    "Set the scaling rate of the spawned tempmodel\n"
    "If a negative rate is used, the model will shrink"
);
Event EV_Client_SetCircle
(
    "circle",
    EV_DEFAULT,
    NULL,
    NULL,
    "Set the tempmodels to be spawned in a circle around the origin\n"
    "This circle will be generated in the X/Y axis of the model"
);
Event EV_Client_SetSphere(
    "sphere",
    EV_DEFAULT,
    NULL,
    NULL,
    "Set the tempmodels to spawn in a sphere around the origin.\n"
    "If sphereradius is set, the tempmodels will spawn at the radius distance from\n"
    "the origin"
);
Event EV_Client_SetInwardSphere(
    "inwardsphere",
    EV_DEFAULT,
    NULL,
    NULL,
    "Create the tempmodels in a sphere around the origin, and adjust their\n"
    "angle so they point toward the center of the sphere.  This is best used with a\n"
    "spehere radius and some velocity so the models look like they're heading toward the\n"
    "center of the sphere."
);
Event EV_Client_SetRadius
(
    "radius",
    EV_DEFAULT,
    "f",
    "radius",
    "Set the radius of the sphere for the inwardsphere amd sphere settings"
);
Event EV_Client_Hide
(
    "hide",
    EV_DEFAULT,
    NULL,
    NULL,
    "hides the entity"
);
Event EV_Client_Note
(
    "note",
    EV_DEFAULT,
    "s",
    "note",
    "This is a comment"
);
Event EV_Client_Todo
(
    "todo",
    EV_DEFAULT,
    "s",
    "todo",
    "This is an item that needs to be done"
);
Event EV_Client_SetSwarm
(
    "swarm",
    EV_DEFAULT,
    "iff",
    "frequency maxspeed delta",
    "Create a swarm like effect that the tempmodels follow when they are spawned\n"
    "frequency is how often they change direction\n"
    "maxspeed is how fast the tempmodel will move (it's randomly generated every\n"
    "time the frequency is hit\n"
    "delta is how much the tempmodel moves toward the origin every frame"
);
Event EV_Client_SetWavy(
    "wavy",
    EV_DEFAULT,
    "SF",
    "[random|crandom] distance",
    "Set the tempmodel to move in a wavy path to a maxmimun of the distance specified\n"
    "If random is specified, the distance is between 0 and +specified distance\n"
    "If crandom is specified, the distance is between -specified and +specified distance"
);
Event EV_Client_SetAlign
(
    "align",
    EV_DEFAULT,
    NULL,
    NULL,
    "Align the tempmodels to the direction they are traveling"
);
Event EV_Client_SetAlignOnce
(
    "alignonce",
    EV_DEFAULT,
    NULL,
    NULL,
    "Align the tempmodels to the direction they are traveling at the time they are initialized"
);
Event EV_Client_SetFlickerAlpha
(
    "flicker",
    EV_DEFAULT,
    NULL,
    NULL,
    "Set the tempmodel to change it's alpha every frame.  Creates a flickering effect"
);
Event EV_Client_SetCollision(
    "collision",
    EV_DEFAULT,
    "S",
    "water",
    "Turn on collision for the tempmodel.\n"
    "If the keyword water is specified, then the tempmodel will collide with water"
);
Event EV_Client_TagSpawn
(
    "tagspawn",
    EV_DEFAULT,
    "s",
    "tagName",
    "Spawn tempmodels from the specified tag.\n"
    "This command is followed by a ( to specify a block of commands that modify the tempmodels"
);
Event EV_Client_OriginSpawn
(
    "originspawn",
    EV_DEFAULT,
    NULL,
    NULL,
    "Spawn tempmodels from the origin.\n"
    "This command is followed by a ( to specify a block of commands that modify the tempmodels"
);
Event EV_Client_OriginBeamSpawn
(
    "originbeamspawn",
    EV_DEFAULT,
    NULL,
    NULL,
    "Spawn a beam from the origin.\n"
    "This command is followed by a ( to specify a block of commands that modify the beam"
);
Event EV_Client_OriginBeamEmitter
(
    "originbeamemitter",
    EV_DEFAULT,
    NULL,
    NULL,
    "Spawn beams from the origin.\n"
    "This command is followed by a ( to specify a block of commands that modify the beam"
);
Event EV_Client_TagEmitter
(
    "tagemitter",
    EV_DEFAULT,
    "ssSB",
    "tagName emitterName tikiName startoff",
    "Create an emitter that spawns tempmodels from the specified tag.\n"
    "This command is followed by a ( to specify a block of commands that modify the tempmodels"
);
Event EV_Client_OriginEmitter
(
    "originemitter",
    EV_DEFAULT,
    "sSB",
    "emitterName tikiName startoff",
    "Create an emitter that spawns tempmodels from the origin.\n"
    "This command is followed by a ( to specify a block of commands that modify the tempmodels"
);
Event EV_Client_EmitterOn
(
    "emitteron",
    EV_DEFAULT,
    "s",
    "emitterName",
    "Turn the specified emitter on"
);
Event EV_Client_EmitterOff
(
    "emitteroff",
    EV_DEFAULT,
    "s",
    "emitterName",
    "Turn the specified emitter off"
);
Event EV_Client_Sound
(
    "sound",
    EV_DEFAULT,
    "sIFFFF",
    "soundName channel volume min_distance max_pitch_difference min_pitch_difference",
    "Play the specified sound"
);
Event EV_Client_StopSound
(
    "stopsound",
    EV_DEFAULT,
    "i",
    "channel",
    "Stops the sound on the specified channel."
);
Event EV_Client_LoopSound
(
    "loopsound",
    EV_DEFAULT,
    "sFFf",
    "soundName volume min_distance pitch",
    "Play the specified sound as a looping sound"
);
Event EV_Client_Cache
(
    "cache",
    EV_CACHE,
    "s",
    "resourceName",
    "Cache the specified resource"
);
Event EV_Client_AliasCache
(
    "aliascache",
    EV_DEFAULT,
    "ssSSSSSS",
    "alias realPath arg1 arg2 arg3 arg4 arg5 arg6",
    "Create an alias to the specified path and cache the resource"
);
Event EV_Client_Alias
(
    "alias",
    EV_DEFAULT,
    "ssSSSSSS",
    "alias realPath arg1 arg2 arg3 arg4 arg5 arg6",
    "Create an alias to the specified path"
);
Event EV_Client_Footstep(
    "footstep",
    EV_DEFAULT,
    "sS",
    "tagname creaturetype",
    "Place a footprint that is appropriate to the surface we are currently stepping on"
);
Event EV_Client_Client
(
    "client",
    EV_DEFAULT,
    "SSSSSS",
    "arg1 arg2 arg3 arg4 arg5 arg6",
    "Execute the specified command arg string"
);
Event EV_Client_OriginDynamicLight(
    "origindlight",
    EV_DEFAULT,
    "fffffSS",
    "red green blue intensity life type1 type2",
    "Spawn a dynamic light from the origin of the model\n"
    "The red,green,blue parms are the color of the light\n"
    "The intensity is the radius of the light\n"
    "type is the type of light to create (lensflare,viewlensflare,additive)"
);
Event EV_Client_TagDynamicLight(
    "tagdlight",
    EV_DEFAULT,
    "sfffffSS",
    "tagName red green blue intensity life type1 type2",
    "Spawn a dynamic light from the specified tag\n"
    "The red,green,blue parms are the color of the light\n"
    "The intensity is the radius of the light\n"
    "type is the type of light to create (lensflare,viewlensflare,additive)"
);
Event EV_Client_DynamicLight(
    "dlight",
    EV_DEFAULT,
    "ffffSS",
    "red green blue intensity type1 type2",
    "This makes the emitter itself a dynamic light"
    "The red,green,blue parms are the color of the light\n"
    "The intensity is the radius of the light\n"
    "type is the type of light to create (lensflare,viewlensflare,additive)"
);
Event EV_Client_LensFlare(
    "lensflare",
    EV_DEFAULT,
    "sfffFS",
    "lensflare_name color_red color_green color_blue scale replacement_shader_name",
    "This makes the emitter itself a lensflare"
);
Event EV_Client_SetEntityColor
(
    "entcolor",
    EV_DEFAULT,
    "fffF",
    "red green blue alpha",
    "Set the color(modulate) of this entity"
);
// ⚠️ DELIBERATELY REGISTERED AND NEVER DISPATCHED.  Do not "fix" this by adding a
// :60913 and the .bss reservation at :164813 -- and NEVER in the response table
Event EV_Client_SetTexAnimTime
(
    "texanimtime",
    EV_DEFAULT,
    "f",
    "time",
    "Set the texture animation speed"
);
Event EV_Client_SetGlobalFade
(
    "globalfade",
    EV_DEFAULT,
    "S",
    "[in|out]",
    "Set the tempmodels to globally fade in or out together"
);
Event EV_Client_SetParentLink
(
    "parentlink", EV_DEFAULT,
    NULL,
    NULL,
    "Set the tempmodels linked to the parent, so they move with the parent model"
);
Event EV_Client_SetHardLink(
    "hardlink",
    EV_DEFAULT,
    NULL,
    NULL,
    "Set the tempmodels linked to the model they are spawned from, so they move with it"
);
Event EV_Client_SetRandomRoll
(
    "randomroll",
    EV_DEFAULT,
    NULL,
    NULL,
    "Set the tempmodels so they pick a random roll value every frame"
);
Event EV_Client_ParentAngles
(
    "parentangles",
    EV_DEFAULT,
    NULL,
    NULL,
    "Set the tempmodels angles to that of its parent"
);
Event EV_Client_SetAngles(
    "angles",
    EV_DEFAULT,
    "SfFSfFSfF",
    "[random|crandom|range] pitch [pitch2] [random|crandom|range] yaw [yaw2] [random|crandom|range] roll [roll2]",
    "If random is specified, the component will range from 0 to +specified value.\n"
    "If crandom is specified, the component will range from -specified to +specified value.\n"
    "If range is specified, the component needs two values; it will randomly pick a number in the range\n"
    "from the first number to the first number plus the second number.\n"
    "If no keyword is explicitly specified, then the component will be just set\n"
);
Event EV_Client_Swipe
(
    "swipe",
    EV_DEFAULT,
    "V",
    "origin",
    "Do a swipe and add it on to the swipe rendering list."
);
Event EV_Client_SwipeOn
(
    "swipeon",
    EV_DEFAULT,
    "ssff",
    "shader startTagName endTagNamelife life",
    "Signal the start of a swipe from the current tag"
);
Event EV_Client_SwipeOff
(
    "swipeoff",
    EV_DEFAULT,
    NULL,
    NULL,
    "Signal the end of a swipe"
);
Event EV_Client_BeginTagBeamEmitter
(
    "tagbeamemitter",
    EV_DEFAULT,
    "ssS",
    "tagstart name tagend",
    "Create a beam emitter that uses 2 tags to determine it's start and end position"
);
Event EV_Client_BeginTagBeamSpawn
(
    "tagbeamspawn",
    EV_DEFAULT,
    "sf",
    "tagstart name",
    "Create a beam emitter that uses the tag to determine it's starting position."
);
Event EV_Client_AnimateOnce
(
    "animateonce",
    EV_DEFAULT,
    NULL,
    NULL,
    "Set a tempmodel to animate once and then get removed"
);
Event EV_Client_SetAnim
(
    "anim",
    EV_DEFAULT,
    "s",
    "animation",
    "Set a tempmodel the the specified animation"
);
Event EV_Client_SetSubdivisions
(
    "numsegments",
    EV_DEFAULT,
    "i",
    "numsegments",
    "Set the number of segments in a beam"
);
Event EV_Client_SetMinOffset
(
    "minoffset",
    EV_DEFAULT,
    "f",
    "minoffset",
    "Set the minimum offset from center in a beam"
);
Event EV_Client_SetMaxOffset
(
    "maxoffset",
    EV_DEFAULT,
    "f",
    "maxoffset",
    "Set the maximum offset from center in a beam"
);
Event EV_Client_SetBeamShader
(
    "beamshader",
    EV_DEFAULT,
    "s",
    "shadername",
    "Set the shader to use for the beam"
);
Event EV_Client_SetBeamLength
(
    "beamlength",
    EV_DEFAULT,
    "f",
    "length",
    "Set the length of the beam or trace length (for decals)"
);
Event EV_Client_SetBeamDelay(
    "beamdelay",
    EV_DEFAULT,
    "SF",
    "[random] delay",
    "Set the delay time between creating new beams.\n"
    "If the keyword random is specified, the delay between beams will occur randomly between 0 and the time specified"
);
Event EV_Client_SetBeamToggleDelay(
    "beamtoggledelay",
    EV_DEFAULT,
    "SF",
    "[random] delay",
    "Set a delay between toggling the beams on and off.\n"
    "If the keyword random is specified, the delay between toggling will occur randomly between 0 and the time "
    "specified"
);
Event EV_Client_SetBeamPersist
(
    "beampersist",
    EV_DEFAULT,
    NULL,
    NULL,
    "Make the beams persist instead of blinking out"
);
Event EV_Client_SetBeamOffsetEndpoints
(
    "beam_offset_endpoints",
    EV_DEFAULT,
    NULL,
    NULL,
    "Make the beams endpoints offset to reduce the bunching up effect"
);
Event EV_Client_BeamSphere
(
    "beamsphere",
    EV_DEFAULT,
    "i",
    "count",
    "Create a sphere shaped beam effect from the origin. Count is the number of beams"
);
Event EV_Client_Spread
(
    "spread",
    EV_DEFAULT,
    "ffFFF",
    "spreadx spready endspreadx endspready spreadtimedelta",
    "Add a random variance in the spawned beam in the forward direction by the amound specified in spreadx and spready"
);
Event EV_Client_UseLastTraceEnd
(
    "uselasttraceend",
    EV_DEFAULT,
    NULL,
    NULL,
    "Makes this trace command use the end results of the last trace command"
);
Event EV_Client_OffsetAlongAxis(
    "offsetalongaxis",
    EV_DEFAULT,
    "SfFSfFSfF",
    "[crandom|random|range] offsetx [offsetx2] [crandom|random|range] offsety [offsety2] [crandom|random|range] "
    "offsetz [offsetz2]",
    "If random is specified, the component will range from 0 to specified offset.\n"
    "If crandom is specified, the component will range from -specified to +specified offset.\n"
    "If range is specified, the component needs two values; it will randomly pick a number in the range\n"
    "from the first number to the first number plus the second number.\n"
    "If no keyword is explicitly specified, then the component will just be added on\n"
    "without randomness.\n"
    "This offset is applied using the model's local axis"
);
Event EV_Client_SetEndAlpha
(
    "endalpha",
    EV_DEFAULT,
    "f",
    "alpha",
    "Set the alpha of the beam's endpoint"
);
Event EV_Client_RandomChance
(
    "randomchance",
    EV_DEFAULT,
    "fSSSSSS",
    "percentage [arg1] [arg2] [arg3] [arg4] [arg5] [arg6]",
    "Set the percentage chance that command will occur"
);
Event EV_Client_CommandDelay
(
    "commanddelay",
    EV_DEFAULT,
    "fiSSSSSS",
    "time commandnumber [arg1] [arg2] [arg3] [arg4] [arg5] [arg6]",
    "Set the time delay between this command getting executed.  This requires a command number to be assigned here.\n"
    "This is internally used to keep track of the commands this entity executes and will resolve naming conflicts."
);
Event EV_Client_BounceDecal(
    "bouncedecal",
    EV_DEFAULT,
    "iI",
    "maxamount temporary",
    "Put a mark when the tempmodel bounces and hits a surface\n"
    "maxamount = Max amount of decals to make when bouncing\n"
    "temporary = specify 1 for a temporary mark that only appears for a short time, 0 for a decal that stays around "
    "longer (default is 0)\n"
);
Event EV_Client_SetDecalRadius
(
    "decalradius",
    EV_DEFAULT,
    "f",
    "radius",
    "Set the radius of the decal"
);
Event EV_Client_SetDecalOrientation
(
    "orientation",
    EV_DEFAULT,
    "f",
    "degrees",
    "Set the degrees to orient the decal.  Specify 'random' to use a random orientation"
);
Event EV_Client_SetDecalShader
(
    "decalshader",
    EV_DEFAULT,
    "s",
    "shadername",
    "Set the shader to use for the impact decal"
);
Event EV_Client_SetTraceLength
(
    "tracelength",
    EV_DEFAULT,
    "f",
    "length",
    "Set the length of the trace for the decal"
);
Event EV_Client_TagList
(
    "taglist",
    EV_DEFAULT,
    "ssSSSSSS",
    "arg1 arg2 arg3 arg4 arg5 arg6 arg7 arg8",
    "Set the tag list to create a beam that travels between all the tags"
);
Event EV_Client_Lightstyle(
    "lightstyle",
    EV_DEFAULT,
    "s",
    "nameOfImage",
    "Set a lightstyle to determine the color of this tempmodel, the image\n"
    "specified is used to determine the look of the light style"
);
Event EV_Client_PhysicsRate
(
    "physicsrate",
    EV_DEFAULT,
    "s",
    "rate",
    "Set the rate (in updates per second) for the tempmodel's physics to be updated"

);
Event EV_Client_Parallel
(
    "parallel",
    EV_DEFAULT,
    NULL,
    NULL,
    "Set tempmodel to be parallel to the viewer"
);
Event EV_Client_Detail
(
    "detail",
    EV_DEFAULT,
    NULL,
    NULL,
    "Set emitter/tempmodel to be detail. Which can be turned off by detail"
);
Event EV_Client_Print
(
    "print",
    EV_DEFAULT,
    "s",
    "string",
    "Prints a string."
);
Event EV_Client_ColorRange
(
    "colorrange",
    EV_DEFAULT,
    "ffffffF",
    "color_red_start color_green_start color_blue_start color_red_end color_green_end color_blue_end time_seconds",
    "Sets the color range of the spawned tempmodel."
);
Event EV_Client_UseLastTrace
(
    "useLastTrace",
    EV_DEFAULT,
    NULL,
    NULL,
    "Makes this trace command use the results from the last trace command"
);
Event EV_Client_ItemRingShader
(
    "itemringshader",
    EV_DEFAULT,
    "s",
    "shader_name",
    "Makes this item use a different item shader."
);
Event EV_Client_AutoCache
(
    "autocache",
    EV_DEFAULT,
    "s",
    "name",
    "Cache the specified resource"
);
Event EV_Client_DuplicateCount
(
    "duplicatecount",
    EV_DEFAULT,
    "i",
    "num",
    "Set a duplication number for the tempmodels"
);
Event EV_Client_TraceCount
(
    "tracecount",
    EV_DEFAULT,
    "i",
    "count",
    "Set the number of traces that are done"
);
Event EV_Client_SetWindEffect
(
    "setwindeffect",
    EV_DEFAULT,
    "f",
    "windeffect",
    "sets the amount that emitted objects are affected by wind"
);
Event EV_Client_Drag
(
    "drag",
    EV_DEFAULT,
    "fF",
    "dragValue [randomValue]",
    "Set the drag coeffcient.  Example, 0.5 drag is half the speed every frame. dragDelay is the time the particle waits before the drag is applied."
);
Event EV_Client_DoFade
(
    "dofade",
    EV_DEFAULT,
    "ffb",
    "fadeDelay fadeinTime global",
    "Fades the tempmodel in over fadeinTime milliseconds, after waiting fadeDelay\n"
    "milliseconds.  A non-zero global moves the fade bits into the global fade path."
);
Event EV_Client_LScaleRate
(
    "lscalerate",
    EV_DEFAULT,
    "f",
    "rate",
    "Sets the linear scaling rate of the spawned tempmodel"
);
Event EV_Client_SetSpinRate
(
    "setspinrate",
    EV_DEFAULT,
    "fff",
    "spin_Pitch spin_Yaw spin_Roll",
    "sets spin on particles"
);
Event EV_Client_EmitterSpin
(
    "emitterspin",
    EV_DEFAULT,
    NULL,
    NULL,
    "sets particles to spin based on emitter origin"
);
Event EV_Client_ParticleSpin
(
    "particlespin",
    EV_DEFAULT,
    NULL,
    NULL,
    "sets particles to spin based on own origin"
);
Event EV_Client_TBCSpeed
(
    "tbc_speed",
    EV_DEFAULT,
    "fffb",
    "forwardVel randomVel bias useBias",
    "TimeBased Speed Command"
);
Event EV_Client_TBCSpawnRate
(
    "tbc_spawnrate",
    EV_DEFAULT,
    "fffb",
    "rate randomRate bias useBias",
    "TimeBased SpawnRate Command"
);
Event EV_Client_TBCParticleSpin
(
    "tbc_particlespin",
    EV_DEFAULT,
    "fffbffb",
    "spin randomSpin bias useBias spin2 randomSpin2 useBias2",
    "TimeBased Spin Command"
);
Event EV_Client_TBCDirection
(
    "tbc_direction",
    EV_DEFAULT,
    "SfSfSffb",
    "[randomtype] yawDir [randomtype] pitchDir [randomtype] rollDir speed useBias",
    "Add a random component to the regular velocity."
);
Event EV_Client_Direction
(
    "direction",
    EV_DEFAULT,
    "SfSfSf",
    "[randomtype] yawDir [randomtype] pitchDir [randomtype] rollDir",
    "Add a random component to the regular velocity."
);
Event EV_Client_ScaleDelay
(
    "scaledelay",
    EV_DEFAULT,
    "f",
    "delay",
    "Sets the Delay before the model scales"
);
Event EV_Client_OneFrameOnly
(
    "oneFrameOnly",
    EV_DEFAULT,
    NULL,
    NULL,
    "Makes the emitter only last one frame"
);
Event EV_Client_BeamWave
(
    "beamwave",
    EV_DEFAULT,
    NULL,
    NULL,
    "Makes the beam behave as a wave."
);
Event EV_Client_FullBeamWave
(
    "fullbeamwave",
    EV_DEFAULT,
    NULL,
    NULL,
    "Makes the beam behave as one big wave."
);
Event EV_Client_AlwaysCreate
(
    "alwayscreate",
    EV_DEFAULT,
    NULL,
    NULL,
    "Always creates a new beam instead of replacing odd one."
);
Event EV_Client_CanEndEarly
(
    "canendearly",
    EV_DEFAULT,
    NULL,
    NULL,
    "Makes it so the beam can end early if it hits something."
);
Event EV_Client_ChanceToEmitAtCamera
(
    "chancetoemitatcamera",
    EV_DEFAULT,
    "f",
    "chance",
    "sets the percent chance that particles will be emitted towards camera"
);
Event EV_Client_Freq
(
    "freq",
    EV_DEFAULT,
    "f",
    "frequency",
    "Modifies the frequency of a wavy beam (default is 10)."
);
Event EV_Client_ExclusiveLight
(
    "exclusivelight",
    EV_DEFAULT,
    "s",
    "exclusiveName",
    "Set the exclusive entity lighting tag name for a tiki"
);
Event EV_Client_FootstepSound
(
    "footstepsound",
    EV_DEFAULT,
    "IFF",
    "channel volume min_dist",
    "Places an appropriate footstep sound based on the surface we are currently stepping on"
);
Event EV_Client_SurfaceSound
(
    "surfaceSound",
    EV_DEFAULT,
    "sIFF",
    "baseSurfaceSound channel volume min_dist",
    "Places an appropriate sound based on the surface we are currently stepping on"
);
Event EV_Client_TagTraceImpactSpawn
(
    "tagtraceimpactspawn",
    EV_DEFAULT,
    "s",
    "tagname",
    "Perform a trace from the specified tag to the maxlength and spawn the specified model there."
);
Event EV_Client_TagTraceImpactSound
(
    "tagtraceimpactsound",
    EV_DEFAULT,
    "sisB",
    "tagname maxlength sound isalias",
    "Perform a trace from the specified tag to the maxlength and play a sound at that position"
);
Event EV_Client_TagTraceImpactMark
(
    "tagtraceimpactmark",
    EV_DEFAULT,
    "sI",
    "tagname temporary",
    "Perform a trace from the specified tag to the maxlength and put the shader as a decal on the surface it hits"
);

Event EV_Client_EffectMap
(
    "effectmap",
    EV_DEFAULT,
    "s",
    "name",
    "Declaration for the effect map"
);
Event EV_Client_AddSpawnEffect
(
    "addspawneffect",
    EV_DEFAULT,
    "ss",
    "surfacetype name",
    "The surface type and name of the effect"
);
Event EV_Client_AddSoundEffect
(
    "addsoundeffect",
    EV_DEFAULT,
    "ss",
    "surfacetype name",
    "The surface type and name of the sound effect"
);
Event EV_Client_DefineSpawnEffect
(
    "definespawneffect",
    EV_DEFAULT,
    "ss",
    "effectname mapname",
    "Defines the spawn effect"
);
Event EV_Client_DefineSoundEffect
(
    "definesoundeffect",
    EV_DEFAULT,
    "sss",
    "effectname mapname soundname",
    "Defines the sound effect"
);
Event EV_Client_SpawnEffect
(
    "spawneffect",
    EV_DEFAULT,
    "s",
    "name",
    "Name of the effect"
);
Event EV_Client_HitEntities
(
    "hitEntities",
    EV_DEFAULT,
    "b",
    "hit_entities_bool",
    "Whether or not entities can be hit with the trace"
);
Event EV_Client_HitWorld
(
    "hitWorld",
    EV_DEFAULT,
    "b",
    "hit_world_bool",
    "Whether or not world can be hit with the trace"
);

CLASS_DECLARATION(Listener, ClientGameCommandManager, NULL) {
    {&EV_Client_StartBlock,                 &ClientGameCommandManager::StartBlock                },
    {&EV_Client_EndBlock,                   &ClientGameCommandManager::EndBlock                  },
    {&EV_Client_EmitterStartOff,            &ClientGameCommandManager::EmitterStartOff           },
    {&EV_Client_OriginSpawn,                &ClientGameCommandManager::BeginOriginSpawn          },
    {&EV_Client_OriginBeamSpawn,            &ClientGameCommandManager::BeginOriginBeamSpawn      },
    {&EV_Client_OriginBeamEmitter,          &ClientGameCommandManager::BeginOriginBeamEmitter    },
    {&EV_Client_TagSpawn,                   &ClientGameCommandManager::BeginTagSpawn             },
    {&EV_Client_TagEmitter,                 &ClientGameCommandManager::BeginTagEmitter           },
    {&EV_Client_BeginTagBeamEmitter,        &ClientGameCommandManager::BeginTagBeamEmitter       },
    {&EV_Client_BeginTagBeamSpawn,          &ClientGameCommandManager::BeginTagBeamSpawn         },
    {&EV_Client_OriginEmitter,              &ClientGameCommandManager::BeginOriginEmitter        },
    {&EV_Client_EmitterOn,                  &ClientGameCommandManager::EmitterOn                 },
    {&EV_Client_EmitterOff,                 &ClientGameCommandManager::EmitterOff                },
    {&EV_Client_SetAlpha,                   &ClientGameCommandManager::SetAlpha                  },
    {&EV_Client_SetDieTouch,                &ClientGameCommandManager::SetDieTouch               },
    {&EV_Client_SetBounceFactor,            &ClientGameCommandManager::SetBounceFactor           },
    {&EV_Client_SetBounceSound,             &ClientGameCommandManager::SetBounceSound            },
    {&EV_Client_SetBounceSoundOnce,         &ClientGameCommandManager::SetBounceSoundOnce        },
    {&EV_Client_SetScale,                   &ClientGameCommandManager::SetScale                  },
    {&EV_Client_SetScaleUpDown,             &ClientGameCommandManager::SetScaleUpDown            },
    {&EV_Client_SetScaleMin,                &ClientGameCommandManager::SetScaleMin               },
    {&EV_Client_SetScaleMax,                &ClientGameCommandManager::SetScaleMax               },
    {&EV_Client_SetModel,                   &ClientGameCommandManager::SetModel                  },
    {&EV_Client_SetLife,                    &ClientGameCommandManager::SetLife                   },
    {&EV_Client_SetColor,                   &ClientGameCommandManager::SetColor                  },
    {&EV_Client_SetVelocity,                &ClientGameCommandManager::SetVelocity               },
    {&EV_Client_SetAngularVelocity,         &ClientGameCommandManager::SetAngularVelocity        },
    {&EV_Client_SetRandomVelocity,          &ClientGameCommandManager::SetRandomVelocity         },
    {&EV_Client_SetRandomVelocityAlongAxis, &ClientGameCommandManager::SetRandomVelocityAlongAxis},
    {&EV_Client_SetAccel,                   &ClientGameCommandManager::SetAccel                  },
    {&EV_Client_SetCount,                   &ClientGameCommandManager::SetCount                  },
    {&EV_Client_SetFade,                    &ClientGameCommandManager::SetFade                   },
    {&EV_Client_SetFadeDelay,               &ClientGameCommandManager::SetFadeDelay              },
    {&EV_Client_SetFadeIn,                  &ClientGameCommandManager::SetFadeIn                 },
    {&EV_Client_SetTwinkle,                 &ClientGameCommandManager::SetTwinkle                },
    {&EV_Client_SetTrail,                   &ClientGameCommandManager::SetTrail                  },
    {&EV_Client_SetSpawnRate,               &ClientGameCommandManager::SetSpawnRate              },
    {&EV_Client_SetScaleRate,               &ClientGameCommandManager::SetScaleRate              },
    {&EV_Client_SetOriginOffset,            &ClientGameCommandManager::SetOriginOffset           },
    {&EV_Client_SetSphere,                  &ClientGameCommandManager::SetSphere                 },
    {&EV_Client_SetCircle,                  &ClientGameCommandManager::SetCircle                 },
    {&EV_Client_SetInwardSphere,            &ClientGameCommandManager::SetInwardSphere           },
    {&EV_Client_SetRadius,                  &ClientGameCommandManager::SetRadius                 },
    {&EV_Client_SetSwarm,                   &ClientGameCommandManager::SetSwarm                  },
    {&EV_Client_Hide,                       &ClientGameCommandManager::Hide                      },
    {&EV_Client_Note,                       &ClientGameCommandManager::TikiNote                  },
    {&EV_Client_Todo,                       &ClientGameCommandManager::TikiTodo                  },
    {&EV_Client_SetWavy,                    &ClientGameCommandManager::SetWavy                   },
    {&EV_Client_SetAlign,                   &ClientGameCommandManager::SetAlign                  },
    {&EV_Client_SetAlignOnce,               &ClientGameCommandManager::SetAlignOnce              },
    {&EV_Client_SetCollision,               &ClientGameCommandManager::SetCollision              },
    {&EV_Client_SetFlickerAlpha,            &ClientGameCommandManager::SetFlickerAlpha           },
    {&EV_Client_Sound,                      &ClientGameCommandManager::Sound                     },
    {&EV_Client_StopSound,                  &ClientGameCommandManager::StopSound                 },
    {&EV_Client_LoopSound,                  &ClientGameCommandManager::LoopSound                 },
    {&EV_Client_Cache,                      &ClientGameCommandManager::Cache                     },
    {&EV_Client_AliasCache,                 &ClientGameCommandManager::AliasCache                },
    {&EV_Client_Alias,                      &ClientGameCommandManager::Alias                     },
    {&EV_Client_Footstep,                   &ClientGameCommandManager::Footstep                  },
    {&EV_Client_Client,                     &ClientGameCommandManager::Client                    },
    {&EV_Client_TagDynamicLight,            &ClientGameCommandManager::TagDynamicLight           },
    {&EV_Client_OriginDynamicLight,         &ClientGameCommandManager::OriginDynamicLight        },
    {&EV_Client_DynamicLight,               &ClientGameCommandManager::DynamicLight              },
    {&EV_Client_LensFlare,                  &ClientGameCommandManager::LensFlare                 },
    {&EV_Client_SetEntityColor,             &ClientGameCommandManager::SetEntityColor            },
    {&EV_Client_SetGlobalFade,              &ClientGameCommandManager::SetGlobalFade             },
    {&EV_Client_SetParentLink,              &ClientGameCommandManager::SetParentLink             },
    {&EV_Client_SetHardLink,                &ClientGameCommandManager::SetHardLink               },
    {&EV_Client_SetRandomRoll,              &ClientGameCommandManager::SetRandomRoll             },
    {&EV_Client_SetAngles,                  &ClientGameCommandManager::SetAngles                 },
    {&EV_Client_ParentAngles,               &ClientGameCommandManager::ParentAngles              },
    {&EV_Client_Swipe,                      &ClientGameCommandManager::Swipe                     },
    {&EV_Client_SwipeOn,                    &ClientGameCommandManager::SwipeOn                   },
    {&EV_Client_SwipeOff,                   &ClientGameCommandManager::SwipeOff                  },
    {&EV_Client_SetAnim,                    &ClientGameCommandManager::SetAnim                   },
    {&EV_Client_AnimateOnce,                &ClientGameCommandManager::AnimateOnce               },
    {&EV_Client_SetSubdivisions,            &ClientGameCommandManager::SetSubdivisions           },
    {&EV_Client_SetMinOffset,               &ClientGameCommandManager::SetMinOffset              },
    {&EV_Client_SetMaxOffset,               &ClientGameCommandManager::SetMaxOffset              },
    {&EV_Client_SetBeamShader,              &ClientGameCommandManager::SetShader                 },
    {&EV_Client_SetBeamLength,              &ClientGameCommandManager::SetLength                 },
    {&EV_Client_SetDecalShader,             &ClientGameCommandManager::SetShader                 },
    {&EV_Client_SetTraceLength,             &ClientGameCommandManager::SetLength                 },
    {&EV_Client_SetBeamDelay,               &ClientGameCommandManager::SetBeamDelay              },
    {&EV_Client_SetBeamToggleDelay,         &ClientGameCommandManager::SetBeamToggleDelay        },
    {&EV_Client_SetBeamPersist,             &ClientGameCommandManager::SetBeamPersist            },
    {&EV_Client_SetBeamOffsetEndpoints,     &ClientGameCommandManager::SetBeamOffsetEndpoints    },
    {&EV_Client_BeamSphere,                 &ClientGameCommandManager::SetBeamSphere             },
    {&EV_Client_Spread,                     &ClientGameCommandManager::SetSpread                 },
    {&EV_Client_UseLastTraceEnd,            &ClientGameCommandManager::SetUseLastTraceEnd        },
    {&EV_Client_OffsetAlongAxis,            &ClientGameCommandManager::SetOffsetAlongAxis        },
    {&EV_Client_SetEndAlpha,                &ClientGameCommandManager::SetEndAlpha               },
    {&EV_Client_RandomChance,               &ClientGameCommandManager::RandomChance              },
    {&EV_Client_CommandDelay,               &ClientGameCommandManager::CommandDelay              },
    {&EV_Client_BounceDecal,                &ClientGameCommandManager::SetBounceDecal            },
    {&EV_Client_SetDecalRadius,             &ClientGameCommandManager::SetDecalRadius            },
    {&EV_Client_SetDecalOrientation,        &ClientGameCommandManager::SetDecalOrientation       },
    {&EV_Client_TagList,                    &ClientGameCommandManager::TagList                   },
    {&EV_Client_Lightstyle,                 &ClientGameCommandManager::SetLightstyle             },
    {&EV_Client_PhysicsRate,                &ClientGameCommandManager::SetPhysicsRate            },
    {&EV_Client_Parallel,                   &ClientGameCommandManager::SetParallel               },
    {&EV_Client_Detail,                     &ClientGameCommandManager::SetDetail                 },
    {&EV_Client_Print,                      &ClientGameCommandManager::Print                     },
    {&EV_Client_ColorRange,                 &ClientGameCommandManager::SetColorRange             },
    {&EV_Client_UseLastTrace,               &ClientGameCommandManager::SetUseLastTrace           },
    {&EV_Client_ItemRingShader,             &ClientGameCommandManager::SetItemRingShader         },
    {&EV_Client_AutoCache,                  &ClientGameCommandManager::Cache                     },
    {&EV_Client_DuplicateCount,             &ClientGameCommandManager::SetDuplicateCount         },
    {&EV_Client_TraceCount,                 &ClientGameCommandManager::SetTraceCount             },
    {&EV_Client_SetWindEffect,              &ClientGameCommandManager::SetWindEffect             },
    // `drag` is retail SetDrag @0x0000E8F0, not MOHAA's SetFriction -- retail's cgame has no
    {&EV_Client_Drag,                       &ClientGameCommandManager::SetDrag                   },
    {&EV_Client_DoFade,                     &ClientGameCommandManager::SetDoFade                 },
    // `lscalerate` is retail SetLinearScaleRate @0x0000B9B0, not SetScaleRate -- the two are
    {&EV_Client_LScaleRate,                 &ClientGameCommandManager::SetLinearScaleRate        },
    {&EV_Client_SetSpinRate,                &ClientGameCommandManager::SetSpinRate               },
    {&EV_Client_EmitterSpin,                &ClientGameCommandManager::SetEmitterSpin            },
    {&EV_Client_ParticleSpin,               &ClientGameCommandManager::SetParticleSpin           },
    {&EV_Client_TBCSpeed,                   &ClientGameCommandManager::TBC_SetSpeed              },
    {&EV_Client_TBCSpawnRate,               &ClientGameCommandManager::TBC_SetSpawnRate          },
    {&EV_Client_TBCParticleSpin,            &ClientGameCommandManager::TBC_SetParticleSpin       },
    {&EV_Client_TBCDirection,               &ClientGameCommandManager::TBC_SetDirection          },
    {&EV_Client_Direction,                  &ClientGameCommandManager::SetRandomDirection        },
    {&EV_Client_ScaleDelay,                 &ClientGameCommandManager::SetScaleDelay             },
    {&EV_Client_OneFrameOnly,               &ClientGameCommandManager::SetOneFrameOnly           },
    {&EV_Client_BeamWave,                   &ClientGameCommandManager::SetBeamWave               },
    {&EV_Client_FullBeamWave,               &ClientGameCommandManager::SetFullBeamWave           },
    {&EV_Client_AlwaysCreate,               &ClientGameCommandManager::SetAlwaysCreate           },
    {&EV_Client_CanEndEarly,                &ClientGameCommandManager::SetCanEndEarly            },
    {&EV_Client_ChanceToEmitAtCamera,       &ClientGameCommandManager::SetChanceToEmitAtCamera   },
    {&EV_Client_Freq,                       &ClientGameCommandManager::SetFreq                   },
    {&EV_Client_ExclusiveLight,             &ClientGameCommandManager::SetExclusiveLightName     },
    {&EV_Client_FootstepSound,              &ClientGameCommandManager::FootstepSound             },
    {&EV_Client_SurfaceSound,               &ClientGameCommandManager::SurfaceSound              },
    {&EV_Client_TagTraceImpactSpawn,        &ClientGameCommandManager::TagTraceImpactSpawn       },
    {&EV_Client_TagTraceImpactSound,        &ClientGameCommandManager::TagTraceImpactSound       },
    {&EV_Client_TagTraceImpactMark,         &ClientGameCommandManager::BeginTagTraceImpactMark   },
    {&EV_Client_EffectMap,                  &ClientGameCommandManager::EffectMap                 },
    {&EV_Client_AddSpawnEffect,             &ClientGameCommandManager::AddSpawnEffect            },
    {&EV_Client_AddSoundEffect,             &ClientGameCommandManager::AddSoundEffect            },
    {&EV_Client_DefineSpawnEffect,          &ClientGameCommandManager::DefineSpawnEffect         },
    {&EV_Client_DefineSoundEffect,          &ClientGameCommandManager::DefineSoundEffect         },
    {&EV_Client_SpawnEffect,                &ClientGameCommandManager::SpawnEffect               },
    {&EV_Client_HitEntities,                &ClientGameCommandManager::HitEntities               },
    {&EV_Client_HitWorld,                   &ClientGameCommandManager::HitWorld                  },
    {NULL,                                  NULL                                                 }
};

static int DLightNameToNum(str s)
{
    if (!s.icmp("normal")) {
        return 0;
    } else if (!s.icmp("lensflare")) {
        return lensflare;
    } else if (!s.icmp("viewlensflare")) {
        return viewlensflare;
    } else if (!s.icmp("additive")) {
        return additive;
    } else {
        return 0;
    }
}

float RandomizeRange(float start, float end)
{
    float t;

    if (start > end) {
        return start;
    }

    t = start + ((end - start) * random());
    return t;
}

static int CGGetRandomNumberData(Event *ev, int argindex, randtype_t *type, float *base, float *amplitude)
{
    str org;

    org = ev->GetString(argindex);

    if (!strcmp(org, "crandom")) {
        *type = CRANDOM;
        *base = ev->GetFloat(argindex + 1);
        return 2;
    }

    if (!strcmp(org, "random")) {
        *type = RANDOM;
        *base = ev->GetFloat(argindex + 1);
        return 2;
    }

    if (!strcmp(org, "randomrange")) {
        *type      = RANDOMRANGE;
        *base      = ev->GetFloat(argindex + 1);
        *amplitude = ev->GetFloat(argindex + 2);
        return 3;
    }

    if (!strcmp(org, "crandomrange")) {
        *type      = CRANDOMRANGE;
        *base      = ev->GetFloat(argindex + 1);
        *amplitude = ev->GetFloat(argindex + 2);
        return 3;
    }

    if (!strcmp(org, "grandom")) {
        *type      = GRANDOM;
        *base      = ev->GetFloat(argindex + 1);
        *amplitude = ev->GetFloat(argindex + 2);
        return 3;
    }

    if (!strcmp(org, "erandom")) {
        *type = ERANDOM;
        *base = ev->GetFloat(argindex + 1);
        return 2;
    }

    *type = NOT_RANDOM;
    *base = ev->GetFloat(argindex);
    return 1;
}

static void CGSetRandomTriple(Event *ev, Vector& base, Vector& amplitude, randtype_t *types)
{
    int i = 1;
    int j = 0;

    for (j = 0; j < 3; j++) {
        float      fBase = 0.0f;
        float      fAmp  = 0.0f;
        randtype_t type  = NOT_RANDOM;

        i += CGGetRandomNumberData(ev, i, &type, &fBase, &fAmp);

        base[j]      = fBase;
        amplitude[j] = fAmp;

        if (types) {
            types[j] = type;
        }
    }
}

void ClientGameCommandManager::SetBaseAndAmplitude(Event *ev, Vector& base, Vector& amplitude)
{
    CGSetRandomTriple(ev, base, amplitude, NULL);
}

ClientGameCommandManager::ClientGameCommandManager()
{
    m_seed = 0;

    m_currentEffectMap = NULL;

    InitializeTempModels();
    // NOTE: InitializeEmitters() is intentionally NOT called here. At static-init
}

void ClientGameCommandManager::Print(Event *ev)
{
    if (current_entity) {
        cgi.DPrintf("%d:%s\n", current_entity->entityNumber, ev->GetString(1));
    }
}


// Argument syntax is retail's own `commanddelay` line, and it is NOT MOHAA's:
void ClientGameCommandManager::CommandDelay(Event *ev)
{
    int            delay;
    int            commandnum;
    int            i;
    int            num;
    float          fWait;
    commandtime_t *ct;
    Event         *ev1;

    fWait = ev->GetFloat(1);
    delay = (int)(1000.0f * fWait);

    if (!current_entity) {
        warning("CCM:CommandDelay", "Cannot perform command delay on spawned tempmodels");
    } else {
        commandnum = ev->GetInteger(2);
        ct = m_command_time_manager.GetLastCommandTime(current_entity->entityNumber, commandnum);

        if (!ct->last_command_time) {
            ct->last_command_time = cg.time + delay;
            return;
        }

        if (ct->last_command_time > cg.time) {
            return;
        }

        ct->last_command_time = cg.time + delay;
    }

    ev1 = new Event(ev->GetString(3));

    num = ev->NumArgs();
    for (i = 4; i <= num; i++) {
        ev1->AddToken(ev->GetToken(i));
    }

    ProcessEvent(ev1);
}

void ClientGameCommandManager::RandomChance(Event *ev)
{
    int   i;
    float percentage = ev->GetFloat(1);

    if ((random() < percentage) && (ev->NumArgs() >= 2)) {
        Event *ev1;

        ev1 = new Event(ev->GetString(2));

        for (i = 3; i <= ev->NumArgs(); i++) {
            ev1->AddToken(ev->GetToken(i));
        }
        ProcessEvent(ev1);
    }
}

void ClientGameCommandManager::SetDetail(Event *ev)
{
    m_spawnthing->cgd.flags |= T_DETAIL;
}

void ClientGameCommandManager::SetWindEffect(Event *ev)
{
    if (!m_spawnthing) {
        return;
    }

    m_spawnthing->windEffect = ev->GetFloat(1);
}






void ClientGameCommandManager::SetDecalRadius(Event *ev)
{
    if (!m_spawnthing) {
        return;
    }

    m_spawnthing->cgd.decal_radius = ev->GetFloat(1);
}

void ClientGameCommandManager::SetDecalOrientation(Event *ev)
{
    str deg;

    if (!m_spawnthing) {
        return;
    }

    deg = ev->GetString(1);

    if (!deg.icmp("random")) {
        m_spawnthing->cgd.decal_orientation = random() * 360;
    } else {
        m_spawnthing->cgd.decal_orientation = ev->GetFloat(1);
    }
}

void ClientGameCommandManager::SetBounceDecal(Event *ev)
{
    if (!m_spawnthing) {
        return;
    }

    m_spawnthing->cgd.flags2 |= T2_BOUNCE_DECAL;
    if (ev->NumArgs() > 1) {
        m_spawnthing->cgd.maxbouncecount = ev->GetInteger(1);

        if (ev->NumArgs() > 2) {
            qboolean b = ev->GetBoolean(2);
            if (b) {
                m_spawnthing->cgd.flags2 |= T2_TEMPORARY_DECAL;
            }
        }
    }
}

void ClientGameCommandManager::SetParallel(Event *ev)
{
    m_spawnthing->cgd.flags2 |= T2_PARALLEL;
}

void ClientGameCommandManager::SetPhysicsRate(Event *ev)
{
    str rate;

    if (!m_spawnthing) {
        return;
    }

    rate = ev->GetString(1);

    if (!rate.icmp(rate, "every")) {
        m_spawnthing->cgd.flags2 |= T2_PHYSICS_EVERYFRAME;
    } else {
        m_spawnthing->cgd.physicsRate = atof(rate);
    }
}

void ClientGameCommandManager::SetLightstyle(Event *ev)
{
    if (!m_spawnthing) {
        return;
    }

    m_spawnthing->cgd.lightstyle = CG_RegisterLightStyle(ev->GetString(1));
}

void ClientGameCommandManager::SetBeamSphere(Event *ev)
{
    if (!m_spawnthing) {
        return;
    }

    m_spawnthing->numspherebeams = ev->GetInteger(1);
    m_spawnthing->beamflags      = BEAM_SPHERE_EFFECT;
}

void ClientGameCommandManager::SetSpread(Event *ev)
{
    if (!m_spawnthing) {
        return;
    }

    m_spawnthing->spreadx = ev->GetFloat(1);
    m_spawnthing->spready = ev->GetFloat(2);

    if (ev->NumArgs() == 5) {
        m_spawnthing->endspreadx      = ev->GetFloat(3);
        m_spawnthing->endspready      = ev->GetFloat(4);
        m_spawnthing->spreadtimedelta = ev->GetFloat(5);
    } else {
        m_spawnthing->spreadtimedelta = 0;
    }
}

void ClientGameCommandManager::SetUseLastTraceEnd(Event *ev)
{
    m_spawnthing->use_last_trace_end = qtrue;
}

void ClientGameCommandManager::SetUseLastTrace(Event *ev)
{
    m_spawnthing->useLastTrace = qtrue;
}

void ClientGameCommandManager::SetSubdivisions(Event *ev)
{
    if (!m_spawnthing) {
        return;
    }

    m_spawnthing->numSubdivisions = ev->GetInteger(1);
}

void ClientGameCommandManager::SetBeamPersist(Event *ev)
{
    m_spawnthing->beamflags |= BEAM_PERSIST_EFFECT;
}

void ClientGameCommandManager::SetBeamOffsetEndpoints(Event *ev)
{
    m_spawnthing->beamflags |= BEAM_OFFSET_ENDPOINTS;
}

void ClientGameCommandManager::SetBeamDelay(Event *ev)
{
    if (!m_spawnthing) {
        return;
    }

    if (ev->NumArgs() > 1) {
        str r = ev->GetString(1);
        if (r.icmp("random")) {
            m_spawnthing->beamflags |= BEAM_RANDOM_DELAY;
        }
        m_spawnthing->delay = ev->GetFloat(2) * 1000.0f;
    } else {
        m_spawnthing->delay = ev->GetFloat(1) * 1000.0f;
    }
}

void ClientGameCommandManager::SetBeamToggleDelay(Event *ev)
{
    if (!m_spawnthing) {
        return;
    }

    m_spawnthing->beamflags |= BEAM_TOGGLE;
    if (ev->NumArgs() > 1) {
        str r = ev->GetString(1);
        if (!r.icmp("random")) {
            m_spawnthing->beamflags |= BEAM_RANDOM_TOGGLEDELAY;
        }

        m_spawnthing->toggledelay = ev->GetFloat(2) * 1000.0f;
    } else {
        m_spawnthing->toggledelay = ev->GetFloat(1) * 1000.0f;
    }
}

void ClientGameCommandManager::SetLength(Event *ev)
{
    if (!m_spawnthing) {
        return;
    }

    m_spawnthing->length = ev->GetFloat(1);
}

void ClientGameCommandManager::SetShader(Event *ev)
{
    if (!m_spawnthing) {
        return;
    }

    m_spawnthing->cgd.shadername = ev->GetString(1);
}

void ClientGameCommandManager::SetMinOffset(Event *ev)
{
    if (!m_spawnthing) {
        return;
    }

    m_spawnthing->min_offset = ev->GetInteger(1);
}

void ClientGameCommandManager::SetMaxOffset(Event *ev)
{
    if (!m_spawnthing) {
        return;
    }

    m_spawnthing->max_offset = ev->GetInteger(1);
}

void ClientGameCommandManager::SetAnim(Event *ev)
{
    if (!m_spawnthing) {
        return;
    }

    m_spawnthing->animName = ev->GetString(1);
}

void ClientGameCommandManager::AnimateOnce(Event *ev)
{
    m_spawnthing->cgd.flags |= T_ANIMATEONCE;
}

void ClientGameCommandManager::EmitterStartOff(Event *ev)
{
    m_spawnthing->startoff = qtrue;
}

void ClientGameCommandManager::StartBlock(Event *ev)
{
    if (ev->NumArgs() > 1) {
        cgi.DPrintf("CCM::StartBlock : Invalid commands on start block '{'\n");
    }
}

void ClientGameCommandManager::EndBlock(Event *ev)
{

    if (ev->NumArgs() > 1) {
        cgi.DPrintf("CCM::StartBlock : Invalid commands on end block '{'\n");
    }

    // was never built.  The m_spawnthing guard is the port's; retail dereferences it directly.
    if (m_spawnthing && m_spawnthing->m_tbcManager) {
        m_spawnthing->m_tbcManager->SortAllTBILists();

        if (m_spawnthing->m_tbcManager->GetNumberOfEmitterCommands() > 0) {
            m_spawnthing->createTBC_Controller();
        }
    }

    if (endblockfcn) {
        (this->*endblockfcn)();
    }

    endblockfcn = NULL;
    m_spawnthing = NULL;
}

void ClientGameCommandManager::SetSwarm(Event *ev)
{
    if (!m_spawnthing) {
        return;
    }

    m_spawnthing->cgd.swarmfreq     = ev->GetInteger(1);
    m_spawnthing->cgd.swarmmaxspeed = ev->GetFloat(2);
    m_spawnthing->cgd.swarmdelta    = ev->GetFloat(3);
    m_spawnthing->cgd.flags |= T_SWARM;
}

void ClientGameCommandManager::Hide(Event *ev)
{
    m_spawnthing->hide = qtrue;
}

void ClientGameCommandManager::TikiNote(Event *ev) {}

void ClientGameCommandManager::TikiTodo(Event *ev) {}


void ClientGameCommandManager::SetWavy(Event *ev)
{
    if (!m_spawnthing) {
        return;
    }

    // exactly ONE writer of bit 8, this verb: SetWavy @0x0000A300 size=272 ends its body
    // The keyword branch is retail's GetRandomNumber @0x00022A40, not the registered event
    {
        randtype_t type = NOT_RANDOM;
        float      fBase = 0.0f;
        float      fAmp = 0.0f;

        CGGetRandomNumberData(ev, 1, &type, &fBase, &fAmp);

        if (type == NOT_RANDOM) {
            m_spawnthing->cgd.wavyDistance = ev->GetFloat(1);
        } else {
            m_spawnthing->cgd.wavyDistance = CGResolveRandomNumber(type, fBase, fAmp);
        }
    }

    m_spawnthing->cgd.flags |= T_WAVE;
}

void ClientGameCommandManager::SetCircle(Event *ev)
{
    m_spawnthing->cgd.flags |= T_CIRCLE;
}

void ClientGameCommandManager::SetSphere(Event *ev)
{
    m_spawnthing->cgd.flags |= T_SPHERE;
}

void ClientGameCommandManager::SetRadius(Event *ev)
{
    if (!m_spawnthing) {
        return;
    }

    m_spawnthing->sphereRadius = ev->GetFloat(1);
}

void ClientGameCommandManager::SetInwardSphere(Event *ev)
{
    if (!m_spawnthing) {
        return;
    }

    m_spawnthing->cgd.flags |= T_INWARDSPHERE;
    if (ev->NumArgs() > 0) {
        m_spawnthing->sphereRadius = ev->GetFloat(1);
    }
}

void ClientGameCommandManager::SetAlign(Event *ev)
{
    m_spawnthing->cgd.flags |= T_ALIGN;
}

void ClientGameCommandManager::SetAlignOnce(Event *ev)
{
    m_spawnthing->cgd.flags |= T_ALIGNONCE;
}

void ClientGameCommandManager::SetRandomRoll(Event *ev)
{
    m_spawnthing->cgd.flags |= T_RANDOMROLL;
}


void ClientGameCommandManager::SetCollision(Event *ev)
{
    str mask;

    if (!m_spawnthing) {
        return;
    }

    m_spawnthing->cgd.flags |= T_COLLISION;
    m_spawnthing->cgd.collisionmask = (CONTENTS_SOLID);

    if (ev->NumArgs() > 1) {
        mask = ev->GetString(1);

        if (mask == "water") {
            m_spawnthing->cgd.collisionmask = (CONTENTS_SOLID | CONTENTS_WATER);
        }
    }
}

void ClientGameCommandManager::SetFlickerAlpha(Event *ev)
{
    m_spawnthing->cgd.flags |= T_FLICKERALPHA;
}

void ClientGameCommandManager::SetEndAlpha(Event *ev)
{
    if (!m_spawnthing) {
        return;
    }

    m_spawnthing->endalpha = ev->GetFloat(1);
}

void ClientGameCommandManager::SetOffsetAlongAxis(Event *ev)
{
    if (!m_spawnthing) {
        return;
    }

    CGSetRandomTriple(ev, m_spawnthing->axis_offset_base, m_spawnthing->axis_offset_amplitude, m_spawnthing->axis_offset_rand);
}


void ClientGameCommandManager::SetOriginOffset(Event *ev)
{
    if (!m_spawnthing) {
        return;
    }

    CGSetRandomTriple(ev, m_spawnthing->origin_offset_base, m_spawnthing->origin_offset_amplitude, m_spawnthing->offset_rand);
}

void ClientGameCommandManager::SetAlpha(Event *ev)
{
    if (!m_spawnthing) {
        return;
    }

    m_spawnthing->cgd.alpha = ev->GetFloat(1);
}

void ClientGameCommandManager::SetDieTouch(Event *ev)
{
    m_spawnthing->cgd.flags |= T_DIETOUCH;
}

void ClientGameCommandManager::SetBounceFactor(Event *ev)
{
    if (!m_spawnthing) {
        return;
    }

    m_spawnthing->cgd.bouncefactor = ev->GetFloat(1);
    m_spawnthing->cgd.flags |= T_COLLISION;
    m_spawnthing->cgd.collisionmask = (CONTENTS_SOLID);
}

void ClientGameCommandManager::SetBounceSound(Event *ev)
{
    if (!m_spawnthing) {
        return;
    }

    m_spawnthing->cgd.bouncesound = ev->GetString(1);

    if (ev->NumArgs() == 2) {
        m_spawnthing->cgd.bouncesound_delay = ev->GetFloat(1) * 1000;
    }

    m_spawnthing->cgd.flags |= T_BOUNCESOUND;
}

void ClientGameCommandManager::SetBounceSoundOnce(Event *ev)
{
    if (!m_spawnthing) {
        return;
    }

    m_spawnthing->cgd.flags |= T_BOUNCESOUNDONCE;
    SetBounceSound(ev);
}

void ClientGameCommandManager::SetScale(Event *ev)
{
    if (!m_spawnthing) {
        return;
    }

    m_spawnthing->cgd.scale = ev->GetFloat(1);
}

void ClientGameCommandManager::SetScaleUpDown(Event *ev)
{
    m_spawnthing->cgd.flags |= T_SCALEUPDOWN;
}

void ClientGameCommandManager::SetScaleMin(Event *ev)
{
    if (!m_spawnthing) {
        return;
    }

    m_spawnthing->cgd.flags |= T_RANDSCALE;
    m_spawnthing->cgd.scalemin = ev->GetFloat(1);
}

void ClientGameCommandManager::SetScaleMax(Event *ev)
{
    if (!m_spawnthing) {
        return;
    }

    m_spawnthing->cgd.flags |= T_RANDSCALE;
    m_spawnthing->cgd.scalemax = ev->GetFloat(1);
}

void ClientGameCommandManager::SetScaleRate(Event *ev)
{
    if (!m_spawnthing) {
        return;
    }

    m_spawnthing->cgd.scaleRate = ev->GetFloat(1);
}

void ClientGameCommandManager::SetFade(Event *ev)
{
    int global = 0;

    if (!m_spawnthing) {
        return;
    }

    if (ev->NumArgs() > 0) {
        global = ev->GetInteger(1);
    }

    if (global) {
        m_spawnthing->cgd.flags2 |= T2_FADE;
    } else {
        m_spawnthing->cgd.flags |= T_FADE;
    }
}

void ClientGameCommandManager::SetDoFade(Event *ev)
{
    float fadeDelay;
    float fadein;
    int   global = 0;

    fadeDelay = ev->GetFloat(1);
    fadein    = ev->GetFloat(2);
    if (ev->NumArgs() > 2) {
        global = ev->GetInteger(3);
    }

    if (fadeDelay != 0.0f) {
        m_spawnthing->cgd.fadedelay = (int)(1000.0f * fadeDelay);
    }

    if (fadein != 0.0f) {
        m_spawnthing->cgd.fadeintime = (int)(1000.0f * fadein);
        if (global) {
            m_spawnthing->cgd.flags2 |= T2_DOFADE;
        } else {
            m_spawnthing->cgd.flags |= T_FADEIN;
        }
    }

    if (global) {
        m_spawnthing->cgd.flags2 |= T2_FADE;
    } else {
        m_spawnthing->cgd.flags |= T_FADE;
    }
}

void ClientGameCommandManager::SetFadeDelay(Event *ev)
{
    if (!m_spawnthing) {
        return;
    }

    m_spawnthing->cgd.fadedelay = ev->GetFloat(1) * 1000;
    m_spawnthing->cgd.flags |= T_FADE;
}


void ClientGameCommandManager::SetFadeIn(Event *ev)
{
    if (!m_spawnthing) {
        return;
    }

    m_spawnthing->cgd.flags |= T_FADEIN;

    if (ev->NumArgs() > 0) {
        m_spawnthing->cgd.fadeintime = ev->GetFloat(1) * 1000;
    }
}

void ClientGameCommandManager::SetTwinkle(Event *ev)
{
    if (!m_spawnthing) {
        return;
    }

    m_spawnthing->cgd.flags |= T_TWINKLE;
    m_spawnthing->cgd.min_twinkletimeoff = ev->GetFloat(1) * 1000;
    m_spawnthing->cgd.max_twinkletimeoff = ev->GetFloat(2) * 1000;
    m_spawnthing->cgd.min_twinkletimeon  = ev->GetFloat(3) * 1000;
    m_spawnthing->cgd.max_twinkletimeon  = ev->GetFloat(4) * 1000;
}

void ClientGameCommandManager::SetTrail(Event *ev)
{
    if (!m_spawnthing) {
        return;
    }

    m_spawnthing->cgd.flags2 |= T2_TRAIL;
    m_spawnthing->cgd.swipe_shader    = ev->GetString(1);
    m_spawnthing->cgd.swipe_tag_start = ev->GetString(2);
    m_spawnthing->cgd.swipe_tag_end   = ev->GetString(3);
    m_spawnthing->cgd.swipe_life      = ev->GetFloat(4);
}

void ClientGameCommandManager::SetSpawnRate(Event *ev)
{
    if (!m_spawnthing) {
        return;
    }

    m_spawnthing->spawnRate = (float)(int)(1000.0f * (1.0f / ev->GetFloat(1)));
}

void ClientGameCommandManager::SetParentLink(Event *ev)
{
    m_spawnthing->cgd.flags |= T_PARENTLINK;
}

void ClientGameCommandManager::SetHardLink(Event *ev)
{
    if (!m_spawnthing) {
        return;
    }

    m_spawnthing->cgd.flags |= T_HARDLINK;
    m_spawnthing->cgd.parent = current_entity_number;

    if (current_entity) {
        m_spawnthing->cgd.origin = m_spawnthing->cgd.origin - current_entity->origin;
    }
}

void ClientGameCommandManager::SetColor(Event *ev)
{
    if (!m_spawnthing) {
        return;
    }

    m_spawnthing->cgd.color[0] = ev->GetFloat(1);
    m_spawnthing->cgd.color[1] = ev->GetFloat(2);
    m_spawnthing->cgd.color[2] = ev->GetFloat(3);

    if (ev->NumArgs() == 4) {
        m_spawnthing->cgd.color[3] = ev->GetFloat(4);
        m_spawnthing->cgd.alpha    = ev->GetFloat(4);
    }
}

void ClientGameCommandManager::SetColorRange(Event *ev)
{
    int i;

    if (!m_spawnthing) {
        return;
    }

    for (i = 0; i < 3; i++) {
        float lo = ev->GetFloat(i + 1);
        float hi = ev->GetFloat(i + 4);

        m_spawnthing->cgd.colorRangeMin[i] = (byte)(255.0f * lo);
        m_spawnthing->cgd.colorRangeMax[i] = (byte)(255.0f * hi);
    }

    m_spawnthing->cgd.hasColorRange = qtrue;

    if (ev->NumArgs() > 6) {
        m_spawnthing->cgd.colorRangeTimeMs = (int)(1000.0f * ev->GetFloat(7));
    }
}

void ClientGameCommandManager::SetSpinRate(Event *ev)
{
    if (!m_spawnthing) {
        return;
    }

    m_spawnthing->cgd.spinRate[0] = ev->GetFloat(1);
    m_spawnthing->cgd.spinRate[1] = ev->GetFloat(2);
    m_spawnthing->cgd.spinRate[2] = ev->GetFloat(3);
}

void ClientGameCommandManager::SetEmitterSpin(Event *ev)
{
    if (!m_spawnthing) {
        return;
    }

    m_spawnthing->cgd.flags2 |= T2_EMITTERSPIN;
}

void ClientGameCommandManager::SetParticleSpin(Event *ev)
{
    if (!m_spawnthing) {
        return;
    }

    m_spawnthing->cgd.flags2 |= T2_PARTICLESPIN;
}

void ClientGameCommandManager::SetDuplicateCount(Event *ev)
{
    if (!m_spawnthing) {
        return;
    }

    m_spawnthing->duplicateCount = ev->GetInteger(1);
}

void ClientGameCommandManager::SetTraceCount(Event *ev)
{
    if (!m_spawnthing) {
        return;
    }

    m_spawnthing->traceCount = ev->GetInteger(1);
}

void ClientGameCommandManager::SetRandomDirection(Event *ev)
{
    int i;
    int argindex;

    if (!m_spawnthing) {
        return;
    }

    if (ev->NumArgs() < 3) {
        warning("ClientGameCommandManager::SetRandomDirection", "Expecting at least 3 args for command randvel");
    }

    argindex = 1;
    for (i = 0; i < 3; i++) {
        argindex += CGGetRandomNumberData(
            ev,
            argindex,
            &m_spawnthing->randDirType[i],
            &m_spawnthing->randDirBase[i],
            &m_spawnthing->randDirAmp[i]
            );
    }

    m_spawnthing->cgd.flags2 |= T2_MOVE;
}

void ClientGameCommandManager::TBC_SetSpeed(Event *ev)
{
    TBC_DataPoint *dp;
    float          speed;

    if (!m_spawnthing) {
        return;
    }

    if (!m_spawnthing->m_tbcManager) {
        m_spawnthing->m_tbcManager = new TimeBasedCommandManager;
    }

    speed = ev->GetFloat(1);

    if (speed < CG_TBC_LITERAL_0) {
        cgi.Printf("");
        return;
    }

    dp = new TBC_DataPoint;

    dp->time  = ev->GetFloat(1);
    dp->value = ev->GetFloat(2);
    dp->type  = TBC_TYPE_SPEED;

    m_spawnthing->m_tbcManager->m_speedList.AddObject(dp);
}

void ClientGameCommandManager::TBC_SetSpawnRate(Event *ev)
{
    TBC_DataPoint *dp;
    float          rate;

    if (!m_spawnthing) {
        return;
    }

    if (!m_spawnthing->m_tbcManager) {
        m_spawnthing->m_tbcManager = new TimeBasedCommandManager;
    }

    rate = ev->GetFloat(1);

    dp = new TBC_DataPoint;

    dp->time  = CG_TBC_LITERAL_1000 * (CG_TBC_LITERAL_1 / rate);
    dp->value = ev->GetFloat(2);
    dp->type  = TBC_TYPE_SPAWNRATE;

    m_spawnthing->m_tbcManager->m_spawnRateList.AddObject(dp);
}

void ClientGameCommandManager::TBC_SetParticleSpin(Event *ev)
{
    TBC_DataPoint *dp;

    if (!m_spawnthing) {
        return;
    }

    if (!m_spawnthing->m_tbcManager) {
        m_spawnthing->m_tbcManager = new TimeBasedCommandManager;
    }

    dp = new TBC_DataPoint;

    dp->axis[0] = CG_TBC_LITERAL_0;                    
    dp->axis[1] = CG_TBC_LITERAL_0;                    
    dp->axis[2] = CG_TBC_LITERAL_0;                    

    dp->axis[0] = ev->GetFloat(1);                     
    dp->axis[1] = ev->GetFloat(2);                     
    dp->axis[2] = ev->GetFloat(3);                     
    dp->useBias = ev->GetInteger(4) ? qtrue : qfalse;  
    dp->value   = ev->GetFloat(5);                     
    dp->type    = TBC_TYPE_PARTICLESPIN;               

    m_spawnthing->m_tbcManager->m_particleSpinList.AddObject(dp);
}

void ClientGameCommandManager::TBC_SetDirection(Event *ev)
{
    TBC_DataPoint *dp;
    int            argindex;
    int            i;

    if (!m_spawnthing) {
        return;
    }

    if (!m_spawnthing->m_tbcManager) {
        m_spawnthing->m_tbcManager = new TimeBasedCommandManager;
    }

    argindex = 1;

    while (argindex <= ev->NumArgs()) {
        dp = new TBC_DataPoint;

        for (i = 0; i < 3; i++) {
            argindex += CGGetRandomNumberData(
                ev,
                argindex,
                (randtype_t *)&dp->dirType[i],
                &dp->dirBase[i],
                &dp->dirAmp[i]
                );
        }

        for (i = 0; i < 3; i++) {
            dp->axis[i] = CG_TBC_LITERAL_0;            
        }

        dp->value = ev->GetFloat(argindex);
        dp->type = TBC_TYPE_DIRECTION;

        argindex += 3;

        m_spawnthing->m_tbcManager->m_directionList.AddObject(dp);
    }
}


void ClientGameCommandManager::SetScaleDelay(Event *ev)
{
    if (!m_spawnthing) {
        return;
    }

    // ⚠️ CORRECTED (W26-REPORT2).  The earlier note here said retail never reads +0x508 back
    m_spawnthing->cgd.scaleDelay = 1000.0f * ev->GetFloat(1);
}

void ClientGameCommandManager::SetLinearScaleRate(Event *ev)
{
    if (!m_spawnthing) {
        return;
    }

    m_spawnthing->cgd.linearScaleRate = ev->GetFloat(1);
}

void ClientGameCommandManager::SetOneFrameOnly(Event *ev)
{
    if (!m_spawnthing) {
        return;
    }

    m_spawnthing->cgd.flags2 |= T2_ONEFRAMEONLY;
}

void ClientGameCommandManager::SetBeamWave(Event *ev)
{
    if (!m_spawnthing) {
        return;
    }

    m_spawnthing->beamflags = BEAM_WAVE;
}

void ClientGameCommandManager::SetFullBeamWave(Event *ev)
{
    if (!m_spawnthing) {
        return;
    }

    m_spawnthing->beamflags = BEAM_FULLWAVE;
}

void ClientGameCommandManager::SetAlwaysCreate(Event *ev)
{
    if (!m_spawnthing) {
        return;
    }

    m_spawnthing->alwaysCreateBeam = qtrue;
}

void ClientGameCommandManager::SetCanEndEarly(Event *ev)
{
    m_spawnthing->canEndEarly = qtrue;
}

void ClientGameCommandManager::SetChanceToEmitAtCamera(Event *ev)
{
    if (!m_spawnthing) {
        return;
    }

    m_spawnthing->chanceToEmitAtCamera = ev->GetFloat(1);
}

void ClientGameCommandManager::SetFreq(Event *ev)
{
    if (!m_spawnthing) {
        return;
    }

    m_spawnthing->wavespeed = ev->GetFloat(1);
}



void ClientGameCommandManager::SetItemRingShader(Event *ev)
{
    str name;

    if (!current_centity) {
        cgi.Com_WDPrintf(
            "CCM::SetItemRingShader : itemringshader command should be in animation section\n");
        return;
    }

    if (ev->NumArgs() < 1) {
        ev->Error("Index %d out of range.", 1);
        name = "";
    } else {
        name = ev->GetString(1);
    }

    current_centity->itemRingShader = cgi.R_RegisterShader(name);
}


void ClientGameCommandManager::SetExclusiveLightName(Event *ev)
{
    str           name;
    unsigned int  hash;
    dtiki_t      *tiki;

    if (ev->NumArgs() < 1) {
        ev->Error("Index %d out of range.", 1);
        name = "";
    } else {
        name = ev->GetString(1);
    }

    hash = GenerateHashForName(name, 0, 0);

    tiki = cgi.TIKI_GetTiki(current_tiki);
    if (tiki) {
        tiki->exclusive_light_name_hash = hash;
    }
}

void ClientGameCommandManager::FootstepSound(Event *ev)
{
    int   channel    = 0;
    float volume     = -1.0f;
    float minDistance = -1.0f;

    if (ev->NumArgs() > 0) {
        channel = ev->GetInteger(1);
    }
    if (ev->NumArgs() > 1) {
        volume = ev->GetFloat(2);
    } else {
        volume = 0.0f;
    }
    if (ev->NumArgs() > 2) {
        minDistance = ev->GetFloat(3);
    } else {
        minDistance = 0.0f;
    }

    CG_PlaySurfaceSound(current_centity, "snd_footstep", channel, volume, minDistance);
}

void ClientGameCommandManager::SurfaceSound(Event *ev)
{
    str   baseSurfaceSound;
    int   channel     = 0;
    float volume      = -1.0f;
    float minDistance = -1.0f;

    // There is NO m_spawnthing guard in retail: the handler is not tied to an emitter.
    baseSurfaceSound = ev->GetString(1);
    if (ev->NumArgs() > 1) {
        channel = ev->GetInteger(2);
    }
    if (ev->NumArgs() > 2) {
        volume = ev->GetFloat(3);
    } else {
        volume = 0.0f;
    }
    if (ev->NumArgs() > 3) {
        minDistance = ev->GetFloat(4);
    } else {
        minDistance = 0.0f;
    }

    CG_PlaySurfaceSound(current_centity, baseSurfaceSound.c_str(), channel, volume, minDistance);
}

void ClientGameCommandManager::TagTraceImpactSpawn(Event *ev)
{
    str tagname;

    tagname = ev->GetString(1);

    endblockfcn = &ClientGameCommandManager::EndTagTraceImpactSpawn;

    m_spawnthing = &m_localemitter;
    InitializeSpawnthing(m_spawnthing);

    m_spawnthing->traceImpactSpawn = qtrue;

    GetOrientation(tagname, m_spawnthing);
}

// 100-byte trace_t at TOC 0x64148 -- not an array -- and the loop counter never indexes
void ClientGameCommandManager::EndTagTraceImpactSpawn(void)
{

    int     i;
    Vector  start;
    Vector  aim0, aim1, aim2;
    Vector  end;
    trace_t trace;

    if (!m_spawnthing) {
        return;
    }

    start = m_spawnthing->cgd.origin;
    VectorCopy(m_spawnthing->aim_axis[0], aim0);
    VectorCopy(m_spawnthing->aim_axis[1], aim1);
    VectorCopy(m_spawnthing->aim_axis[2], aim2);

    for (i = 0; i < m_spawnthing->traceCount; i++) {
        if (m_spawnthing->useLastTrace) {
            trace = lastImpactTrace;
        } else {
            if (m_spawnthing->use_last_trace_end) {
                end = last_trace_end;
            } else {
                end = start + aim0 * m_spawnthing->length
                    + aim1 * (crandom() * m_spawnthing->spreadx)
                    + aim2 * (crandom() * m_spawnthing->spready);
                last_trace_end = end;
            }

            CG_Trace(
                &trace,
                start,
                vec_zero,
                vec_zero,
                end,
                ENTITYNUM_NONE,
                MASK_SHOT,
                qfalse,
                qtrue,
                "EndTagTraceImpactSpawn"
            );

            lastImpactTrace = trace;
        }

        static int uirows_i;
        if (PROBE_On() && PROBE_Throttle(&uirows_i, 16, 2000)) {
            UIROWS_PROBE(
                "impact:spawn",
                "ent=%d '%s' tag='%s' start=(%.1f,%.1f,%.1f) end=(%.1f,%.1f,%.1f) "
                "aim0=(%.2f,%.2f,%.2f) len=%.1f count=%d uselasttrace=%d uselastend=%d "
                "-> hit=(%.1f,%.1f,%.1f) frac=%.3f ent=%d surf=0x%x\n",
                current_entity_number,
                current_tiki > 0 ? cgi.TIKI_Name(current_tiki) : "?",
                m_spawnthing->tagname.c_str(),
                start[0], start[1], start[2],
                end[0], end[1], end[2],
                aim0[0], aim0[1], aim0[2],
                m_spawnthing->length,
                m_spawnthing->traceCount,
                m_spawnthing->useLastTrace,
                m_spawnthing->use_last_trace_end,
                trace.endpos[0], trace.endpos[1], trace.endpos[2],
                trace.fraction,
                trace.entityNum,
                trace.surfaceFlags);
        }

        GetSpawnEffect(trace);

        if ((trace.entityNum == ENTITYNUM_WORLD && !m_spawnthing->hitWorld)
            || (trace.entityNum != ENTITYNUM_WORLD && !m_spawnthing->hitEntities)) {
            break;
        }

        if (trace.fraction != 1.0f && !(trace.surfaceFlags & 4)) {
            VectorCopy(trace.plane.normal, m_spawnthing->aim_axis[0]);
            VectorCopy(m_spawnthing->aim_axis[0], m_spawnthing->tag_axis[0]);
            m_spawnthing->cgd.origin = trace.endpos + Vector(trace.plane.normal);

            SpawnTempModel(m_spawnthing->count, 0);
        }
    }
}

void ClientGameCommandManager::GetSpawnEffect(trace_t& trace)
{
    spawn_effectmap *map;
    spawn_effect    *chosen;
    int              surftype;
    int              i;

    if (!m_spawnthing) {
        return;
    }

    map = m_spawnthing->m_armedEffectMap;
    if (!map) {
        return;
    }

    surftype = trace.surfaceFlags;

    chosen = NULL;
    for (i = 1; i <= map->m_spawneffects.NumObjects(); i++) {
        spawn_effect *effect = map->m_spawneffects.ObjectAt(i);
        if (effect->surfacetype == surftype) {
            chosen = effect;
            break;
        }
    }
    if (!chosen) {
        chosen = map->m_defaultSpawn;
    }

    if (chosen) {
        CopySpawnthing(m_spawnthing, chosen->st);
    }

    m_spawnthing->m_armedEffectMap = NULL;
}

void ClientGameCommandManager::GetSoundEffect(const str &sound, trace_t &trace, str &out)
{
    spawn_effectmap *map;
    sound_effect    *chosen;
    int              surftype;
    int              i;

    map = NULL;
    for (i = 1; i <= m_effectMaps.NumObjects(); i++) {
        spawn_effectmap *candidate = m_effectMaps.ObjectAt(i);

        if (!strcmp(candidate->name.c_str(), sound.c_str())) {
            map = candidate;
            break;
        }
    }

    m_currentEffectMap = map;  

    if (!map) {
        return;
    }

    surftype = trace.surfaceFlags & 0xC7F1200;

    chosen = NULL;
    for (i = 1; i <= map->m_soundeffects.NumObjects(); i++) {
        sound_effect *effect = map->m_soundeffects.ObjectAt(i);

        if (effect->surfacetype == surftype) {
            chosen = effect;
            break;
        }
    }
    if (!chosen) {
        chosen = map->m_defaultSound;
    }

    if (chosen) {
        out = chosen->soundname;
    }
}

void ClientGameCommandManager::TagTraceImpactSound(Event *ev)
{
    vec3_t  end;
    trace_t trace;
    str     sound;
    int     isalias;
    int     i;

    if (!m_spawnthing) {
        return;
    }

    m_spawnthing->tagname = ev->GetString(1);
    GetOrientation(m_spawnthing->tagname, m_spawnthing);

    for (i = 0; i < 3; i++) {
        end[i] = m_spawnthing->cgd.origin[i] + m_spawnthing->aim_axis[0][i] * ev->GetFloat(2);
    }

    CG_Trace(
        &trace,
        m_spawnthing->cgd.origin,
        vec_zero,
        vec_zero,
        end,
        ENTITYNUM_NONE,
        MASK_SHOT,
        qfalse,
        qtrue,
        "TagTraceImpactSound"
    );

    if ((trace.entityNum != ENTITYNUM_WORLD || m_spawnthing->hitWorld)
        && (trace.entityNum == ENTITYNUM_WORLD || m_spawnthing->hitEntities)) {
        if (trace.fraction != 1.0f && !(trace.surfaceFlags & 4)) {
            sound   = ev->GetString(3);
            isalias = ev->GetInteger(4);

            if (isalias) {
                str resolved;

                GetSoundEffect(sound, trace, resolved);
                if (resolved.length()) {
                    PlaySound(resolved, trace.endpos, 1);
                }
            } else {
                PlaySound(sound, trace.endpos, 0);
            }
        }
    }
}

void ClientGameCommandManager::BeginTagTraceImpactMark(Event *ev)
{
    str tagname;
    int temporary;

    endblockfcn = &ClientGameCommandManager::EndTagTraceImpactMark;

    m_spawnthing = &m_localemitter;
    InitializeSpawnthing(m_spawnthing);

    tagname = ev->GetString(1);

    GetOrientation(tagname, m_spawnthing);

    if (ev->NumArgs() > 2) {
        temporary = ev->GetInteger(2);
        if (temporary) {
            m_spawnthing->cgd.flags2 |= T2_TEMPORARY_DECAL;
        }
    }
}

void ClientGameCommandManager::EndTagTraceImpactMark(void)
{

    int     i;
    Vector  start;
    Vector  aim0, aim1, aim2;
    Vector  end;
    trace_t trace;

    if (!m_spawnthing) {
        return;
    }

    start = m_spawnthing->cgd.origin;
    VectorCopy(m_spawnthing->aim_axis[0], aim0);
    VectorCopy(m_spawnthing->aim_axis[1], aim1);
    VectorCopy(m_spawnthing->aim_axis[2], aim2);

    for (i = 0; i < m_spawnthing->traceCount; i++) {
        if (m_spawnthing->useLastTrace) {
            trace = lastImpactTrace;
        } else {
            if (m_spawnthing->use_last_trace_end) {
                end = last_trace_end;
            } else {
                end = start + aim0 * m_spawnthing->length
                    + aim1 * (crandom() * m_spawnthing->spreadx)
                    + aim2 * (crandom() * m_spawnthing->spready);
                last_trace_end = end;
            }

            CG_Trace(
                &trace,
                start,
                vec_zero,
                vec_zero,
                end,
                ENTITYNUM_NONE,
                MASK_SHOT,
                qfalse,
                qtrue,
                "EndTagTraceImpactMark"
            );

            lastImpactTrace = trace;
        }

        if ((trace.entityNum == ENTITYNUM_WORLD && !m_spawnthing->hitWorld)
            || (trace.entityNum != ENTITYNUM_WORLD && !m_spawnthing->hitEntities)) {
            break;
        }

        GetSpawnEffect(trace);

        if (trace.fraction != 1.0f && !(trace.surfaceFlags & 4)) {
            CG_ImpactMark(
                cgi.R_RegisterShader(m_spawnthing->cgd.shadername),
                trace.endpos,
                trace.plane.normal,
                m_spawnthing->cgd.decal_orientation,
                m_spawnthing->cgd.color[0],
                m_spawnthing->cgd.color[1],
                m_spawnthing->cgd.color[2],
                m_spawnthing->cgd.alpha,
                m_spawnthing->cgd.flags & (1 << 26),
                m_spawnthing->cgd.decal_radius,
                qfalse ,
                m_spawnthing->cgd.flags2 & (1 << 26),
                qfalse  
            );
        }
    }
}


static spawn_effectmap *CG_FindEffectMap(Container<spawn_effectmap *>& maps, const char *name)
{
    int i;
    int len = (int)strlen(name);

    for (i = 1; i <= maps.NumObjects(); i++) {
        spawn_effectmap *m = maps.ObjectAt(i);
        if ((int)m->name.length() == len && strcmp(m->name.c_str(), name) == 0) {
            return m;
        }
    }

    return NULL;
}

void ClientGameCommandManager::EffectMap(Event *ev)
{
    str              name = ev->GetString(1);
    spawn_effectmap *map  = CG_FindEffectMap(m_effectMaps, name.c_str());

    if (map) {
        m_currentEffectMap = NULL;
        return;
    }

    map       = new spawn_effectmap;
    map->name = name;
    m_effectMaps.AddObject(map);

    m_currentEffectMap = map;
}

void ClientGameCommandManager::AddSpawnEffect(Event *ev)
{
    spawn_effect *effect;

    if (!m_currentEffectMap) {
        return;
    }

    effect                  = new spawn_effect;
    effect->surfacetypeName = ev->GetString(1);
    effect->name            = ev->GetString(2);
    effect->surfacetype     = cgi.SurfaceNameToType(effect->surfacetypeName.c_str());

    InitializeSpawnthing(&effect->st);

    if (effect->surfacetype == EF2_SURFACETYPE_DEFAULT) {
        m_currentEffectMap->m_defaultSpawn = effect;
    } else {
        m_currentEffectMap->m_spawneffects.AddObject(effect);
    }
}

void ClientGameCommandManager::AddSoundEffect(Event *ev)
{
    sound_effect *effect;

    if (!m_currentEffectMap) {
        return;
    }

    effect                  = new sound_effect;
    effect->surfacetypeName = ev->GetString(1);
    effect->name            = ev->GetString(2);
    effect->surfacetype     = cgi.SurfaceNameToType(effect->surfacetypeName.c_str());

    if (effect->surfacetype == EF2_SURFACETYPE_DEFAULT) {
        m_currentEffectMap->m_defaultSound = effect;
    } else {
        m_currentEffectMap->m_soundeffects.AddObject(effect);
    }
}

void ClientGameCommandManager::DefineSpawnEffect(Event *ev)
{
    str              effectname = ev->GetString(1);
    str              mapname    = ev->GetString(2);
    spawn_effectmap *map        = CG_FindEffectMap(m_effectMaps, mapname.c_str());
    int              i;

    if (!map) {
        return;
    }

    if (map->m_defaultSpawn
        && (int)strlen(effectname.c_str()) == (int)map->m_defaultSpawn->name.length()
        && strcmp(map->m_defaultSpawn->name.c_str(), effectname.c_str()) == 0) {
        m_spawnthing = &map->m_defaultSpawn->st;
        return;
    }

    for (i = 1; i <= map->m_spawneffects.NumObjects(); i++) {
        spawn_effect *effect = map->m_spawneffects.ObjectAt(i);
        if (effect->effectId == 0
            && (int)strlen(effectname.c_str()) == (int)effect->name.length()
            && strcmp(effect->name.c_str(), effectname.c_str()) == 0) {
            m_spawnthing = &effect->st;
            return;
        }
    }
}

void ClientGameCommandManager::DefineSoundEffect(Event *ev)
{
    str              effectname = ev->GetString(1);
    str              mapname    = ev->GetString(2);
    str              soundname  = ev->GetString(3);
    spawn_effectmap *map        = CG_FindEffectMap(m_effectMaps, mapname.c_str());
    int              i;

    if (!map) {
        return;
    }

    // The name tests are retail's length + imported strcmp (0x25xxx), not str::icmp.
    if (map->m_defaultSound
        && (int)strlen(effectname.c_str()) == (int)map->m_defaultSound->name.length()
        && strcmp(map->m_defaultSound->name.c_str(), effectname.c_str()) == 0) {
        map->m_defaultSound->soundname = soundname;
        return;
    }

    for (i = 1; i <= map->m_soundeffects.NumObjects(); i++) {
        sound_effect *effect = map->m_soundeffects.ObjectAt(i);
        if (effect->effectId == 1
            && (int)strlen(effectname.c_str()) == (int)effect->name.length()
            && strcmp(effect->name.c_str(), effectname.c_str()) == 0) {
            effect->soundname = soundname;
            return;
        }
    }
}

void ClientGameCommandManager::SpawnEffect(Event *ev)
{
    m_currentEffectMap = CG_FindEffectMap(m_effectMaps, ev->GetString(1));

    if (m_spawnthing) {
        m_spawnthing->m_armedEffectMap = m_currentEffectMap;
    }
}

void ClientGameCommandManager::HitEntities(Event *ev)
{
    if (!m_spawnthing) {
        return;
    }

    m_spawnthing->hitEntities = ev->GetBoolean(1);
}

void ClientGameCommandManager::HitWorld(Event *ev)
{
    if (!m_spawnthing) {
        return;
    }

    m_spawnthing->hitWorld = ev->GetBoolean(1);
}

void ClientGameCommandManager::SpawnEffectMapOnImpact(spawnthing_t *impactST, trace_t *trace)
{
    spawn_effectmap *map;
    spawn_effect    *chosen;
    int              surftype;
    int              i;

    if (!impactST || !trace) {
        return;
    }

    map = impactST->m_armedEffectMap;
    if (!map) {
        return;
    }

    if (trace->entityNum == ENTITYNUM_WORLD) {
        if (!impactST->hitWorld) {
            return;
        }
    } else {
        if (!impactST->hitEntities) {
            return;
        }
    }

    surftype = trace->surfaceFlags & 0xC7F1200;

    chosen = NULL;
    for (i = 1; i <= map->m_spawneffects.NumObjects(); i++) {
        spawn_effect *effect = map->m_spawneffects.ObjectAt(i);
        if (effect->surfacetype == surftype) {
            chosen = effect;
            break;
        }
    }
    if (!chosen) {
        chosen = map->m_defaultSpawn;
    }

    if (chosen) {
        int count;

        chosen->st.cgd.origin = trace->endpos;
        count                 = (chosen->st.count > 0) ? chosen->st.count : 1;
        SpawnEffect(count, &chosen->st);
    }

    {
        sound_effect *chosenSound = NULL;

        for (i = 1; i <= map->m_soundeffects.NumObjects(); i++) {
            sound_effect *se = map->m_soundeffects.ObjectAt(i);
            if (se->surfacetype == surftype) {
                chosenSound = se;
                break;
            }
        }
        if (!chosenSound) {
            chosenSound = map->m_defaultSound;
        }

        if (chosenSound && chosenSound->soundname.length()) {
            PlaySound(chosenSound->soundname, trace->endpos);
        }
    }

}

void ClientGameCommandManager::SetEntityColor(Event *ev)
{
    if (!m_spawnthing) {
        return;
    }

    if (current_centity) {
        current_centity->client_color[0] = ev->GetFloat(1);
        current_centity->client_color[1] = ev->GetFloat(2);
        current_centity->client_color[2] = ev->GetFloat(3);
        if (ev->NumArgs() == 4) {
            current_centity->client_color[3] = ev->GetFloat(4);
        }
    }
}

void ClientGameCommandManager::SetGlobalFade(Event *ev)
{
    str mode;

    if (!m_spawnthing) {
        return;
    }

    mode = ev->GetString(1);

    if (mode == "in") {
        m_spawnthing->cgd.flags |= T_GLOBALFADEIN;
    } else if (mode == "out") {
        m_spawnthing->cgd.flags |= T_GLOBALFADEOUT;
    } else {
        cgi.DPrintf("Illegal globalfade parm: %s\n", mode.c_str());
    }
}

void ClientGameCommandManager::SetRandomVelocity(Event *ev)
{
    Vector randval;
    str    vel;

    if (!m_spawnthing) {
        return;
    }

    if (ev->NumArgs() < 3) {
        warning("ClientGameCommandManager::SetRandomVelocity", "Expecting at least 3 args for command randvel");
    }

    CGSetRandomTriple(ev, m_spawnthing->randvel_base, m_spawnthing->randvel_amplitude, m_spawnthing->randvel_rand);

    m_spawnthing->cgd.flags2 |= T2_MOVE;
}

void ClientGameCommandManager::SetRandomVelocityAlongAxis(Event *ev)
{
    if (!m_spawnthing) {
        return;
    }

    m_spawnthing->cgd.flags |= T_RANDVELAXIS;
    SetRandomVelocity(ev);
}



void ClientGameCommandManager::SetVelocity(Event *ev)
{
    if (!m_spawnthing) {
        return;
    }

    m_spawnthing->forwardVelocity = ev->GetFloat(1);
    m_spawnthing->cgd.flags2 |= T2_MOVE;
}

void ClientGameCommandManager::SetAngularVelocity(Event *ev)
{
    Vector randval;
    str    vel;

    if (!m_spawnthing) {
        return;
    }

    if (ev->NumArgs() < 3) {
        warning("ClientGameCommandManager::SetAngularVelocity", "Expecting at least 3 args for command randvel");
    }

    CGSetRandomTriple(ev, m_spawnthing->avelocity_base, m_spawnthing->avelocity_amplitude, m_spawnthing->avelocity_rand);

    m_spawnthing->cgd.flags2 |= T2_AMOVE;
}

void ClientGameCommandManager::SetAngles(Event *ev)
{
    Vector randval;
    str    vel;

    if (!m_spawnthing) {
        return;
    }

    if (ev->NumArgs() < 3) {
        warning("ClientGameCommandManager::SetAngles", "Expecting at least 3 args for command randvel");
    }

    CGSetRandomTriple(ev, m_spawnthing->cgd.angles, m_spawnthing->angles_amplitude, m_spawnthing->angles_rand);

    m_spawnthing->cgd.flags |= T_ANGLES;
}


void ClientGameCommandManager::ParentAngles(Event *ev)
{
    if (!m_spawnthing) {
        return;
    }

    if (current_centity) {
        m_spawnthing->cgd.angles = Vector(current_centity->currentState.angles);
    }
    m_spawnthing->cgd.flags |= T_ANGLES;
}


void ClientGameCommandManager::SetAccel(Event *ev)
{
    float arg1, arg2, arg3, delay;

    if (!m_spawnthing) {
        return;
    }

    arg3 = ev->GetFloat(3);
    arg2 = ev->GetFloat(2);
    arg1 = ev->GetFloat(1);

    if (ev->NumArgs() <= 3) {
        delay = 0;
    } else {
        delay = ev->GetFloat(4) * 1000.0f;
    }

    m_spawnthing->cgd.accel[0] = arg1;
    m_spawnthing->cgd.accel[1] = arg2;
    m_spawnthing->cgd.accel[2] = arg3;

    m_spawnthing->cgd.flags2 |= T2_ACCEL;

    m_spawnthing->cgd.accelDelay = delay;
}

// retail; the wind arm preserves the same non-NULL invariant.
// NOTE: this call also mutates the LIVE spawnthing's cgd.flags2/accel, not the
// point.  Reproduced deliberately: retail's wind effect therefore persists on the
void ClientGameCommandManager::SetAccel(const Vector &accel, float delay)
{
    m_spawnthing->cgd.accel[0] = accel[0];
    m_spawnthing->cgd.accel[1] = accel[1];
    m_spawnthing->cgd.accel[2] = accel[2];

    m_spawnthing->cgd.flags2 |= T2_ACCEL;

    m_spawnthing->cgd.accelDelay = delay;
}

void ClientGameCommandManager::SetDrag(Event *ev)
{
    if (!m_spawnthing) {
        return;
    }

    m_spawnthing->cgd.drag = ev->GetFloat(1);

    if (ev->NumArgs() > 1) {
        m_spawnthing->cgd.dragDelay = ev->GetFloat(2) * 1000.0f;
    }
}



void ClientGameCommandManager::SetCount(Event *ev)
{
    if (!m_spawnthing) {
        return;
    }

    m_spawnthing->count = ev->GetInteger(1);
}

void ClientGameCommandManager::SetLife(Event *ev)
{
    str life;

    if (!m_spawnthing) {
        return;
    }

    life = ev->GetString(1);

    // Retail SetLife @0x0000E6A0 tests `autocalc' FIRST but does NOT return on it -- the
    if (!life.icmp("autocalc")) {
        m_spawnthing->cgd.flags |= T_AUTOCALCLIFE;
    }

    {
        float fBase = 0.0f;
        float fAmp  = 0.0f;

        CGGetRandomNumberData(ev, 1, &m_spawnthing->life_rand, &fBase, &fAmp);

        m_spawnthing->cgd.life    = (int)(fBase * 1000.0f);
        m_spawnthing->life_random = fAmp * 1000.0f;
    }
}

void ClientGameCommandManager::SetModel(Event *ev)
{
    if (!m_spawnthing) {
        return;
    }

    int i;
    int num = ev->NumArgs();

    for (i = 1; i <= num; i++) {
        str s_arg(ev->GetString(i));
        m_spawnthing->m_modellist.AddObject(s_arg);
        CacheResource(ev->GetString(i));
    }
}

void ClientGameCommandManager::TagList(Event *ev)
{
    int i;
    int num = ev->NumArgs();

    if (!m_spawnthing) {
        return;
    }

    if (num < 2) {
        warning(
            "CCG::TagList",
            "Invalid number of tags specified in taglist.  "
            "Minimum set of tags is 2.\n"
        );
        return;
    }

    for (i = 1; i <= num; i++) {
        str s_arg(ev->GetString(i));
        m_spawnthing->m_taglist.AddObject(s_arg);
    }
}



spawnthing_t *ClientGameCommandManager::InitializeSpawnthing(spawnthing_t *sp)
{
    int i;

    sp->m_modellist.ClearObjectList();
    AxisClear(sp->axis);
    AxisClear(sp->aim_axis);
    AxisClear(sp->tag_axis);

    sp->startoff                = qfalse;
    sp->origin_offset_base      = Vector(0, 0, 0);
    sp->origin_offset_amplitude = Vector(0, 0, 0);
    sp->axis_offset_base        = Vector(0, 0, 0);
    sp->axis_offset_amplitude   = Vector(0, 0, 0);
    sp->randvel_base            = Vector(0, 0, 0);
    sp->randvel_amplitude       = Vector(0, 0, 0);
    sp->avelocity_base          = Vector(0, 0, 0);
    sp->avelocity_amplitude     = Vector(0, 0, 0);
    sp->angles_amplitude        = Vector(0, 0, 0);
    sp->life_random             = 0;

    sp->randvel_rand[0]     = NOT_RANDOM;
    sp->randvel_rand[1]     = NOT_RANDOM;
    sp->randvel_rand[2]     = NOT_RANDOM;
    sp->offset_rand[0]      = NOT_RANDOM;
    sp->offset_rand[1]      = NOT_RANDOM;
    sp->offset_rand[2]      = NOT_RANDOM;
    sp->avelocity_rand[0]   = NOT_RANDOM;
    sp->avelocity_rand[1]   = NOT_RANDOM;
    sp->avelocity_rand[2]   = NOT_RANDOM;
    sp->angles_rand[0]      = NOT_RANDOM;
    sp->angles_rand[1]      = NOT_RANDOM;
    sp->angles_rand[2]      = NOT_RANDOM;
    sp->axis_offset_rand[0] = NOT_RANDOM;
    sp->axis_offset_rand[1] = NOT_RANDOM;
    sp->axis_offset_rand[2] = NOT_RANDOM;
    sp->life_rand           = NOT_RANDOM;
    sp->forwardVelocity         = 0;
    sp->sphereRadius            = 0;
    sp->coneHeight              = 0;
    sp->spawnRate               = 0.1f;
    sp->count                   = 1;
    sp->length                  = 1000;
    sp->max_offset              = 0;
    sp->min_offset              = 0;
    sp->overlap                 = 0;
    sp->numSubdivisions         = 1;
    sp->delay                   = 0;
    sp->toggledelay             = 0;
    sp->endalpha                = 1.0f;
    sp->wavespeed               = 0;
    sp->spreadx                 = 0;
    sp->spready                 = 0;
    sp->beamflags               = 0;
    sp->startoff                = 0;
    sp->numspherebeams          = 0;
    sp->use_last_trace_end      = qfalse;
    sp->useLastTrace            = qfalse;
    sp->hide                    = qfalse;
    sp->tagnum                  = -1;
    // not retail-offset-pinned here.
    sp->entnum                  = ENTITYNUM_NONE;

    sp->alwaysCreateBeam = qfalse;
    sp->canEndEarly      = qfalse;
    sp->hasLensFlare     = qfalse;
    sp->lensflareNum     = -1;

    sp->hitEntities      = qfalse;
    sp->hitWorld         = qtrue;

    sp->m_tbcManager        = NULL;
    sp->tbcController       = NULL;
    sp->tbcNextCommandTime  = 0;
    sp->tbcEmitterCursor    = 0;
    sp->tbcTimeOffset       = 0;

    sp->randDirBase      = Vector(0, 0, 0);
    sp->randDirAmp       = Vector(0, 0, 0);
    sp->randDirType[0]   = NOT_RANDOM;
    sp->randDirType[1]   = NOT_RANDOM;
    sp->randDirType[2]   = NOT_RANDOM;
    sp->duplicateCount   = 0;
    sp->traceCount       = 1;
    sp->windEffect       = 0;
    sp->chanceToEmitAtCamera = 0;

    sp->cgd.scale              = 1.0f;
    sp->cgd.createTime         = cg.time;
    sp->cgd.life               = EMITTER_DEFAULT_LIFE;
    sp->cgd.avelocity          = Vector(0, 0, 0);
    sp->cgd.accel              = Vector(0, 0, 0);
    sp->cgd.accelDelay         = 0;
    sp->cgd.origin             = Vector(0, 0, 0);
    sp->cgd.oldorigin          = Vector(0, 0, 0);
    sp->cgd.angles             = Vector(0, 0, 0);
    sp->cgd.alpha              = 1.0f;
    sp->cgd.scalemin           = 0;
    sp->cgd.scalemax           = 999999.0f;
    sp->cgd.scaleRate          = 0;
    sp->cgd.bouncefactor       = 0.3f;
    sp->cgd.bouncesound        = "";
    sp->cgd.bouncesound_delay  = 1000;
    sp->cgd.bouncecount        = 0;
    sp->cgd.maxbouncecount     = 3;
    sp->cgd.flags              = 0;
    sp->cgd.flags2             = 0;
    sp->cgd.swarmfreq          = 0;
    sp->cgd.swarmdelta         = 0;
    sp->cgd.swarmmaxspeed      = 0;
    sp->cgd.fadeintime         = 0;
    sp->cgd.fadedelay          = 0;
    sp->cgd.lightIntensity     = 0;
    sp->cgd.lightType          = 0;
    sp->cgd.collisionmask      = (CONTENTS_SOLID);
    sp->cgd.parent             = -1;
    sp->cgd.tiki               = -1;
    sp->cgd.lightstyle         = -1;
    sp->cgd.physicsRate        = cg_physics_fps->integer;
    sp->cgd.shadername         = "beamshader";
    sp->cgd.decal_orientation  = 0;
    sp->cgd.decal_radius       = 10;
    sp->cgd.max_twinkletimeoff = 0;
    sp->cgd.min_twinkletimeoff = 0;
    sp->cgd.max_twinkletimeon  = 0;
    sp->cgd.min_twinkletimeon  = 0;
    sp->cgd.swipe_life         = 0;
    sp->cgd.scale2             = 1;
    sp->cgd.spinRate[0]        = 0;
    sp->cgd.spinRate[1]        = 0;
    sp->cgd.spinRate[2]        = 0;
    sp->cgd.wavyDistance       = 0;
    sp->cgd.hasColorRange      = qfalse;
    sp->cgd.colorRangeTimeMs   = 0;
    sp->fMinRangeSquared       = 0;
    sp->fMaxRangeSquared       = 9.9999997e37f;

    for (i = 0; i < 3; i++) {
        sp->dcolor[i]    = 1.0f;
        sp->cgd.color[i] = 1.0;

        sp->cgd.colorRangeMin[i] = 255;
        sp->cgd.colorRangeMax[i] = 255;
    }

    sp->cgd.color[3] = 1.0;

    return sp;
}

void ClientGameCommandManager::SpawnEffect(int count, spawnthing_t *sp)
{
    m_spawnthing = sp;
    SpawnTempModel(count, 0);
    m_spawnthing = NULL;
}

void ClientGameCommandManager::SpawnEffect(int count, int timealive)
{
    if (!m_spawnthing) {
        return;
    }

    SpawnTempModel(count, 0);
}

spawnthing_t *ClientGameCommandManager::GetEmitterByName(str name)
{
    int i;
    for (i = 1; i <= m_emitters.NumObjects(); i++) {
        spawnthing_t *st = m_emitters.ObjectAt(i);
        if (st->emittername == name) {
            return st;
        }
    }
    return NULL;
}

spawnthing_t *ClientGameCommandManager::CreateNewEmitter(str name)
{
    spawnthing_t *st;

    st = new spawnthing_t;

    InitializeSpawnthing(st);

    st->emittername = name;

    m_emitters.AddObject(st);

    return st;
}

void ClientGameCommandManager::DeleteEmitters(int tiki)
{
    int           i;
    spawnthing_t *spawnthing;

    for (i = m_emitters.NumObjects(); i > 0; i--) {
        spawnthing = m_emitters.ObjectAt(i);
        if (spawnthing->cgd.tiki == tiki) {
            FreeSpawnthing(spawnthing);
        }
    }
}

void CG_DeleteEmitters(int tiki)
{
    commandManager.DeleteEmitters(tiki);
}

spawnthing_t *ClientGameCommandManager::CreateNewEmitter(void)
{
    return CreateNewEmitter("");
}

void ClientGameCommandManager::BeginOriginSpawn(Event *ev)
{
    if (!current_entity) {
        m_spawnthing = NULL;
        return;
    }

    endblockfcn = &ClientGameCommandManager::EndOriginSpawn;

    m_spawnthing = &m_localemitter;
    if (!m_spawnthing) {
        return;
    }
    InitializeSpawnthing(m_spawnthing);

    m_spawnthing->cgd.origin = current_entity->origin;

    AxisCopy(current_entity->axis, m_spawnthing->aim_axis);
}

void ClientGameCommandManager::EndOriginSpawn(void)
{
        SpawnEffect(m_spawnthing->count, 0);
}

void ClientGameCommandManager::BeginTagEmitter(Event *ev)
{
    str tagname;
    int  tagnum;
    int  i;


    if (ev->NumArgs() > 2) {
        LoadExternalEmitter(ev, 1);
        return;
    }

    tagname = ev->GetString(1);

    if (!tagname.length()) {
        warning(
            "CCM::BeginTagEmitter", "Tagname not specified for tagemitter in model: '%s'\n", cgi.TIKI_Name(current_tiki)
        );
    }

    // it, exactly like retail's emitter that never finds its tag.
    tagnum = cgi.Tag_NumForName(current_tiki, tagname.c_str());

    endblockfcn = &ClientGameCommandManager::EndTagEmitter;


    if (ev->NumArgs() > 1) {
        str name = ev->GetString(2);

        for (i = m_emitters.NumObjects(); i > 0; i--) {
            spawnthing_t *sp = m_emitters.ObjectAt(i);

            if (sp->emittername == name && sp->cgd.tiki == current_tiki) {
                m_emitters.RemoveObjectAt(i);
            }
        }
    }

    m_spawnthing               = CreateNewEmitter();
    m_spawnthing->tagnum       = tagnum;
    m_spawnthing->tagname      = tagname;
    m_spawnthing->emittername  = ev->GetString(2);
    if (!m_spawnthing->emittername.length()) {
        warning(
            "CCM::BeginTagEmitter",
            "Emittername not specified for tagemitter in model: '%s'\n",
            cgi.TIKI_Name(current_tiki)
        );
    }

    m_spawnthing->cgd.tiki = current_tiki;
}

void ClientGameCommandManager::EndTagEmitter(void)
{
    endblockfcn = NULL;
}

void ClientGameCommandManager::BeginTagBeamEmitter(Event *ev)
{
    endblockfcn = &ClientGameCommandManager::EndTagBeamEmitter;


    if (ev->NumArgs() > 1) {
        str name = ev->GetString(2);
        int  i;

        for (i = m_emitters.NumObjects(); i > 0; i--) {
            spawnthing_t *sp = m_emitters.ObjectAt(i);

            if (sp->emittername == name && sp->cgd.tiki == current_tiki) {
                m_emitters.RemoveObjectAt(i);
            }
        }
    }

    m_spawnthing = CreateNewEmitter();
    m_spawnthing->cgd.flags |= T_BEAMTHING;


    m_spawnthing->startTag = ev->GetString(1);

    if (m_spawnthing->startTag == "USE_TAGLIST") {
        if (ev->NumArgs() == 2) {
            m_spawnthing->emittername = ev->GetString(2);
        }
    } else {
        if (ev->NumArgs() == 3) {
            m_spawnthing->emittername = ev->GetString(2);
            m_spawnthing->endTag      = ev->GetString(3);
        } else if (ev->NumArgs() == 2) {
            m_spawnthing->emittername = ev->GetString(2);
        }
    }

    m_spawnthing->cgd.tiki = current_tiki;
}

void ClientGameCommandManager::EndTagBeamEmitter(void)
{
    endblockfcn = NULL;
}

void ClientGameCommandManager::BeginOriginEmitter(Event *ev)
{
    if (ev->NumArgs() > 1) {
        LoadExternalEmitter(ev, 0);
        return;
    }

    endblockfcn = &ClientGameCommandManager::EndOriginEmitter;

    m_spawnthing = CreateNewEmitter();

    m_spawnthing->emittername = ev->GetString(1);

    if (!m_spawnthing->emittername.length()) {
        warning(
            "CCM::BeginOriginEmitter",
            "Emittername not specified for originemitter in model: '%s'\n",
            cgi.TIKI_Name(current_tiki)
        );
    }

    m_spawnthing->cgd.tiki = current_tiki;
}

void ClientGameCommandManager::EndOriginEmitter(void)
{
    endblockfcn = NULL;
}

void ClientGameCommandManager::LoadExternalEmitter(Event *ev, int bTagEmitter)
{
    str       tagname;
    str       emittername;
    str       filename;
    str       full_filename;
    int       startoff = 0;
    int       i, j;
    qhandle_t model;
    int       handle;
    tiki_cmd_t tiki_cmd;

    if (bTagEmitter) {
        tagname     = ev->GetString(1);
        emittername = ev->GetString(2);
        filename    = ev->GetString(3);

        if (ev->NumArgs() > 3) {
            startoff = ev->GetInteger(4) != 0;
        }
    } else {
        emittername = ev->GetString(1);
        filename    = ev->GetString(2);

        if (ev->NumArgs() > 2) {
            startoff = ev->GetInteger(3) != 0;
        }
    }

    if (strncmp(filename, "models", 6)) {
        full_filename = "models/" + filename;
    } else {
        full_filename = filename;
    }

    model  = cgi.R_RegisterModel(full_filename);
    handle = cgi.R_Model_GetHandle(model);
    if (handle < 0) {
        return;
    }

    if (!cgi.TIKI_InitCommands_Client(handle, &tiki_cmd)) {
        return;
    }

    for (i = 0; i < tiki_cmd.num_cmds; i++) {
        tiki_singlecmd_t *cmd = &tiki_cmd.cmds[i];
        Event             *e;

        if (!strcmp(cmd->args[0], "originemitter") || !strcmp(cmd->args[0], "tagemitter")) {
            int skip = 3 - (strcmp(cmd->args[0], "originemitter") == 0);

            e = new Event(bTagEmitter ? EV_Client_TagEmitter : EV_Client_OriginEmitter);

            if (bTagEmitter) {
                e->AddToken(tagname);
            }

            e->AddToken(emittername);

            if (skip < cmd->num_args) {
                e->AddToken(cmd->args[skip]);
                e->AddInteger(startoff);
            }

            commandManager.ProcessEvent(e);

            if (skip >= cmd->num_args && startoff) {
                commandManager.ProcessEvent(new Event(EV_Client_EmitterStartOff));
            }
        } else {
            e = new Event(cmd->args[0]);

            for (j = 1; j < cmd->num_args; j++) {
                e->AddToken(cmd->args[j]);
            }

            commandManager.ProcessEvent(e);
        }
    }
}

void ClientGameCommandManager::BeginOriginBeamEmitter(Event *ev)
{


    if (ev->NumArgs() > 1) {
        str name = ev->GetString(1);
        int  i;

        for (i = m_emitters.NumObjects(); i > 0; i--) {
            spawnthing_t *sp = m_emitters.ObjectAt(i);

            if (sp->emittername == name && sp->cgd.tiki == current_tiki) {
                m_emitters.RemoveObjectAt(i);
            }
        }
    }

    endblockfcn = &ClientGameCommandManager::EndOriginBeamEmitter;

    m_spawnthing = CreateNewEmitter();
    m_spawnthing->emittername = ev->GetString(1);

    if (!m_spawnthing->emittername.length()) {
        warning(
            "CCM::BeginOriginEmitter",
            "Emittername not specified for originemitter in model: '%s'\n",
            cgi.TIKI_Name(current_tiki)
        );
    }

    m_spawnthing->cgd.tiki = current_tiki;
    m_spawnthing->cgd.flags |= T_BEAMTHING;
}

void ClientGameCommandManager::EndOriginBeamEmitter(void)
{
    endblockfcn = NULL;
}

void ClientGameCommandManager::GetOrientation(int tagnum, spawnthing_t *sp)
{
    int           i;
    orientation_t _or;

    assert(current_entity);
    assert(current_tiki);

    if (!current_entity || current_tiki < 0) {
        return;
    }

    cgi.TIKI_Tag_LerpOrientation(&_or, current_tiki, current_entity, tagnum);

    {
        static int widget_res;

        if (WIDGET_On() && WIDGET_Throttle(&widget_res, 16, 48)) {
            const char *widget_tn = cgi.TIKI_Name(current_tiki);

            WIDGET_A1_PROBE(
                "tag:resolve",
                "host=%d tiki='%s' tagnum=%d or=(%.2f,%.2f,%.2f) ax0=(%.3f,%.3f,%.3f) "
                "hostorg=(%.1f,%.1f,%.1f) legsLive=%d torsoLive=%d rawanim=%d maskanim=%d "
                "oldanim=%d frame=%d state_anim=%d fail=%d"
                " csc=%.6g esc=%.6g nsc=%.6g fi=%.6g lsc=%.6g\n",
                current_entity_number,
                widget_tn ? widget_tn : "?",
                tagnum,
                _or.origin[0], _or.origin[1], _or.origin[2],
                _or.axis[0][0], _or.axis[0][1], _or.axis[0][2],
                current_entity->origin[0], current_entity->origin[1], current_entity->origin[2],
                current_entity->legsLive,
                current_entity->torsoLive,
                current_entity->anim,
                current_entity->anim & ANIM_MASK,
                current_entity->oldanim,
                current_entity->frame,
                (current_centity ? current_centity->animstate[0].base.anim : -1),
                (_or.origin[0] == 0.0f && _or.origin[1] == 0.0f && _or.origin[2] == 0.0f
                 && _or.axis[0][0] == 1.0f && _or.axis[0][1] == 0.0f && _or.axis[0][2] == 0.0f)
                    ? 1
                    : 0,
                current_entity->scale,
                (current_centity ? current_centity->currentState.scale : -1.0f),
                (current_centity ? current_centity->nextState.scale : -1.0f),
                cg.frameInterpolation,
                (current_entity->tiki ? current_entity->tiki->load_scale : -1.0f));
        }
    }

    {
    }

    VectorCopy(current_entity->origin, sp->cgd.origin);

    for (i = 0; i < 3; i++) {
        VectorMA(sp->cgd.origin, _or.origin[i], current_entity->axis[i], sp->cgd.origin);
    }

    MatrixMultiply(_or.axis, current_entity->axis, sp->axis);

    AxisCopy(sp->axis, sp->aim_axis);

}

Vector ClientGameCommandManager::GetViewEndPoint(int entnum, Vector org, qboolean useEndpoint)
{
    vec3_t      axis[3];
    vec3_t      vieworg, end, dir_view, dir_org;
    trace_t     trace1, trace2;
    cvar_t      *znear;
    float       *crosshair;
    float       tan_y, tan_x, ofs_y, ofs_x;
    float       dist_view, len, dot;
    int         i;

    AnglesToAxis(cg.refdefViewAngles, axis);

    VectorCopy(cg.refdef.vieworg, vieworg);

    znear = cgi.Cvar_Get("r_znear", "2", 0);
    crosshair = cgi.get_crosshairoffset();

    tan_y = znear->value * (float)tan(M_PI * cg.refdef.fov_y / 360.0);
    tan_x = znear->value * (float)tan(M_PI * cg.refdef.fov_x / 360.0);

    ofs_y = crosshair[1] * (-tan_y / 240.0f);
    ofs_x = crosshair[0] * (-tan_x / 320.0f);

    for (i = 0; i < 3; i++) {
        end[i] = vieworg[i] + (axis[0][i] * znear->value + axis[1][i] * ofs_x + axis[2][i] * ofs_y) * 2000.0f;
    }

    CG_Trace(&trace1, vieworg, vec3_origin, vec3_origin, end, entnum, MASK_SHOT, qfalse, qtrue, "GetViewEndPoint1");
    CG_Trace(&trace2, org, vec3_origin, vec3_origin, trace1.endpos, entnum, MASK_SHOT, qfalse, qtrue, "GetViewEndPoint2");

    VectorSubtract(trace1.endpos, vieworg, dir_view);
    VectorSubtract(trace1.endpos, org, dir_org);

    dist_view = VectorLength(dir_view);

    len = VectorLength(dir_org);
    if (len != 0.0f) {
        VectorScale(dir_org, 1.0f / len, dir_org);
    }

    len = VectorLength(dir_view);
    if (len != 0.0f) {
        VectorScale(dir_view, 1.0f / len, dir_view);
    }

    dot = DotProduct(dir_org, dir_view);

    if (trace2.startsolid || dot < 0.707f) {
        VectorMA(vieworg, 100.0f + dist_view, axis[0], end);
    }

    if (useEndpoint) {
        return Vector(end);
    }

    VectorSubtract(end, org, dir_org);
    len = VectorLength(dir_org);
    if (len != 0.0f) {
        VectorScale(dir_org, 1.0f / len, dir_org);
    }

    VectorMA(org, 0.814f + len, dir_org, end);

    return Vector(end);
}

static void GetOrientationAimAxis(const vec3_t dir, vec3_t axis[3])
{
    vec3_t angles;
    float  yaw, pitch;

    yaw = 0.0f;
    pitch = 90.0f;

    if (dir[0] != 0.0f || dir[1] != 0.0f) {
        yaw = (float)(180.0 * atan2(dir[1], dir[0]) / M_PI);
        if (yaw < 0.0f) {
            yaw += 360.0f;
        }
        pitch = (float)(180.0 * atan2(dir[2], sqrt(dir[0] * dir[0] + dir[1] * dir[1])) / M_PI);
        if (pitch < 0.0f) {
            pitch += 360.0f;
        }
    } else if (dir[2] <= 0.0f) {
        pitch = 270.0f;
    }

    VectorSet(angles, -pitch, yaw, 0.0f);
    AngleVectors(angles, axis[0], axis[1], axis[2]);
}

// "Tagname '%s' does not exist" does not exist in the retail cgame image and its
void ClientGameCommandManager::GetOrientation(const str& tagname, spawnthing_t *sp)
{
    int       centnum;
    centity_t *cent;
    vec3_t    fwd, right, up, end, dir;

    if (!current_tiki) {
        return;
    }

    int tagnum = cgi.Tag_NumForName(current_tiki, tagname.c_str());
    if (tagnum == -1) {
        warning("GetOrientation", "Tagname %s in model %s", tagname.c_str(), cgi.TIKI_Name(current_tiki));
        return;
    }

    GetOrientation(tagnum, sp);

    if (current_centity) {
        centnum = current_centity->currentState.parent;
        if (centnum != ENTITYNUM_NONE && centnum != ENTITYNUM_WORLD) {
            cent = &cg_entities[centnum];

            if (!Q_stricmpn(sp->tagname.c_str(), "tag_barrel", 10)
                || !Q_stricmpn(sp->startTag.c_str(), "tag_barrel", 10)
                || !Q_stricmpn(tagname.c_str(), "tag_barrel", 10)) {
                VectorClear(fwd);
                VectorClear(right);
                VectorClear(up);

                {
                    static int widget_armgate;

                    if (WIDGET_On() && WIDGET_Throttle(&widget_armgate, 12, 16)) {
                        WIDGET_A1_PROBE(
                            "beam:armgate",
                            "beamowner=%d tag='%s' startTag='%s' parent=%d pclient=%d me=%d 3rd=%d "
                            "esv=(%.1f,%.1f) esorg=(%.0f,%.0f,%.0f)\n",
                            current_entity_number,
                            tagname.c_str(),
                            sp->startTag.c_str(),
                            centnum,
                            cent->currentState.clientNum,
                            cg.snap ? cg.snap->ps.clientNum : -1,
                            cg.renderingThirdPerson,
                            cent->currentState.viewangles[1],
                            cent->currentState.viewangles[0],
                            cent->currentState.origin[0],
                            cent->currentState.origin[1],
                            cent->currentState.origin[2]);
                    }
                }

                if (cent->currentState.clientNum != cg.snap->ps.clientNum || cg.renderingThirdPerson) {
                    if (cent->currentState.clientNum != ENTITYNUM_NONE) {
                        trace_t trace;
                        vec3_t  start;
                        vec3_t  axis[3];

                        VectorCopy(cent->currentState.origin, start);
                        start[2] += (float)cg.snap->ps.viewheight;
                        AngleVectors(cent->currentState.viewangles, fwd, right, up);
                        VectorMA(start, 6000.0f, fwd, end);

                        CG_Trace(&trace, start, vec3_origin, vec3_origin, end,
                                 ENTITYNUM_NONE, MASK_SHOT, qfalse, qtrue, "GetOrientation");

                        VectorSubtract(trace.endpos, sp->cgd.origin, dir);
                        GetOrientationAimAxis(dir, axis);
                        AxisCopy(axis, sp->aim_axis);    

                        {
                            static int widget_aimarm;

                            if (WIDGET_On() && WIDGET_Throttle(&widget_aimarm, 12, 8)) {
                                vec3_t aimfwd;
                                float  esv_yaw, esv_pitch, aim_yaw, aim_pitch;
                                float  dx, dy, dz, dl;

                                AngleVectors(cent->currentState.viewangles, aimfwd, NULL, NULL);
                                esv_yaw   = vectoyaw(cent->currentState.viewangles);
                                esv_pitch = cent->currentState.viewangles[0];

                                dx = axis[0][0];
                                dy = axis[0][1];
                                dz = axis[0][2];
                                dl = sqrt(dx * dx + dy * dy + dz * dz);
                                if (dl > 0.0f) {
                                    aim_yaw   = vectoyaw(axis[0]);
                                    aim_pitch = 180.0f * asin(dz / dl) / M_PI;
                                } else {
                                    aim_yaw   = 0.0f;
                                    aim_pitch = 0.0f;
                                }

                                WIDGET_A1_PROBE(
                                    "beam:aimarm",
                                    "beamowner=%d tag='%s' parent=%d pclient=%d me=%d 3rd=%d "
                                    "esv=(%.1f,%.1f) porg=(%.0f,%.0f,%.0f) fwd=(%.2f,%.2f,%.2f) "
                                    "tstart=(%.0f,%.0f,%.0f) tend=(%.0f,%.0f,%.0f) hit=%d "
                                    "aim0=(%.2f,%.2f,%.2f) aimYaw=%.1f aimPitch=%.1f refYaw=%.1f\n",
                                    current_entity_number,
                                    tagname.c_str(),
                                    centnum,
                                    cent->currentState.clientNum,
                                    cg.snap ? cg.snap->ps.clientNum : -1,
                                    cg.renderingThirdPerson,
                                    esv_yaw, esv_pitch,
                                    cent->currentState.origin[0], cent->currentState.origin[1],
                                    cent->currentState.origin[2],
                                    aimfwd[0], aimfwd[1], aimfwd[2],
                                    start[0], start[1], start[2],
                                    trace.endpos[0], trace.endpos[1], trace.endpos[2],
                                    trace.fraction < 1.0f,
                                    axis[0][0], axis[0][1], axis[0][2],
                                    aim_yaw, aim_pitch, cg.refdefViewAngles[1]);
                            }
                        }
                    }
                } else {
                    Vector endpt = GetViewEndPoint(cent->currentState.number, sp->cgd.origin, qtrue);
                    vec3_t axis[3];

                    VectorSubtract(endpt, sp->cgd.origin, dir);
                    GetOrientationAimAxis(dir, axis);
                    AxisCopy(axis, sp->aim_axis);

                    {
                    }
                }
            }
        }
    }

    if (!(sp->cgd.flags & T_ANGLES)) {
        vectoangles(sp->aim_axis[0], sp->cgd.angles);
    }

    AxisCopy(sp->axis, sp->tag_axis);
}

void ClientGameCommandManager::GetOrientation(const str& tagname, orientation_t *ori)
{
    int tagnum;

    if (!current_tiki || !current_entity) {
        return;
    }

    tagnum = cgi.Tag_NumForName(current_tiki, tagname.c_str());
    if (tagnum == -1) {
        warning("GetOrientation", "Tagname %s in model %s", tagname.c_str(), cgi.TIKI_Name(current_tiki));
        return;
    }

    cgi.TIKI_Tag_LerpOrientation(ori, current_tiki, current_entity, tagnum);
}


void ClientGameCommandManager::BeginTagSpawn(Event *ev)
{
    str tagname;

    tagname = ev->GetString(1);

    endblockfcn = &ClientGameCommandManager::EndTagSpawn;

    m_spawnthing = &m_localemitter;
    InitializeSpawnthing(m_spawnthing);

    GetOrientation(tagname, m_spawnthing);
}

void ClientGameCommandManager::EndTagSpawn(void)
{
    SpawnEffect(m_spawnthing->count, 0);
}

void ClientGameCommandManager::BeginTagBeamSpawn(Event *ev)
{
    str tagname;

    tagname = ev->GetString(1);

    endblockfcn = &ClientGameCommandManager::EndTagBeamSpawn;

    m_spawnthing = &m_localemitter;
    InitializeSpawnthing(m_spawnthing);

    GetOrientation(tagname, m_spawnthing);
}

void ClientGameCommandManager::EndTagBeamSpawn(void)
{
    int     i;
    int     renderfx;
    Vector  end;
    trace_t trace;
    float   scale = 1.0f;

    if (current_entity) {
        scale = current_entity->scale;
    }

    for (i = 0; i < 3; i++) {
        m_spawnthing->cgd.origin +=
            Vector(m_spawnthing->tag_axis[i])
            * (m_spawnthing->axis_offset_base[i] + m_spawnthing->axis_offset_amplitude[i] * random());
    }

    // cached global trace_t (TOC 0x64148) over its local and NEVER CALLS CG_Trace -- the
    if (m_spawnthing->useLastTrace) {
        trace = lastImpactTrace;
    } else {
        if (m_spawnthing->use_last_trace_end) {
            end = last_trace_end;
        } else {
            end = m_spawnthing->cgd.origin + Vector(m_spawnthing->aim_axis[0]) * m_spawnthing->length
                + Vector(m_spawnthing->aim_axis[1]) * crandom() * m_spawnthing->spreadx
                + Vector(m_spawnthing->aim_axis[2]) * crandom() * m_spawnthing->spready;
            last_trace_end = end;
            widget_last_trace_end_time = cg.time;
        }

        {
            static int widget_lt;

            if (WIDGET_On() && WIDGET_Throttle(&widget_lt, 10, 40)) {
                WIDGET_A1_PROBE(
                    "beam:lasttrace",
                    "beamowner=%d name='%s' useLastTrace=%d useLastEnd=%d end=(%.1f,%.1f,%.1f) "
                    "age=%d start=(%.1f,%.1f,%.1f) len=%.1f\n",
                    current_entity_number,
                    m_spawnthing->emittername.c_str(),
                    m_spawnthing->useLastTrace,
                    m_spawnthing->use_last_trace_end,
                    end[0], end[1], end[2],
                    cg.time - widget_last_trace_end_time,
                    m_spawnthing->cgd.origin[0], m_spawnthing->cgd.origin[1],
                    m_spawnthing->cgd.origin[2],
                    m_spawnthing->length);
            }
        }

        CG_Trace(
            &trace,
            m_spawnthing->cgd.origin,
            vec_zero,
            vec_zero,
            end,
            ENTITYNUM_NONE,
            MASK_SHOT,
            qfalse,
            qtrue,
            "EndTagBeamSpawn"
        );

        lastImpactTrace = trace;
    }

    if (current_entity) {
        renderfx = (current_entity->renderfx & ~(RF_FLAGS_NOT_INHERITED | RF_LIGHTING_ORIGIN));
    } else {
        renderfx = 0;
    }

    byte modulate[4];

    for (i = 0; i < 4; i++) {
        modulate[i] = (byte)(m_spawnthing->cgd.color[i] * 255.0f);
    }

    {
    }

    CG_CreateBeam(
        m_spawnthing->cgd.origin,
        vec_zero,
        current_entity_number,
        1,
        m_spawnthing->cgd.alpha,
        m_spawnthing->cgd.scale * scale,
        (m_spawnthing->beamflags | BEAM_LIGHTNING_EFFECT),
        m_spawnthing->length,
        m_spawnthing->cgd.life,
        m_spawnthing->alwaysCreateBeam,
        trace.endpos,
        m_spawnthing->min_offset,
        m_spawnthing->max_offset,
        m_spawnthing->overlap,
        m_spawnthing->numSubdivisions,
        m_spawnthing->delay,
        m_spawnthing->cgd.shadername,
        modulate,
        m_spawnthing->numspherebeams,
        m_spawnthing->sphereRadius,
        m_spawnthing->toggledelay,
        m_spawnthing->endalpha,
        renderfx,
        m_spawnthing->emittername,
        m_spawnthing->wavespeed,
        m_spawnthing->canEndEarly,
        CGResolveRandomNumber(
            m_spawnthing->randvel_rand[0],
            m_spawnthing->randvel_base.x,
            m_spawnthing->randvel_amplitude.x
        )
    );
}

void ClientGameCommandManager::BeginOriginBeamSpawn(Event *ev)
{
    m_spawnthing = &m_localemitter;
    InitializeSpawnthing(m_spawnthing);

    m_spawnthing->cgd.origin = current_entity->origin;

    AxisCopy(current_entity->axis, m_spawnthing->aim_axis);
    MatrixToEulerAngles(m_spawnthing->aim_axis, m_spawnthing->cgd.angles);

    endblockfcn = &ClientGameCommandManager::EndOriginBeamSpawn;
}

void ClientGameCommandManager::EndOriginBeamSpawn(void)
{
    int     i, renderfx, c, count;
    Vector  end;
    trace_t trace;
    float   scale = 1.0f;

    if (current_entity) {
        scale = current_entity->scale;
    }

    count = m_spawnthing->count * cg_effectdetail->value;

    for (c = 0; c <= count; c++) {
        Vector angles;

        for (i = 0; i < 3; i++) {
            angles[i] = m_spawnthing->cgd.angles[i] + random() * m_spawnthing->angles_amplitude[i];

            m_spawnthing->cgd.origin +=
                Vector(m_spawnthing->tag_axis[i])
                * (m_spawnthing->axis_offset_base[i] + m_spawnthing->axis_offset_amplitude[i] * random());
        }

        AnglesToAxis(angles, m_spawnthing->axis);
        AxisCopy(m_spawnthing->axis, m_spawnthing->aim_axis);
        end = m_spawnthing->cgd.origin + Vector(m_spawnthing->aim_axis[0]) * m_spawnthing->length
            + Vector(m_spawnthing->aim_axis[1]) * crandom() * m_spawnthing->spreadx
            + Vector(m_spawnthing->aim_axis[2]) * crandom() * m_spawnthing->spready;

        CG_Trace(
            &trace,
            m_spawnthing->cgd.origin,
            vec_zero,
            vec_zero,
            end,
            ENTITYNUM_NONE,
            MASK_SHOT,
            qfalse,
            qtrue,
            "EndOriginBeamSpawn"
        );

        if (current_entity) {
            renderfx = (current_entity->renderfx & ~(RF_FLAGS_NOT_INHERITED | RF_LIGHTING_ORIGIN));
        } else {
            renderfx = 0;
        }

        byte modulate[4];

        for (i = 0; i < 4; i++) {
            modulate[i] = (byte)(m_spawnthing->cgd.color[i] * 255.0f);
        }

        CG_CreateBeam(
            m_spawnthing->cgd.origin,
            vec_zero,
            current_entity_number,
            1,
            m_spawnthing->cgd.alpha,
            m_spawnthing->cgd.scale * scale,
            (m_spawnthing->beamflags | BEAM_LIGHTNING_EFFECT),
            m_spawnthing->length,
            m_spawnthing->cgd.life,
            qtrue,
            trace.endpos,
            m_spawnthing->min_offset,
            m_spawnthing->max_offset,
            m_spawnthing->overlap,
            m_spawnthing->numSubdivisions,
            m_spawnthing->delay,
            m_spawnthing->cgd.shadername,
            modulate,
            m_spawnthing->numspherebeams,
            m_spawnthing->sphereRadius,
            m_spawnthing->toggledelay,
            m_spawnthing->endalpha,
            renderfx,
            m_spawnthing->emittername,
            m_spawnthing->wavespeed,
            m_spawnthing->canEndEarly,
            CGResolveRandomNumber(
                m_spawnthing->randvel_rand[0],
                m_spawnthing->randvel_base.x,
                m_spawnthing->randvel_amplitude.x
            )
        );
    }
}

extern EmitterLoader emitterLoader;

// port-only "load once" guard) is retail's separate free function
void ClientGameCommandManager::RestartEmitterClocks(int time)
{
    int i;

    for (i = 1; i <= m_emitters.NumObjects(); i++) {
        spawnthing_t *st = m_emitters.ObjectAt(i);

        if (st->cgd.createTime > time) {
            st->cgd.createTime -= time;
            st->lastTime -= time;
        }
    }
}

void ClientGameCommandManager::InitializeEmitters(void)
{
}

void CG_InitializeClientEmitters(void)
{
    Script script;

    Com_Printf("Loading client emitters...\n");

    script.LoadFile("global/emitters.scr");
    emitterLoader.Load(script);
}

void ClientGameCommandManager::EmitterOn(Event *ev)
{
    emitterOn(ev->GetString(1));
}

void ClientGameCommandManager::EmitterOff(Event *ev)
{
    emitterOff(ev->GetString(1));
}

// sentinel for an out-of-range index, where retail's read is simply undefined.
static int CG_EmitterInstanceNumber(int entity_number)
{
    if (entity_number < 0 || entity_number >= MAX_GENTITIES) {
        return -1;
    }

    return cg_entities[entity_number].currentState.instanceNumber;
}

void ClientGameCommandManager::emitterOn(const str& emittername)
{
    int i;

    for (i = 1; i <= m_emitters.NumObjects(); i++) {
        spawnthing_t *st = m_emitters.ObjectAt(i);

        if (st->emittername == emittername) {
            emittertime_t *et;

            et = st->GetEmitTime(current_entity_number, CG_EmitterInstanceNumber(current_entity_number));

            et->active         = qtrue;
            et->last_emit_time = cg.time;
            et->lerp_emitter   = qfalse;

            st->spreadstarttime = cg.time;
        }
    }
}

void ClientGameCommandManager::emitterOff(const str& emittername)
{
    int i;

    for (i = 1; i <= m_emitters.NumObjects(); i++) {
        spawnthing_t *st = m_emitters.ObjectAt(i);

        if (st->emittername == emittername) {
            emittertime_t *et;

            et = st->GetEmitTime(current_entity_number, CG_EmitterInstanceNumber(current_entity_number));
            et->active = qfalse;

            if (st->cgd.flags & T_BEAMTHING) {
                CG_KillBeams(current_entity_number);
            }
        }
    }
}

void CG_TurnEmitterOn(const char *emittername, entityState_t *es)
{
    int saved_number;

    saved_number          = current_entity_number;
    current_entity_number = es->number;


    commandManager.emitterOn(str(emittername));

    current_entity_number = saved_number;
}

void ClientGameCommandManager::PlaySound(
    const str   &sound_name,
    const vec3_t origin,
    int          channel,
    float        volume,
    float        min_distance,
    float        pitch
)
{
    const char *name = NULL;

    if (current_tiki >= 0) {
        name = cgi.TIKI_Alias_FindRandom(current_tiki, sound_name.c_str());
    }

    if (!name) {
        name = cgi.Alias_FindRandom(sound_name.c_str());
    }

    if (!name) {
        name = sound_name.c_str();
    }

    sfxHandle_t handle = cgi.S_RegisterSound(name);

    {
        static cvar_t *movsnd_movsnd = NULL;
        static int     movsnd_rows;

        if (!movsnd_movsnd) {
            movsnd_movsnd = cgi.Cvar_Get("qa_movsnd", "1", 0);   
        }

        movsnd_rows++;

        if (movsnd_movsnd && movsnd_movsnd->integer && (movsnd_rows <= 48 || (movsnd_rows % 16) == 0)) {
            cgi.Printf(
                "[MOVSND:movsnd] built='%s' resolved='%s' handle=%d entnum=%d chan=%d vol=%.3f min_dist=%.3f pitch=%.3f origin=%s\n",
                sound_name.c_str(),
                name,
                handle,
                current_entity_number,
                channel,
                volume,
                min_distance,
                pitch,
                origin ? "vec" : "NULL"
            );
        }
    }

    cgi.S_StartSound(
        origin,
        current_entity_number,
        channel,
        handle,
        volume,
        min_distance,
        pitch
    );
}

void ClientGameCommandManager::Sound(Event *ev)
{
    static cvar_t *cg_modifypitch = cgi.Cvar_Get("cg_modifypitch", "1", CVAR_ARCHIVE);

    int   channel = CHAN_AUTO;
    str   sound_name;
    float volume       = -1.0;
    float min_distance = -1.0;
    float pitch_min    = 1.0;
    float pitch_max    = 1.0;
    float pitch        = 1.0;

    // Retail Sound @0x0001A700 does NOT drop a sound when there is no current entity.
    // origin) and `lfs f3, 8(r28)' at 0x1AD28 (the literal 1.0f pitch, NOT the computed
    if (ev->NumArgs() < 1) {
        return;
    }


    sound_name = ev->GetString(1);

    if (ev->NumArgs() > 1) {
        channel = ev->GetInteger(2);
    }

    if (ev->NumArgs() > 2) {
        volume = ev->GetFloat(3);
    }

    if (ev->NumArgs() > 3) {
        min_distance = ev->GetFloat(4);
    }

    if (ev->NumArgs() > 4) {
        pitch_min = ev->GetFloat(5);
    }

    if (ev->NumArgs() > 5) {
        pitch_max = ev->GetFloat(6);
    }

    if (pitch_max != pitch_min && cg_modifypitch->integer) {
        if (pitch_min > pitch_max) {
            pitch = pitch_min;
        } else {
            pitch = (pitch_max - pitch_min) * random() + pitch_min;
        }
    }

    if (current_entity) {
        PlaySound(sound_name, current_entity->origin, channel, volume, min_distance, pitch);
    } else {
        PlaySound(sound_name, NULL, channel, volume, min_distance, 1.0f);
    }
}

void ClientGameCommandManager::StopSound(Event *ev)
{
    int channel;

    if (ev->NumArgs() > 0) {
        channel = ev->GetInteger(1);
    } else {
        return;
    }

    cgi.S_StopSound(current_entity_number, channel);
}


void ClientGameCommandManager::LoopSound(Event *ev)
{
    str         sound_name;
    float       volume   = -1.0;
    float       min_dist = -1.0;
    const char *name     = NULL;

    if (!current_centity) {
        cgi.DPrintf("CCM::LoopSound : LoopSound in %s without current_centity\n", cgi.TIKI_Name(current_tiki));
        return;
    }

    if (ev->NumArgs() < 1) {
        return;
    }

    sound_name = ev->GetString(1);

    if (ev->NumArgs() > 1) {
        volume = ev->GetFloat(2);
    }

    if (ev->NumArgs() > 2) {
        min_dist = ev->GetFloat(3);
    }

    name = NULL;

    if (current_tiki >= 0) {
        name = cgi.TIKI_Alias_FindRandom(current_tiki, sound_name.c_str());
    }

    if (!name) {
        name = cgi.Alias_FindRandom(sound_name.c_str());
    }

    if (!name) {
        name = sound_name.c_str();
    }

    current_centity->tikiLoopSound        = cgi.S_RegisterSound(name);
    current_centity->tikiLoopSoundVolume  = volume;
    current_centity->tikiLoopSoundMinDist = min_dist;
}


Container<int> cg_CachedTikis;

void CacheResource(const char *stuff)
{
    str real_stuff;

    if (stuff) {
        real_stuff = stuff;
    }
    real_stuff.tolower();

    if (strstr(real_stuff.c_str(), ".wav")) {
        cgi.S_RegisterSound(real_stuff.c_str());
    } else if (strstr(real_stuff.c_str(), ".mp3")) {
        cgi.S_RegisterSound(real_stuff.c_str());
    } else if (strstr(real_stuff.c_str(), ".tik")) {
        qhandle_t hModel = cgi.R_RegisterModel(real_stuff.c_str());

        cg_CachedTikis.AddUniqueObject(hModel);
    } else if (strstr(real_stuff.c_str(), ".ska")) {
        cgi.TIKI_CacheAnim(real_stuff.c_str());
    } else if (strstr(real_stuff.c_str(), ".spr")) {
        cgi.R_RegisterShader(real_stuff.c_str());
    }
}

void ClientGameCommandManager::Cache(Event *ev)
{
    if (ev->NumArgs() < 1) {
        return;
    }

    CacheResource(ev->GetString(1));
}



void AliasResource(int pmdl, const char *alias, const char *realname, const char *parameters)
{
    if (pmdl < 0) {
        cgi.Alias_Add(alias, realname, parameters);
        return;
    }

    cgi.TIKI_Alias_Add(pmdl, alias, realname, parameters);
}


void ClientGameCommandManager::AliasCache(Event *ev)
{
    str parmbuffer;
    int i;

    if (ev->NumArgs() < 2) {
        return;
    }

    for (i = 3; i <= ev->NumArgs(); i++) {
        parmbuffer += ev->GetToken(i);
        parmbuffer += " ";
    }

    AliasResource(current_tiki, ev->GetString(1), ev->GetString(2), parmbuffer.c_str());
    CacheResource(ev->GetString(2));
}

void ClientGameCommandManager::Alias(Event *ev)
{
    str parmbuffer;
    int i;

    if (ev->NumArgs() < 2) {
        return;
    }

    for (i = 3; i <= ev->NumArgs(); i++) {
        parmbuffer += ev->GetToken(i);
        parmbuffer += " ";
    }

    AliasResource(current_tiki, ev->GetString(1), ev->GetString(2), parmbuffer.c_str());
}

// -- retail dead code.  The alias field is never authored: shipped global/footprints.txt
void ClientGameCommandManager::Footstep(Event *ev)
{
    str sTagName;
    str sCreatureType;

    if (ev->NumArgs() > 0) {
        sTagName = ev->GetString(1);
    }

    if (ev->NumArgs() > 1) {
        sCreatureType = ev->GetString(2);
    }

    if (current_centity) {
        CG_Footstep(current_centity, sTagName.c_str(), sCreatureType.c_str());
    }
}



void ClientGameCommandManager::Client(Event *ev)
{
    Event      *event;
    const char *eventname;
    int         i;

    if (ev->NumArgs() < 1) {
        return;
    }

    eventname = ev->GetString(1);
    event     = new Event(eventname);

    for (i = 2; i <= ev->NumArgs(); i++) {
        event->AddToken(ev->GetToken(i));
    }
    ProcessEvent(event);
}

void ClientGameCommandManager::TagDynamicLight(Event *ev)
{
    str tagname;

    m_spawnthing = &m_localemitter;
    InitializeSpawnthing(m_spawnthing);

    tagname = ev->GetString(1);

    GetOrientation(tagname, m_spawnthing);

    m_spawnthing->cgd.flags |= T_DLIGHT;
    m_spawnthing->cgd.color[0]       = ev->GetFloat(2);
    m_spawnthing->cgd.color[1]       = ev->GetFloat(3);
    m_spawnthing->cgd.color[2]       = ev->GetFloat(4);
    m_spawnthing->cgd.color[3]       = 1.0;
    m_spawnthing->cgd.lightIntensity = ev->GetFloat(5);
    m_spawnthing->cgd.life           = ev->GetFloat(6) * 1000;
    if (ev->NumArgs() > 6) {
        m_spawnthing->cgd.lightType |= DLightNameToNum(ev->GetString(7));
        if (ev->NumArgs() > 7) {
            m_spawnthing->cgd.lightType |= DLightNameToNum(ev->GetString(8));
        }
    } else {
        m_spawnthing->cgd.lightType = 0;
    }
    SpawnEffect(1, m_spawnthing);
}

void ClientGameCommandManager::OriginDynamicLight(Event *ev)
{
    str tagname;

    if (!current_entity) {
        m_spawnthing = NULL;
        return;
    }

    m_spawnthing = &m_localemitter;
    InitializeSpawnthing(m_spawnthing);

    m_spawnthing->cgd.origin = current_entity->origin;
    m_spawnthing->cgd.flags |= T_DLIGHT;
    m_spawnthing->cgd.color[0]       = ev->GetFloat(1);
    m_spawnthing->cgd.color[1]       = ev->GetFloat(2);
    m_spawnthing->cgd.color[2]       = ev->GetFloat(3);
    m_spawnthing->cgd.color[3]       = 1.0;
    m_spawnthing->cgd.lightIntensity = ev->GetFloat(4);
    m_spawnthing->cgd.life           = ev->GetFloat(5) * 1000;
    if (ev->NumArgs() > 5) {
        m_spawnthing->cgd.lightType |= DLightNameToNum(ev->GetString(6));
        if (ev->NumArgs() > 6) {
            m_spawnthing->cgd.lightType |= DLightNameToNum(ev->GetString(7));
        }
    } else {
        m_spawnthing->cgd.lightType = 0;
    }
    SpawnEffect(1, m_spawnthing);
}

void ClientGameCommandManager::DynamicLight(Event *ev)
{
    if (!m_spawnthing) {
        return;
    }

    m_spawnthing->cgd.flags |= T_DLIGHT;
    m_spawnthing->dcolor[0]          = ev->GetFloat(1);
    m_spawnthing->dcolor[1]          = ev->GetFloat(2);
    m_spawnthing->dcolor[2]          = ev->GetFloat(3);
    m_spawnthing->cgd.lightIntensity = ev->GetFloat(4);
    if (ev->NumArgs() > 4) {
        m_spawnthing->cgd.lightType |= (dlighttype_t)ev->GetInteger(5);
        if (ev->NumArgs() > 5) {
            m_spawnthing->cgd.lightType |= DLightNameToNum(ev->GetString(6));
        }
    } else {
        m_spawnthing->cgd.lightType = 0;
    }
}

// did, would light the world from an emitter that retail never lights from, and would do it
void ClientGameCommandManager::LensFlare(Event *ev)
{
    if (!m_spawnthing) {
        return;
    }

    m_spawnthing->hasLensFlare = qtrue;

    m_spawnthing->lensflareNum = cgi.R_LensflareNameToNum(ev->GetString(1));

    m_spawnthing->dcolor[0] = ev->GetFloat(2);
    m_spawnthing->dcolor[1] = ev->GetFloat(3);
    m_spawnthing->dcolor[2] = ev->GetFloat(4);

    if (ev->NumArgs() > 4) {
        m_spawnthing->cgd.scale = ev->GetFloat(5);
    } else {
        m_spawnthing->cgd.scale = 1.0f;
    }

    if (ev->NumArgs() > 5) {
        m_spawnthing->cgd.shadername = ev->GetString(6);
    } else {
        m_spawnthing->cgd.shadername = "";
    }
}

// A beam naming a tag its model does not have is NOT fatal in retail.  Proven against the
static void CG_BeamTagMissing(const char *tagname, const char *context)
{
    static Container<str> warned;
    int                   i;

    if (!tagname || !*tagname) {
        return;
    }

    for (i = 1; i <= warned.NumObjects(); i++) {
        if (!warned.ObjectAt(i).icmp(tagname)) {
            return;
        }
    }

    warned.AddObject(str(tagname));
    cgi.DPrintf("CG_BeamTagMissing: tag '%s' not found on the current model (%s) -- beam skipped\n",
                tagname, context);
}

void ClientGameCommandManager::UpdateBeam(
    int tiki, int entity_number, int instance_number, spawnthing_t *beamthing
)
{
    Vector         start, end;
    int            renderfx;
    int            iLife;
    int            mi;
    qboolean       addstartpoint;
    float          scale, alpha;
    emittertime_t *et;

    et = beamthing->GetEmitTime(entity_number, instance_number);

    if (!et->active) {
        return;
    }

    if (current_entity) {
        renderfx = (current_entity->renderfx & ~(RF_FLAGS_NOT_INHERITED | RF_LIGHTING_ORIGIN));
        scale    = current_entity->scale;
        alpha    = (float)current_entity->shaderRGBA[3] / 255.0f;
    } else {
        renderfx = 0;
        scale    = 1.0f;
        alpha    = 1.0f;
    }

    iLife = m_spawnthing->cgd.life;
    if (m_spawnthing->life_random != 0.0) {
        iLife = (int)(random() * m_spawnthing->life_random + (float)iLife);
    }

    if (beamthing->startTag == "USE_TAGLIST") {
        int    i, count;
        str    startTag, endTag;
        int    tagnum;
        Vector dir;

        count = beamthing->m_taglist.NumObjects();

        if (!count) {
            return;
        }

        startTag = beamthing->m_taglist.ObjectAt(1);
        tagnum   = cgi.Tag_NumForName(current_tiki, startTag.c_str());
        if (tagnum == -1) {
            CG_BeamTagMissing(startTag.c_str(), "UpdateBeam taglist start");
            return;
        }

        // GetOrientation(int, spawnthing_t *) does not exist in retail at all, and calling
        GetOrientation(startTag, beamthing);
        start = beamthing->cgd.origin;
        if (beamthing->beamflags & BEAM_OFFSET_ENDPOINTS) {
            dir = Vector(crandom(), crandom(), crandom());
            start += (crandom() * beamthing->min_offset * dir) + (crandom() * beamthing->max_offset * dir);
        }

        CG_MultiBeamBegin();
        addstartpoint = qtrue;

        for (i = 2; i <= count; i++) {
            str s;

            s = va("%d", i);

            endTag = beamthing->m_taglist.ObjectAt(i);
            tagnum = cgi.Tag_NumForName(current_tiki, endTag.c_str());
            if (tagnum == -1) {
                CG_BeamTagMissing(endTag.c_str(), "UpdateBeam taglist segment");
                continue;
            }

            GetOrientation(endTag, beamthing);  
            end = beamthing->cgd.origin;

            if (beamthing->beamflags & BEAM_OFFSET_ENDPOINTS) {
                dir = Vector(crandom(), crandom(), crandom());
                end += (crandom() * beamthing->min_offset * dir) + (crandom() * beamthing->max_offset * dir);
            }

            CG_MultiBeamAddPoints(
                start,
                end,
                beamthing->numSubdivisions,
                beamthing->beamflags,
                beamthing->min_offset,
                beamthing->max_offset,
                addstartpoint
            );

            addstartpoint = qfalse;
            startTag      = endTag;
            start         = end;
        }

        byte newcolor[4];

        newcolor[0] = (byte)(beamthing->cgd.color[0] * alpha * 255.0);
        newcolor[1] = (byte)(beamthing->cgd.color[1] * alpha * 255.0);
        newcolor[2] = (byte)(beamthing->cgd.color[2] * alpha * 255.0);
        newcolor[3] = (byte)(beamthing->cgd.color[3] * alpha * 255.0);

        CG_MultiBeamEnd(
            beamthing->cgd.scale * scale,
            renderfx,
            beamthing->cgd.shadername,
            newcolor,
            beamthing->beamflags,
            entity_number,
            beamthing->cgd.life
        );
    } else {
        if (beamthing->startTag.length()) {
            int tagnum = cgi.Tag_NumForName(current_tiki, beamthing->startTag.c_str());
            if (tagnum == -1) {
                CG_BeamTagMissing(beamthing->startTag.c_str(), "UpdateBeam startTag");
                return;
            }

            GetOrientation(beamthing->startTag, beamthing);
            start = beamthing->cgd.origin;
        } else {
            start = beamthing->cgd.origin;
        }

        if (beamthing->endTag.length()) {
            int tagnum = cgi.Tag_NumForName(current_tiki, beamthing->endTag.c_str());
            if (tagnum == -1) {
                CG_BeamTagMissing(beamthing->endTag.c_str(), "UpdateBeam endTag");
                return;
            }

            GetOrientation(beamthing->endTag, beamthing);
            end = beamthing->cgd.origin;
        } else {
            float sx = beamthing->spreadx;
            float sy = beamthing->spready;

            if (sx == 0.0f && sy == 0.0f) {
                end = start + Vector(beamthing->aim_axis[0]) * beamthing->length;
            } else {
                Vector forward = Vector(beamthing->aim_axis[0]);
                Vector right(forward[1], -forward[0], 0);
                Vector up(
                    -forward[2] * forward[0],
                    -forward[2] * forward[1],
                    forward[0] * forward[0] + forward[1] * forward[1]
                );

                if (beamthing->spreadtimedelta != 0.0f) {
                    float frac = (float)((double)(cg.time - beamthing->spreadstarttime) * 0.001
                                         / beamthing->spreadtimedelta);

                    if (frac > 1.0f) {
                        frac = 1.0f;
                    }

                    sx = sx + (beamthing->endspreadx - sx) * frac;
                    sy = sy + (beamthing->endspready - sy) * frac;
                }

                end = start + forward * beamthing->length + right * (crandom() * sx)
                    + up * (crandom() * sy);
            }
        }

        byte beammodulate[4];

        for (mi = 0; mi < 4; mi++) {
            beammodulate[mi] = (byte)(beamthing->cgd.color[mi] * 255.0f);
        }

        {
        }

        CG_CreateBeam(
            start,
            vec_zero,
            entity_number,
            1,
            beamthing->cgd.alpha * alpha,
            beamthing->cgd.scale * scale,
            (beamthing->beamflags | BEAM_LIGHTNING_EFFECT),
            beamthing->length,
            iLife,
            qfalse,
            end,
            beamthing->min_offset,
            beamthing->max_offset,
            beamthing->overlap,
            beamthing->numSubdivisions,
            beamthing->delay,
            beamthing->cgd.shadername,
            beammodulate,
            beamthing->numspherebeams,
            beamthing->sphereRadius,
            beamthing->toggledelay,
            beamthing->endalpha,
            renderfx,
            beamthing->emittername,
            beamthing->wavespeed,
            beamthing->canEndEarly,
            CGResolveRandomNumber(
                beamthing->randvel_rand[0],
                beamthing->randvel_base.x,
                beamthing->randvel_amplitude.x
            )
        );
    }
}

// The two int parameters are NOT the same thing, and retail's call site gives them
// floats at a time out of r20 (0x0001F0DC / 0x1F268 / 0x1F330) -- not passed by value.
void ClientGameCommandManager::UpdateEmitter(
    int tiki, vec3_t axis[3], int entity_number, int instance_number, int parent_link_number,
    const Vector& entity_origin
)
{
    int            parent, lastparent, i;
    emittertime_t *et = NULL;
    centity_t     *pc;
    int            count = 0;
    float          scale = 1.0f;

    // only the T_WAVE branch that retail does not have, so it is gone.
    if (current_entity) {
        scale = current_entity->scale;
    }

    for (i = 1; i <= m_emitters.NumObjects(); i++) {
        m_spawnthing = m_emitters.ObjectAt(i);

        if (m_spawnthing->cgd.tiki != tiki) {
            continue;
        }

        et = m_spawnthing->GetEmitTime(entity_number, instance_number);

        if (!et->active) {
            continue;
        }

        {
            static int effects_emitmatch_n;
            const char *effects_emit_host = (tiki >= 0) ? cgi.TIKI_Name(tiki) : NULL;

            if (PROBE_On() && PROBE_Throttle(&effects_emitmatch_n, 24, 512)) {
                PROBE(
                    "emit:match",
                    "emitter=%d model='%s' emittername='%s' tiki=%d hosttiki='%s' "
                    "ent=%d hostent=%d inst=%d et_ent=%d et_inst=%d active=%d "
                    "hostorg=(%.1f,%.1f,%.1f)\n",
                    i,
                    m_spawnthing->GetModel().c_str(),
                    m_spawnthing->emittername.c_str(),
                    m_spawnthing->cgd.tiki,
                    effects_emit_host ? effects_emit_host : "?",
                    entity_number,
                    (current_centity ? current_centity->currentState.number : -1),
                    instance_number,
                    et->entity_number,
                    et->instance_number,
                    (int)et->active,
                    entity_origin[0], entity_origin[1], entity_origin[2]
                );
            }
        }

        if ((m_spawnthing->cgd.flags & T_DETAIL) && !cg_detail->integer) {
            et->last_emit_time = 0;
            continue;
        }

        if ((m_spawnthing->cgd.flags2 & T2_ONEFRAMEONLY) && et->last_emit_time != cg.time) {
            et->active = qfalse;
            CG_KillBeams(et->entity_number);
            continue;
        }

        VectorCopy(entity_origin, m_spawnthing->cgd.origin);

        if (m_spawnthing->cgd.flags & T_BEAMTHING) {
            if (m_spawnthing->cgd.flags & T_ANGLES) {
                vec3_t angleAxis[3];

                AnglesToAxis(m_spawnthing->cgd.angles, angleAxis);
                AxisCopy(angleAxis, m_spawnthing->aim_axis);
            }

            UpdateBeam(tiki, entity_number, instance_number, m_spawnthing);
            continue;
        }
        if (m_spawnthing->tagname.length()) {
            // Retail tests the tag NAME, not the number: 0x0001F1A0 `lwz r0, 0x7b4(r5)'
            GetOrientation(m_spawnthing->tagname, m_spawnthing);
        } else if (axis) {
            AxisCopy(axis, m_spawnthing->aim_axis);
        } else {
            AxisClear(m_spawnthing->aim_axis);
        }

        if (m_spawnthing->cgd.flags & T_HARDLINK) {
            m_spawnthing->cgd.parent = entity_number;
            m_spawnthing->cgd.origin -= entity_origin;
        } else if (m_spawnthing->cgd.flags & T_PARENTLINK) {
            lastparent = ENTITYNUM_NONE;

            parent = parent_link_number;

            while (parent != ENTITYNUM_NONE) {
                pc         = &cg_entities[parent];
                lastparent = parent;
                parent     = pc->currentState.parent;
            }

            if (lastparent != ENTITYNUM_NONE) {
                pc = &cg_entities[lastparent];

                m_spawnthing->cgd.origin -= pc->lerpOrigin;
                m_spawnthing->cgd.parent = lastparent;
            }
        }

        m_spawnthing->cgd.createTime = cg.time;
        // (@0x00011700, the CCM method, not the free function) writes only cgd.origin

        if (m_spawnthing->cgd.flags & T_DLIGHT) {
            cgi.R_AddLightToScene(
                m_spawnthing->cgd.origin,
                m_spawnthing->cgd.lightIntensity * scale,
                m_spawnthing->dcolor[0],
                m_spawnthing->dcolor[1],
                m_spawnthing->dcolor[2],
                m_spawnthing->cgd.lightType
            );
            continue;
        }

        if (m_spawnthing->hasLensFlare) {
            qhandle_t replacementShader = 0;

            if (m_spawnthing->cgd.shadername.length()) {
                replacementShader = cgi.R_RegisterShader(m_spawnthing->cgd.shadername);
            }

            cgi.R_AddLensflareToScene(
                (void *)(intptr_t)entity_number,
                m_spawnthing->cgd.origin,
                m_spawnthing->lensflareNum,
                m_spawnthing->dcolor[0],
                m_spawnthing->dcolor[1],
                m_spawnthing->dcolor[2],
                m_spawnthing->cgd.scale,
                replacementShader
            );
            continue;
        }

        Vector save_origin = m_spawnthing->cgd.origin;
        Vector delta       = m_spawnthing->cgd.origin - et->oldorigin;

        if (m_spawnthing->spawnRate < 0) {
            warning("ClientGameCommandManager::UpdateEmitter", "Invalid spawnrate (negative)");
            continue;
        }

        if (paused->integer) {
            et->last_emit_time = 0;
        } else if ((et->last_emit_time > 0) && (m_spawnthing->spawnRate)) {
            int   dtime = cg.time - et->last_emit_time;
            float lerp, lerpfrac;

            if (et->last_emit_time > cg.time) {
                et->last_emit_time = cg.time;
            }

            if (dtime > 2000) {
                dtime = 2000;
            }

            count = dtime * cg_effectdetail->value / m_spawnthing->spawnRate;
            et->last_emit_time += count * m_spawnthing->spawnRate;

            if (!count) {
                continue;
            } else if (count == 1) {
                SpawnEffect(1, 0);
                et->last_emit_time = cg.time;
            } else {
                lerpfrac = 1.0f / (float)count;

                lerp = 0;
                while (dtime > (m_spawnthing->spawnRate * (1.0f / cg_effectdetail->value))) {
                    et->last_emit_time = cg.time;

                    dtime -= m_spawnthing->spawnRate * (1.0f / cg_effectdetail->value);

                    if (et->lerp_emitter) {
                        m_spawnthing->cgd.origin = et->oldorigin + (delta * lerp);
                    }

                    SpawnEffect(1, dtime);

                    et = m_spawnthing->GetEmitTime(entity_number, instance_number);
                    if (!et) {
                        break;
                    }

                    lerp += lerpfrac;
                }
            }
        } else {
            et->last_emit_time = cg.time;
        }

        if (cg_showemitters->integer) {
            if (count) {
                cgi.DPrintf("%d:%s:%d tempmodels emitted\n", entity_number, m_spawnthing->emittername.c_str(), count);
            }
        }

        m_spawnthing->cgd.origin = save_origin;
        et->oldorigin            = save_origin;
        et->lerp_emitter         = qtrue;
    }

    m_spawnthing = NULL;
}

void ClientGameCommandManager::RemoveClientEntity(int number, int tiki, centity_t *cent, ctempmodel_t *p)
{
    int i;

    if (number != -1) {
        for (i = 1; i <= m_emitters.NumObjects(); i++) {
            spawnthing_t *st = m_emitters.ObjectAt(i);

            if (st->cgd.tiki == tiki) {
                st->RemoveEntity(number);
            }
        }
    }

    if ((cent && (cent->clientFlags & CF_UPDATESWIPE)) || (p && (p->cgd.flags2 & T2_TRAIL))) {
        int        oldnum  = current_entity_number;
        centity_t *oldcent = current_centity;

        current_centity       = cent;
        current_entity_number = number;
        ProcessEvent(EV_Client_SwipeOff);
        current_entity_number = oldnum;
        current_centity       = oldcent;
    }

    if (p && p->cgd.flags & T_ASSIGNED_NUMBER) {
        m_command_time_manager.RemoveEntity(number);
    }

}



void CG_RestartCommandManager(int time)
{
    commandManager.FreeAllTempModels();
    commandManager.RestartEmitterClocks(time);
}

void ClientGameCommandManager::FlushCommandManager()
{
    FreeAllTempModels();

    m_emitters.FreeObjectList();
}

void CG_FlushCommandManager(void)
{
    commandManager.FlushCommandManager();
}


// `cache`-only pass the port used to make does not exist in retail.  That filter is what
// was never reached (port 0 calls vs retail 5,790 on the same route).
// dword_67638 is current_tiki, not current_entity_number: CG_UpdateEntity @0x000201C0
static void CG_ProcessTikiInitCommands(int handle)
{
    int         i, j;
    tiki_cmd_t  tiki_cmd;

    if (handle < 0) {
        return;
    }

    // Retail ProcessInitCommands @0x0001FF60 does NOT walk the dtiki_t itself --
    if (cgi.TIKI_InitCommands_Client(handle, &tiki_cmd)) {
        for (i = 0; i < tiki_cmd.num_cmds; i++) {
            tiki_singlecmd_t *cmd = &tiki_cmd.cmds[i];
            Event            *ev;

            ev = new Event(cmd->args[0]);

            for (j = 1; j < cmd->num_args; j++) {
                ev->AddToken(cmd->args[j]);
            }

            current_tiki = handle;
            commandManager.ProcessEvent(ev);
            current_tiki = -1;
        }
    }

    autoCacheTiki(handle);
}

// (-3) is NOT visited, matching retail.
static void autoCacheTiki(int tiki)
{
    tiki_cmd_t tiki_cmd;
    int        numanims, numframes;
    int        animnum, framenum, i;

    if (tiki < 0) {
        return;
    }

    numanims = cgi.TIKI_NumAnims(tiki);

    for (animnum = 0; animnum < numanims; animnum++) {
        if (!cgi.TIKI_AnimLoaded(tiki, animnum)) {
            continue;
        }

        numframes = cgi.Anim_NumFrames(tiki, animnum);

        for (framenum = -2; framenum < numframes; framenum++) {
            if (!cgi.Frame_Commands(tiki, animnum, framenum, &tiki_cmd, NULL)) {
                continue;
            }

            for (i = 0; i < tiki_cmd.num_cmds; i++) {
                tiki_singlecmd_t *cmd = &tiki_cmd.cmds[i];

                if (cmd->num_args > 1
                    && (!strcmp(cmd->args[0], "model") || !strcmp(cmd->args[0], "sound")
                        || !strcmp(cmd->args[0], "loopsound"))) {
                    CacheResource(cmd->args[1]);
                }
            }
        }
    }
}

void CG_ProcessInitCommands(int tiki, refEntity_t *ent)
{
    int          i;
    refEntity_t *old_entity;
    int          old_tiki;

    if (tiki < 0) {
        return;
    }

    old_entity     = current_entity;
    old_tiki       = current_tiki;
    current_entity = ent;
    current_tiki   = tiki;

    CG_ProcessTikiInitCommands(tiki);

    for (i = 1; i <= cg_CachedTikis.NumObjects(); i++) {
        int cached = cgi.R_Model_GetHandle(cg_CachedTikis.ObjectAt(i));

        CG_ProcessTikiInitCommands(cached);
    }

    cg_CachedTikis.ClearObjectList();

    current_entity = old_entity;
    current_tiki   = old_tiki;
}


void CG_EndTiki(int tiki)
{
    CG_DeleteEmitters(tiki);
}

void CG_UpdateEntityEmitters(int tiki, refEntity_t *ent, centity_t *cent)
{
    refEntity_t *old_entity = current_entity;
    int          old_tiki   = current_tiki;
    int          oldnum     = current_entity_number;
    centity_t   *oldcent    = current_centity;

    // not for a non-".tik" CS_MODELS entry (retail: cgs.model_tiki[i] == -1, the
    current_entity        = ent;
    current_tiki          = tiki;
    current_centity       = cent;
    current_entity_number = cent->currentState.number;

    {
    }

    {
        static int uivk_beamfx_host_n;
        const char *uivk_tn = (tiki >= 0) ? cgi.TIKI_Name(tiki) : NULL;

        if (PROBE_On() && uivk_tn && Q_stristr(uivk_tn, "fx-beamsparkle")
            && PROBE_Throttle(&uivk_beamfx_host_n, 4, 512)) {
            UIVK_PROBE(
                "BEAMFX:host",
                "ent=%d tiki='%s' refentscale=%.4f esscale=0x%08x origin=(%.1f,%.1f,%.1f)\n",
                cent->currentState.number,
                uivk_tn,
                ent->scale,
                *(unsigned int *)&cent->currentState.scale,
                ent->origin[0],
                ent->origin[1],
                ent->origin[2]
            );
        }
    }

    if (cent->currentState.parent != ENTITYNUM_NONE) {
        commandManager.UpdateEmitter(
            current_tiki, ent->axis, cent->currentState.number, cent->currentState.instanceNumber,
            cent->currentState.parent, ent->origin
        );
    } else {
        commandManager.UpdateEmitter(
            current_tiki, ent->axis, cent->currentState.number, cent->currentState.instanceNumber,
            cent->currentState.parent, cent->lerpOrigin
        );
    }

    current_entity        = old_entity;
    current_tiki          = old_tiki;
    current_centity       = oldcent;
    current_entity_number = oldnum;
}

extern "C" {
    void CG_AnimationDebugMessage(int number, const char *fmt, ...);
}

void CG_ProcessEntityCommands(int tiki, int frame, int anim, int entnum, refEntity_t *ent, centity_t *cent)
{
    int i, j;
    int num_args;

    tiki_cmd_t tikicmds;

    if (cgi.Frame_Commands(tiki, anim, frame, &tikicmds, cent ? cent->currentState.effectsAnims : NULL)) {
        current_entity        = ent;
        current_centity       = cent;
        current_entity_number = entnum;
        current_tiki          = tiki;
        CG_AnimationDebugMessage(entnum, "Processing Ent Commands: Entity: %3d Anim:#(%i) Frame:#(%i)\n", entnum, anim, frame);

        {
        }

        {
            static int widget_ccmd;

            if (WIDGET_On() && WIDGET_Throttle(&widget_ccmd, 8, 64) && cent
                && (cent->currentState.eType == ET_MODELANIM || cent->currentState.eType == ET_PLAYER)) {
                WIDGET_A1_PROBE(
                    "ccmd",
                    "ent=%d etype=%d anim=%d frame=%d(%s) num_cmds=%d verb0='%s' tiki=%d\n",
                    entnum,
                    cent->currentState.eType,
                    anim,
                    frame,
                    (frame == TIKI_FRAME_ENTRY) ? "ENTRY" : (frame == TIKI_FRAME_EXIT) ? "EXIT" : "frame",
                    tikicmds.num_cmds,
                    (tikicmds.num_cmds > 0 && tikicmds.cmds[0].num_args > 0) ? tikicmds.cmds[0].args[0] : "-",
                    tiki);
            }
        }

        for (i = 0; i < tikicmds.num_cmds; i++) {
            Event *ev;

            num_args = tikicmds.cmds[i].num_args;

            if (num_args > 0) {
                {
                }
                ev = new Event(tikicmds.cmds[i].args[0]);

                for (j = 1; j < num_args; j++) {
                    ev->AddToken(tikicmds.cmds[i].args[j]);
                }

                commandManager.ProcessEvent(ev);
            }
        }

        current_tiki          = -1;
        current_entity_number = -1;
        current_entity        = NULL;
        current_centity       = NULL;
    }
}

void CG_ClientCommands(int tiki, int frame, int anim, animstate_t *state, refEntity_t *ent, centity_t *cent)
{
    qboolean bRunCommands;
    int      iLastFrame;
    int      iLastAnim;

    bRunCommands = qfalse;
    if (cent) {
        bRunCommands = cent->currentState.effectsAnims[0] != -1;
    }

    if (tiki >= 0 && !paused->integer && !(cent->currentState.eFlags & EF_DONT_PROCESS_COMMANDS)
        && frame >= 0 && frame < state->numframes) {
        iLastFrame = state->last_cmd_frame;
        iLastAnim  = state->last_cmd_anim - 1;

        if (iLastAnim != anim && iLastAnim >= 0) {
            CG_ProcessEntityCommands(tiki, TIKI_FRAME_EXIT, iLastAnim, cent->currentState.number, ent, cent);
            iLastFrame = 0;
            cent->tikiLoopSound = 0;  
        }

        if (state->has_commands || bRunCommands) {
            if (iLastAnim != anim) {
                CG_ProcessEntityCommands(tiki, TIKI_FRAME_ENTRY, anim, cent->currentState.number, ent, cent);
            }

            if (state->driven) {
                iLastFrame %= state->numframes;
                if (iLastFrame < frame) {
                    iLastFrame = frame;
                }
            } else if (frame != iLastFrame - 1) {
                if (state->numframes > 1) {
                    iLastFrame %= state->numframes;
                    while (iLastFrame != frame) {
                        if (cent->currentState.frame & FRAME_EXPLICIT) {
                            iLastFrame = frame;
                        }
                        state->last_cmd_time = cg.time + 20;
                        CG_ProcessEntityCommands(tiki, iLastFrame, anim, cent->currentState.number, ent, cent);
                        if (!(cent->currentState.frame & FRAME_EXPLICIT)) {
                            iLastFrame = (iLastFrame + 1) % state->numframes;
                        }
                    }
                }
            }

            if (iLastFrame == frame
                || (cg.time > state->last_cmd_time && (cent->currentState.eFlags & EF_EVERYFRAME))) {
                state->last_cmd_time = cg.time + 20;
                CG_ProcessEntityCommands(tiki, frame, anim, cent->currentState.number, ent, cent);
            }
        }

        if (cent->clientFlags & CF_UPDATESWIPE) {
            current_entity        = ent;
            current_tiki          = tiki;
            current_centity       = cent;
            current_entity_number = cent->currentState.number;

            commandManager.ProcessEvent(EV_Client_Swipe);

            current_tiki          = -1;
            current_entity_number = -1;
            current_entity        = NULL;
            current_centity       = NULL;
        }

        state->last_cmd_anim  = anim + 1;
        state->last_cmd_frame = frame + 1;
    } else {
        static int uirows_q;
        if (PROBE_On() && PROBE_Throttle(&uirows_q, 8, 1000)) {
            const char *uirows_tn  = cgi.TIKI_Name(tiki);
            const char *uirows_an  = cgi.Anim_NameForNum(tiki, anim);
            const char *uirows_an2 = cgi.Anim_NameForNum(tiki, cent->currentState.anim & ANIM_MASK);

            UIROWS_PROBE(
                "fx:reject2",
                "ent=%d tiki=%d '%s' anim=%d '%s' frame=%d state_nf=%d animframes=%d "
                "es_anim_name='%s' es.frame=0x%x es.anim=%d last_cmd_anim=%d last_cmd_frame=%d eff0=%d\n",
                cent ? cent->currentState.number : -1,
                tiki,
                uirows_tn ? uirows_tn : "?",
                anim,
                uirows_an ? uirows_an : "?",
                frame,
                state ? state->numframes : -1,
                cgi.Anim_NumFrames(tiki, anim),
                uirows_an2 ? uirows_an2 : "?",
                cent ? cent->currentState.frame : 0,
                cent ? cent->currentState.anim : -1,
                state ? state->last_cmd_anim - 1 : -1,
                state ? state->last_cmd_frame : -1,
                cent ? cent->currentState.effectsAnims[0] : -2);
        }  
        {
            static int widget_rej;

            if (WIDGET_On() && WIDGET_Throttle(&widget_rej, 8, 32)) {
                const char *widget_bn = cgi.Anim_NameForNum(tiki, state->base.anim & ANIM_MASK);

                WIDGET_A1_PROBE(
                    "fx:reject3",
                    "ent=%d tiki=%d anim=%d '%s' frame=%d state_nf=%d animframes=%d "
                    "base.anim=%d base_idx=%d base_anim_nf=%d base_name='%s' es.frame=0x%x "
                    "es.anim=%d last_cmd_anim_idx=%d last_cmd_frame=%d\n",
                    cent ? cent->currentState.number : -1,
                    tiki,
                    anim,
                    cgi.Anim_NameForNum(tiki, anim)
                        ? cgi.Anim_NameForNum(tiki, anim) : "?",
                    frame,
                    state->numframes,
                    cgi.Anim_NumFrames(tiki, anim),
                    state->base.anim,
                    state->base.anim & ANIM_MASK,
                    cgi.Anim_NumFrames(tiki, state->base.anim & ANIM_MASK),
                    widget_bn ? widget_bn : "?",
                    cent ? cent->currentState.frame : 0,
                    cent ? cent->currentState.anim : -1,
                    state->last_cmd_anim - 1,
                    state->last_cmd_frame);
            }
        }
    }
}

void CG_InitializeCommandManager(void)
{
    cg_showemitters = cgi.Cvar_Get("cg_showemitters", "0", 0);
    commandManager.InitializeTempModelCvars();
    commandManager.InitializeTempModels();
    commandManager.InitializeEmitters();
    CG_ClearSwipes();
    CG_InitTestEmitter();
    CG_InitializeClientEmitters();
}

void CG_RemoveClientEntity(int number, int tiki, centity_t *cent)
{
    commandManager.RemoveClientEntity(number, tiki, cent);

    RemoveBeamList(number);
}

qboolean CG_Command_ProcessFile(const char *filename, qboolean quiet, int curTiki)
{
    char       *buffer;
    const char *bufstart;
    char        com_token[MAX_STRING_CHARS];

    if (cgi.FS_ReadFile(filename, (void **)&buffer, quiet) == -1) {
        return qfalse;
    }

    if (!quiet) {
        cgi.DPrintf("CG_Command_ProcessFile: %s\n", filename);
    }

    current_tiki = -1;

    bufstart = buffer;

    while (1) {
        Event *ev;

        Q_strncpyz(com_token, COM_ParseExt(&buffer, qtrue), sizeof(com_token));
        if (!com_token[0]) {
            break;
        }

        if (!Q_stricmp(com_token, "end") || !Q_stricmp(com_token, "server")) {
            while (1) {
                Q_strncpyz(com_token, COM_ParseExt(&buffer, qfalse), sizeof(com_token));
                if (!com_token[0]) {
                    break;
                }
            }
            continue;
        }

        ev = new Event(com_token);

        while (1) {
            Q_strncpyz(com_token, COM_ParseExt(&buffer, qfalse), sizeof(com_token));
            if (!com_token[0]) {
                break;
            }

            ev->AddToken(com_token);
        }
        commandManager.ProcessEvent(ev);
    }
    cgi.FS_FreeFile((void *)bufstart);

    return qtrue;
}

void CG_EventList_f(void)
{
    const char *mask;

    mask = NULL;
    if (cgi.Argc() > 1) {
        mask = cgi.Argv(1);
    }
    Event::ListCommands(mask);
}

void CG_EventHelp_f(void)
{
    const char *mask;

    mask = NULL;
    if (cgi.Argc() > 1) {
        mask = cgi.Argv(1);
    }
    Event::ListDocumentation(mask);
}

void CG_DumpEventHelp_f(void)
{
    const char *mask;

    mask = NULL;
    if (cgi.Argc() > 1) {
        mask = cgi.Argv(1);
    }
    Event::ListDocumentation(mask, true);
}

void CG_PendingEvents_f(void)
{
    const char *mask;

    mask = NULL;
    if (cgi.Argc() > 1) {
        mask = cgi.Argv(1);
    }
    Event::PendingEvents(mask);
}

void CG_ClassList_f(void)
{
    listAllClasses();
}

void CG_ClassTree_f(void)
{
    if (cgi.Argc() > 1) {
        listInheritanceOrder(cgi.Argv(1));
    } else {
        cgi.Printf("Syntax: cg_classtree [classname].\n");
    }
}

void CG_ClassEvents_f(void)
{
    if (cgi.Argc() > 1) {
        ClassEvents(cgi.Argv(1), qfalse);
    } else {
        Com_Printf("Syntax: cg_classevents [classname].\n");
    }
}

void CG_DumpClassEvents_f(void)
{
    if (cgi.Argc() > 1) {
        ClassEvents(cgi.Argv(1), qtrue);
    } else {
        Com_Printf("Syntax: cg_dumpclassevents [classname].\n");
    }
}

void CG_DumpAllClasses_f(void)
{
    int         typeFlag   = EVENT_ALL;
    int         outputType = OUTPUT_ALL;
    const char *filename   = NULL;

    if (cgi.Argc() > 1) {
        const char *arg = cgi.Argv(1);

        if (!strcmp(arg, "tiki")) {
            typeFlag = EVENT_TIKI_ONLY;
        }
        if (!strcmp(arg, "script")) {
            typeFlag = EVENT_SCRIPT_ONLY;
        }
    }

    if (cgi.Argc() > 2) {
        const char *arg = cgi.Argv(2);

        if (!strcmp(arg, "html")) {
            outputType = OUTPUT_HTML;
        }
        if (!strcmp(arg, "cmdmap")) {
            outputType = OUTPUT_CMD;
        }
    }

    if (cgi.Argc() > 3) {
        filename = cgi.Argv(3);
    }

    DumpAllClasses(typeFlag, outputType, filename);
}

#define throw_assert assert(0), throw

EmitterLoader emitterLoader;

Event EV_EmitterLoader_Emitter
(
    "emitter",
    EV_DEFAULT,
    "s",
    "emittername",
    "Create a new emitter"
);

CLASS_DECLARATION(Listener, EmitterLoader, NULL) {
    {&EV_EmitterLoader_Emitter, &EmitterLoader::Emitter},
    {NULL,                      NULL                   }
};

EmitterLoader::EmitterLoader()
{
    emitterActive = false;
}

void EmitterLoader::Emitter(Event *ev)
{
    spawnthing_t *st;

    st = commandManager.CreateNewEmitter(ev->GetString(1));

    if (st) {
        emitterActive = true;
        commandManager.SetSpawnthing(st);
    }
}

void EmitterLoader::ProcessEmitter(Script& script)
{
    str token;

    while (script.TokenAvailable(true)) {
        Event *ev = NULL;

        token = script.GetToken(true);

        if (!str::cmp("}", token)) {
            break;
        }

        ev = new Event(token);

        while (script.TokenAvailable(false)) {
            ev->AddToken(script.GetToken(false));
        }

        if (emitterActive) {
            commandManager.ProcessEvent(ev);
        }
    }

    commandManager.SetSpawnthing(NULL);
    emitterActive = false;
}

bool EmitterLoader::Load(Script& script)
{
    str token;
    str errortext;

    try {
        while (script.TokenAvailable(true)) {
            Event *ev = NULL;

            token = script.GetToken(true);

            if (!str::cmp("{", token)) {
                ProcessEmitter(script);
            } else if (!token.length() || !ValidEvent(token)) {
                throw_assert "invalid token";
            } else {
                ev = new Event(token);

                while (script.TokenAvailable(false)) {
                    ev->AddToken(script.GetToken(false));
                }

                ProcessEvent(ev);
            }
        }
    } catch (const char *s) {
        cgi.DPrintf("emitter: error on line %d: %s\n", script.GetLineNumber(), s);
        return false;
    }

    return true;
}

void CG_Emitter(centity_t *cent)
{
    spawnthing_t  *emitter;
    float          dtime;
    emittertime_t *et;
    vec3_t         mins, maxs;

    emitter = commandManager.GetEmitterByName(CG_ConfigString(CS_IMAGES + cent->currentState.tag_num));

    if (emitter) {
        cgi.R_ModelBounds(cgs.inlineDrawModel[cent->currentState.modelindex], mins, maxs);

        emitter->cgd.origin[0]       = cent->lerpOrigin[0] + mins[0] + (random() * (maxs[0] - mins[0]));
        emitter->cgd.origin[1]       = cent->lerpOrigin[1] + mins[1] + (random() * (maxs[1] - mins[1]));
        emitter->cgd.origin[2]       = cent->lerpOrigin[2] + maxs[2];
        emitter->cgd.parentOrigin    = Vector(cent->lerpOrigin);
        emitter->cgd.parentOrigin[2] = cent->lerpOrigin[2] + maxs[2];
        emitter->cgd.parentMins      = mins;
        emitter->cgd.parentMaxs      = maxs;

        AnglesToAxis(cent->lerpAngles, emitter->aim_axis);

        et = emitter->GetEmitTime(cent->currentState.number, cent->currentState.instanceNumber);

        if (et->last_emit_time > 0) {
            dtime = cg.time - et->last_emit_time;
            while (dtime > emitter->spawnRate) {
                dtime -= emitter->spawnRate;
                commandManager.SpawnEffect(1, emitter);
                et->last_emit_time = cg.time;
            }
        }
    }
}

void ClientGameCommandManager::CGEvent(centity_t *cent)
{
    str      modelname;
    int      tiki;

    tiki = cgi.R_Model_GetHandle(cgs.model_draw[cent->currentState.modelindex]);

    {
        static int uivk_dfx_n;
        if (PROBE_On() && PROBE_Throttle(&uivk_dfx_n, 64, 64)) {
            PROBE("DFX:etevent", "ent=%d modelindex=%d tiki=%d org=(%.1f,%.1f,%.1f)\n",
                       cent->currentState.number,
                       cent->currentState.modelindex,
                       tiki,
                       cent->lerpOrigin[0], cent->lerpOrigin[1], cent->lerpOrigin[2]);
        }
    }

    if (tiki < 0) {
        return;
    }

    CG_EntityEffects(cent);

    modelname    = cgi.TIKI_Name(tiki);
    m_spawnthing = InitializeSpawnthing(&m_localemitter);

    AnglesToAxis(cent->lerpAngles, m_spawnthing->axis);
    m_spawnthing->cgd.angles = cent->lerpAngles;
    m_spawnthing->cgd.origin = cent->lerpOrigin;
    m_spawnthing->cgd.scale  = cent->currentState.scale;
    m_spawnthing->cgd.alpha  = cent->currentState.alpha;

    m_spawnthing->cgd.color[0] = cent->color[0];
    m_spawnthing->cgd.color[1] = cent->color[1];
    m_spawnthing->cgd.color[2] = cent->color[2];
    m_spawnthing->cgd.color[3] = cent->color[3];

    Event *ev;
    ev = new Event("model");
    ev->AddString(modelname);
    ProcessEvent(ev);

    ev = new Event("anim");
    ev->AddString("idle");
    ProcessEvent(ev);

    SpawnEffect(1, 0);
}

void CG_Event(centity_t *cent)
{
    commandManager.CGEvent(cent);
}



int ClientGameCommandManager::IdForTempModel(const ctempmodel_t *model)
{
    int index;

    if (model == &m_active_tempmodels) {
        return -1;
    }

    if (!model) {
        return -2;
    }

    index = model - m_tempmodels;
    if (index >= MAX_TEMPMODELS) {
        return -2;
    }

    if (model != &m_tempmodels[index]) {
        return -2;
    }

    return model - m_tempmodels;
}

ctempmodel_t *ClientGameCommandManager::TempModelForId(int id)
{
    if (id == -1) {
        return &m_active_tempmodels;
    }

    if (id == -2) {
        return NULL;
    }

    return &m_tempmodels[id];
}

int ClientGameCommandManager::IdForSpawnThing(const spawnthing_t *sp)
{
    int i;

    if (!sp) {
        return 0;
    }

    for (i = 1; i <= m_emitters.NumObjects(); i++) {
        if (sp == m_emitters.ObjectAt(i)) {
            return i;
        }
    }

    return 0;
}

spawnthing_t *ClientGameCommandManager::SpawnThingForId(int id)
{
    if (!id) {
        return 0;
    }

    return m_emitters.ObjectAt(id);
}




int CompareData(const void *elem1, const void *elem2)
{
    float f1 = **(float **)elem1;
    float f2 = **(float **)elem2;

    if (f1 <= f2) {
        return -(f1 < f2);
    }

    return 1;
}


TimeBasedCommand::~TimeBasedCommand(void)
{
}

void TimeBasedCommand::Init(void)
{
    m_startValue   = CG_TBC_LITERAL_0;                        
    m_endValue     = CG_TBC_LITERAL_10;                       
    m_currentValue = CG_TBC_LITERAL_0;                        
    m_delta        = CG_TBC_LITERAL_0;                        
    m_type         = 0;                                       
    m_interpolate  = qtrue;                                   
}

void TimeBasedCommand::HandleCommand(ctempmodel_t *ct)
{
}

void TimeBasedCommand::HandleCommand(spawnthing_t *sp)
{
}

void TimeBasedCommand::SetControllerCommand(ctempmodel_t *ct)
{
}

void TimeBasedCommand::SetControllerCommand(spawnthing_t *sp)
{
}


TimeBasedCommandIndex::~TimeBasedCommandIndex(void)
{
}


TBC_Speed::~TBC_Speed(void)
{
}

void TBC_Speed::Init(void)
{
    TimeBasedCommand::Init();

    m_startTime = CG_TBC_LITERAL_0;                           
    m_endTime   = CG_TBC_LITERAL_0;                           
}

TBC_SpawnRate::~TBC_SpawnRate(void)
{
}

void TBC_SpawnRate::Init(void)
{
    TimeBasedCommand::Init();

    m_startTime = CG_TBC_LITERAL_0;                           
    m_endTime   = CG_TBC_LITERAL_0;                           
}

TBC_ParticleSpin::~TBC_ParticleSpin(void)
{
}

void TBC_ParticleSpin::Init(void)
{
    TimeBasedCommand::Init();

    m_spin[0] = CG_TBC_LITERAL_0;                             
    m_spin[1] = CG_TBC_LITERAL_0;                             
    m_spin[2] = CG_TBC_LITERAL_0;                             
}

TBC_Direction::~TBC_Direction(void)
{
}

void TBC_Direction::Init(void)
{
    TimeBasedCommand::Init();

    m_dirBase[0] = CG_TBC_LITERAL_0;                          
    m_dirBase[1] = CG_TBC_LITERAL_0;                          
    m_dirBase[2] = CG_TBC_LITERAL_0;                          
    m_dirAmp[0] = CG_TBC_LITERAL_0;                           
    m_dirAmp[1] = CG_TBC_LITERAL_0;                           
    m_dirAmp[2] = CG_TBC_LITERAL_0;                           
    m_curDir[0] = CG_TBC_LITERAL_0;                           
    m_curDir[1] = CG_TBC_LITERAL_0;                           
    m_curDir[2] = CG_TBC_LITERAL_0;                           
    m_endDirBase[0] = CG_TBC_LITERAL_0;                       
    m_endDirBase[1] = CG_TBC_LITERAL_0;                       
    m_endDirBase[2] = CG_TBC_LITERAL_0;                       
    m_endDirAmp[0] = CG_TBC_LITERAL_0;                        
    m_endDirAmp[1] = CG_TBC_LITERAL_0;                        
    m_endDirAmp[2] = CG_TBC_LITERAL_0;                        
    m_endCurDir[0] = CG_TBC_LITERAL_0;                        
    m_endCurDir[1] = CG_TBC_LITERAL_0;                        
    m_endCurDir[2] = CG_TBC_LITERAL_0;                        
}

TimeBasedCommand *TBC_Speed::clone(void)
{
    TBC_Speed *c;

    c = new TBC_Speed;
    c->Init();

    c->m_startValue = m_startValue;
    c->m_endValue = m_endValue;
    c->m_currentValue = m_currentValue;
    c->m_delta = m_delta;
    c->m_type = m_type;
    c->m_interpolate = m_interpolate;
    c->m_startTime = m_startTime;
    c->m_endTime = m_endTime;

    return c;
}

TimeBasedCommand *TBC_SpawnRate::clone(void)
{
    TBC_SpawnRate *c;

    c = new TBC_SpawnRate;
    c->Init();

    c->m_startValue = m_startValue;
    c->m_endValue = m_endValue;
    c->m_currentValue = m_currentValue;
    c->m_delta = m_delta;
    c->m_type = m_type;
    c->m_interpolate = m_interpolate;
    c->m_startTime = m_startTime;
    c->m_endTime = m_endTime;

    return c;
}

TimeBasedCommand *TBC_ParticleSpin::clone(void)
{
    TBC_ParticleSpin *c;

    c = new TBC_ParticleSpin;
    c->Init();

    c->m_startValue = m_startValue;
    c->m_endValue = m_endValue;
    c->m_currentValue = m_currentValue;
    c->m_delta = m_delta;
    c->m_type = m_type;
    c->m_interpolate = m_interpolate;
    c->m_spin[0] = m_spin[0];
    c->m_spin[1] = m_spin[1];
    c->m_spin[2] = m_spin[2];
    c->m_useBias = m_useBias;

    return c;
}

TimeBasedCommand *TBC_Direction::clone(void)
{
    TBC_Direction *c;

    c = new TBC_Direction;
    c->Init();

    c->m_startValue = m_startValue;
    c->m_endValue = m_endValue;
    c->m_currentValue = m_currentValue;
    c->m_delta = m_delta;
    c->m_type = m_type;
    c->m_interpolate = m_interpolate;
    c->m_curDir[0] = m_curDir[0];
    c->m_curDir[1] = m_curDir[1];
    c->m_curDir[2] = m_curDir[2];
    c->m_endCurDir[0] = m_endCurDir[0];
    c->m_endCurDir[1] = m_endCurDir[1];
    c->m_endCurDir[2] = m_endCurDir[2];

    return c;
}


void TBC_Speed::SetControllerCommand(ctempmodel_t *ct)
{
    if (!ct->tbcController) {
        ct->createTBC_Controller();
    }

    if (ct->tbcController) {
        m_currentValue = CG_TBC_MS_SCALE * m_endValue + (float)ct->cgd.createTime;
        ct->tbcController->m_command = clone();
    }
}

void TBC_SpawnRate::SetControllerCommand(spawnthing_t *sp)
{
    if (!sp->tbcController) {
        sp->createTBC_Controller();
    }

    if (sp->tbcController) {
        m_endValue = m_startTime;
        m_currentValue = CG_TBC_MS_SCALE * m_endValue + (float)sp->cgd.createTime;
        m_delta = m_endValue - m_startValue;
        sp->tbcController->m_command = clone();
    }
}

void TBC_ParticleSpin::SetControllerCommand(ctempmodel_t *ct)
{
    if (!ct->tbcController) {
        ct->createTBC_Controller();
    }

    if (ct->tbcController) {
        m_startTime = (float)ct->cgd.createTime;
        m_currentValue = CG_TBC_MS_SCALE * m_endValue + (float)ct->cgd.createTime;
        m_delta = m_endValue - m_startValue;
        ct->tbcController->m_command = clone();
    }
}

void TBC_ParticleSpin::HandleCommand(ctempmodel_t *ct)
{
    int   elapsed;
    float yaw;
    float pitch;
    float yawTotal;
    float pitchTotal;
    float rollTotal;
    float step;
    float sinYaw, cosYaw, sinPitch, cosPitch;

    if ((float)cg.time > m_currentValue) {
        return;
    }

    ct->cgd.spinRate[0] = m_spin[0];
    ct->cgd.spinRate[1] = m_spin[1];
    ct->cgd.spinRate[2] = m_spin[2];

    if (m_useBias) {
        elapsed = cg.time - ct->cgd.createTime;
    } else {
        elapsed = (int)(m_startTime - ((float)cg.time - m_startTime)
                        - (float)ct->cgd.createTime);
    }

    if (ct->cgd.avelocity[0] == CG_TBC_LITERAL_0 && ct->cgd.avelocity[1] == CG_TBC_LITERAL_0) {
        yaw = CG_TBC_LITERAL_0;
        pitch = (ct->cgd.avelocity[2] > CG_TBC_LITERAL_0) ? CG_TBC_LITERAL_90
                                                          : CG_TBC_LITERAL_270;
    } else {
        yaw = CG_TBC_RAD_TO_DEG(atan2(ct->cgd.avelocity[1], ct->cgd.avelocity[0]));
        if (yaw < CG_TBC_LITERAL_0) {
            yaw += CG_TBC_LITERAL_360;
        }

        pitch = CG_TBC_RAD_TO_DEG(atan2(ct->cgd.avelocity[2],
                                        sqrt(ct->cgd.avelocity[0] * ct->cgd.avelocity[0]
                                             + ct->cgd.avelocity[1] * ct->cgd.avelocity[1])));
        if (pitch < CG_TBC_LITERAL_0) {
            pitch += CG_TBC_LITERAL_360;
        }
    }

    pitch = -pitch;

    yawTotal   = (float)elapsed * (ct->cgd.spinRate[1] / CG_TBC_MS_SCALE) + yaw;
    pitchTotal = (float)elapsed * (ct->cgd.spinRate[0] / CG_TBC_MS_SCALE) + pitch;
    rollTotal  = (float)elapsed * (ct->cgd.spinRate[2] / CG_TBC_MS_SCALE);

    sinYaw   = sin((float)(CG_TBC_DEG_TO_RAD * yawTotal));
    cosYaw   = cos((float)(CG_TBC_DEG_TO_RAD * yawTotal));
    sinPitch = sin((float)(CG_TBC_DEG_TO_RAD * pitchTotal));
    cosPitch = cos((float)(CG_TBC_DEG_TO_RAD * pitchTotal));
    (void)sin((float)(CG_TBC_DEG_TO_RAD * rollTotal));
    (void)cos((float)(CG_TBC_DEG_TO_RAD * rollTotal));

    step = ct->cgd.spinSpeed * (float)elapsed / CG_TBC_MS_SCALE;

    ct->cgd.origin[0] = ct->cgd.spinBase[0] + cosPitch * cosYaw * step;
    ct->cgd.origin[1] = ct->cgd.spinBase[1] + cosPitch * sinYaw * step;
    ct->cgd.origin[2] = ct->cgd.spinBase[2] + -sinPitch * step;
}


void TBC_SpawnRate::HandleCommand(spawnthing_t *sp)
{
    float value;

    if ((float)cg.time > m_currentValue) {
        return;
    }

    if (m_interpolate) {
        m_startValue = m_currentValue - CG_TBC_MS_SCALE * m_delta;

        if (m_currentValue - (float)sp->cgd.createTime == CG_TBC_LITERAL_0) {
            value = m_startTime;
        } else {
            value = (m_endTime - m_startTime) / (m_currentValue - (float)sp->cgd.createTime)
                    * (float)(cg.time - sp->cgd.createTime) + m_startTime;
        }
    } else {
        value = m_endTime;
    }

    sp->spawnRate = value;
}

void TBC_Speed::HandleCommand(ctempmodel_t *ct)
{
    float value;
    float len;
    float scale;

    if ((float)cg.time > m_currentValue) {
        return;
    }

    if (m_interpolate) {
        m_startValue = m_currentValue - CG_TBC_MS_SCALE * m_delta;

        if (m_currentValue - (float)ct->cgd.createTime == CG_TBC_LITERAL_0) {
            value = m_startTime;
        } else {
            value = (m_endTime - m_startTime) / (m_currentValue - (float)ct->cgd.createTime)
                    * (float)(cg.time - ct->cgd.createTime) + m_startTime;
        }
    } else {
        value = m_endTime;
    }

    if (value != CG_TBC_LITERAL_0) {
        len = VectorLength(ct->cgd.avelocity);

        if (len != CG_TBC_LITERAL_0) {
            scale = CG_TBC_LITERAL_1 / len;
            ct->cgd.avelocity[0] *= scale;
            ct->cgd.avelocity[1] *= scale;
            ct->cgd.avelocity[2] *= scale;
        }

        ct->cgd.avelocity[0] *= value;
        ct->cgd.avelocity[1] *= value;
        ct->cgd.avelocity[2] *= value;
    }

    ct->cgd.flags2 |= 1;
}

void TBC_Direction::SetControllerCommand(ctempmodel_t *ct)
{
    int    i;
    float  len;
    float  d0, d1;
    float  sinD0, cosD0, sinD1, cosD1;

    for (i = 0; i < 3; i++) {
        m_endCurDir[i] = CGResolveRandomNumber((randtype_t)m_endDirType[i], m_endDirBase[i],
                                               m_endDirAmp[i]);
    }

    for (i = 0; i < 3; i++) {
        m_curDir[i] = CGResolveRandomNumber((randtype_t)m_dirType[i], m_dirBase[i], m_dirAmp[i]);
    }

    if (!ct->tbcController) {
        ct->createTBC_Controller();
    }

    if (ct->tbcController) {
        m_currentValue = CG_TBC_MS_SCALE * m_endValue + (float)ct->cgd.createTime;

        len = VectorLength(ct->cgd.avelocity);

        d0 = m_curDir[0] * (float)M_PI / 180.0f;
        d1 = m_curDir[1] * (float)M_PI / 180.0f;

        sinD0 = sin(d0);
        cosD0 = cos(d0);
        sinD1 = sin(d1);
        cosD1 = cos(d1);

        ct->cgd.avelocity[0] = len * (cosD0 * cosD1);
        ct->cgd.avelocity[1] = len * (cosD0 * sinD1);
        ct->cgd.avelocity[2] = len * -sinD0;

        ct->tbcController->m_command = clone();
    }
}

//   5b824  bl    0x613a4 -> 0x76A5C     ; sin(v2)   (roll -- never read back)
//   5b83c  bl    0x613bc -> 0x76A50     ; cos(v2)   (roll -- never read back)
void TBC_Direction::HandleCommand(ctempmodel_t *ct)
{
    float v0, v1, v2;
    float len;
    float sinD0, cosD0, sinD1, cosD1;
    float d0, d1, d2;

    if ((float)cg.time > m_currentValue) {
        return;
    }

    if (m_interpolate) {
        float denom;
        float numer;

        m_startValue = m_currentValue - CG_TBC_MS_SCALE * m_delta;
        denom = m_currentValue - (float)ct->cgd.createTime;
        numer = (float)(cg.time - ct->cgd.createTime);

        v0 = (denom == CG_TBC_LITERAL_0) ? m_curDir[0]
                                         : (m_endCurDir[0] - m_curDir[0]) / denom * numer
                                           + m_curDir[0];
        v1 = (denom == CG_TBC_LITERAL_0) ? m_curDir[1]
                                         : (m_endCurDir[1] - m_curDir[1]) / denom * numer
                                           + m_curDir[1];
        v2 = (denom == CG_TBC_LITERAL_0) ? m_curDir[2]
                                         : (m_endCurDir[2] - m_curDir[2]) / denom * numer
                                           + m_curDir[2];
    } else {
        v0 = m_endCurDir[0];
        v1 = m_endCurDir[1];
        v2 = m_endCurDir[2];
    }

    d0 = (float)(CG_TBC_DEG_TO_RAD * v0);
    d1 = (float)(CG_TBC_DEG_TO_RAD * v1);
    d2 = (float)(CG_TBC_DEG_TO_RAD * v2);

    sinD1 = sin(d1);
    cosD1 = cos(d1);
    sinD0 = sin(d0);
    cosD0 = cos(d0);
    (void)sin(d2);
    (void)cos(d2);

    len = VectorLength(ct->cgd.avelocity);

    ct->cgd.avelocity[0] = len * (cosD0 * cosD1);
    ct->cgd.avelocity[1] = len * (cosD0 * sinD1);
    ct->cgd.avelocity[2] = len * -sinD0;

    ct->cgd.flags2 |= 1;
}


TimeBasedCommandManager::TimeBasedCommandManager(void)
{
}

TimeBasedCommandManager::~TimeBasedCommandManager(void)
{
    m_directionList.FreeObjectList();
    m_speedList.FreeObjectList();
    m_particleSpinList.FreeObjectList();
    m_spawnRateList.FreeObjectList();
    ClearParticleTBCList();
    ClearParticleTBIndexList();
    ClearEmitterTBCList();
    ClearEmitterTBIndexList();
}

float TimeBasedCommandManager::GetParticleFirstStartTime(void)
{
    return m_particleIndex.ObjectAt(1)->m_key;
}

int TimeBasedCommandManager::GetNumberOfParticleCommands(void)
{
    return m_particleCommands.NumObjects();
}

int TimeBasedCommandManager::GetNumberOfEmitterCommands(void)
{
    return m_emitterCommands.NumObjects();
}

void TimeBasedCommandManager::ClearParticleTBIndexList(void)
{
    int i;
    TimeBasedCommandIndex *idx;

    for (i = m_particleIndex.NumObjects(); i > 0; i--) {
        idx = m_particleIndex.ObjectAt(i);
        m_particleIndex.RemoveObjectAt(i);
        if (idx) {
            delete idx;
        }
    }
}

void TimeBasedCommandManager::ClearParticleTBCList(void)
{
    int i;
    TimeBasedCommand *cmd;

    for (i = m_particleCommands.NumObjects(); i > 0; i--) {
        cmd = m_particleCommands.ObjectAt(i);
        m_particleCommands.RemoveObjectAt(i);
        if (cmd) {
            delete cmd;
        }
    }

    ClearParticleTBIndexList();
}

void TimeBasedCommandManager::ClearEmitterTBIndexList(void)
{
    int i;
    TimeBasedCommandIndex *idx;

    for (i = m_emitterIndex.NumObjects(); i > 0; i--) {
        idx = m_emitterIndex.ObjectAt(i);
        m_emitterIndex.RemoveObjectAt(i);
        if (idx) {
            delete idx;
        }
    }
}

void TimeBasedCommandManager::ClearEmitterTBCList(void)
{
    int i;
    TimeBasedCommand *cmd;

    for (i = m_emitterCommands.NumObjects(); i > 0; i--) {
        cmd = m_emitterCommands.ObjectAt(i);
        m_emitterCommands.RemoveObjectAt(i);
        if (cmd) {
            delete cmd;
        }
    }

    ClearEmitterTBIndexList();
}

void TimeBasedCommandManager::GenerateEmitterTBI(float key, int index)
{
    TimeBasedCommandIndex *idx;

    idx = new TimeBasedCommandIndex;

    idx->m_key   = CG_TBC_LITERAL_0;
    idx->m_index = 0;

    idx->m_key   = key;
    idx->m_index = index;

    m_emitterIndex.AddObject(idx);
}

void TimeBasedCommandManager::GenerateParticleTBI(float key, int index)
{
    TimeBasedCommandIndex *idx;

    idx = new TimeBasedCommandIndex;

    idx->m_key   = CG_TBC_LITERAL_0;
    idx->m_index = 0;

    idx->m_key   = key;
    idx->m_index = index;

    m_particleIndex.AddObject(idx);
}

void TimeBasedCommandManager::AddEmitterTBC(TimeBasedCommand *cmd)
{
    m_emitterCommands.AddObject(cmd);

    GenerateEmitterTBI(0.0f, m_emitterCommands.NumObjects());
}

void TimeBasedCommandManager::SetNextEmitterCommand(spawnthing_t *sp)
{
    int cursor;
    int count;
    TimeBasedCommandIndex *idx;
    TimeBasedCommandIndex *next;

    cursor = sp->tbcEmitterCursor;
    count = m_emitterIndex.NumObjects();

    if (cursor + 1 > count) {
        return;
    }

    idx = m_emitterIndex.ObjectAt(cursor + 1);
    if (!idx) {
        return;
    }

    {
        TimeBasedCommand *cmd = m_emitterCommands.ObjectAt(idx->m_index);

        if (cmd) {
            cmd->SetControllerCommand(sp);
        }
    }

    if (cursor + 2 <= m_emitterIndex.NumObjects()) {
        next = m_emitterIndex.ObjectAt(cursor + 2);
        if (next) {
            sp->tbcNextCommandTime = CG_TBC_MS_SCALE * next->m_key + sp->tbcTimeOffset;
            sp->tbcEmitterCursor = cursor + 1;
        }
    } else {
        sp->tbcEmitterCursor = cursor + 1;
    }
}

void TimeBasedCommandManager::SetNextParticleCommand(ctempmodel_t *ct)
{
    int cursor;
    TimeBasedCommandIndex *idx;
    TimeBasedCommandIndex *next;

    cursor = ct->tbcParticleCursor;

    if (cursor + 1 > m_particleIndex.NumObjects()) {
        return;
    }

    idx = m_particleIndex.ObjectAt(cursor + 1);
    if (!idx) {
        return;
    }

    {
        TimeBasedCommand *cmd = m_particleCommands.ObjectAt(idx->m_index);

        if (cmd) {
            cmd->SetControllerCommand(ct);
        }
    }

    if (cursor + 2 <= m_particleIndex.NumObjects()) {
        next = m_particleIndex.ObjectAt(cursor + 2);
        if (next) {
            ct->tbcNextCommandTime = CG_TBC_MS_SCALE * next->m_key + (float)ct->cgd.createTime;
            ct->tbcParticleCursor = cursor + 1;
        }
    } else {
        ct->tbcParticleCursor = cursor + 1;
    }
}

void TimeBasedCommandManager::SortParticleTBIList(void)
{
    int i;
    int swapped;
    TimeBasedCommandIndex *a;
    TimeBasedCommandIndex *b;
    TimeBasedCommandIndex *swap;

    if (m_speedList.NumObjects() > 1) {
        m_speedList.Sort(CompareData);
        Container<TBC_DataPoint *> copy = m_speedList;
        GenerateCommands(copy, TBC_SPEED);
    }

    if (m_directionList.NumObjects() > 1) {
        m_directionList.Sort(CompareData);
        Container<TBC_DataPoint *> copy = m_directionList;
        GenerateCommands(copy, TBC_DIRECTION);
    }

    if (m_particleSpinList.NumObjects() > 1) {
        m_particleSpinList.Sort(CompareData);
        Container<TBC_DataPoint *> copy = m_particleSpinList;
        GenerateCommands(copy, TBC_PARTICLESPIN);
    }

    do {
        swapped = 0;

        for (i = 1; i <= m_particleIndex.NumObjects(); i++) {
            if (i + 1 > m_particleIndex.NumObjects()) {
                continue;
            }

            a = m_particleIndex.ObjectAt(i);
            b = m_particleIndex.ObjectAt(i + 1);

            if (a->m_key <= b->m_key) {
                continue;
            }

            swap = new TimeBasedCommandIndex;
            swap->m_key = a->m_key;
            swap->m_index = a->m_index;
            a->m_key = b->m_key;
            a->m_index = b->m_index;
            b->m_key = swap->m_key;
            b->m_index = swap->m_index;
            delete swap;

            swapped = 1;
        }
    } while (swapped);
}

void TimeBasedCommandManager::SortEmitterTBIList(void)
{
    int i;
    int swapped;
    TimeBasedCommandIndex *a;
    TimeBasedCommandIndex *b;
    TimeBasedCommandIndex *swap;

    if (m_spawnRateList.NumObjects() > 1) {
        m_spawnRateList.Sort(CompareData);
        Container<TBC_DataPoint *> copy = m_spawnRateList;
        GenerateCommands(copy, TBC_SPAWNRATE);
    }

    do {
        swapped = 0;

        for (i = 1; i <= m_emitterIndex.NumObjects(); i++) {
            if (i + 1 > m_emitterIndex.NumObjects()) {
                continue;
            }

            a = m_emitterIndex.ObjectAt(i);
            b = m_emitterIndex.ObjectAt(i + 1);

            if (a->m_key <= b->m_key) {
                continue;
            }

            swap = new TimeBasedCommandIndex;
            swap->m_key = a->m_key;
            swap->m_index = a->m_index;
            a->m_key = b->m_key;
            a->m_index = b->m_index;
            b->m_key = swap->m_key;
            b->m_index = swap->m_index;
            delete swap;

            swapped = 1;
        }
    } while (swapped);
}

void TimeBasedCommandManager::SortAllTBILists(void)
{
    SortParticleTBIList();
    SortEmitterTBIList();
}

void TimeBasedCommandManager::GenerateCommands(Container<TBC_DataPoint *>& list, tbc_t type)
{
    int i;
    TBC_DataPoint *prev;
    TBC_DataPoint *cur;
    TBC_Direction *dir;
    TBC_Speed *speed;
    TBC_ParticleSpin *spin;
    TBC_SpawnRate *rate;

    if (type == TBC_SPAWNRATE) {
        if (list.NumObjects() < 2) {
            return;
        }

        for (i = 2; i <= list.NumObjects(); i++) {
            prev = list.ObjectAt(i - 1);
            cur = list.ObjectAt(i);

            rate = new TBC_SpawnRate;
            rate->Init();

            rate->m_startValue = prev->value;
            rate->m_endValue = cur->value;
            rate->m_delta = cur->value - prev->value;
            rate->m_type = prev->type;
            rate->m_startTime = prev->time;
            rate->m_endTime = cur->time;

            AddEmitterTBC(rate);
        }

        return;
    }

    if (type == TBC_PARTICLESPIN) {
        if (list.NumObjects() < 2) {
            return;
        }

        for (i = 2; i <= list.NumObjects(); i++) {
            prev = list.ObjectAt(i - 1);
            cur = list.ObjectAt(i);

            spin = new TBC_ParticleSpin;
            spin->Init();

            spin->m_startValue = prev->value;
            spin->m_endValue = cur->value;
            spin->m_delta = cur->value - prev->value;
            spin->m_type = prev->type;
            spin->m_spin[0] = prev->axis[0];
            spin->m_spin[1] = prev->axis[1];
            spin->m_spin[2] = prev->axis[2];
            spin->m_useBias = prev->useBias;

            m_particleCommands.AddObject(spin);

            GenerateParticleTBI(spin->m_startValue, m_particleCommands.NumObjects());
        }

        return;
    }

    if (type == TBC_DIRECTION) {
        if (list.NumObjects() < 2) {
            return;
        }

        for (i = 2; i <= list.NumObjects(); i++) {
            prev = list.ObjectAt(i - 1);
            cur = list.ObjectAt(i);

            dir = new TBC_Direction;
            dir->Init();

            dir->m_startValue = prev->value;
            dir->m_endValue = cur->value;
            dir->m_delta = cur->value - prev->value;
            dir->m_type = prev->type;
            dir->m_dirType[0] = prev->dirType[0];
            dir->m_dirType[1] = prev->dirType[1];
            dir->m_dirType[2] = prev->dirType[2];
            dir->m_dirBase[0] = prev->dirBase[0];
            dir->m_dirBase[1] = prev->dirBase[1];
            dir->m_dirBase[2] = prev->dirBase[2];
            dir->m_dirAmp[0] = prev->dirAmp[0];
            dir->m_dirAmp[1] = prev->dirAmp[1];
            dir->m_dirAmp[2] = prev->dirAmp[2];
            dir->m_endDirType[0] = cur->dirType[0];
            dir->m_endDirType[1] = cur->dirType[1];
            dir->m_endDirType[2] = cur->dirType[2];
            dir->m_endDirBase[0] = cur->dirBase[0];
            dir->m_endDirBase[1] = cur->dirBase[1];
            dir->m_endDirBase[2] = cur->dirBase[2];
            dir->m_endDirAmp[0] = cur->dirAmp[0];
            dir->m_endDirAmp[1] = cur->dirAmp[1];
            dir->m_endDirAmp[2] = cur->dirAmp[2];

            m_particleCommands.AddObject(dir);

            GenerateParticleTBI(dir->m_startValue, m_particleCommands.NumObjects());
        }

        return;
    }

    if (list.NumObjects() < 2) {
        return;
    }

    for (i = 2; i <= list.NumObjects(); i++) {
        prev = list.ObjectAt(i - 1);
        cur = list.ObjectAt(i);

        speed = new TBC_Speed;
        speed->Init();

        speed->m_startValue = prev->value;
        speed->m_endValue = cur->value;
        speed->m_delta = cur->value - prev->value;
        speed->m_type = prev->type;
        speed->m_startTime = prev->time;
        speed->m_endTime = cur->time;

        m_particleCommands.AddObject(speed);

        GenerateParticleTBI(speed->m_startValue, m_particleCommands.NumObjects());
    }
}


TimeBasedCommandParticleController::TimeBasedCommandParticleController(void)
{
    m_command = NULL;
}

TimeBasedCommandParticleController::~TimeBasedCommandParticleController(void)
{
    if (m_command) {
        delete m_command;
        m_command = NULL;
    }
}

void TimeBasedCommandParticleController::Update(ctempmodel_t *ct)
{
    if ((float)cg.time >= ct->tbcNextCommandTime) {
        if (ct->m_spawnthing && ct->m_spawnthing->m_tbcManager) {
            ct->m_spawnthing->m_tbcManager->SetNextParticleCommand(ct);
        }
    }

    if (m_command) {
        m_command->HandleCommand(ct);
    }
}

TimeBasedCommandEmitterController::TimeBasedCommandEmitterController(void)
{
    m_command = NULL;
}

TimeBasedCommandEmitterController::~TimeBasedCommandEmitterController(void)
{
    if (m_command) {
        delete m_command;
        m_command = NULL;
    }
}

void TimeBasedCommandEmitterController::Update(spawnthing_t *sp)
{
    if ((float)cg.time >= sp->tbcNextCommandTime) {
        if (sp->m_tbcManager) {
            sp->m_tbcManager->SetNextEmitterCommand(sp);
        }
    }

    if (m_command) {
        m_command->HandleCommand(sp);
    }
}

spawnthing_t::~spawnthing_t(void)
{
    if (m_tbcManager) {
        delete m_tbcManager;
        m_tbcManager = NULL;
    }

    if (tbcController) {
        delete tbcController;
        tbcController = NULL;
    }
}

void spawnthing_t::createTBC_Controller(void)
{
    tbcController = new TimeBasedCommandEmitterController;
}


void ctempmodel_t::createTBC_Controller(void)
{
    tbcController = new TimeBasedCommandParticleController;
}

void ctempmodel_t::deleteTBC_Controller(void)
{
    if (tbcController) {
        delete tbcController;
        tbcController = NULL;
    }
}
