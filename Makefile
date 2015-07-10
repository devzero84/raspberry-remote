DESCRIPTION = "RCSwitch on Raspberry Pi"
LICENSE = "GPL"
VERSION = 1.0

CXXFLAGS += -Wall
CXXFLAGS += -lwiringPi

default: daemon

daemon: RCSwitch.o daemon.o
	$(CXX) -pthread $+ -o $@ $(CXXFLAGS) $(LDFLAGS)

daemon_new: RCSwitch.o daemon_new.o
	$(CXX) -pthread $+ -o $@ $(CXXFLAGS) $(LDFLAGS)

send: RCSwitch.o send.o
	$(CXX) $+ -o $@ $(CXXFLAGS) $(LDFLAGS)

clean:
	rm -f *.o send daemon
