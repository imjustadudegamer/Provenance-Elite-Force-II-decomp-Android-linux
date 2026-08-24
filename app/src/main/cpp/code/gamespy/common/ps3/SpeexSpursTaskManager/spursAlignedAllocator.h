// NOTE:  this code has been provided by Sony for usage in Speex SPURS Manager


#ifndef BT_ALIGNED_ALLOCATOR
#define BT_ALIGNED_ALLOCATOR


void*	spursAlignedAlloc	(int size, int alignment);

void	spursAlignedFree	(void* ptr);


typedef int	size_type;


template < typename T , unsigned Alignment >
class spursAlignedAllocator {
	
	typedef spursAlignedAllocator< T , Alignment > self_type;
	
public:

	spursAlignedAllocator() {}

	template < typename Other >
	spursAlignedAllocator( const spursAlignedAllocator< Other , Alignment > & ) {}

	typedef const T*         const_pointer;
	typedef const T&         const_reference;
	typedef T*               pointer;
	typedef T&               reference;
	typedef T                value_type;

	pointer       address   ( reference        ref ) const                           { return &ref; }
	const_pointer address   ( const_reference  ref ) const                           { return &ref; }
	pointer       allocate  ( size_type        n   , const_pointer *      hint = 0 ) {
		(void)hint;
		return reinterpret_cast< pointer >(spursAlignedAlloc( sizeof(value_type) * n , Alignment ));
	}
	void          construct ( pointer          ptr , const value_type &   value    ) { new (ptr) value_type( value ); }
	void          deallocate( pointer          ptr ) {
		spursAlignedFree( reinterpret_cast< void * >( ptr ) );
	}
	void          destroy   ( pointer          ptr )                                 { ptr->~value_type(); }
	

	template < typename O > struct rebind {
		typedef spursAlignedAllocator< O , Alignment > other;
	};
	template < typename O >
	self_type & operator=( const spursAlignedAllocator< O , Alignment > & ) { return *this; }

	friend bool operator==( const self_type & , const self_type & ) { return true; }
};



#endif 

