#include <IOKit/IOKitLib.h>
#include "smc.hpp"
#include <IOKit/ps/IOPowerSources.h>
#include <IOKit/ps/IOPSKeys.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <signal.h> 
#include <string>
#include <vector>
#include <libproc.h>
#include <sstream>
#include <iostream>
#include <fstream>

using namespace std;
void toggle_charging(bool mode){
	UInt32Char_t B = { 0 };
	UInt32Char_t C = { 0 };
	strncpy(B,"CH0B",sizeof(B));
	B[sizeof(B) - 1] = '\0';
	
	strncpy(C,"CH0C",sizeof(C));
	C[sizeof(C) - 1] = '\0';

	char toggle[3];
	if (mode){
		strncpy(toggle,"00\0",sizeof(toggle));
	}else{
		strncpy(toggle,"02\0",sizeof(toggle));
	}
	io_connect_t g_conn=0;
	SMCOpen(&g_conn);
	SMCWriteSimple(B,toggle,g_conn);
	SMCWriteSimple(C,toggle,g_conn);
	SMCClose(g_conn);
	
}

void test(){
	printf("Hello!\n");
}

int get_battery_percentage(){
	UInt32Char_t SBAS = { 0 };
	strncpy(SBAS,"SBAS",sizeof(SBAS));
	SBAS[sizeof(SBAS) - 1] = '\0';

	SMCVal_t val;

	io_connect_t g_conn=0;
	SMCOpen(&g_conn);
	SMCReadKey2(SBAS,&val,g_conn);
	SMCClose(g_conn);

	float charge;
	memcpy(&charge,val.bytes,sizeof(float));
	return int(charge);
}

bool is_charging(){
	UInt32Char_t B = { 0 };
	strncpy(B,"CH0B",sizeof(B));
	B[sizeof(B) - 1] = '\0';

	SMCVal_t val;

	io_connect_t g_conn=0;
	SMCOpen(&g_conn);
	SMCReadKey2(B,&val,g_conn);
	SMCClose(g_conn);

	char toggle[val.dataSize];
	for (int i = 0; i < val.dataSize; i++){
		sprintf(&toggle[i],"%02x",(unsigned char) val.bytes[i]);
	}
	return (strcmp(toggle,"00")==0) ? true : false;
	
	
}

string get_daemon(bool should_kill){
	std::ifstream daemon_log("/tmp/battery_info.txt");
	if (!daemon_log.good()){
		return "";
	}
	vector<string> contents;
	string line;
	while(getline(daemon_log,line)){
		contents.push_back(line);
	}
	if (should_kill){
		int pid=stoi(contents[1]);
		kill(pid,SIGINT);
		while (kill(pid,0)==0){
		}
		return "";
	}else{
		return contents[0];
	}
}

void write_to_file(string format_string,int limit){
	ofstream daemon_log("/tmp/battery_info.txt");
	if (!daemon_log.good()){
		return;
	}
	char str[80];
	sprintf(str,(format_string+" %i%%").c_str(),limit);
	//daemon_log << format(format_string,get_battery_percentage(info_dict)) << endl;
	daemon_log << str << endl;
	daemon_log << getpid() << endl;
	daemon_log.close();
	}

IOPMAssertionID assertionID;
IOReturn success;
void toggle_discharge(bool mode){
	UInt32Char_t I = { 0 };
	strncpy(I,"CH0I",sizeof(I));
	I[sizeof(I) - 1] = '\0';
	char toggle[3];
	if (mode){
		strncpy(toggle,"01\0",sizeof(toggle));
	}else{
		strncpy(toggle,"00\0",sizeof(toggle));
	}
	io_connect_t g_conn=0;
	SMCOpen(&g_conn);
	SMCWriteSimple(I,toggle,g_conn);
	SMCClose(g_conn);

}

void intHandler ( int dummy){
	toggle_charging(1); //By default, enable charging when exiting
	toggle_discharge(0); //0 enables discharge, even on adapter
	remove("/tmp/battery_info.txt");
	exit(0);
}

int setup(string format_string,string setting){
	if (fork()==0){
	signal(SIGINT, intHandler);
	signal(SIGHUP,SIG_IGN);
	get_daemon(1);
	int limit = (setting=="") ? 80 : stoi(setting);
	printf((format_string+" %i%%\n").c_str(),limit);
	write_to_file(format_string,limit);
	return limit;
	}else{
		exit(0);
	}
}

int main(int argc, char *argv[]){
	string action=(argc>=2) ? argv[1] : "maintain";
	string setting=(argc>=3) ? argv[2] : "";
	int limit;
	if (action=="charging"){
			get_daemon(1);
			printf("Setting charging to %s\n",(setting=="off") ? "off" : "on");
			toggle_charging((setting=="off") ? 0 : 1);

	}else if (action=="charge"){
		limit=setup("Charging to",setting);
		toggle_charging(1);
		while (true){
			int percentage=get_battery_percentage();
			if (percentage>= limit){
				toggle_charging(0);
				break;
			sleep(60);
			}
		}
		printf("Charged to %i%%\n",limit);

	}else if (action=="maintain"){
		limit=setup("Maintaining charge at",setting);
		while (true){
			bool charging=is_charging();
			int percentage=get_battery_percentage();
			if (percentage>= limit && charging){
				toggle_charging(0);
			}else if (percentage < limit && !charging){
				toggle_charging(1);
			}
			sleep(60);
		}

	}else if (action=="discharging"){
		limit=setup("Discharging to",setting);
		toggle_charging(0);
		toggle_discharge(1);
		while (true){
			int percentage=get_battery_percentage();
			if (percentage< limit){
				toggle_charging(1);
				toggle_discharge(0);
				break;
			}
			sleep(60);
		}
	}else if(action=="status"){
		bool charging=is_charging();
		int percentage=get_battery_percentage();
		printf("Charge: %i%%\n",percentage);
		printf("Charging: %s\n",(charging) ? "Enabled" : "Disabled");
		ifstream file("/tmp/battery_info.txt");
		if (file.good()){
			printf("%s\n",get_daemon(0).c_str());
		}
	}else if(action=="reset"){
		get_daemon(true);
		intHandler(1);
	}
}
