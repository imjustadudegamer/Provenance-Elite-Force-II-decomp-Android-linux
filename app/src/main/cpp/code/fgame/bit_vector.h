//-----------------------------------------------------------------------------
//
//  $Logfile:: /Code/DLLs/game/bit_vector.h                                  $
// $Revision:: 4                                                              $
//   $Author:: Steven                                                         $
//     $Date:: 10/13/03 8:53a                                                 $
//
// Copyright (C) 2001 by Ritual Entertainment, Inc.
// All rights reserved.
//
// This source may not be distributed and/or modified without
// expressly written permission by Ritual Entertainment, Inc.
//
//
// DESCRIPTION:
// This class is to used to manage arrays of bits.
// SPDX-License-Identifier: LicenseRef-Ritual-EF2-SDK
// Modified 2026-10-04 for this port.  The Ritual Elite Force II SDK agreement
// permits non-commercial distribution of the SDK and its New Creations
// with the agreement attached (docs/EF2-SDK-licenseAgreement.txt); it
// forbids derivative works (section 2(j)) and requires the notice above
// to stay intact (section 2(k)).



class BitVector;
class BitReference;

#ifndef __BIT_VECTOR_H__
#define __BIT_VECTOR_H__


class BitVector 
   {
	public:


							BitVector( unsigned int numBits, bool value = false );
							BitVector( const BitVector &old );
							~BitVector();


		void				SetBit( unsigned int index, bool value = true );
		void				ClearBit( unsigned int index );
		void				FlipBit( unsigned int index );
		bool				GetBit( unsigned int index ) const;


		void				Set( void );
		void				Clear( void );
		void				Flip( void );


		void				SetRange( unsigned int first_index, unsigned int last_index, bool value = true );
		void				ClearRange( unsigned int first_index, unsigned int last_index );


		unsigned int	BitSize( void ) const;
		unsigned int	ByteSize( void ) const;
		unsigned int	Count( void ) const;
		bool				Any( void ) const;
		bool				None( void ) const;


		bool				operator[]( unsigned int index ) const;
		BitReference	operator[]( unsigned int index );
		bool				operator== ( const BitVector &other );
		bool				operator!= ( const BitVector &other );
		void				operator|= ( const BitVector &other );
		void				operator&= ( const BitVector &other );
		BitVector		operator~ ( void );

	private:


		unsigned char	*_bytes;
		unsigned int	_numBytes;
		unsigned int	_numBits;


		friend BitVector operator| ( const BitVector &left, const BitVector &right );
		friend BitVector operator& ( const BitVector &left, const BitVector &right );

		friend BitReference;
	};


class BitReference 
	{
	public:


		BitReference&	operator= ( bool b );
		BitReference&	operator= ( const BitReference &other );
							operator bool() const;

	private:


		BitVector		*_vector;
		unsigned int	_index;


		void				Setup( BitVector *vector, unsigned int index );


		friend BitVector;
   };


BitVector::BitVector( unsigned int numBits, bool value )
   {

	_numBits = numBits;
	_numBytes = ( _numBits + 7 ) >> 3;
	_bytes = new unsigned char[ _numBytes ];


	if ( value )
		Set();
	else
		Clear();
	}


BitVector::BitVector( const BitVector &old )
   {
	int i;


	_numBits = old._numBits;
	_numBytes = old._numBytes;
	_bytes = new unsigned char[ _numBytes ];


	for ( i = 0 ; i < _numBytes ; i++ )
		_bytes[ i ] = old._bytes[ i ];
	}


BitVector::~BitVector()
   {

	delete[] _bytes;
	}


inline void BitVector::SetBit( unsigned int index, bool value )
   {
	if ( index < _numBits )
		{
		if ( value )
			_bytes[ index >> 3 ] |= ( 1 << ( index & 7 ) );				
		else
			_bytes[ index >> 3 ] &= ~( 1 << ( index & 7 ) );			
		}
	}


inline void BitVector::ClearBit( unsigned int index )
	{
	SetBit( index, false );
	}


inline void BitVector::FlipBit( unsigned int index )
	{
	bool value;

	value = GetBit( index );
	SetBit( index, !value );
	}


inline bool BitVector::GetBit( unsigned int index ) const 
	{
	if ( index < _numBits )
		return ( ( _bytes[ index >> 3 ] & ( 1 << ( index & 7 ) ) ) != 0 );
	else
		return false;
	}


