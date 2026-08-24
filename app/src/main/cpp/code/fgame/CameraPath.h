#ifndef _CAMERA_PATH_H_
#define _CAMERA_PATH_H_

inline size_t __attribute__((noinline)) EF2_AllocCount(int count)
{
    return ( size_t )count;
}



#include "class.h"
#include <qcommon/quaternion.h>

class CameraPath;
class CameraKeyFramePath;
class CameraKeyFrame;

const float DEFAULT_KEY_FRAME_LENGTH_SECONDS = 0.05f;


class CameraKeyFrame : public Class
{
private:
	Vector						_position;		
	Quaternion					_orientation;	
	float						_fovDegrees;	
	float						_seconds;		

public:
	inline const Vector&		GetPosition( void ) const { return _position; }
	inline const Quaternion&	GetOrientation( void ) const { return _orientation; }
	inline const float			GetFOV( void ) const { return _fovDegrees; }
	inline const float			GetSeconds( void ) const { return _seconds; }

public:
	CLASS_PROTOTYPE( CameraKeyFrame );
								CameraKeyFrame(); 
								CameraKeyFrame( const Vector& position, const Quaternion& orientation, const float fovDegrees, const float seconds );
public:
	bool						ParseFrameInfo( int frameNumber, Script& cameraPathFile, float& frameLengthInSeconds );
	const CameraKeyFrame&		Interpolate( const CameraKeyFrame& before, const CameraKeyFrame& after, const float fraction, const bool normalizeQuaternion );
	const Vector				GetEulerAngles( void ) const;
	const CameraKeyFrame&		TransformToPlaybackOffsets( const float yawOffsetDegrees, const Vector& originOffset );

	void						Archive( Archiver &arc ) override;

private:
	bool						ParseFrameInfoBlock( Script& cameraPathFile, float& frameLengthInSeconds );
};

inline void CameraKeyFrame::Archive
(
	Archiver &arc
)
{
	Class::Archive( arc );

	float x = 0.0f ;
	float y = 0.0f ;
	float z = 0.0f ;
	float w = 0.0f ;


	arc.ArchiveVector( &_position );

	if ( arc.Saving() )
	{
		_orientation.GetToXYZW( x, y, z, w );
	}

	arc.ArchiveFloat( &x );
	arc.ArchiveFloat( &y );
	arc.ArchiveFloat( &z );
	arc.ArchiveFloat( &w );

	if ( arc.Loading() )
	{
		_orientation.SetFromXYZW( x, y, z, w );
	}

	arc.ArchiveFloat( &_fovDegrees );
	arc.ArchiveFloat( &_seconds );
}



class CameraKeyFramePath : public Class
{
private:
	bool						_isLoaded;
	int							_numKeyFrames;
	CameraKeyFrame*				_keyFrameArray;
	float						_totalSeconds;

public:
	inline const bool			IsLoaded( void ) const { return _isLoaded; }
private:
	inline const int			GetNumKeyFrames( void ) const { return _numKeyFrames; }

public:
	CLASS_PROTOTYPE( CameraKeyFramePath );
								CameraKeyFramePath(); 

public:
	void						Unload( void );
	bool						ParsePathInfo( Script& cameraPathFile );
	const CameraKeyFrame		CreateInterpolatedFrameForTime( const float seconds );
	float						GetPathLengthInSeconds( void );
	void						Archive( Archiver &arc ) override;


private:
	void						GetClosestFramesForTime( CameraKeyFrame*& before, CameraKeyFrame*& after, const float seconds );
	bool						CreateFrames( const int numberOfFrames );
	const CameraKeyFrame		CreateInterpolatedFrame( const CameraKeyFrame& before, const CameraKeyFrame& after, const float seconds );
};



inline void CameraKeyFramePath::Archive
(
	Archiver &arc
)
{
	Class::Archive( arc );

	arc.ArchiveBool( &_isLoaded );
	arc.ArchiveInteger( &_numKeyFrames );

	if ( arc.Loading() )
	{
		_keyFrameArray = new CameraKeyFrame[ EF2_AllocCount( _numKeyFrames ) ];
	}

	for ( int frameIdx = 0; frameIdx < _numKeyFrames; ++frameIdx )
	{
		arc.ArchiveObject( &_keyFrameArray[ frameIdx ] );
	}

	arc.ArchiveFloat( &_totalSeconds );
}





class CameraPath : public Class
{
private:
	bool						_isLoaded;
	str							_fileName;
	CameraKeyFramePath*			_keyFramePath;
	float						_yawPlaybackOffsetDegrees;
	Vector						_originPlaybackOffset;

public:
	inline const bool			IsLoaded( void ) const { return _isLoaded; }

public:
	CLASS_PROTOTYPE( CameraPath );
								CameraPath( const str& fileName );
								CameraPath()						: _isLoaded( false ), _keyFramePath( 0 ), _yawPlaybackOffsetDegrees( 0 ) { };

public:
	void						Unload( void );
	bool						Reload( void );
	const CameraKeyFrame		GetInterpolatedFrameForTime( const float seconds );
	void						SetPlaybackOffsets( const float yawOffsetDegrees, const Vector& originOffset );
	float						GetPathLengthInSeconds( void );

	void						Archive( Archiver &arc ) override;

private:
	bool						LoadKeyFramedCameraFile( const str& fileName );
	bool						ParsePathInfo( Script& cameraPathFile );
};


inline void CameraPath::Archive
(
	Archiver &arc
)
{
	Class::Archive( arc );

	arc.ArchiveBool( &_isLoaded );
	arc.ArchiveString( &_fileName );


	if ( arc.Loading() )
	{
		_keyFramePath = ( CameraKeyFramePath*)arc.ReadObject();
	}
	else
	{
		arc.ArchiveObject( _keyFramePath );
	}

	arc.ArchiveFloat( &_yawPlaybackOffsetDegrees );
	arc.ArchiveVector( &_originPlaybackOffset );
}


#endif 
