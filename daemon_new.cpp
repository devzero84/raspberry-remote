#include <bitset>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <sstream>
#include <vector>

#include <netinet/in.h>

#include "daemon_new.h"


RaspberryRemoteDaemon::RaspberryRemoteDaemon()
	: mSrvSockFd(-1)
	, mCliSockFd(-1)
	, mSystemCode(0)
	, mUnitCode(0)
	, mDelay(0)
	, mCmd(STATE)
{
	mRCSwitch = new RCSwitch();
}


RaspberryRemoteDaemon::~RaspberryRemoteDaemon()
{
	delete mRCSwitch;
}


bool RaspberryRemoteDaemon::init()
{
	cout << __PRETTY_FUNCTION__ << endl;

	if(wiringPiSetup() == WPI_MODE_UNINITIALISED)
		return false;

	piHiPri(20);
	mRCSwitch->setPulseLength(300);
	usleep(50000);
	mRCSwitch->enableTransmit(0);


	mSrvSockFd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	if(mSrvSockFd == -1)
	{
		cerr << "ERROR " << __PRETTY_FUNCTION__ << endl;
		return false;
	}

	int optval = 1;
	setsockopt(mSrvSockFd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

	// TODO: IPv6
	struct sockaddr_in srvAddr;
	memset(&srvAddr, 0, sizeof(srvAddr));
	srvAddr.sin_addr.s_addr = INADDR_ANY;
	srvAddr.sin_family = AF_INET;
	srvAddr.sin_port = htons(11337);

	if(bind(mSrvSockFd, reinterpret_cast<sockaddr*>(&srvAddr), sizeof(sockaddr_in)) == -1)
	{
		cerr << "ERROR " << __PRETTY_FUNCTION__ << endl;
		return false;
	}

	if(listen(mSrvSockFd, 5) != 0)
	{
		cerr << "ERROR " << __PRETTY_FUNCTION__ << endl;
		return false;
	}

	return true;
}


void RaspberryRemoteDaemon::serverLoop()
{
	cout << __PRETTY_FUNCTION__ << endl;

	struct sockaddr_in cliAddr;
	unsigned int cliLen = sizeof(cliAddr);

	while(true)
	{
		mCliSockFd = accept(mSrvSockFd, reinterpret_cast<sockaddr*>(&cliAddr), &cliLen);

		cout << __PRETTY_FUNCTION__ << endl;

		char buffer[20];
		int n = read(mCliSockFd, buffer, sizeof(buffer));
		cout << "\tread: " << n << endl;
		buffer[n] = '\0';
		cout << "\tbuffer: '" << buffer << "'" << endl;

		setInput(buffer);
		if(parseInput())
		{
			processInput();
			dumpPowerStateOn();
		}
		close(mCliSockFd);
	}
}


void RaspberryRemoteDaemon::setInput(string inputStr)
{
	mRecvStr = inputStr.erase(inputStr.find_last_not_of('\n') + 1);
}


bool RaspberryRemoteDaemon::parseDecimalSystemUnitCode()
{
	cout << __PRETTY_FUNCTION__ << endl;

	if(mRecvStr.length() >= 5 && mRecvStr.length() <= 11)
	{
		istringstream buf(mRecvStr);
		vector<string> v;

		for(string token; getline(buf, token, '/'); )
		{
			v.push_back(token);
		}

		if(v.size() >= 3)
		{
			unsigned short systemCode, unitCode, cmd;
			unsigned short delay = 0;
			vector<string>::iterator it = v.begin();
			stringstream ss;
			ss.exceptions(ios::failbit);

			try
			{
				ss.str(*it++);
				ss >> systemCode;
				ss.clear();

				ss.str(*it++);
				ss >> unitCode;
				ss.clear();

				ss.str(*it++);
				ss >> cmd;

				if(it != v.end())
				{
					ss.clear();
					ss.str(*it++);
					ss >> delay;
				}
			}
			catch(...)
			{
				return false;
			}

			if( systemCode >= 0 && systemCode <= 31 &&
				unitCode >= 0 && unitCode <= 31 &&
				cmd >= 0 && cmd <= 2 &&
				delay >= 0 && delay <= 999 )
			{
				mSystemCode = systemCode;
				mUnitCode = unitCode;
				mCmd = static_cast<rcswitch_cmd_t>(cmd);
				mDelay = delay;
				return true;
			}
		}
	}

	return false;
}


bool RaspberryRemoteDaemon::parseLegacy()
{
	cout << __PRETTY_FUNCTION__ << endl;

	if(mRecvStr.length() >= 8 && mRecvStr.length() <= 11)
	{
		for(int i = 0; i <= 4; i++)
		{
			if(mRecvStr[i] != '0' && mRecvStr[i] != '1')
				return false;
		}

		// check whether unit code and cmd are valid
		if(!(mRecvStr[5] == '0' && mRecvStr[6] >= '1' && mRecvStr[6] <= '5' && mRecvStr[7] >= '0' && mRecvStr[7] <= '2'))
			return false;

		mSystemCode = static_cast<unsigned short>(bitset<5>(mRecvStr.substr(0, 5)).to_ulong());
		switch(mRecvStr[6])
		{
			case '1': mUnitCode = 16; break;
			case '2': mUnitCode =  8; break;
			case '3': mUnitCode =  4; break;
			case '4': mUnitCode =  2; break;
			case '5': mUnitCode =  1; break;
		}
		mCmd = static_cast<rcswitch_cmd_t>(mRecvStr[7] - 0x30);

		// check if delay is given and valid, otherwise set to 0
		if(mRecvStr.length() > 8)
		{
			stringstream ss(mRecvStr.substr(8, mRecvStr.length() - 8));
			ss >> mDelay;
			if(ss.fail())
				return false;
		}
		else
		{
			mDelay = 0;
		}

		return true;
	}

	return false;
}


bool RaspberryRemoteDaemon::parseInput()
{
	cout << __PRETTY_FUNCTION__ << endl;

	if( parseDecimalSystemUnitCode() || parseLegacy() )
	{
		printf("\tmRecvStr '%s' is valid (system code: '%d', unit code: '%d', cmd: '%d', delay: '%d')\n", mRecvStr.c_str(), mSystemCode, mUnitCode, mCmd, mDelay);
		return true;
	}
	else
	{
		cout << "\tmRecvStr '" << mRecvStr << "' is NOT valid!" << endl;
	}

	return false;
}


void RaspberryRemoteDaemon::processInput()
{
	cout << __PRETTY_FUNCTION__ << endl;

	char* systemCodePtr = const_cast<char*>(bitset<5>(mSystemCode).to_string().c_str());

	switch(mCmd)
	{
		case OFF:
			mRCSwitch->switchOffBinary(systemCodePtr, mUnitCode);
			savePowerState(false);
			writePowerStateToSocket();
			break;

		case ON:
			mRCSwitch->switchOnBinary(systemCodePtr, mUnitCode);
			savePowerState(true);
			writePowerStateToSocket();
			break;

		case STATE:
			writePowerStateToSocket();
			break;
	}
}


unsigned short RaspberryRemoteDaemon::getPlugAddr()
{
	cout << __PRETTY_FUNCTION__ << endl;

	unsigned short plugAddr = (mSystemCode<<5) | mUnitCode;

	cout << "\t" << plugAddr << endl;
	return plugAddr;
}


void RaspberryRemoteDaemon::dumpPowerStateOn()
{
	cout << __PRETTY_FUNCTION__ << endl;

	for(list<unsigned short>::iterator it = mPowerState.begin(); it != mPowerState.end(); ++it)
	{
		unsigned short systemCode = *(it) >> 5;
		unsigned short unitCode   = *(it) & ((1<<5)-1);
		cout << "\t" << *it << " (" << systemCode << "/" << unitCode << ")" << endl;
	}

}


void RaspberryRemoteDaemon::savePowerState(bool stateOn)
{
	cout << __PRETTY_FUNCTION__ << endl;

	if(stateOn)
	{
		mPowerState.push_back(getPlugAddr());
		mPowerState.sort();
		mPowerState.unique();
	}
	else
	{
		mPowerState.remove(getPlugAddr());
	}
}


bool RaspberryRemoteDaemon::getPowerState()
{
	cout << __PRETTY_FUNCTION__ << endl;

	for(list<unsigned short>::iterator it = mPowerState.begin(); it != mPowerState.end(); ++it)
	{
		if(*it == getPlugAddr())
		{
			return true;
		}
	}

	return false;
}


void RaspberryRemoteDaemon::writePowerStateToSocket()
{
	cout << __PRETTY_FUNCTION__ << endl;

	char powerState = getPowerState() ? '1' : '0';
	write(mCliSockFd, &powerState, 1);
}


bool RaspberryRemoteDaemon::doFork()
{
	pid_t pid = fork();

	if(pid > 0)
		_exit(0);
	else if(pid < 0)
		return false;

	return true;
}


bool RaspberryRemoteDaemon::daemonize()
{
	if(getppid() == -1)
		return false;

	if(!doFork())
		return false;

	if(setsid() < 0)
		return false;

	if(!doFork())
		return false;

	umask(0);
	chdir("/");

	freopen("/dev/null", "r", stdin);
	freopen("/dev/null", "w", stdout);
	freopen("/dev/null", "w", stderr);

	return true;
}


int main(int argc, char* argv[])
{
	if(argc == 2 && (argv[1] == string("-d") || argv[1] == string("--daemon")))
	{
		if(!RaspberryRemoteDaemon::daemonize())
			return -1;
	}

	RaspberryRemoteDaemon rrd;

	if(!rrd.init())
		return -1;

	rrd.serverLoop();

	return 0;
}
