#include <map>
#include <string>

#include "RCSwitch.h"


using namespace std;

enum rcswitch_cmd_t { OFF = 0, ON, STATE };


class RaspberryRemoteDaemon
{
	public:
		RaspberryRemoteDaemon();
		~RaspberryRemoteDaemon();
		bool init();
		void serverLoop();
		void setInput(string inputStr);
		bool parseInput();
		void processInput();
		unsigned short getPlugAddr();
		void dumpPowerState();


	private:
		void savePowerState(bool stateOn);
		bool getPowerState();
		void writePowerStateToSocket();

		RCSwitch* mRCSwitch;
		int mSrvSockFd;
		int mCliSockFd;
		unsigned short mSystemCode;
		unsigned short mUnitCode;
		unsigned short mDelay;
		string mRecvStr;
		rcswitch_cmd_t mCmd;
		map<unsigned short, bool> mPowerState;
};
