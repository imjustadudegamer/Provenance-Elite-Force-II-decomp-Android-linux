//-----------------------------------------------------------------------------
// Quaternion.h
// 
// Author:  Squirrel Eiserloh
// 
// Copyright (C) 1997 by Ritual Entertainment, Inc.
// All rights reserved.
//
// This source is may not be distributed and/or modified without
// expressly written permission by Ritual Entertainment, Inc.
// 
// DESCRIPTION:
//	Header for (and inline implementation of) a basic Quaternion class.
//	This should ultimately be part of a yet-to-be-written general-purpose
//	Rotation class at some point, time-permitting.
// 
// SPDX-License-Identifier: LicenseRef-Ritual-EF2-SDK
// Modified 2026-10-04 for this port.  The Ritual Elite Force II SDK agreement
// permits non-commercial distribution of the SDK and its New Creations
// with the agreement attached (docs/EF2-SDK-licenseAgreement.txt); it
// forbids derivative works (section 2(j)) and requires the notice above
// to stay intact (section 2(k)).
#ifndef _QUATERNION_H_
#define _QUATERNION_H_

#include <math.h>
#include "../corepp/vector.h"

#define UNUSED_ARG (void)



class Quaternion
{
private:
	float _x; 
	float _y; 
	float _z; 
	float _w; 

protected:

private:

public:
	~Quaternion();
	explicit Quaternion(); 
	Quaternion( const Quaternion& rhs ); 
	explicit Quaternion( const float x, const float y, const float z, const float w );
	explicit Quaternion( const float w, const Vector& vec );
	explicit Quaternion( const Vector& eulerAngles );

public:
	float CalcLength( void ) const;
	float CalcLengthSquared( void ) const;
	float Normalize( void );

	void SetFromSV( const float w, const Vector& vec );
	void SetFromXYZW( const float x, const float y, const float z, const float w );
	void SetFromEuler( const Vector& eulerAngles );

	void GetToSV( float& w, Vector& vec ) const;
	void GetToXYZW( float& x, float& y, float& z, float& w ) const;
	void GetToEuler( Vector& eulerAngles ) const;

	const Quaternion& operator = ( const Quaternion& rhs );
	const Quaternion& operator += ( const Quaternion& rhs );
	const Quaternion& operator -= ( const Quaternion& rhs );
	const Quaternion& operator *= ( const float scale );
	const Quaternion& operator *= ( const Quaternion& rhs );
	const Quaternion& operator /= ( const float invScale );
	const Quaternion operator - () const;

	const Quaternion operator + ( const Quaternion& rhs ) const;
	const Quaternion operator - ( const Quaternion& rhs ) const;

	bool operator == ( const Quaternion& rhs ) const;
	bool operator != ( const Quaternion& rhs ) const;

private:
	const Quaternion operator * ( const float scale ) const; 
};
const Quaternion operator * ( const Quaternion& lhs, const Quaternion& rhs );
const Quaternion operator * ( const float scale, const Quaternion& rhs );
const Quaternion operator / ( const Quaternion& lhs, const float invScale );
const Quaternion CalcSlerp( const Quaternion& q1, const Quaternion& q2, const float fraction );
const Quaternion CalcLerp( const Quaternion& q1, const Quaternion& q2, const float q2Fraction );
const Quaternion CalcNoLerp( const Quaternion& q1, const Quaternion& q2, const float q2Fraction );
float QuaternionDotProduct( const Quaternion& q1, const Quaternion& q2 );



inline Quaternion::~Quaternion()
{
}


inline Quaternion::Quaternion()
{
}


inline Quaternion::Quaternion( const Quaternion& rhs )
{
	SetFromXYZW( rhs._x, rhs._y, rhs._z, rhs._w );
}


inline Quaternion::Quaternion( const float x, const float y, const float z, const float w )
{
	SetFromXYZW( x, y, z, w );
}


inline Quaternion::Quaternion( const float w, const Vector& vec )
{
	SetFromSV( w, vec );
}


inline Quaternion::Quaternion( const Vector& eulerAngles )
{
	SetFromEuler( eulerAngles );
}




