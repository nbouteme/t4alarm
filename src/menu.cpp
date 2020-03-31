#include <memory>
#include <vector>
#include <functional>
#include <cstring>
#include "IMenuRenderer.h"

#include /* u wouldn't date */ "AClock.h" // ?

using namespace std;

int get_hl_pos(char *buff, int tc, const char *spec = ":\n./ ABCDEFGHIJLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz");

const char *days[] = {
	"Mon.",
	"Tues.",
	"Wed.",
	"Thurs.",
	"Fri.",
	"Sat.",
	"Sun."
};

tmElements_t tm = {0, 0, 0, 1, 1, 1, 50};
vector<Alarm> alarms;

// Ces variables sont globales car utilisé dans des lambdas top-level,
// et donc ne peuvent pas être capturées dans une portée locale
int tc = 0;
char c_time[32];

// https://www.hackerearth.com/blog/developers/how-to-find-the-day-of-a-week
// Avec un ajustement...
int dow(int y, int m, int d) {
  static int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
  y -= m < 3;
  return ((y + y / 4 - y / 100 + y / 400 + t[m - 1] + d) - 1) % 7;
}

unique_ptr<vector<Entry>> FileList(const string &folder, string &fn, int depth = 1) {
	auto *entr = new vector<Entry>;
	auto f = SD.open(folder.c_str());

	if (folder != "")
		entr->push_back(Entry("..",
							 [=, &fn](IMenuRenderer *renderer, Entry *self) {
								 renderer->back();
							 }));

	auto validateFile =
		[=, &fn](const string &efn) {
			puts(efn.c_str());
			return
				Entry((folder + "/" + efn) == fn ? string("<") + efn + ">" : efn,
					  [=, &fn](IMenuRenderer *renderer, Entry *self) {
						  fn = folder + "/" + efn;
						  int k = depth;
						  while (k--)
							  renderer->back();
					  });
		};

	auto validateFolder =
		[=, &fn](const string &efn) {
			puts(efn.c_str());
			return
				Entry(efn,
					  [=, &fn](IMenuRenderer *renderer, Entry *self) {
						  renderer->navigate(FileList(folder + "/" + efn, fn, depth + 1));
					  });
		};

	File ent;
	while ((ent = f.openNextFile())) {
		if (ent.isDirectory())
			entr->push_back(validateFolder(string(ent.name()) + "/"));
		else
			entr->push_back(validateFile(ent.name()));
	}
	if (entr->size() == 0)
		entr->push_back(Entry("<Root Empty>",
					  [=, &fn](IMenuRenderer *renderer, Entry *self) {
						  renderer->back();
					  }));
	return unique_ptr<vector<Entry>>(entr);
}

Entry FilePicker(string &ofn) {
	string folder = "";
	if (ofn[0]) {
		if (auto e = ofn.find_last_of("/"); e != string::npos) {
			folder = ofn.substr(0, e);
		}
	}

	return Entry("Set Music...",
				 [=, &ofn](IMenuRenderer *renderer, Entry *self) {
					 renderer->navigate(FileList(folder, ofn));
				 });
}

