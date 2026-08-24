
#include "_pch_cpp.h"
#include "CameraPath.h"




CLASS_DECLARATION( Class, CameraKeyFrame, NULL )
{
	{ NULL, NULL }
};

CameraKeyFrame::CameraKeyFrame()
:
_position( Vector( 0.0f, 0.0f, 0.0f ) ),
_orientation( Quaternion( 0.0f, 0.0f, 0.0f, 1.0f ) ),
_fovDegrees( 90.0f ),
_seconds( 0.0f )
{
}


CameraKeyFrame::CameraKeyFrame( const Vector& position, const Quaternion& orientation, const float fovDegrees, const float seconds )
:
_position( position ),
_orientation( orientation ),
_fovDegrees( fovDegrees ),
_seconds( seconds )
{
}


bool CameraKeyFrame::ParseFrameInfo( int frameNumber, Script& cameraPathFile, float& totalPathTime )
{
	const char* token;

	token = cameraPathFile.GetToken( true );
	if( !stricmp( token, "Frame" ) )
	{
		int actualFrame = cameraPathFile.GetInteger( false );
		if( actualFrame != frameNumber )
		{
			cameraPathFile.error( "CameraKeyFrame::ParseFrameInfo", "Incorrect Frame number (found %d, expecting %d)\n", frameNumber, actualFrame );
			return( false );
		}
	}
	else 
	{
		gi.Printf( "Unexpected token %s in camera path file (expecting \"Frame\").\n", token );
		cameraPathFile.error( "CameraKeyFrame::ParseFrameInfo", "Unexpected token %s in camera path file (expecting \"Frame\").\n", token );
		return( false );
	}

	return( ParseFrameInfoBlock( cameraPathFile, totalPathTime ) );
}


bool CameraKeyFrame::ParseFrameInfoBlock( Script& cameraPathFile, float& totalPathTime )
{
	const char* token;
	float frameTimeInSeconds = DEFAULT_KEY_FRAME_LENGTH_SECONDS;

	token = cameraPathFile.GetToken( true );
	if( *token != '{' )
	{
		cameraPathFile.error( "CameraKeyFrame::ParseFrameInfoBlock", "Expected '{', found \"%s\"\n", token );
		return( false );
	}

	token = cameraPathFile.GetToken( true );
	while( *token != '}' )
	{
		if( !stricmp( token, "fov" ) )
		{
			_fovDegrees = cameraPathFile.GetFloat( false );
		}
		else if( !stricmp( token, "position" ) )
		{
			_position = cameraPathFile.GetVector( false );
		}
		else if( !stricmp( token, "quaternion" ) )
		{
			float x = cameraPathFile.GetFloat( false );
			float y = cameraPathFile.GetFloat( false );
			float z = cameraPathFile.GetFloat( false );
			float w = cameraPathFile.GetFloat( false );
			_orientation.SetFromXYZW( x, y, z, w );
		}
		else 
		{
			gi.Printf( "Unexpected token %s in camera path file.\n", token );
			cameraPathFile.error( "CameraKeyFrame::ParseFrameInfoBlock", "Unexpected token %s in camera path file.\n", token );
			return( false );
		}

		token = cameraPathFile.GetToken( true );
	}

	_seconds = totalPathTime;
	totalPathTime += frameTimeInSeconds;
	return( true );
}



const CameraKeyFrame& CameraKeyFrame::Interpolate( const CameraKeyFrame& before, const CameraKeyFrame& after, const float fraction, const bool normalizeQuaternion )
{
	const Vector& pos1 = before.GetPosition();
	const Vector& pos2 = after.GetPosition();
	const Quaternion& quat1 = before.GetOrientation();
	const Quaternion& quat2 = after.GetOrientation();
	float fov1 = before.GetFOV();
	float fov2 = after.GetFOV();


	_position = pos1 + ( pos2 - pos1 ) * fraction; 

	_orientation = CalcSlerp( quat1, quat2, (double) fraction ); 
	_fovDegrees = fov1 + (fraction * (fov2 - fov1) ); 

	if( normalizeQuaternion )
		_orientation.Normalize();

	return( *this );
}


const Vector CameraKeyFrame::GetEulerAngles( void ) const
{
	Vector eulerAngles;
	_orientation.GetToEuler( eulerAngles );
	return eulerAngles;
}




