#include <bitset>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <sstream>

#include <netinet/in.h>
#include <unistd.h>

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
	struct sockaddr_in cliAddr;
	unsigned int cliLen = sizeof(cliAddr);

	while(true)
	{
		mCliSockFd = accept(mSrvSockFd, reinterpret_cast<sockaddr*>(&cliAddr), &cliLen);

		char buffer[20];
		int n = read(mCliSockFd, buffer, sizeof(buffer));
		cout << "read: " << n << endl;
		buffer[n] = '\0';
		cout << "buffer: '" << buffer << "'" << endl;

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


bool RaspberryRemoteDaemon::parseInput()
{
	bool isValid = false;
	mDelay = 0;

	// check if raspberry-remote compatible
	isValid = true;
	if(mRecvStr.length() >= 8)
	{
		for(int i = 0; i <= 4; i++)
		{
			if(mRecvStr[i] != '0' && mRecvStr[i] != '1')
			{
				isValid = false;
				break;
			}
		}

		// check whether unit code and cmd are valid
		if(isValid)
		{
			if(mRecvStr[5] == '0' && mRecvStr[6] >= '1' && mRecvStr[6] <= '5' && mRecvStr[7] >= '0' && mRecvStr[7] <= '2')
			{
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
			}
			else
			{
				isValid = false;
			}
		}

		// check if delay is given and valid
		if(isValid)
		{
			if(mRecvStr.length() > 8) {
				stringstream ss(mRecvStr.substr(8, mRecvStr.length() - 8));
				ss >> mDelay;
				if(ss.fail())
				{
					cerr << "delay value invalid!" << endl;
					isValid = false;
				}
			}
		}

		if(isValid)
		{
			printf("string '%s' is raspberry-remote compatible (system code: '%d', unit code: '%d', cmd: '%d', delay: '%d')\n", mRecvStr.c_str(), mSystemCode, mUnitCode, mCmd, mDelay);
			return true;
		}
	}
	cout << "string '" << mRecvStr << "' is NOT raspberry-remote compatible" << endl;
	isValid = false;

	// TODO: other protocols

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
	unsigned short plugAddr = (mSystemCode<<5) | mUnitCode;
	cout << __PRETTY_FUNCTION__ << " plugAddr: " << plugAddr << endl;
	return plugAddr;
}


void RaspberryRemoteDaemon::dumpPowerStateOn()
{
	cout << __PRETTY_FUNCTION__ << endl;
	for(list<unsigned short>::iterator it = mPowerState.begin(); it != mPowerState.end(); ++it)
	{
		unsigned short systemCode = *(it) >> 5;
		unsigned short unitCode   =  *(it) & ((1<<5)-1);
		cout << "\t" << *it << " (" << systemCode << "/" << unitCode << ")" << endl;
	}

}


void RaspberryRemoteDaemon::savePowerState(bool stateOn)
{
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
	bool ret = false;
	for(list<unsigned short>::iterator it = mPowerState.begin(); it != mPowerState.end(); ++it)
	{
		if(*it == getPlugAddr())
		{
			ret = true;
			break;
		}
	}

	cout << __PRETTY_FUNCTION__ << " ret: " << ret << endl;
	return ret;
}


void RaspberryRemoteDaemon::writePowerStateToSocket()
{
	char powerState = getPowerState() ? '1' : '0';
	write(mCliSockFd, &powerState, 1);
}


int main()
{
	RaspberryRemoteDaemon rrd;

	if(!rrd.init())
		return -1;

	rrd.serverLoop();

	return 0;
}