unique_ptr<vector<Entry>> AlarmEditor(Alarm *a) {
	auto *entr = new vector<Entry>;

	// shared car capturé par copie dans le contexte de deux lambdas
	auto state = make_shared<AlarmState>();
	
	auto rebuild_disp =
		[=]() {
			sprintf(state->days,
					"%c%c%c%c%c%c%c %02d:%02d",
					a->days[0] ? 'M' : '-',
					a->days[1] ? 'T' : '-',
					a->days[2] ? 'W' : '-',
					a->days[3] ? 'T' : '-',
					a->days[4] ? 'F' : '-',
					a->days[5] ? 'S' : '-',
					a->days[6] ? 'S' : '-',
					a->hour,
					a->minutes);
			return string(state->days);
		};

	entr->push_back(
		Entry("Set time...",
			  [=](IMenuRenderer *renderer, Entry *self) {
				  self->disp = rebuild_disp();
				  int k = get_hl_pos(state->days, state->selected_field) + 1;
				  renderer->setCursor(k % 16, k / 16);
				  renderer->cursor();
				  renderer->blink();
				  renderer->lock_input();
			  },
			  [=](IMenuRenderer *renderer, Entry *self, char c) {
				  switch(c) {
				  case '.':
					  renderer->unlock_input();
					  renderer->noBlink();
					  self->disp = "Set time...";
					  return;
				  case '<':
					  state->selected_field--;
					  break;
				  case '>':
					  state->selected_field++;
					  break;
				  case '5':
					  if (state->selected_field <= 6) {
						  a->days[state->selected_field] ^= 1;
						  state->selected_field++;
						  break;
					  }
				  case '0':
				  case '1':
				  case '2':
				  case '3':
				  case '4':
				  case '6':
				  case '7':
				  case '8':
				  case '9':
					  if (state->selected_field >= 7) {
						  state->days[get_hl_pos(state->days, state->selected_field, ":\n./ ")] = c;
						  sscanf(state->days + 8, "%02d:%02d", &a->hour, &a->minutes);
						  state->selected_field++;
					  }
				  }
				  if (a->minutes < 0)
					  a->minutes = 59;
				  if (a->minutes > 59) {
					  a->minutes = 0;
					  a->hour++;
				  }
				  if (a->hour < 0)
					  a->hour = 23;
				  if (a->hour > 23)
					  a->hour = 0;
				  int k = get_hl_pos(state->days, state->selected_field, ":\n./ ");
				  renderer->setCursor(k % 16 + 1, k / 16);
				  self->disp = rebuild_disp();
				  state->selected_field %= 11;
			  }));

	entr->push_back(FilePicker(a->music));	
	entr->push_back(Entry("Delete",
						[=](IMenuRenderer *renderer, Entry *self) {
							alarms.erase(remove_if(begin(alarms), end(alarms),
							[=](const auto &e) {
								return &e == a;
							}), end(alarms));

							renderer->back();
							renderer->back();
							renderer->exec_input('>'); // :^)
						}));	
	entr->push_back(Entry("Done",
						[=](IMenuRenderer *renderer, Entry *self) {
							renderer->back();
							renderer->back();
							renderer->exec_input('>'); // :^)
						}));	
	return unique_ptr<vector<Entry>>(entr);
}

Entry AlarmEditEntry(Alarm *a) {
	char buff[16];
	sprintf(buff,
			"%c%c%c%c%c%c%c %02d:%02d",
			a->days[0] ? 'M' : '-',
			a->days[1] ? 'T' : '-',
			a->days[2] ? 'W' : '-',
			a->days[3] ? 'T' : '-',
			a->days[4] ? 'F' : '-',
			a->days[5] ? 'S' : '-',
			a->days[6] ? 'S' : '-',
			a->hour,
			a->minutes);
	return Entry(buff,
				 [=](IMenuRenderer *renderer, Entry *self) {
					 renderer->navigate(AlarmEditor(a));
				 });
}

int get_hl_pos(char *buff, int tc, const char *spec) {
	if (tc < 0)
		tc = 0;
	int n = tc;
	int k = 0;
	while (n != 0 || strchr(spec, buff[k])) {
		if (!strchr(spec, buff[k]))
			n--;
		++k;
		if (!buff[k])
			k = 0;
	}
	return k;
}

int isLeap(int y) {
	// Every year that is exactly divisible by four is
	// a leap year, except for years that are exactly
	// divisible by 100, but these centurial years are
	// leap years if they are exactly divisible by 400.
	return (!(y % 4)) && ((y % 100) || !(y % 400));
}

int dim(int m, int y) {
	static short t = 0xAD5;
	--m;
	if (m == 1)
		return 28 + isLeap(y);
	return 30 + !!(t & (1 << m));
}

