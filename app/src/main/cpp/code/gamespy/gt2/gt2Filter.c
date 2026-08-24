/*
GameSpy GT2 SDK
Dan "Mr. Pants" Schoenblum
dan@gamespy.com

Copyright 2002 GameSpy Industries, Inc

devsupport@gamespy.com
*/

#include "gt2Filter.h"
#include "gt2Callback.h"
#include "gt2Message.h"
#include "gt2Utility.h"

static int GS_STATIC_CALLBACK gti2SendFiltersCompare
(
	const void * elem1,
	const void * elem2
)
{
	gt2SendFilterCallback * callback1 = (gt2SendFilterCallback *)elem1;
	gt2SendFilterCallback * callback2 = (gt2SendFilterCallback *)elem2;

	if(*callback1 == *callback2)
		return 0;

	return 1;
}

static int GS_STATIC_CALLBACK gti2ReceiveFiltersCompare
(
	const void * elem1,
	const void * elem2
)
{
	gt2ReceiveFilterCallback * callback1 = (gt2ReceiveFilterCallback *)elem1;
	gt2ReceiveFilterCallback * callback2 = (gt2ReceiveFilterCallback *)elem2;

	if(*callback1 == *callback2)
		return 0;

	return 1;
}

GT2Bool gti2AddSendFilter(GT2Connection connection, gt2SendFilterCallback callback)
{
	if(!connection->sendFilters)
		return GT2False;

	ArrayAppend(connection->sendFilters, &callback);

	return (ArraySearch(connection->sendFilters, &callback, gti2SendFiltersCompare, 0, 0) != NOT_FOUND);
}

GT2Bool gti2AddReceiveFilter(GT2Connection connection, gt2ReceiveFilterCallback callback)
{
	if(!connection->receiveFilters)
		return GT2False;

	ArrayAppend(connection->receiveFilters, &callback);

	return (ArraySearch(connection->receiveFilters, &callback, gti2ReceiveFiltersCompare, 0, 0) != NOT_FOUND);
}

void gti2RemoveSendFilter(GT2Connection connection, gt2SendFilterCallback callback)
{
	int index;

	if(!connection->sendFilters)
		return;

	if(!callback)
	{
		ArrayClear(connection->sendFilters);
		return;
	}

	index = ArraySearch(connection->sendFilters, &callback, gti2SendFiltersCompare, 0, 0);
	if(index == NOT_FOUND)
		return;

	ArrayRemoveAt(connection->sendFilters, index);
}

void gti2RemoveReceiveFilter(GT2Connection connection, gt2ReceiveFilterCallback callback)
{
	int index;

	if(!connection->receiveFilters)
		return;

	if(!callback)
	{
		ArrayClear(connection->receiveFilters);
		return;
	}

	index = ArraySearch(connection->receiveFilters, &callback, gti2ReceiveFiltersCompare, 0, 0);
	if(index == NOT_FOUND)
		return;

	ArrayRemoveAt(connection->receiveFilters, index);
}

GT2Bool gti2FilteredSend(GT2Connection connection, int filterID, const GT2Byte * message, int len, GT2Bool reliable)
{
	int num;

	if(connection->state != GTI2Connected)
		return GT2True;

	gti2MessageCheck(&message, &len);

	num = ArrayLength(connection->sendFilters);

	if(filterID < 0)
		return GT2True;
	if(filterID >= num)
		return GT2True;

	if(filterID == (num - 1))
	{
		if(!gti2Send(connection, message, len, reliable))
			return GT2False;
	}
	else
	{
		if(!gti2SendFilterCallback(connection, ++filterID, message, len, reliable))
			return GT2False;
	}

	return GT2True;
}

GT2Bool gti2FilteredReceive(GT2Connection connection, int filterID, GT2Byte * message, int len, GT2Bool reliable)
{
	int num;

	if(connection->state != GTI2Connected)
		return GT2True;

	num = ArrayLength(connection->receiveFilters);

	if(filterID < 0)
		return GT2True;
	if(filterID >= num)
		return GT2True;

	if(filterID == (num - 1))
	{
		if(!gti2ReceivedCallback(connection, message, len, reliable))
			return GT2False;
	}
	else
	{
		if(!gti2ReceiveFilterCallback(connection, ++filterID, message, len, reliable))
			return GT2False;
	}

	return GT2True;
}
