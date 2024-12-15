#ifndef _CONFIG_IFACE_H_
#define _CONFIG_IFACE_H_

#include <stdint.h>

extern uint8_t LastID[8];

void ResetSelect();
void Select(uint8_t ID[8]);
int SelectNext();
void Release();
void SendCmd(uint8_t cmd);
void SendCommandUID(uint16_t uid);
char* RecvString();
char* RecvData(char* str,uint16_t len);
uint16_t RecvU16();
int SendString(char * str);

#endif /* _CONFIG_IFACE_H_ */
