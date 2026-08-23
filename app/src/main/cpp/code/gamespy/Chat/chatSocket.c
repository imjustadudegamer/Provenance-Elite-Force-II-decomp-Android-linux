
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <limits.h>
#include <stdlib.h>
#include "chatMain.h"
#include "chatSocket.h"


#if defined(_WIN32)
#pragma warning(disable:4127)
#endif

#define BUFFER_INC      8192
#define RECV_LEN        4096

#define ASSERT_SOCK(sock)   {\
								assert((sock) != NULL);\
								assert(((sock)->connectState == ciNotConnected) ||\
									((sock)->connectState == ciConnected) ||\
									((sock)->connectState == ciDisconnected));\
								ASSERT_BUFFER(&(sock)->inputQueue);\
								ASSERT_BUFFER(&(sock)->outputQueue);\
							}

#define ASSERT_CONNECTED(sock)  assert((sock)->connectState == ciConnected)

#define ASSERT_BUFFER(buffer)   {\
									assert((buffer) != NULL);\
									assert((buffer)->size >= 0);\
									assert(((buffer)->size % BUFFER_INC) == 0);\
									assert((buffer)->length >= 0);\
									assert((buffer)->length <= (buffer)->size);\
								}
#define RESET(ptr)  {if (ptr) {gsifree(ptr); ptr = NULL;} }

#ifdef IRC_LOG
static const char * ciGetTime(void)
{
#if defined(UNDER_CE) || defined(_PS2) || defined(_NITRO)
	return "";
#else
	static char buffer[256];
	time_t timer;
	struct tm * now;

	timer = time(NULL);
	now = localtime(&timer);
	if(now) 
	{
		if(now->tm_year > 99)
			now->tm_year -= 100;
		sprintf(buffer, "%02d.%02d.%02d %02d:%02d.%02d", now->tm_mon + 1, now->tm_mday, now->tm_year, now->tm_hour, now->tm_min, now->tm_sec);
	}
	else
		strcpy(buffer, "00.00.00 00:00.00");

	return buffer;
#endif
}
#endif

static CHATBool ciBufferInit(ciBuffer * buffer)
{
	assert(buffer != NULL);

	buffer->length = 0;
	buffer->size = BUFFER_INC;
	buffer->buffer = (char *)gsimalloc((unsigned int)buffer->size + 1);
	if(buffer->buffer == NULL)
		return CHATFalse;

	buffer->buffer[0] = '\0';

	return CHATTrue;
}

static void ciBufferFree(ciBuffer * buffer)
{
	gsifree(buffer->buffer);
}

static CHATBool ciBufferPreAppend(ciBuffer * buffer, int len)
{
	int total;
	char * tempPtr;

	ASSERT_BUFFER(buffer);
	assert(len >= 0);
	assert(len <= SHRT_MAX); 

	total = (buffer->length + len);
	if(total <= buffer->size)
		return CHATTrue;

	total += BUFFER_INC;
	total -= (total % BUFFER_INC);

	tempPtr = (char *)gsirealloc(buffer->buffer, (unsigned int)total + 1);
	if(tempPtr == NULL)
		return CHATFalse;

	buffer->buffer = tempPtr;
	buffer->size = total;

	return CHATTrue;
}

static void ciBufferClipFront(ciBuffer * buffer, int len)
{
	ASSERT_BUFFER(buffer);
	assert(len >= 0);
	assert(len <= buffer->length);

	buffer->length -= len;
	memmove(buffer->buffer, &buffer->buffer[len], (unsigned int)buffer->length);
	buffer->buffer[buffer->length] = '\0';
}


#ifdef __MWERKS__ 
CHATBool ciParseParam(const char *pText, ciServerMessage * message);
#endif

CHATBool ciSocketInit(ciSocket * sock, const char * nick)
{
#ifdef IRC_LOG
	FILE * log;
#endif

	assert(sock != NULL);

	memset(sock, 0, sizeof(ciSocket));

	sock->sock = INVALID_SOCKET;
	if(!ciBufferInit(&sock->inputQueue))
		return CHATFalse;
	if(!ciBufferInit(&sock->outputQueue))
	{
		ciBufferFree(&sock->inputQueue);
		return CHATFalse;
	}
#ifdef IRC_LOG
	sprintf(sock->filename, "%s_irc.log", nick);
	log = fopen(sock->filename, "at");
	if(log != NULL)
	{
		fprintf(log, "\n\n\n\n\nCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC\n");
		fclose(log);
	}
#endif

	GSI_UNUSED(nick);
	return CHATTrue;	
}

