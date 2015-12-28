#!/bin/bash
reset
rm -rf wspace_controller *.o
g++ -Os -pipe -fno-caller-saves -Wno-error=unused-but-set-variable -fpic -o time_util.o -c time_util.cc

g++ -Os -pipe -fno-caller-saves -Wno-error=unused-but-set-variable -fpic -o monotonic_timer.o -c monotonic_timer.cc

g++ -Os -pipe -fno-caller-saves -Wno-error=unused-but-set-variable -fpic -o wspace_asym_util.o -c wspace_asym_util.cc

g++ -Os -pipe -fno-caller-saves -Wno-error=unused-but-set-variable -fpic -o tun.o -c tun.cc

#g++ -Os -pipe -fno-caller-saves -Wno-error=unused-but-set-variable -fpic -o rate_adaptation.o -c rate_adaptation.cc

g++ -Os -pipe -fno-caller-saves -Wno-error=unused-but-set-variable -fpic -o select_bs.o -c select_bs.cc

g++ -Os -pipe -fno-caller-saves -Wno-error=unused-but-set-variable -fpic -o wspace_controller.o -c wspace_controller.cc
#rate_adaptation.o
g++ -Os -pipe -fno-caller-saves -Wno-error=unused-but-set-variable -fpic  time_util.o monotonic_timer.o wspace_asym_util.o select_bs.o tun.o wspace_controller.o -o wspace_controller -lpthread -lrt