inline float Quaternion::CalcLength( void ) const
{
	float length = (float) sqrt( CalcLengthSquared() );
	return( length );
}


inline float Quaternion::CalcLengthSquared( void ) const
{
	float lengthSquared;
	lengthSquared = (_x * _x) + (_y * _y) + (_z * _z) + (_w * _w);
	return( lengthSquared );
}


inline float Quaternion::Normalize( void )
{
	float length = CalcLength();
	if( !length )
		return( 0.0f );

	*this /= length;
	return( length );
}


inline void Quaternion::SetFromSV( const float w, const Vector& vec )
{
	SetFromXYZW( vec.x, vec.y, vec.z, w );
}


inline void Quaternion::SetFromXYZW( const float x, const float y, const float z, const float w )
{
	_x = x;
	_y = y;
	_z = z;
	_w = w;
}


inline void Quaternion::SetFromEuler( const Vector& eulerAngles )
{
	float quat[ 4 ];
	vec3_t eulerAng;

	eulerAngles.copyTo( eulerAng );
	EulerToQuat( eulerAng, quat );

	_x = quat[ 0 ];
	_y = quat[ 1 ];
	_z = quat[ 2 ];
	_w = quat[ 3 ];
}




inline void Quaternion::GetToSV( float& w, Vector& vec ) const
{
	GetToXYZW( vec.x, vec.y, vec.z, w );
}


inline void Quaternion::GetToXYZW( float& x, float& y, float& z, float& w ) const
{
	x = _x;
	y = _y;
	z = _z;
	w = _w;
}


inline void Quaternion::GetToEuler( Vector& eulerAngles ) const
{
	float matrix[ 3 ][ 3 ];
	float quat[ 4 ];
	vec3_t eulerAng;

	quat[ 0 ] = _x;
	quat[ 1 ] = _y;
	quat[ 2 ] = _z;
	quat[ 3 ] = _w;

	QuatToMat( quat, matrix );
	MatrixToEulerAngles( matrix, eulerAng );
	eulerAngles.setXYZ( eulerAng[0], eulerAng[1], eulerAng[2] );
}




inline const Quaternion& Quaternion::operator = ( const Quaternion& rhs )
{
	if( this == &rhs )
		return *this;

	_x = rhs._x;
	_y = rhs._y;
	_z = rhs._z;
	_w = rhs._w;
	return *this;
}


inline const Quaternion& Quaternion::operator += ( const Quaternion& rhs )
{
	_x += rhs._x;
	_y += rhs._y;
	_z += rhs._z;
	_w += rhs._w;
	return *this;
}


inline const Quaternion& Quaternion::operator -= ( const Quaternion& rhs )
{
	_x -= rhs._x;
	_y -= rhs._y;
	_z -= rhs._z;
	_w -= rhs._w;
	return *this;
}


inline const Quaternion& Quaternion::operator *= ( const float scale )
{
	_x *= scale;
	_y *= scale;
	_z *= scale;
	_w *= scale;
	return *this;
}


inline const Quaternion& Quaternion::operator *= ( const Quaternion& rhs )
{
	float s1, s2, s3; 
	Vector v1, v2, v3; 

	GetToSV( s1, v1 );
	rhs.GetToSV( s2, v2 );

	s3 = (s1 * s2) - Vector::Dot( v1, v2 );

	Vector v1CrossV2; 
	v1CrossV2.CrossProduct( v1, v2 ); 
	v3 = (s1 * v2) + (s2 * v1) + v1CrossV2;

	SetFromSV( s3, v3 );
	return *this;
}


inline const Quaternion& Quaternion::operator /= ( const float invScale )
{
	if( invScale )
	{
		_x /= invScale;
		_y /= invScale;
		_z /= invScale;
		_w /= invScale;
	}
	else
	{
		SetFromXYZW( 0.0f, 0.0f, 0.0f, 0.0f );
	}

	return *this;
}


inline const Quaternion Quaternion::operator - () const
{
	return Quaternion( -_x, -_y, -_z, -_w );
}


