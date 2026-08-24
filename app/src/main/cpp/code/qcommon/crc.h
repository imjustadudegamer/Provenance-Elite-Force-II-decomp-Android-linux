
#include "q_shared.h"

typedef unsigned short crc_t;

void CRC_Init( unsigned short *crcvalue );
void CRC_ProcessByte( unsigned short *crcvalue, byte data );
unsigned short CRC_Value( unsigned short crcvalue );
unsigned short CRC_Block( const unsigned char *data, int length );