const CameraKeyFrame& CameraKeyFrame::TransformToPlaybackOffsets( const float yawOffsetDegrees, const Vector& originOffset )
{
	vec3_t zNormal = { 0.0f, 0.0f, 1.0f };
	vec3_t point;
	vec3_t rotated;
	_position.copyTo( point ); 
	RotatePointAroundVector( rotated, zNormal, point, yawOffsetDegrees );
	_position = rotated;

	Vector eulerOrientation;
	_orientation.GetToEuler( eulerOrientation );
	eulerOrientation.y += yawOffsetDegrees;
	_orientation.SetFromEuler( eulerOrientation );
	_orientation.Normalize();

	_position += originOffset; 

	return *this;
}



CLASS_DECLARATION( Class, CameraKeyFramePath, NULL )
{
	{ NULL, NULL }
};


CameraKeyFramePath::CameraKeyFramePath()
:
_isLoaded( false ),
_numKeyFrames( 0 ),
_keyFrameArray( NULL ),
_totalSeconds( 0.0f )
{
}


void CameraKeyFramePath::Unload( void )
{
	if( IsLoaded() )
	{
		delete [] _keyFrameArray;
		_keyFrameArray = NULL;
	}

	_totalSeconds = 0.0f;
	_isLoaded = false;
}


bool CameraKeyFramePath::CreateFrames( const int numberOfFrames )
{
	Unload();


	_keyFrameArray = new CameraKeyFrame[ EF2_AllocCount( numberOfFrames ) ];
	if( !_keyFrameArray )
		return( false );

	_numKeyFrames = numberOfFrames;
	return( true );
}


bool CameraKeyFramePath::ParsePathInfo( Script& cameraPathFile )
{
	const char* token;

	token = cameraPathFile.GetToken( true );
	if( *token != '{' )
	{
		cameraPathFile.error( "CameraKeyFramePath::ParsePathInfo", "Expected '{', found \"%s\"\n", token );
		return( false );
	}

	token = cameraPathFile.GetToken( true );
	while( *token != '}' )
	{
		if( !stricmp( token, "frameCount" ) )
		{
			_numKeyFrames = cameraPathFile.GetInteger( false );
			assert( _numKeyFrames > 0 );
		}
		else 
		{
			gi.Printf( "Unexpected token %s in camera path file.\n", token );
			cameraPathFile.error( "CameraKeyFramePath::ParsePathInfo", "Unexpected token %s in camera path file.\n", token );
			return( false );
		}
		token = cameraPathFile.GetToken( true );
	}

	CreateFrames( _numKeyFrames );

	for( int i = 0; i < _numKeyFrames; i ++ )
	{
		CameraKeyFrame& keyFrame = _keyFrameArray[ i ];
		bool success = keyFrame.ParseFrameInfo( i, cameraPathFile, _totalSeconds );
		if( !success )
			return( false );
	}

	_isLoaded = true;
	return( true );
}


void CameraKeyFramePath::GetClosestFramesForTime( CameraKeyFrame*& before, CameraKeyFrame*& after, const float seconds )
{
	int i;

	before = &_keyFrameArray[ 0 ];
	for( i = 0; i < _numKeyFrames; i ++ )
	{
		CameraKeyFrame& frame = _keyFrameArray[ i ];
		if( frame.GetSeconds() > seconds )
			break;

		before = &frame;
	}

	after = &_keyFrameArray[ _numKeyFrames - 1 ];
	for( i = _numKeyFrames - 1; i >= 0; i -- )
	{
		CameraKeyFrame& frame = _keyFrameArray[ i ];
		if( frame.GetSeconds() < seconds )
			break;

		after = &frame;
	}

}


const CameraKeyFrame CameraKeyFramePath::CreateInterpolatedFrameForTime( const float seconds )
{
	if( seconds < 0.0f )
	{
		return( _keyFrameArray[ 0 ] );
	}
	else if( seconds > _totalSeconds )
	{
		return( _keyFrameArray[ _numKeyFrames - 1 ] );
	}

	CameraKeyFrame* closestFrameBefore = NULL;
	CameraKeyFrame* closestFrameAfter = NULL;
	GetClosestFramesForTime( closestFrameBefore, closestFrameAfter, seconds );
	assert( closestFrameBefore );
	assert( closestFrameAfter );

	if( closestFrameBefore == closestFrameAfter )
		return( *closestFrameBefore );

	return( CreateInterpolatedFrame( *closestFrameBefore, *closestFrameAfter, seconds ) );
}


