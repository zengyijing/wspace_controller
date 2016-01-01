#
# To compile, type "make" 
# To remove files, type "make clean"
#
LIBS = -lpthread -lrt
CXXFLAGS = -std=c++11
all: wspace_controller

wspace_controller: time_util.o monotonic_timer.o wspace_asym_util.o tun.o wspace_controller.o 
	$(CXX) $(CXXFLAGS) $^ -o wspace_controller $(LIBS)

#wspace_controller_test: controller_test.o wspace_asym_util.o 
#	$(CXX) $(CXXFLAGS) $^ -o wspace_controller_test $(LIBS)
 
%.o: %.cc
	$(CXX) $(CXXFLAGS) -o $@ -c $<

clean:
	rm -rf wspace_controller *.o 

tag: 
	ctags -R *
