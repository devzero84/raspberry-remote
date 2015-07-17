#ifndef DAEMON_NEW_H
#define DAEMON_NEW_H

#include <list>
#include <map>
#include <string>

#include <sys/stat.h>
#include <unistd.h>

#include "RCSwitch.h"


using namespace std;

enum rcswitch_cmd_t { OFF = 0, ON, STATE };


class RaspberryRemoteDaemon
{
	public:
		RaspberryRemoteDaemon();
		~RaspberryRemoteDaemon();
		bool init();
		bool serverLoop();
		void setInput(string inputStr);
		bool parseInput();
		void processInput();
		unsigned short getPlugAddr();
		void dumpPowerStateOn();
		static bool daemonize();


	private:
		bool parseDecimalSystemUnitCode();
		bool parseLegacy();
		void savePowerState(bool stateOn);
		bool getPowerState();
		void writePowerStateToSocket();
		static bool doFork();

		RCSwitch* mRCSwitch;
		int mSrvSockFd;
		int mCliSockFd;
		fd_set mSockFdSet;
		unsigned short mSystemCode;
		unsigned short mUnitCode;
		unsigned short mDelay;
		string mRecvStr;
		rcswitch_cmd_t mCmd;
		list<unsigned short> mPowerState;
		map<unsigned short, pair<bool, unsigned short> > mDelayPending;
};

#endif /* DAEMON_NEW_H */
