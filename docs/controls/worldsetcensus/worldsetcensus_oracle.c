/*
 * worldsetcensus_oracle.c -- the world-set census's offline oracle.
 *
 * It compiles the census's own decision functions out of the SHIPPING
 * code/renderervulkan/vk_shade.c (region_extract.py takes the text between the
 * WORLD_SET_CENSUS markers, so there is no copy to drift) and drives them over
 * a table of cases built from SHIPPED data:
 *
 *   - the `textures/borg/met_trim3` set of pak0.pk3 shaders/borg.shader:1871
 *     as the engine presents it: four declared stages, the first two collapsed
 *     to one multitexture pass -> three passes and four bound units (the
 *     reference's own stage collapse, mirrored in the port's FinishShader);
 *   - the three single-image `menu/Intrepid-02` stages that misfired;
 *   - a two-image animMap bundle;
 *   - a one-image bundle whose unit 1 still holds the previous bind;
 *   - a lightmapped surface (bundle[0] = *lightmap15, the per-surface instance
 *     the 15:18 log names).
 *
 * Green on the shipping census; red on each planted fault, selected by
 * region_extract.py.
 *
 * Build/run: docs/controls/worldsetcensus/run.sh
 */

#include "tr_local.h"

#include <stdio.h>
#include <string.h>

#include "worldset_census_region.inc"

/* ---- checks --------------------------------------------------------------- */

static int	s_checks;
static int	s_fails;

#define CHECK( cond, ... )													\
	do {																	\
		s_checks++;															\
		if ( !( cond ) ) {													\
			s_fails++;														\
			printf( "  FAIL  " );											\
			printf( __VA_ARGS__ );											\
			printf( "\n" );													\
		}																	\
	} while ( 0 )

/* ---- the shipped images --------------------------------------------------- */

#define MAX_CASE_IMAGES 16

static image_t			s_images[MAX_CASE_IMAGES];
static int				s_imageCount;

static image_t *Image( const char *name, int texnum )
{
	image_t	*image;

	if ( s_imageCount >= MAX_CASE_IMAGES ) {
		printf( "  FAIL  the case table overflowed its image pool\n" );
		s_fails++;
		return &s_images[0];
	}
	image = &s_images[s_imageCount++];
	memset( image, 0, sizeof( *image ) );
	snprintf( image->imgName, sizeof( image->imgName ), "%s", name );
	image->texnum = (unsigned int)texnum;
	return image;
}

static void Bundle1( textureBundle_t *bundle, image_t *image )
{
	memset( bundle, 0, sizeof( *bundle ) );
	bundle->image[0] = image;
	/* a `map`/`clampmap` stage: the parser leaves numImageAnimations at 0 */
	bundle->numImageAnimations = 0;
}

static void Bundle2( textureBundle_t *bundle, image_t *a, image_t *b )
{
	memset( bundle, 0, sizeof( *bundle ) );
	bundle->image[0] = a;
	bundle->image[1] = b;
	bundle->numImageAnimations = 2;
}