void set_time(const char *ref) {
	int ye;
	sscanf(ref,
		   "%02hhd / %02hhd / %04d\n"
		   "%02hhd:%02hhd:%02hhd",
		   &tm.Day, &tm.Month, &ye,
		   &tm.Hour, &tm.Minute, &tm.Second);
	if (ye > 2222)
		ye = 2222;
	tm.Year = (uint8_t)(ye - 1970);
	auto clamp =
		[](const int minv, uint8_t &v, const int maxv) {
			if (v < minv)
				v = minv;
			if (v > maxv)
				v = maxv;
		};
	clamp(1, tm.Day, dim(tm.Month, ye));
	clamp(1, tm.Month, 12);
	clamp(0, tm.Second, 59);
	clamp(0, tm.Minute, 59);
	clamp(0, tm.Hour, 23);
	int d = dow(ye, tm.Month, tm.Day) + 1;
	if (d == 8)
		d = 1;
	tm.Wday = d;   // inutile puisque j'ai de quoi
	// calculer plus rapidement localement, mais je
	// sais pas si il est utilisé en interne dans le RTC
	RTC.write(tm);
}

vector<Entry> *root_entries = new vector<Entry>{
	(Entry) {
		"< Set date >",
		[](IMenuRenderer *renderer, Entry *self) {
			RTC.read(tm);
			renderer->cursor();
			renderer->blink();
			renderer->lock_input();
		},
		[](IMenuRenderer *renderer, Entry *self, char c) {
			switch (c) {
			case '.':
				renderer->unlock_input();
				set_time(c_time);
				renderer->noBlink();
				break;
			case '>':
			    tc++;
			    break;
			case '<':
    			tc--;
				break;
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				int k = get_hl_pos(c_time, tc);
				c_time[k] = c;
				set_time(c_time);
				tc++;
				break;
			}
		},
		[](IMenuRenderer *renderer, Entry *self) {
			sprintf(c_time,
					"%02d / %02d / %04d  "
					"%02d:%02d:%02d %s",
					tm.Day, tm.Month, tm.Year + 1970,
					tm.Hour, tm.Minute, tm.Second,
					days[dow(tm.Year + 1970, tm.Month, tm.Day)]);
			int k = get_hl_pos(c_time, tc);
			renderer->setCursor(0, 0);
			renderer->print(string(c_time).substr(0, 16).c_str());
			renderer->setCursor(0, 1);
			renderer->print(string(c_time).substr(16).c_str());
			renderer->setCursor(k % 16, k / 16);
		}
	},
	(Entry) {
		"< Alarms >",
		[](IMenuRenderer *renderer, Entry *self) {
			auto alrm = new vector<Entry>();
			for (auto &a : alarms) {
				alrm->push_back(AlarmEditEntry(&a));
			}
			alrm->push_back(Entry("+ new Alarm...",
								[=](IMenuRenderer *renderer, Entry *self) {
									Alarm l{{1, 1, 1, 1, 1, 1, 1}, tm.Hour, tm.Minute, DEFAULT_MUSIC};
									alarms.push_back(l);
									renderer->navigate(AlarmEditor(&alarms.back()));
								}));
			auto ptr = unique_ptr<vector<Entry>>(alrm);
			renderer->navigate(move(ptr));
		}
	},
	(Entry) {
		"< Save & Exit >",
		[](IMenuRenderer *renderer, Entry *self) {
			renderer->back();
			auto cfg = SD.open("/clock.cfg", FILE_WRITE | O_TRUNC);
			size_t s = alarms.size();
			cfg.write((uint8_t*)&s, sizeof(s));
			for (auto &a : alarms) {
				size_t sl = a.music.size();
				cfg.write((uint8_t*)a.days, sizeof(a.days));
				cfg.write((uint8_t*)&a.hour, sizeof(a.hour));
				cfg.write((uint8_t*)&a.minutes, sizeof(a.minutes));
				cfg.write((uint8_t*)&sl, sizeof(sl));
				cfg.write((uint8_t*)a.music.c_str(), sl);
			}
			cfg.flush();
		}
	}
};
