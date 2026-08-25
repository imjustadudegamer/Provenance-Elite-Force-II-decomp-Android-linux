#include "tr_local.h"

static backEndCounters_t pc_save;

typedef struct rbFrame_s {
	backEndData_t	*backEnd;		
	const void		*cmds;			
	int				smpFrame;		
	int				frameCount;		
	int				boneArena;		
	int				backEndMsec;	
} rbFrame_t;

static rbFrame_t	rb_frames[SMP_FRAMES];
static rbFrame_t	*rb_activeFrame = NULL;		
static volatile rbFrame_t	*rb_lastExecuted = NULL;	

static qboolean		rb_serialIssue = qfalse;

int RB_FrameSmpFrame( void ) {
	return rb_activeFrame ? rb_activeFrame->smpFrame : tr.smpFrame;
}

int RB_FrameCount( void ) {
	return rb_activeFrame ? rb_activeFrame->frameCount : tr.frameCount;
}

int RB_FrameBoneArena( void ) {
	return rb_activeFrame ? rb_activeFrame->boneArena : tr.smpFrame;
}

int RB_FrameBackEndMsec( void ) {
	return rb_lastExecuted ? rb_lastExecuted->backEndMsec : backEnd.pc.msec;
}

void R_SavePerformanceCounters(void) {
    memcpy(&pc_save, &backEnd.pc, sizeof(pc_save));
}

void R_PerformanceCounters( void ) {
	if (r_fps->integer) {
		ri.SetPerformanceCounters(
			backEnd.pc.c_totalIndexes / 3,
			backEnd.pc.c_vertexes,
			R_SumOfUsedImages(),
			pc_save.c_totalIndexes / 3,
			pc_save.c_vertexes,
			backEnd.pc.c_characterlights
			);
	}
	if ( !r_speeds->integer ) {
		Com_Memset( &tr.pc, 0, sizeof( tr.pc ) );
		Com_Memset( &backEnd.pc, 0, sizeof( backEnd.pc ) );
		return;
	}

	if (r_speeds->integer == 1) {
		ri.Printf (PRINT_ALL, "%i/%i shaders/surfs %i leafs %i verts %i/%i tris %.2f mtex %.2f dc\n",
			backEnd.pc.c_shaders, backEnd.pc.c_surfaces, tr.pc.c_leafs, backEnd.pc.c_vertexes, 
			backEnd.pc.c_indexes/3, backEnd.pc.c_totalIndexes/3, 
			R_SumOfUsedImages()/(1000000.0f), backEnd.pc.c_overDraw / (float)(glConfig.vidWidth * glConfig.vidHeight) ); 
	} else if (r_speeds->integer == 2) {
		ri.Printf (PRINT_ALL, "(patch) %i sin %i sclip  %i sout %i bin %i bclip %i bout\n",
			tr.pc.c_sphere_cull_patch_in, tr.pc.c_sphere_cull_patch_clip, tr.pc.c_sphere_cull_patch_out, 
			tr.pc.c_box_cull_patch_in, tr.pc.c_box_cull_patch_clip, tr.pc.c_box_cull_patch_out );
		ri.Printf (PRINT_ALL, "(md3) %i sin %i sclip  %i sout %i bin %i bclip %i bout\n",
			tr.pc.c_sphere_cull_md3_in, tr.pc.c_sphere_cull_md3_clip, tr.pc.c_sphere_cull_md3_out, 
			tr.pc.c_box_cull_md3_in, tr.pc.c_box_cull_md3_clip, tr.pc.c_box_cull_md3_out );
	} else if (r_speeds->integer == 3) {
		ri.Printf (PRINT_ALL, "viewcluster: %i\n", tr.viewCluster );
	} else if (r_speeds->integer == 4) {
		if ( backEnd.pc.c_dlightVertexes ) {
			ri.Printf (PRINT_ALL, "dlight srf:%i  culled:%i  verts:%i  tris:%i\n", 
				tr.pc.c_dlightSurfaces, tr.pc.c_dlightSurfacesCulled,
				backEnd.pc.c_dlightVertexes, backEnd.pc.c_dlightIndexes / 3 );
		}
	} 
	else if (r_speeds->integer == 5 )
	{
		ri.Printf( PRINT_ALL, "zFar: %.0f\n", tr.viewParms.zFar );
	}
	else if (r_speeds->integer == 6 )
	{
		ri.Printf( PRINT_ALL, "flare adds:%i tests:%i renders:%i\n", 
			backEnd.pc.c_flareAdds, backEnd.pc.c_flareTests, backEnd.pc.c_flareRenders );
	}

	Com_Memset( &tr.pc, 0, sizeof( tr.pc ) );
	Com_Memset( &backEnd.pc, 0, sizeof( backEnd.pc ) );
}


