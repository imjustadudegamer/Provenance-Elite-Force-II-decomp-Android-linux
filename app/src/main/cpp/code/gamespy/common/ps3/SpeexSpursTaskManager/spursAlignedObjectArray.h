/* Third-party component; the applicable licence is recorded in THIRDPARTY.md. */



#ifndef BT_OBJECT_ARRAY__
#define BT_OBJECT_ARRAY__

#include "spursScalar.h" 
#include "spursAlignedAllocator.h"


#define BT_USE_PLACEMENT_NEW 1

#ifdef BT_USE_MEMCPY
#include <memory.h>
#include <string.h>
#endif 

#ifdef BT_USE_PLACEMENT_NEW
#include <new> 
#endif 


template <typename T> 
class spursAlignedObjectArray
{
	spursAlignedAllocator<T , 16>	m_allocator;

	int					m_size;
	int					m_capacity;
	T*					m_data;

	protected:
		SIMD_FORCE_INLINE	int	allocSize(int size)
		{
			return (size ? size*2 : 1);
		}
		SIMD_FORCE_INLINE	void	copy(int start,int end, T* dest)
		{
			int i;
			for (i=start;i<end;++i)
#ifdef BT_USE_PLACEMENT_NEW
				new (&dest[i]) T(m_data[i]);
#else
				dest[i] = m_data[i];
#endif 
		}

		SIMD_FORCE_INLINE	void	init()
		{
			m_data = 0;
			m_size = 0;
			m_capacity = 0;
		}
		SIMD_FORCE_INLINE	void	destroy(int first,int last)
		{
			int i;
			for (i=first; i<last;i++)
			{
				m_data[i].~T();
			}
		}

		SIMD_FORCE_INLINE	void* allocate(int size)
		{
			if (size)
				return m_allocator.allocate(size);
			return 0;
		}

		SIMD_FORCE_INLINE	void	deallocate()
		{
			if(m_data)	{
				m_allocator.deallocate(m_data);
				m_data = 0;
			}
		}

	


	public:
		
		spursAlignedObjectArray()
		{
			init();
		}

		~spursAlignedObjectArray()
		{
			clear();
		}

		SIMD_FORCE_INLINE	int capacity() const
		{	
			return m_capacity;
		}
		
		SIMD_FORCE_INLINE	int size() const
		{	
			return m_size;
		}
		
		SIMD_FORCE_INLINE const T& operator[](int n) const
		{
			return m_data[n];
		}

		SIMD_FORCE_INLINE T& operator[](int n)
		{
			return m_data[n];
		}
		

		SIMD_FORCE_INLINE	void	clear()
		{
			destroy(0,size());
			
			deallocate();
			
			init();
		}

		SIMD_FORCE_INLINE	void	pop_back()
		{
			m_size--;
			m_data[m_size].~T();
		}

		SIMD_FORCE_INLINE	void	resize(int newsize, const T& fillData=T())
		{
			int curSize = size();

			if (newsize < size())
			{
				for(int i = curSize; i < newsize; i++)
				{
					m_data[i].~T();
				}
			} else
			{
				if (newsize > size())
				{
					reserve(newsize);
				}
#ifdef BT_USE_PLACEMENT_NEW
				for (int i=curSize;i<newsize;i++)
				{
					new ( &m_data[i]) T(fillData);
				}
#endif 

			}

			m_size = newsize;
		}
	

		SIMD_FORCE_INLINE	T&  expand( const T& fillValue=T())
		{	
			int sz = size();
			if( sz == capacity() )
			{
				reserve( allocSize(size()) );
			}
			m_size++;
#ifdef BT_USE_PLACEMENT_NEW
			new (&m_data[sz]) T(fillValue); 
#endif

			return m_data[sz];		
		}


		SIMD_FORCE_INLINE	void push_back(const T& _Val)
		{	
			int sz = size();
			if( sz == capacity() )
			{
				reserve( allocSize(size()) );
			}
			
#ifdef BT_USE_PLACEMENT_NEW
			new ( &m_data[m_size] ) T(_Val);
#else
			m_data[size()] = _Val;			
#endif 

			m_size++;
		}

	
		
		SIMD_FORCE_INLINE	void reserve(int _Count)
		{	
			if (capacity() < _Count)
			{	
				T*	s = (T*)allocate(_Count);

				copy(0, size(), s);

				destroy(0,size());

				deallocate();

				m_data = s;
				
				m_capacity = _Count;

			}
		}


		class less
		{
			public:

				bool operator() ( const T& a, const T& b )
				{
					return ( a < b );
				}
		};
	

		template <typename L>
		void downHeap(T *pArr, int k, int n,L CompareFunc)
		{
			
			T temp = pArr[k - 1];
			while (k <= n/2) 
			{
				int child = 2*k;
				
				if ((child < n) && CompareFunc(pArr[child - 1] , pArr[child]))
				{
					child++;
				}
				if (CompareFunc(temp , pArr[child - 1]))
				{
					pArr[k - 1] = pArr[child - 1];
					k = child;
				}
				else
				{
					break;
				}
			}
			pArr[k - 1] = temp;
		} 

		void	swap(int index0,int index1)
		{
#ifdef BT_USE_MEMCPY
			char	temp[sizeof(T)];
			memcpy(temp,&m_data[index0],sizeof(T));
			memcpy(&m_data[index0],&m_data[index1],sizeof(T));
			memcpy(&m_data[index1],temp,sizeof(T));
#else
			T temp = m_data[index0];
			m_data[index0] = m_data[index1];
			m_data[index1] = temp;
#endif 

		}

	template <typename L>
	void heapSort(L CompareFunc)
	{
		int k;
		int n = m_size;
		for (k = n/2; k > 0; k--) 
		{
			downHeap(m_data, k, n, CompareFunc);
		}

		while ( n>=1 ) 
		{
			swap(0,n-1); 


			n = n - 1;
			downHeap(m_data, 1, n, CompareFunc);
		} 
	}

	int	findBinarySearch(const T& key) const
	{
		int first = 0;
		int last = size();

		while (first <= last) {
			int mid = (first + last) / 2;  
			if (key > m_data[mid]) 
				first = mid + 1;  
			else if (key < m_data[mid]) 
				last = mid - 1; 
			else
				return mid;     
		}
		return size();    
	}


	int	findLinearSearch(const T& key) const
	{
		int index=size();
		int i;

		for (i=0;i<size();i++)
		{
			if (m_data[i] == key)
			{
				index = i;
				break;
			}
		}
		return index;
	}

	void	remove(const T& key)
	{

		int findIndex = findLinearSearch(key);
		if (findIndex<size())
		{
			swap( findIndex,size()-1);
			pop_back();
		}
	}

};

#endif 


