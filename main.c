#include <IOKit/IOKitLib.h>
#include "smc.h"
#include <IOKit/ps/IOPowerSources.h>
#include <IOKit/ps/IOPSKeys.h>
#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFNumber.h>
#include <signal.h> 

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
		CFStringRef source=CFDictionaryGetValue(info_dict,CFSTR(kIOPSPowerSourceStateKey));
		if(source!=NULL){
			return info_dict;
		}
	}
}

void intHandler ( int dummy){
	toggle_charging(1); //By default, enable charging by exiting
	exit(0);
}
int get_battery_percentage(CFDictionaryRef info_dict){
		int percentage;
		CFNumberRef capacity=CFDictionaryGetValue(info_dict,CFSTR(kIOPSCurrentCapacityKey));
		CFNumberGetValue(capacity, kCFNumberSInt32Type, &percentage);
		return percentage;
}

bool is_charging(CFDictionaryRef info_dict){
	CFBooleanRef charging=CFDictionaryGetValue(info_dict,CFSTR(kIOPSIsChargingKey));
	return (charging==kCFBooleanTrue) ? true : false;
	
}

bool compare_string(char *a, char *b){
	return strcmp(a,b)==0;
}
int main(int argc, char *argv[]){
	char *action=(argc>=2) ? argv[1] : "maintain";
	char *setting=(argc>=3) ? argv[2] : "";

	int limit;
	if (compare_string(action,"charging")){
			printf("Setting charging to %s%%\n",(compare_string(setting,"off")) ? "off" : "on");
			toggle_charging((setting=="off") ? 0 : 1);

	}else if (compare_string(action,"charge")){
		toggle_charging(1);
		limit = (setting=="") ? 80 : atoi(setting);
		printf("Charging to %i%%\n%",limit);
		signal(SIGINT, intHandler);
		while (true){
			CFDictionaryRef info_dict=get_battery_info();
			bool charging=is_charging(info_dict);
			int percentage=get_battery_percentage(info_dict);
			if (percentage>= limit){
				toggle_charging(0);
				break;
			}
		}
		printf("Charged to %i%%\n",limit);

	}else if (compare_string(action,"maintain")){
		signal(SIGINT, intHandler);
		limit = (setting=="") ? 80 : atoi(setting);
		printf("Maintaining charge at %i%%\n",limit); 
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
	}else if(compare_string(action,"status")){
		CFDictionaryRef info_dict=get_battery_info();
		bool charging=is_charging(info_dict);
		int percentage=get_battery_percentage(info_dict);
		printf("Charge: %i%%\n",percentage);
		printf("Charging: %s\n",(charging) ? "Yes" : "No");
	}
}