inline bool Quaternion::operator == ( const Quaternion& rhs ) const
{
	if( _x != rhs._x )
		return false;

	if( _y != rhs._y )
		return false;

	if( _z != rhs._z )
		return false;

	if( _w != rhs._w )
		return false;

	return true;
}


inline bool Quaternion::operator != ( const Quaternion& rhs ) const
{
	return( !(*this == rhs) );
}


inline const Quaternion Quaternion::operator - ( const Quaternion& rhs ) const
{
	Quaternion difference( *this );
	difference -= rhs;
	return( difference );
}



inline const Quaternion operator * ( const Quaternion& lhs, const Quaternion& rhs )
{
	Quaternion product( lhs );
	product *= rhs;
	return product;
}


inline const Quaternion operator * ( const float scale, const Quaternion& rhs )
{
	Quaternion scaled( rhs );
	scaled *= scale;
	return scaled;
}


inline const Quaternion operator / ( const Quaternion& lhs, const float invScale )
{
	Quaternion scaled( lhs );
	if( invScale )
	{
		scaled /= invScale;
	}
	else
	{
		scaled *= 0.0f;
	}
	return scaled;
}


inline const Quaternion CalcLerp( const Quaternion& q1, const Quaternion& q2, const float q2Fraction )
{
	const float q1Fraction = 1.0f - q2Fraction;

	if( &q1 == &q2 )
		return Quaternion( q1 );

	if( q1 == q2 )
		return Quaternion( q1 );

	if( q2Fraction <= 0.0f )
		return Quaternion( q1 );
	if( q2Fraction >= 1.0f )
		return Quaternion( q2 );

	Quaternion lerped( (q1Fraction * q1) + (q2Fraction * q2) );
	float lerpedLength = lerped.Normalize();

	if( !lerpedLength )
	{
		return CalcNoLerp( q1, q2, q2Fraction );
	}
		
	return lerped;
}


inline const Quaternion CalcSlerp( const Quaternion& q1, const Quaternion& q2, const float q2Fraction )
{
	static const float SIN_OMEGA_EPSILON = 0.00001f;
	const float q1Fraction = 1.0f - q2Fraction;

	if( &q1 == &q2 )
		return Quaternion( q1 );

	if( q1 == q2 )
		return Quaternion( q1 );

	if( q2Fraction <= 0.0f )
		return Quaternion( q1 );
	if( q2Fraction >= 1.0f )
		return Quaternion( q2 );

	float cosOmega = QuaternionDotProduct( q1, q2 );

	Quaternion q2copy( q2 );
	if( cosOmega < 0.0f )
	{
		cosOmega = -cosOmega;
		q2copy *= -1.0f;
	}

	if( cosOmega > 1.0f )
	{
		return CalcNoLerp( q1, q2, q2Fraction );
	}

	if( (1.0f - cosOmega) > SIN_OMEGA_EPSILON )
	{
		return CalcLerp( q1, q2, q2Fraction );
	}

	if( (cosOmega + 1.0) < SIN_OMEGA_EPSILON )
	{
		return CalcNoLerp( q1, q2, q2Fraction );
	}

	const float omega = acos( cosOmega );
	const float sinOmega = sin( omega );

	if( (sinOmega > -SIN_OMEGA_EPSILON) && (sinOmega < SIN_OMEGA_EPSILON) )
	{
		return CalcNoLerp( q1, q2, q2Fraction );
	}

	Quaternion slerped;
	float scale1 = sin( q1Fraction * omega ) / sinOmega;
	float scale2 = sin( q2Fraction * omega ) / sinOmega;
	slerped = (scale1 * q1) + (scale2 * q2);


	return slerped;
}




inline float QuaternionDotProduct( const Quaternion& q1, const Quaternion& q2 )
{
	float x1, y1, z1, w1;
	float x2, y2, z2, w2;

	q1.GetToXYZW( x1, y1, z1, w1 );
	q2.GetToXYZW( x2, y2, z2, w2 );

	float dotProduct = (x1*x2) + (y1*y2) + (z1*z2) + (w1*w1);
	return dotProduct;
}



#endif 
