#ifndef _AUX_FUNCS_H_
#define _AUX_FUNCS_H_

#include "link_layer.h"

void alarmHandler(int signal);
int linkTx(LinkLayer connection);
int linkRx(LinkLayer connection);

#endif _AUX_FUNS_H