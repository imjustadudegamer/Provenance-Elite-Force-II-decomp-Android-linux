/* Third-party component; the applicable licence is recorded in THIRDPARTY.md. */
#ifndef DOUBLE_BUFFER_H
#define DOUBLE_BUFFER_H

#include "SpuFakeDma.h"


template<class T, int size>
class DoubleBuffer
{
#ifdef __CELLOS_LV2__
	T m_buffer0[size] __attribute__ ((aligned (128)));
	T m_buffer1[size] __attribute__ ((aligned (128)));
#else
	T m_buffer0[size];
	T m_buffer1[size];
#endif
	
	T *m_frontBuffer;
	T *m_backBuffer;

	unsigned int m_dmaTag;
	bool m_dmaPending;
public:
	bool	isPending() const { return m_dmaPending;}
	DoubleBuffer();

	void init ();

	void backBufferDmaGet(uint64_t ea, unsigned int numBytes, unsigned int tag);
	void backBufferDmaPut(uint64_t ea, unsigned int numBytes, unsigned int tag);

	T *getFront();
	T *getBack();

	T *swapBuffers();
};

template<class T, int size>
DoubleBuffer<T,size>::DoubleBuffer()
{
	init ();
}

template<class T, int size>
void DoubleBuffer<T,size>::init()
{
	this->m_dmaPending = false;
	this->m_frontBuffer = &this->m_buffer0[0];
	this->m_backBuffer = &this->m_buffer1[0];
}

template<class T, int size>
void
DoubleBuffer<T,size>::backBufferDmaGet(uint64_t ea, unsigned int numBytes, unsigned int tag)
{
	m_dmaPending = true;
	m_dmaTag = tag;
	cellDmaLargeGet(m_backBuffer, ea, numBytes, tag, 0, 0);
}

template<class T, int size>
void
DoubleBuffer<T,size>::backBufferDmaPut(uint64_t ea, unsigned int numBytes, unsigned int tag)
{
	m_dmaPending = true;
	m_dmaTag = tag;
	cellDmaLargePut(m_backBuffer, ea, numBytes, tag, 0, 0);
}

template<class T, int size>
T *
DoubleBuffer<T,size>::getFront()
{
	return m_frontBuffer;
}

template<class T, int size>
T *
DoubleBuffer<T,size>::getBack()
{
	return m_backBuffer;
}

template<class T, int size>
T *
DoubleBuffer<T,size>::swapBuffers()
{
	if (m_dmaPending)
	{
		cellDmaWaitTagStatusAll(1<<m_dmaTag);
		m_dmaPending = false;
	}

	T *tmp = m_backBuffer;
	m_backBuffer = m_frontBuffer;
	m_frontBuffer = tmp;

	return m_frontBuffer;
}

#endif