int main( void )
{
	/* pak0.pk3 shaders/borg.shader:1871 -- textures/borg/met_trim3, four
	   stages: opaque diffuse; met_trim3fx add (tcmod scroll, detail); diffuse
	   SRC_ALPHA/ONE_MINUS_SRC_ALPHA; $lightmap DST_COLOR/ZERO depthFunc equal */
	image_t			*trim3     = Image( "textures/borg/met_trim3.tga", 218 );
	image_t			*trim3fx   = Image( "textures/shaderfx/met_trim3fx.tga", 197 );
	image_t			*lm15      = Image( "*lightmap15", 118 );
	image_t			*foreign   = Image( "textures/sprites/steam.tga", 900 );

	/* one texture name, two image_t: a differing picmip or wrap creates a
	   second, and the same art is still the stage's own */
	image_t			*trim3Dup  = Image( "textures/borg/met_trim3.tga", 901 );

	/* pak0.pk3 shaders/menu.shader:1338 -- menu/Intrepid-02, three single-image
	   stages; the same batch that read expected=3 drawn=3 bindmiss=6 */
	image_t			*envDiff   = Image( "models/sky/UI-dome/env_diffused.tga", 42 );
	image_t			*intrepid2 = Image( "models/sky/UI-dome/intrepid2.tga", 43 );
	image_t			*intrepid2g= Image( "models/sky/UI-dome/intrepid2-glow.tga", 44 );

	/* the menu dome's unit 1 after an earlier bind: the star dome's nebula */
	image_t			*nebula    = Image( "models/sky/UI-dome/nebula-red.tga", 51 );

	image_t			*star      = Image( "models/sky/UI-dome/stars-large.tga", 49 );
	image_t			*nebulaB   = Image( "models/sky/UI-dome/nebula-blue.tga", 50 );

	textureBundle_t	p0b0, p0b1, p1b0, p1b1, p2b0, p2b1;
	textureBundle_t	menu0, menu1, menu2, menuU1;
	textureBundle_t	anim, empty;

	/* met_trim3 as FinishShader presents it: pass 0 is the collapsed
	   diffuse+add multitexture pair (bundle 0 the diffuse, bundle 1 the fx),
	   pass 1 the alpha diffuse, pass 2 the lightmap instance of this surface */
	Bundle1( &p0b0, trim3 );
	Bundle1( &p0b1, trim3fx );
	Bundle1( &p1b0, trim3 );
	memset( &p1b1, 0, sizeof( p1b1 ) );
	Bundle1( &p2b0, lm15 );
	p2b0.isLightmap = qtrue;
	memset( &p2b1, 0, sizeof( p2b1 ) );

	Bundle1( &menu0, envDiff );
	Bundle1( &menu1, intrepid2 );
	Bundle1( &menu2, intrepid2g );
	memset( &menuU1, 0, sizeof( menuU1 ) );	/* menu/Intrepid-02 stage 1 has one image */

	Bundle2( &anim, star, nebulaB );
	memset( &empty, 0, sizeof( empty ) );

	/* ---- the predicate: the stage's own image is owned ------------------- */

	/* the three units that read MISS with wanted == bound on every world
	   surface: a single-image bundle carries numImageAnimations == 0 */
	CHECK( VK_StageImageOwned( &p1b0, trim3 ), "met_trim3 pass 1 unit 0: its own image read foreign" );
	CHECK( VK_StageImageOwned( &p0b0, trim3 ), "met_trim3 pass 0 unit 0: its own image read foreign" );
	CHECK( VK_StageImageOwned( &p0b1, trim3fx ), "met_trim3 pass 0 unit 1: its own fx image read foreign" );
	CHECK( VK_StageImageOwned( &p2b0, lm15 ), "met_trim3 pass 2 unit 0: its lightmap read foreign" );

	CHECK( VK_StageImageOwned( &menu0, envDiff ), "menu/Intrepid-02 stage 0: its own image read foreign" );
	CHECK( VK_StageImageOwned( &menu1, intrepid2 ), "menu/Intrepid-02 stage 1: its own image read foreign" );
	CHECK( VK_StageImageOwned( &menu2, intrepid2g ), "menu/Intrepid-02 stage 2: its own image read foreign" );

	/* a genuinely foreign bind is still foreign */
	CHECK( !VK_StageImageOwned( &p1b0, foreign ), "a foreign image in a one-image bundle was accepted" );
	CHECK( !VK_StageImageOwned( &p0b0, foreign ), "a foreign image in a two-image bundle was accepted" );
	CHECK( !VK_StageImageOwned( &anim, trim3fx ), "a foreign image in an animMap bundle was accepted" );
	CHECK( !VK_StageImageOwned( &empty, foreign ), "a foreign image in an empty bundle was accepted" );

	/* an animMap bundle owns every frame the clock can select */
	CHECK( VK_StageImageOwned( &anim, star ), "animMap frame 0 was not owned" );
	CHECK( VK_StageImageOwned( &anim, nebulaB ), "animMap frame 1 was not owned" );

	/* the same texture name resolved to a second image_t */
	CHECK( VK_StageImageOwned( &p1b0, trim3Dup ), "a second image_t of the same name was called foreign" );

	/* ---- the unit decision: an unbound unit is not judged ----------------- */

	CHECK( VK_WorldsetUnitExpected( &p1b0 ), "met_trim3 pass 1 unit 0 is bound and was not expected" );
	CHECK( VK_WorldsetUnitExpected( &p0b1 ), "met_trim3 pass 0 unit 1 is bound and was not expected" );
	CHECK( !VK_WorldsetUnitExpected( &p1b1 ), "an empty bundle was expected to bind" );
	CHECK( !VK_WorldsetUnitExpected( &p2b1 ), "an empty bundle was expected to bind" );

	/* the exact misfire the log holds: the menu dome's unit 1 still holds the
	   star dome's nebula from an earlier bind, and the stage binds no unit 1
	   at all -- so the unit decision is what keeps the stale image out of the
	   census, and the predicate on that empty bundle owns nothing */
	CHECK( !VK_WorldsetUnitExpected( &menuU1 ), "menu/Intrepid-02 stage 1 has no unit 1 and was judged" );
	CHECK( !VK_StageImageOwned( &menuU1, nebula ), "an empty bundle owned the previous bind" );
	CHECK( VK_StageImageOwned( &menu1, intrepid2 ), "menu/Intrepid-02 stage 1 unit 0 stopped being owned" );

	/* ---- the cadence ------------------------------------------------------ */

	CHECK( VK_WorldsetSampleDue( 1, 7, -1 ), "the first rows did not emit" );
	CHECK( !VK_WorldsetSampleDue( 25, 7, -1 ), "a row emitted on a frame outside the interval" );
	CHECK( VK_WorldsetSampleDue( 25, VK_WORLDSET_SAMPLE_FRAMES, -1 ),
		   "a row did not emit on the interval" );
	CHECK( !VK_WorldsetSampleDue( 25, VK_WORLDSET_SAMPLE_FRAMES, VK_WORLDSET_SAMPLE_FRAMES ),
		   "two agreeing rows emitted in one frame" );

	{
		/* the world's shape: ~4700 agreeing batches per frame.  At most one
		   row per frame, and a bounded number over a 60 s session. */
		int	frame, rows, due = 0, maxPerFrame = 0, last = -1;

		for ( frame = 1; frame <= 3600; frame++ ) {
			int	thisFrame = 0;

			for ( rows = VK_WORLDSET_FIRST_ROWS + 1; rows <= 4700; rows++ ) {
				if ( VK_WorldsetSampleDue( rows, frame, last ) ) {
					thisFrame++;
					due++;
					last = frame;
				}
			}
			if ( thisFrame > maxPerFrame ) {
				maxPerFrame = thisFrame;
			}
		}

		CHECK( maxPerFrame <= 1, "a frame emitted %d agreeing rows", maxPerFrame );
		CHECK( due >= 1 && due <= 3600 / VK_WORLDSET_SAMPLE_FRAMES + VK_WORLDSET_FIRST_ROWS + 1,
			   "a 60 s session emitted %d agreeing rows, outside the bound", due );
	}

	printf( "worldsetcensus: %d checks, %d failures\n", s_checks, s_fails );
	return s_fails ? 1 : 0;
}
