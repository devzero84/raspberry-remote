DESCRIPTION = "RCSwitch on Raspberry Pi"
LICENSE = "GPL"
VERSION = 1.1

CXXFLAGS  = -O2 -march=armv6j -mfpu=vfp -mfloat-abi=hard -pipe
CXXFLAGS += -Wall
CXXFLAGS += -lwiringPi

default: daemon_new

daemon: RCSwitch.o daemon.o
	$(CXX) -pthread $+ -o $@ $(CXXFLAGS) $(LDFLAGS)

daemon_new: RCSwitch.o daemon_new.o
	$(CXX) -pthread $+ -o $@ $(CXXFLAGS) $(LDFLAGS)

send: RCSwitch.o send.o
	$(CXX) $+ -o $@ $(CXXFLAGS) $(LDFLAGS)

clean:
	rm -f *.o send daemon daemon_new
