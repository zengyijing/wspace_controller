#
# To compile, type "make" 
# To remove files, type "make clean"
#
LIBS = -lpthread -lrt
CXXFLAGS = -std=c++11 -w -g -rdynamic
all: wspace_controller

wspace_controller: time_util.o monotonic_timer.o wspace_asym_util.o tun.o \
                   wspace_controller.o packet_scheduler.o
	$(CXX) $(CXXFLAGS) $^ -o wspace_controller $(LIBS)

test_packet_scheduler: test_packet_scheduler.o packet_scheduler.o
	$(CXX) $(CXXFLAGS) $^ -o test_packet_scheduler $(LIBS)

%.o: %.cc
	$(CXX) $(CXXFLAGS) -o $@ -c $<

clean:
	rm -rf wspace_controller *.o 

tag: 
	ctags -R *