void R_IssueRenderCommands( qboolean runPerformanceCounters ) {
	renderCommandList_t	*cmdList;
	rbFrame_t			*frame;

	cmdList = &backEndData->commands;
	assert(cmdList);
	*(int *)(cmdList->cmds + cmdList->used) = RC_END_OF_LIST;

	cmdList->used = 0;

	if ( runPerformanceCounters ) {
		R_PerformanceCounters();
	}

	{
		frame = &rb_frames[tr.smpFrame];
		frame->backEnd = backEndData;
		frame->cmds = cmdList->cmds;
		frame->smpFrame = tr.smpFrame;
		frame->frameCount = tr.frameCount;
		frame->boneArena = tr.smpFrame;
		frame->backEndMsec = 0;

		if ( glConfig.smpActive && !rb_serialIssue && !GLimp_IsRenderThread() ) {
			GLimp_WakeRenderer( frame );
			return;
		}

		if ( !r_skipBackEnd->integer ) {
			rb_activeFrame = frame;
			RB_ExecuteRenderCommands( cmdList->cmds );
			frame->backEndMsec = backEnd.pc.msec;
			rb_activeFrame = NULL;
		}
		rb_lastExecuted = frame;
	}
}


void RB_RenderThread( void ) {
	const void *data;

	data = GLimp_RendererSleep();

	while ( data != NULL ) {
		rbFrame_t *frame = (rbFrame_t *)data;

		rb_activeFrame = frame;

		if ( !r_skipBackEnd->integer ) {
			RB_ExecuteRenderCommands( frame->cmds );
			frame->backEndMsec = backEnd.pc.msec;
		}

		rb_activeFrame = NULL;
		rb_lastExecuted = frame;

		{
			static int smp_mcore_render_rows = 0;

			if ( smp_mcore_render_rows < 8 || ( smp_mcore_render_rows % 600 ) == 0 ) {
				ri.Printf( PRINT_ALL,
						   "[SMP:smphandoff] side=render back=%d front=%d arena=%d owner=%d\n",
						   frame->smpFrame, tr.smpFrame, frame->boneArena,
						   GLimp_HandshakeViolations() );
			}
			smp_mcore_render_rows++;
		}

		data = GLimp_RendererSleep();
	}
}


void R_IssuePendingRenderCommands( void ) {
	if ( !tr.registered ) {
		return;
	}
	if ( backEndData->commands.used == 0 ) {
		if ( glConfig.smpActive && !GLimp_IsRenderThread() ) {
			R_SyncRenderThread();
		}
		return;
	}
	if ( glConfig.smpActive && !GLimp_IsRenderThread() ) {
		R_SyncRenderThread();
		rb_serialIssue = qtrue;
		R_IssueRenderCommands( qfalse );
		rb_serialIssue = qfalse;
		return;
	}
	R_IssueRenderCommands( qfalse );
}

