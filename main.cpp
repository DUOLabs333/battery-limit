#include <IOKit/IOKitLib.h>
#include "smc.hpp"
#include <IOKit/ps/IOPowerSources.h>
#include <IOKit/ps/IOPSKeys.h>
#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFNumber.h>
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

CFDictionaryRef get_battery_info(){
	CFTypeRef info_blob=IOPSCopyPowerSourcesInfo();
	CFArrayRef info_list=IOPSCopyPowerSourcesList(info_blob);
	for (int i=0; i<CFArrayGetCount(info_list); i++){
		CFDictionaryRef info_dict=IOPSGetPowerSourceDescription(info_blob,CFArrayGetValueAtIndex(info_list,i));
		CFStringRef source= (CFStringRef) CFDictionaryGetValue(info_dict,CFSTR(kIOPSPowerSourceStateKey));
		if(source!=NULL){
			return info_dict;
		}
	}
}

void intHandler ( int dummy){
	toggle_charging(1); //By default, enable charging by exiting
	remove("/tmp/battery_info.txt");
	exit(0);
}
int get_battery_percentage(CFDictionaryRef info_dict){
		int percentage;
		CFNumberRef capacity= (CFNumberRef) CFDictionaryGetValue(info_dict,CFSTR(kIOPSCurrentCapacityKey));
		CFNumberGetValue(capacity, kCFNumberSInt32Type, &percentage);
		return percentage;
}

bool is_charging(CFDictionaryRef info_dict){
	CFBooleanRef charging= (CFBooleanRef) CFDictionaryGetValue(info_dict,CFSTR(kIOPSIsChargingKey));
	return (charging==kCFBooleanTrue) ? true : false;
	
}

string get_daemon(bool kill1){
	std::ifstream daemon_log("/tmp/battery_info.txt");
	vector<string> contents;
	string line;
	while(getline(daemon_log,line)){
		contents.push_back(line);
	}
	if (kill1){
		kill(stoi(contents[1]),SIGINT);
		return "";
	}else{
		return contents[0];
	}
}

void write_to_file(string format_string,int limit){
	ofstream daemon_log("/tmp/battery_info.txt");
	char str[80];
	sprintf(str,(format_string+" %i%%").c_str(),limit);
	//daemon_log << format(format_string,get_battery_percentage(info_dict)) << endl;
	daemon_log << str << endl;
	daemon_log << getpid() << endl;
	daemon_log.close();
	}

int setup(string format_string,string setting){
	signal(SIGINT, intHandler);
	signal(SIGHUP,SIG_IGN);
	int limit = (setting=="") ? 80 : stoi(setting);
	printf((format_string+" %i%%\n").c_str(),limit);
	write_to_file(format_string,limit);
	return limit;
}
int main(int argc, char *argv[]){
	string action=(argc>=2) ? argv[1] : "maintain";
	string setting=(argc>=3) ? argv[2] : "";
	int limit;
	if (action=="charging"){
			printf("Setting charging to %s%%\n",(setting=="off") ? "off" : "on");
			toggle_charging((setting=="off") ? 0 : 1);

	}else if (action=="charge"){
		limit=setup("Charging to",setting);
		toggle_charging(1);
		while (true){
			CFDictionaryRef info_dict=get_battery_info();
			int percentage=get_battery_percentage(info_dict);
			if (percentage>= limit){
				toggle_charging(0);
				break;
			}
		}
		printf("Charged to %i%%\n",limit);

	}else if (action=="maintain"){
		limit=setup("Maintaining charge at",setting);
		while (true){
			CFDictionaryRef info_dict=get_battery_info();
			bool charging=is_charging(info_dict);
			int percentage=get_battery_percentage(info_dict);
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
		while (true){
			CFDictionaryRef info_dict=get_battery_info();
			int percentage=get_battery_percentage(info_dict);
			if (percentage< limit){
				toggle_charging(1);
				break;
			}
		}
	}else if(action=="status"){
		CFDictionaryRef info_dict=get_battery_info();
		bool charging=is_charging(info_dict);
		int percentage=get_battery_percentage(info_dict);
		printf("Charge: %i%%\n",percentage);
		printf("Charging: %s\n",(charging) ? "Yes" : "No");
		ifstream file("/tmp/battery_info.txt");
		if (file.good()){
			printf("%s\n",get_daemon(0).c_str());
		}
	}
}
