

#ifdef __cplusplus
extern "C" {
#endif

void	StartCinematic( const char *name );

void	StopCinematic( void );

void	ProcessCinematic( void );

void	CL_AfterCinematic_f( void );

int		cinematicRunning( void );

#ifdef __cplusplus
}
#endif