CHATBool ciSocketConnect(ciSocket * sock,
					 const char * serverAddress,
					 int port)
{
	unsigned int ip;
	HOSTENT * host;
	SOCKADDR_IN address;
	int rcode;

#if !defined(INSOCK) && !defined(_NITRO) && !defined(_REVOLUTION)
	int keepalive;
#endif

	ASSERT_SOCK(sock);
	assert(serverAddress != NULL);
	assert(port >= 0);
	assert(port <= USHRT_MAX);
	assert(sock->connectState == ciNotConnected);

	strzcpy(sock->serverAddress, serverAddress, 255);

	ip = inet_addr(serverAddress);
	if(ip == INADDR_NONE)
	{
		host = gethostbyname((char *)serverAddress);
		if(host == NULL)
			return CHATFalse;

		ip = *(unsigned int *)host->h_addr_list[0];
	}

	memset(&address, 0, sizeof(SOCKADDR_IN));
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = ip;
	address.sin_port = htons((unsigned short)port);

	sock->sock = socket(AF_INET, SOCK_STREAM, 0);
	if(sock->sock == INVALID_SOCKET)
		return CHATFalse;

#if !defined(INSOCK) && !defined(_NITRO) && !defined(_REVOLUTION)
	keepalive = 1;
	rcode = setsockopt(sock->sock, SOL_SOCKET, SO_KEEPALIVE, (char *)&keepalive, sizeof(int));
#endif

	rcode = connect(sock->sock, (SOCKADDR *)&address, sizeof(SOCKADDR_IN));
	if(gsiSocketIsError(rcode))
	{
		closesocket(sock->sock);
		return CHATFalse;
	}

	sock->connectState = ciConnected;

	return CHATTrue;
}

void ciSocketDisconnect(ciSocket * sock)
{
	int i;

	ASSERT_SOCK(sock);

	if(sock->sock != INVALID_SOCKET)
	{
		shutdown(sock->sock, 2);
		closesocket(sock->sock);
	}

	sock->connectState = ciDisconnected;

	ciBufferFree(&sock->inputQueue);
	ciBufferFree(&sock->outputQueue);

	gsifree(sock->lastMessage.message);
	gsifree(sock->lastMessage.server);
	gsifree(sock->lastMessage.nick);
	gsifree(sock->lastMessage.user);
	gsifree(sock->lastMessage.host);
	gsifree(sock->lastMessage.command);
	gsifree(sock->lastMessage.middle);
	gsifree(sock->lastMessage.param);
	for(i = 0 ; i < sock->lastMessage.numParams ; i++)
		gsifree(sock->lastMessage.params[i]);
	gsifree(sock->lastMessage.params);
}

static void ciSocketSelect(SOCKET sock, CHATBool * readFlag, CHATBool * writeFlag, CHATBool * exceptFlag)
{
	int aReadFlag   = 0;
	int aWriteFlag  = 0;
	int aExceptFlag = 0;

	GSISocketSelect(sock, &aReadFlag, &aWriteFlag, &aExceptFlag);
	
	if (readFlag)
		*readFlag   = (CHATBool)aReadFlag;
	if (writeFlag)
		*writeFlag  = (CHATBool)aWriteFlag;
	if (exceptFlag)
		*exceptFlag = (CHATBool)aExceptFlag;
}

static void ciSocketThinkSend(ciSocket * sock)
{
	int rcode;
	int len;

	ASSERT_SOCK(sock);
	ASSERT_CONNECTED(sock);

	while(sock->outputQueue.length > 0)
	{
		CHATBool writeFlag;
		
		ciSocketSelect(sock->sock, NULL, &writeFlag, NULL);
		if(!writeFlag)
			return;

		len = min(sock->outputQueue.length, 1024);
		rcode = send(sock->sock, sock->outputQueue.buffer, len, 0);

		if(rcode == 0)
			return;
		if(gsiSocketIsError(rcode))
			return; 

		ciBufferClipFront(&sock->outputQueue, rcode);
	}
}