void *R_GetCommandBufferReserved( int bytes, int reservedBytes ) {
	renderCommandList_t	*cmdList;

	cmdList = &backEndData->commands;
	bytes = PAD(bytes, sizeof(void *));

	if ( cmdList->used + bytes + sizeof( int ) + reservedBytes > MAX_RENDER_COMMANDS ) {
		if ( bytes > MAX_RENDER_COMMANDS - sizeof( int ) ) {
			ri.Error( ERR_FATAL, "R_GetCommandBuffer: bad size %i", bytes );
		}
		ri.Printf( PRINT_WARNING, "Failed to allocate render command of size %d\n", bytes );
		return NULL;
	}

	cmdList->used += bytes;

	return cmdList->cmds + cmdList->used - bytes;
}

void *R_GetCommandBuffer( int bytes ) {
	return R_GetCommandBufferReserved( bytes, PAD( sizeof( swapBuffersCommand_t ), sizeof(void *) ) );
}


void	R_AddDrawSurfCmd( drawSurf_t *drawSurfs, int numDrawSurfs ) {
	drawSurfsCommand_t	*cmd;

	cmd = R_GetCommandBuffer( sizeof( *cmd ) );
	if ( !cmd ) {
		return;
	}
	cmd->commandId = RC_DRAW_SURFS;

	cmd->drawSurfs = drawSurfs;
	cmd->numDrawSurfs = numDrawSurfs;

	cmd->refdef = tr.refdef;
	cmd->viewParms = tr.viewParms;
}


void	R_AddSpriteSurfCmd(drawSurf_t* drawSurfs, int numDrawSurfs) {
    drawSurfsCommand_t* cmd;

    cmd = R_GetCommandBuffer(sizeof(*cmd));
    if (!cmd) {
        return;
    }
    cmd->commandId = RC_SPRITE_SURFS;

    cmd->drawSurfs = drawSurfs;
    cmd->numDrawSurfs = numDrawSurfs;

    cmd->refdef = tr.refdef;
    cmd->viewParms = tr.viewParms;
}


