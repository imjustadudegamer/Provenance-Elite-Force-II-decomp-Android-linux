

#ifndef _GOACDKEYS_H_
#define _GOACDKEYS_H_

#ifdef __cplusplus
extern "C" {
#endif

#define GUSE_ASSERTS

#define QR2CDKEY_INTEGRATION

typedef void (*AuthCallBackFn)(int gameid, int localid, int authenticated, char *errmsg, void *instance);
typedef void (*RefreshAuthCallBackFn)(int gameid, int localid, int hint, char *challenge, void *instance);

extern char gcd_hostname[64];

int gcd_init(int gameid);

#ifdef QR2CDKEY_INTEGRATION

#include "../qr2/qr2.h"
int gcd_init_qr2(qr2_t qrec, int gameid);

#endif

void gcd_shutdown(void);

void gcd_authenticate_user(int gameid, int localid, unsigned int userip, const char *challenge, 
						   const char *response, AuthCallBackFn authfn, RefreshAuthCallBackFn refreshfn, void *instance);

void gcd_process_reauth(int gameid, int localid, int hint, const char *response);


void gcd_disconnect_user(int gameid, int localid);


void gcd_disconnect_all(int gameid);

void gcd_think(void);

char *gcd_getkeyhash(int gameid, int localid);

#ifdef __cplusplus
}
#endif

#endif

