#ifndef _KEY_H_
#define _KEY_H_

typedef struct KeyInformation
{
    u16     cnt;    
    u16     trg;    
    u16     up;     
    u16     rep;    
} KeyInformation;

void KeyInit(void);

const KeyInformation * KeyRead(void);

#define KEY_A_PRESSED(info) (((info)->trg | (info)->rep) & PAD_BUTTON_A)
#define KEY_B_PRESSED(info) (((info)->trg | (info)->rep) & PAD_BUTTON_B)
#define KEY_X_PRESSED(info) (((info)->trg | (info)->rep) & PAD_BUTTON_X)
#define KEY_Y_PRESSED(info) (((info)->trg | (info)->rep) & PAD_BUTTON_Y)
#define KEY_UP_PRESSED(info) (((info)->trg | (info)->rep) & PAD_KEY_UP)
#define KEY_DOWN_PRESSED(info) (((info)->trg | (info)->rep) & PAD_KEY_DOWN)
#define KEY_LEFT_PRESSED(info) (((info)->trg | (info)->rep) & PAD_KEY_LEFT)
#define KEY_RIGHT_PRESSED(info) (((info)->trg | (info)->rep) & PAD_KEY_RIGHT)

void WaitForA(void);

#endif