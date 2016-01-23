#ifndef test_pkt_scheduler_H_
#define test_pkt_scheduler_H_

#include <stdio.h>
#include <string>
#include <iostream>
#include <cassert>

#include "packet_scheduler.h"

void TestPktQueue();
void* LaunchEnqueue(void* arg);
void* LaunchDequeue(void* arg);

#endif