static void ciSocketThinkRecv(ciSocket * sock)
{
	CHATBool readFlag;
	int len;
	int rcode;
	char * pos;
	
	ASSERT_SOCK(sock);
	ASSERT_CONNECTED(sock);

	while(CHATTrue)
	{
		ciSocketSelect(sock->sock, &readFlag, NULL, NULL);

		if(!readFlag)
			return;

		if(!ciBufferPreAppend(&sock->inputQueue, RECV_LEN))
			return; 

		pos = &sock->inputQueue.buffer[sock->inputQueue.length];
		rcode = recv(sock->sock, pos, RECV_LEN, 0);

		if(rcode <= 0) 
		{
			sock->connectState = ciDisconnected;
			return;
		}

		len = rcode;

#if 0
{
		char * buffer;
		int i;
		buffer = (char *)gsimalloc(len + 2);
		assert(buffer != NULL);
		memcpy(buffer, &sock->inputQueue.buffer[sock->inputQueue.length], len);
		buffer[len] = '\0';
		OutputDebugString("--->>>XXXRECV\n");
		for(i = 0 ; i < len ; i += 1023)
			OutputDebugString(&buffer[i]);
		OutputDebugString("<<<---XXXRECV\n");
}
#endif

		if(sock->secure)
			gs_crypt((unsigned char *)pos, len, &sock->inKey);

		sock->inputQueue.length += len;

		sock->inputQueue.buffer[sock->inputQueue.length] = '\0';
	}
}

void ciSocketThink(ciSocket * sock)
{
	ASSERT_SOCK(sock);
	ASSERT_CONNECTED(sock);

	if(sock->connectState == ciDisconnected)
		return;

	ciSocketThinkSend(sock);

	ciSocketThinkRecv(sock);
}

CHATBool ciSocketSend(ciSocket * sock,
				  const char * buffer)
{
#ifdef IRC_LOG
	FILE * log;
#endif
	int len;
	char * pos;

	ASSERT_SOCK(sock);
	ASSERT_CONNECTED(sock);
	assert(buffer != NULL);

	if(sock->connectState == ciDisconnected)
		return CHATTrue;

	len = (int)strlen(buffer);

	if(!ciBufferPreAppend(&sock->outputQueue, len + 2))
		return CHATFalse;

	pos = &sock->outputQueue.buffer[sock->outputQueue.length];
	memcpy(pos, buffer, (unsigned int)len);

	sock->outputQueue.length += len;

	sock->outputQueue.buffer[sock->outputQueue.length++] = 0x0D;
	sock->outputQueue.buffer[sock->outputQueue.length++] = 0x0A;

	if(sock->secure)
		gs_crypt((unsigned char *)pos, len + 2, &sock->outKey);

#ifdef IRC_LOG
	log = fopen(sock->filename, "at");
	if(log != NULL)
	{
		fprintf(log, "%s | OUT | %s\n", ciGetTime(), buffer);
		fclose(log);
	}
#endif

	return CHATTrue;
}

CHATBool ciSocketSendf(ciSocket * sock,
				   const char * format,
				   ...)
{
	static char buffer[4096];
	int num;
	va_list args;

	ASSERT_SOCK(sock);
	ASSERT_CONNECTED(sock);
	assert(buffer != NULL);

	if(sock->connectState == ciDisconnected)
		return CHATTrue;

	va_start(args, format);
	num = vsprintf(buffer, format, args);
	if(num != -1)
		buffer[num] = '\0';
	else
		buffer[sizeof(buffer) - 1] = '\0'; 

	return ciSocketSend(sock, buffer);
}

