//
// Created by sheverdin on 12/13/23.
//

#ifndef TF_AUTORESTATRT_PING_H
#define TF_AUTORESTATRT_PING_H

//int	ping (const char* ip, unsigned long timeout, unsigned long* reply_time);
int ping(char ip[],  unsigned long timeout, unsigned long  *reply_time);

#endif //TF_AUTORESTATRT_PING_H
