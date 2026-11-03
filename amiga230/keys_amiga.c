/*
**  keys_amiga.c
**
**  read keyboard and mouse events native
**
*/

#pragma amiga-align
#include <exec/exec.h>
#include <intuition/intuition.h>
#include <proto/exec.h>
#include "keys_amiga.h"
#pragma default-align


int GetMessagesNat(struct MsgPort *port,struct MsgStruct *msg,int maxmsg)
{
  int i = 0;
  struct IntuiMessage *imsg;

  while ((imsg = (struct IntuiMessage *)GetMsg(port))) {
    if (i < maxmsg) {
      msg[i].Code = imsg->Code;
      msg[i].Class = imsg->Class;
      i++;
    }
    ReplyMsg((struct Message *)imsg);
  }

  return i;
}