static CHATBool ciParseUser(const char *pText, ciServerMessage * message)
{
	char *pTmpNick = NULL, *pTmpUsername = NULL, *pTmpHost = NULL;
	int    nNick = 0, nUsername = 0, nHost = 0;
	char *p;
	
	if(pText == NULL || pText[0] == '\0')
	{
		assert(0);
		return CHATFalse; 
	}
	
	p = pTmpNick = (char *)pText;
	
	while(*p != '\0')
	{
		if(*p != '!')
		{
			++p;
			++nNick;
		}
		else
		{
			pTmpUsername = ++p;
			
			while(*p != '\0')
			{
				if(*p != '@')
				{
					++p;
					++nUsername;
				}
				else
				{
					pTmpHost = ++p;
					
					while(*p != '\0')
					{
						++p;
						++nHost;
					}
				}
			}
		}
	}
	
	if(nNick)
	{
		message->nick = (char *)gsimalloc((unsigned int)nNick + 1);
		
		if(message->nick)
		{
			memcpy(message->nick, pTmpNick, (unsigned int)nNick);
			message->nick[nNick] = '\0';
		}
	}
	else
		message->nick = NULL;
	
	if(nUsername)
	{
		message->user = (char *)gsimalloc((unsigned int)nUsername + 1);
		
		if(message->user)
		{
			memcpy(message->user, pTmpUsername, (unsigned int)nUsername);
			message->user[nUsername] = '\0';
		}
	}
	else
		message->user = NULL;
	
	if(nHost)
	{
		message->host = (char *)gsimalloc((unsigned int)nHost + 1);
		
		if(message->host)
		{
			memcpy(message->host, pTmpHost, (unsigned int)nHost);
			message->host[nHost] = '\0';
		}
	}
	else
		message->host = NULL;

	return CHATTrue;
}

static CHATBool ciAddParam(const char *param, ciServerMessage * message)
{
	void * tempPtr;
	
	tempPtr = gsirealloc(message->params, sizeof(char *) * (message->numParams + 1));
	if(tempPtr == NULL)
		return CHATFalse; 
	message->params = (char **)tempPtr;

	tempPtr = gsimalloc(strlen(param) + 1);
	if(tempPtr == NULL)
		return CHATFalse; 

	strcpy((char *)tempPtr, param);
	message->params[message->numParams++] = (char *)tempPtr;

	return CHATTrue;
}

CHATBool ciParseParam(const char *pText, ciServerMessage * message)
{
	char * colon;
	char * str;
	char * p;

	assert(pText != NULL);
	assert(message != NULL);

	p = (char *)gsimalloc(strlen(pText) + 1);
	if(p == NULL)
		return CHATFalse; 
	strcpy(p, pText);

	if(p[0] == ':')
	{
		p[0] = '\0';
		colon = &p[1];
	}
	else
	{
		colon = strstr(p, " :");
		if(colon != NULL)
		{
			*colon = '\0';
			colon += 2;
		}
	}

	str = strtok(p, " ");
	while(str != NULL)
	{
		if(!ciAddParam(str, message))
		{
			gsifree(p);
			return CHATFalse; 
		}

		str = strtok(NULL, " ");
	}

	if(colon != NULL)
	{
		if(!ciAddParam(colon, message))
		{
			gsifree(p);
			return CHATFalse; 
		}
	}

	gsifree(p);
	return CHATTrue;
}

