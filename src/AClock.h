#ifndef ACLOCK_H
#define ACLOCK_H

#include <string>
#include <vector>
#include <DS1307RTC.h>
#include <SD.h>

#define DEFAULT_MUSIC "jitter.wav"

struct Alarm {
	bool days[7]; // MTWTFSS 16:20
	int hour, minutes;
	std::string music;
};

struct AlarmState {
	int selected_field = 0;
	char days[16];
};

extern std::vector<Alarm> alarms;
extern tmElements_t tm;
extern std::vector<Entry> *root_entries;
extern const char *days[];
int dow(int y, int m, int d);

#define DEBUGLOG

#ifdef DEBUGLOG
#define IFDBG(x) x
#else
#define DEBUG_LOG(...)
#define IFDBG(x)
#endif

#define DEBUG_LOG(...) IFDBG(Serial.printf(__VA_ARGS__))


#endif /* ACLOCK_H */