void	RE_SetColor( const float *rgba ) {
	setColorCommand_t	*cmd;

  if ( !tr.registered ) {
    return;
  }
	cmd = R_GetCommandBuffer( sizeof( *cmd ) );
	if ( !cmd ) {
		return;
	}
	cmd->commandId = RC_SET_COLOR;
	if ( !rgba ) {
		static float white[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

		rgba = white;
	}

	cmd->color[0] = rgba[0];
	cmd->color[1] = rgba[1];
	cmd->color[2] = rgba[2];
	cmd->color[3] = rgba[3];
}


void RE_StretchPic ( float x, float y, float w, float h, 
					  float s1, float t1, float s2, float t2, qhandle_t hShader ) {
	stretchPicCommand_t	*cmd;

  if (!tr.registered) {
    return;
  }
	cmd = R_GetCommandBuffer( sizeof( *cmd ) );
	if ( !cmd ) {
		return;
	}
	cmd->commandId = RC_STRETCH_PIC;
	cmd->shader = R_GetShaderByHandle( hShader );
	cmd->x = x;
	cmd->y = y;
	cmd->w = w;
	cmd->h = h;
	cmd->s1 = s1;
	cmd->t1 = t1;
	cmd->s2 = s2;
	cmd->t2 = t2;
}


void RE_BeginFrame( stereoFrame_t stereoFrame ) {
	drawBufferCommand_t	*cmd = NULL;

	if ( !tr.registered ) {
		return;
	}

	tr.frameCount++;
	tr.frameSceneNum = 0;
	g_nStaticSurfaces = 0;

	if ( r_measureOverdraw->integer )
	{
		if ( glConfig.stencilBits < 4 )
		{
			ri.Printf( PRINT_ALL, "Warning: not enough stencil bits to measure overdraw: %d\n", glConfig.stencilBits );
			ri.Cvar_Set( "r_measureOverdraw", "0" );
			r_measureOverdraw->modified = qfalse;
		}
		else if ( r_shadows->integer == 2 )
		{
			ri.Printf( PRINT_ALL, "Warning: stencil shadows and overdraw measurement are mutually exclusive\n" );
			ri.Cvar_Set( "r_measureOverdraw", "0" );
			r_measureOverdraw->modified = qfalse;
		}
		else
		{
			R_IssuePendingRenderCommands();
			qglEnable( GL_STENCIL_TEST );
			qglStencilMask( ~0U );
			qglClearStencil( 0U );
			qglStencilFunc( GL_ALWAYS, 0U, ~0U );
			qglStencilOp( GL_KEEP, GL_INCR, GL_INCR );
		}
		r_measureOverdraw->modified = qfalse;
	}
	else
	{
		if ( r_measureOverdraw->modified ) {
			R_IssuePendingRenderCommands();
			qglDisable( GL_STENCIL_TEST );
		}
		r_measureOverdraw->modified = qfalse;
	}

	if ( r_textureMode->modified ) {
		R_IssuePendingRenderCommands();
		GL_TextureMode( r_textureMode->string );
		r_textureMode->modified = qfalse;
	}

	if ( r_gamma->modified ) {
		r_gamma->modified = qfalse;

		R_IssuePendingRenderCommands();
		R_SetColorMappings();
	}

	if ( !r_ignoreGLErrors->integer )
	{
		int	err;

		R_IssuePendingRenderCommands();
		if ((err = qglGetError()) != GL_NO_ERROR)
			ri.Error(ERR_FATAL, "RE_BeginFrame() - glGetError() failed (0x%x)!", err);
	}

	if( !(cmd = R_GetCommandBuffer(sizeof(*cmd))) )
		return;

	cmd->commandId = RC_DRAW_BUFFER;

	if ( glConfig.stereoEnabled ) {
		if ( stereoFrame == STEREO_LEFT ) {
			cmd->buffer = (int)GL_BACK_LEFT;
		} else if ( stereoFrame == STEREO_RIGHT ) {
			cmd->buffer = (int)GL_BACK_RIGHT;
		} else {
			ri.Error( ERR_FATAL, "RE_BeginFrame: Stereo is enabled, but stereoFrame was %i", stereoFrame );
		}
	}
	else
	{

		cmd->commandId = RC_DRAW_BUFFER;

		if (!Q_stricmp(r_drawBuffer->string, "GL_FRONT"))
			cmd->buffer = (int)GL_FRONT;
		else
			cmd->buffer = (int)GL_BACK;
	}
	
	tr.refdef.stereoFrame = stereoFrame;
}


void RE_EndFrame( int *frontEndMsec, int *backEndMsec ) {
	swapBuffersCommand_t	*cmd;

	if ( !tr.registered ) {
		return;
	}
	cmd = R_GetCommandBufferReserved( sizeof( *cmd ), 0 );
	if ( !cmd ) {
		return;
	}
	cmd->commandId = RC_SWAP_BUFFERS;

	R_IssueRenderCommands( qtrue );

	R_InitNextFrame();

	RB_MtFrameBoundary();

	if ( frontEndMsec ) {
		*frontEndMsec = tr.frontEndMsec;
	}
	tr.frontEndMsec = 0;
	if ( backEndMsec ) {
		*backEndMsec = RB_FrameBackEndMsec();
	}
}

void RE_TakeVideoFrame( int width, int height,
		byte *captureBuffer, byte *encodeBuffer, qboolean motionJpeg )
{
	videoFrameCommand_t	*cmd;

	if( !tr.registered ) {
		return;
	}

	cmd = R_GetCommandBuffer( sizeof( *cmd ) );
	if( !cmd ) {
		return;
	}

	cmd->commandId = RC_VIDEOFRAME;

	cmd->width = width;
	cmd->height = height;
	cmd->captureBuffer = captureBuffer;
	cmd->encodeBuffer = encodeBuffer;
	cmd->motionJpeg = motionJpeg;
}