const CameraKeyFrame CameraKeyFramePath::CreateInterpolatedFrame( const CameraKeyFrame& before, const CameraKeyFrame& after, const float seconds )
{
	const float startTime = before.GetSeconds();
	const float endTime = after.GetSeconds();
	const float timeSpan = endTime - startTime;
	assert( timeSpan >= 0.0f );
	assert( seconds >= startTime );
	assert( seconds <= endTime );

	float fraction;
	if( timeSpan )
	{
		fraction = (seconds - startTime) / timeSpan;
	}
	else
	{
		fraction = 0.0f;
	}

	CameraKeyFrame lerped;
	lerped.Interpolate( before, after, fraction, true );
	return( lerped );
}



float CameraKeyFramePath::GetPathLengthInSeconds( void )
{
	return( _totalSeconds );
}


CLASS_DECLARATION( Class, CameraPath, NULL )
{
	{ NULL, NULL }
};


CameraPath::CameraPath( const str& fileName )
:
_isLoaded( false ),
_fileName( str( "" ) ),
_keyFramePath( NULL ),
_yawPlaybackOffsetDegrees( 0.0f ),
_originPlaybackOffset( Vector( 0.0f, 0.0f, 0.0f ) )
{
	LoadKeyFramedCameraFile( fileName );
}


bool CameraPath::LoadKeyFramedCameraFile( const str& fileName )
{
	Script			cameraPathFile;
	const char*		token;
	bool			success; 
	str				filePathName;

	Unload();

	_fileName = fileName;

	filePathName = "cams/";
	filePathName += fileName;
	filePathName += ".kfc";

	cameraPathFile.LoadFile( filePathName.c_str() );

	while( cameraPathFile.TokenAvailable( true ) )
	{
		token = cameraPathFile.GetToken( true );
		if( !stricmp( token, "Path" ) )
		{
			success = ParsePathInfo( cameraPathFile );
			if( !success )
				return( false );
		}
		else 
		{
			gi.Printf( "Unexpected token %s in camera path file %s.\n", token, _fileName.c_str() );
			cameraPathFile.error( "CameraPath::LoadKeyFramedCameraFile", "Unexpected token %s in camera path file %s.\n", token, _fileName.c_str() );
			return( false );
		}
	}

	_isLoaded = true;
	return( true );
}


void CameraPath::Unload( void )
{
	if( IsLoaded() )
	{
		_isLoaded = false;
		delete _keyFramePath;
		_keyFramePath = NULL;
	}
}


bool CameraPath::ParsePathInfo( Script& cameraPathFile )
{
	[[maybe_unused]] int pathNumber = cameraPathFile.GetInteger( false );
	assert( pathNumber == 0 ); 

	_keyFramePath = new CameraKeyFramePath;
	bool success = _keyFramePath->ParsePathInfo( cameraPathFile );
	return( success );
}


const CameraKeyFrame CameraPath::GetInterpolatedFrameForTime( const float seconds )
{
	CameraKeyFrame transformedKeyFrame( _keyFramePath->CreateInterpolatedFrameForTime( seconds ) );
	transformedKeyFrame.TransformToPlaybackOffsets( _yawPlaybackOffsetDegrees, _originPlaybackOffset );
	return( transformedKeyFrame );
}


void CameraPath::SetPlaybackOffsets( const float yawOffsetDegrees, const Vector& originOffset )
{
	_yawPlaybackOffsetDegrees = yawOffsetDegrees;
	_originPlaybackOffset = originOffset;
}



float CameraPath::GetPathLengthInSeconds( void )
{
	return( _keyFramePath->GetPathLengthInSeconds() );
}






const Quaternion CalcNoLerp( const Quaternion& q1, const Quaternion& q2, const float q2Fraction )
{
	if( q2Fraction < 0.5f )
	{
		return Quaternion( q1 );
	}
	else
	{
		return Quaternion( q2 );
	}
}

const Quaternion Quaternion::operator + ( const Quaternion& rhs ) const
{
	Quaternion sum( *this );
	sum += rhs;
	return( sum );
}