static CHATBool ciParseMessage(ciSocket * sock, const char *sText)
{	
	int    nMessage = 0, nServer = 0, nCommand = 0, nMiddle = 0, nParam = 0;
	char *p, *temp;
	ciServerMessage * message = &sock->lastMessage;
	
	if(sText == NULL || sText[0] == '\0')
	{
		assert(0);
		return CHATFalse; 
	}

	nMessage = (int)strlen(sText);
	message->message = (char *)gsimalloc((unsigned int)nMessage + 1);
	if(message->message == NULL)
		return CHATFalse; 
	memcpy(message->message, sText, (unsigned int)nMessage);
	message->message[nMessage] = '\0';

	p = (char *)sText;
	
	while(*p == '\n' || *p == '\r')
		++p;
	
	if(*p == ':')
	{  
		message->server = ++p;  
		
		if(*p != '\0')
		{
			while(*p != ' ' && *p != '\0')
			{
				++nServer;
				++p;
			}
		}
	}
	
	while(*p == ' ')
		++p;
	
	if(*p != '\0')
	{  
		message->command = p;
		
		while(*p != ' ' && *p != '\0')
		{
			++nCommand;
			++p;
		}
	}
	
	while(*p == ' ')
		++p;
	
	if(*p != ':' && *p != '\0')
	{  
		message->middle = p;
		
		while(*p != ' ' && *p != '\0')
		{
			++nMiddle;
			++p;
		}
	}
	
	while(*p == ' ')
		++p;
	
	
	if(*p != '\0')
	{  
		message->param = p;
		
		while(*p != '\0')
		{
			++nParam;
			++p;
		}
	}
	
	if(nServer)
	{
		temp = message->server;
		message->server = (char *)gsimalloc((unsigned int)nServer + 1);
		
		if(message->server)
		{
			memcpy(message->server, temp, (unsigned int)nServer);
			message->server[nServer] = '\0';
		}

		if(!ciParseUser(message->server, message))
		{
			RESET(message->message);
			RESET(message->server);
			return CHATFalse; 
		}
	}
	else
	{
		message->server = NULL;
		message->nick = NULL;
		message->user = NULL;
		message->host = NULL;
	}

	if(nMiddle)
	{
		if(!ciParseParam(message->middle, message))
		{
			RESET(message->message);
			RESET(message->server);
			RESET(message->nick);
			RESET(message->user);
			RESET(message->host);
			return CHATFalse; 
		}
	}
	else if(nParam)
	{
		if(!ciParseParam(message->param, message))
		{
			RESET(message->message);
			RESET(message->server);
			RESET(message->nick);
			RESET(message->user);
			RESET(message->host);
			return CHATFalse; 
		}
	}
	else
	{
		message->params = NULL;
		message->numParams = 0;
	}
	
	if(nParam)
	{
		temp = message->param;
		message->param = (char *)gsimalloc((unsigned int)nParam + 1);

		if(message->param)
		{
			memcpy(message->param, temp, (unsigned int)nParam);
			message->param[nParam] = '\0';
		}
	}
	else
	{
		message->param = NULL;
	}
	
	if(nCommand)
	{
		temp = message->command;
		message->command = (char *)gsimalloc((unsigned int)nCommand + 1);
		
		if(message->command)
		{
			memcpy(message->command, temp, (unsigned int)nCommand);
			message->command[nCommand] = '\0';
		}
	}
	else
		message->command = NULL;
	
	if(nMiddle)
	{
		temp = message->middle;
		message->middle = (char *)gsimalloc((unsigned int)nMiddle + 1);
		
		if(message->middle)
		{
			memcpy(message->middle, temp, (unsigned int)nMiddle);
			message->middle[nMiddle] = '\0';
		}
	}
	else
		message->middle = NULL;

	return CHATTrue;
}


static CHATBool ciParseInput(ciSocket * sock)
{
	char *p, *q, *r;
	char   temp;
	int i;
	
	p = sock->inputQueue.buffer;  
	
	if(*p != '\0')
	{
		while(*p == 13 || *p == 10)
			++p;
		
		if(*p != '\0')
		{
			r = q = p;
			
			while(*q != 10 && *q != 13 && *q != '\0')
			{
				if(*q != ' ')
					r = q;
				++q;
			}
			
			if(*q != '\0')
			{
				++r;
				temp = *r;
				*r = '\0';

				RESET(sock->lastMessage.message);
				RESET(sock->lastMessage.server);
				RESET(sock->lastMessage.nick);
				RESET(sock->lastMessage.user);
				RESET(sock->lastMessage.host);
				RESET(sock->lastMessage.command);
				RESET(sock->lastMessage.middle);
				RESET(sock->lastMessage.param);
				for(i = 0 ; i < sock->lastMessage.numParams ; i++)
					RESET(sock->lastMessage.params[i]);
				RESET(sock->lastMessage.params);
				sock->lastMessage.numParams = 0;

				memset(&sock->lastMessage,0,sizeof(sock->lastMessage));
				if(!ciParseMessage(sock, p))
				{
					memset(&sock->lastMessage,0,sizeof(sock->lastMessage));
					return CHATFalse; 
				}

				*r = temp;

				ciBufferClipFront(&sock->inputQueue, (q - sock->inputQueue.buffer));

				return CHATTrue;
			}
		}
	}

	return CHATFalse;
}

ciServerMessage * ciSocketRecv(ciSocket * sock)
{
#ifdef IRC_LOG
	FILE * log;
#endif
	ASSERT_SOCK(sock);


	if(sock->inputQueue.length == 0)
		return NULL;

	if(!ciParseInput(sock))
	{
		return NULL;
	}

#ifdef IRC_LOG
	log = fopen(sock->filename, "at");
	if(log != NULL)
	{
		fprintf(log, "%s | IN  | %s\n", ciGetTime(), sock->lastMessage.message);
		fclose(log);
	}
#endif

	return &sock->lastMessage;
}