inline void BitVector::Set( void )
	{
	unsigned int i;

	for ( i = 0 ; i < _numBytes ; i++ )
		_bytes[ i ] = (unsigned char)0xFF;
	}


inline void BitVector::Clear( void )
	{
	unsigned int i;

	for ( i = 0 ; i < _numBytes ; i++ )
		_bytes[ i ] = 0;
	}


inline void BitVector::Flip( void )
	{
	unsigned int i;

	for ( i = 0 ; i < _numBytes ; i++ )
		{
		_bytes[ i ] = ~_bytes[ i ];
		}
	}


inline void BitVector::SetRange( unsigned int first_index, unsigned int last_index, bool value )
	{
	unsigned int i;

	for ( i = first_index ; i < last_index ; i++ )
		SetBit( i, value );
	}


inline void BitVector::ClearRange( unsigned int first_index, unsigned int last_index )
	{
	SetRange( first_index, last_index, false );
	}


inline unsigned int BitVector::BitSize( void ) const
	{
	return _numBits;
	}


inline unsigned int BitVector::ByteSize( void ) const
	{
	return _numBytes;
	}


inline unsigned int BitVector::Count( void ) const
	{
	unsigned int i;
	unsigned count;

	count = 0;

	for ( i = 0 ; i < _numBits ; i++ )
		{
		if ( GetBit( i ) )
			count++;
		}

	return count;
	}


inline bool BitVector::Any( void ) const
	{
	unsigned int i;

	for ( i = 0 ; i < _numBits ; i++ )
		{
		if ( GetBit( i ) )
			return true;
		}

	return false;
	}


inline bool BitVector::None( void ) const
	{
	return !Any();
	}


inline bool BitVector::operator[]( unsigned int index ) const 
	{
	return GetBit( index );
	}


inline BitReference BitVector::operator[]( unsigned int index )
	{
	BitReference bitRef;

	bitRef.Setup( this, index );
	return bitRef;
	}


inline bool BitVector::operator== ( const BitVector &other )
	{
	unsigned int i;

	if ( BitSize() != other.BitSize() )
		return false;

	for ( i = 0 ; i < _numBytes ; i++ )
		{
		if ( _bytes[ i ] != other._bytes[ i ] )
			return false;
		}

	return true;
	}


inline bool BitVector::operator!= ( const BitVector &other )
	{
	unsigned int i;

	if ( BitSize() != other.BitSize() )
		return true;

	for ( i = 0 ; i < _numBytes ; i++ )
		{
		if ( _bytes[ i ] != other._bytes[ i ] )
			return true;
		}

	return false;
	}


inline void BitVector::operator|= ( const BitVector &other )
	{
	unsigned int i;

	if ( BitSize() != other.BitSize() )
		return;

	for ( i = 0 ; i < _numBytes ; i++ )
		{
		_bytes[ i ] |= other._bytes[ i ];
		}
	}


inline void BitVector::operator&= ( const BitVector &other )
	{
	unsigned int i;

	if ( BitSize() != other.BitSize() )
		return;

	for ( i = 0 ; i < _numBytes ; i++ )
		{
		_bytes[ i ] &= other._bytes[ i ];
		}
	}


inline BitVector BitVector::operator~ ( void )
	{
	BitVector temp_bits( _numBits );
	unsigned int i;

	for ( i = 0 ; i < _numBytes ; i++ )
		{
		temp_bits._bytes[ i ] = ~_bytes[ i ];
		}

	return temp_bits;
	}


inline BitVector operator| ( const BitVector &left, const BitVector &right )
	{
	BitVector temp_bits( left );

	temp_bits |= right;
	return temp_bits;
	}


inline BitVector operator& ( const BitVector &left, const BitVector &right )
	{
	BitVector temp_bits( left );

	temp_bits &= right;
	return temp_bits;
	}


inline BitReference& BitReference::operator= ( bool b )
	{
	if ( _vector )
		_vector->SetBit( _index, b );

	return *this;
	}


inline BitReference& BitReference::operator= ( const BitReference &other )
	{
	if ( _vector )
		_vector->SetBit( _index, other._vector->GetBit( other._index ) );

	return *this;
	}


inline BitReference::operator bool() const
	{
	if ( _vector )
		return _vector->GetBit( _index );
	else
		return false;
	}


inline void BitReference::Setup( BitVector *vector, unsigned int index )
	{
	_vector = vector;
	_index = index;
	}

#endif 
