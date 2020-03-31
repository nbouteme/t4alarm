#include <Arduino.h>

/*
  La bibliothèque LiquidCrystal fournie avec teensyduino contient 2 headers, 
  un vide à la racine, et le vrai dans le dossier src,
  mais dans le cas général, de deux headers avec le même nom,
  je ne sais pas comment décider lequel inclure, ni comment 
  arduino-builder prends cette décision.
  Ou bien les bibliothèques de /usr/share/arduino/libraries 
  sont prioritaire par rapport à celles de /usr/share/arduino/hardware/teensy/avr/libraries
  TODO: Modifier le makefile pour prendre en compte la priorité de chemin de bib? 
 */
#include <src/LiquidCrystal.h>
#include <DS1307RTC.h>
#include <Audio.h>
#include <SD.h>
#include <IRremote.h>

#include <memory>
#include <vector>
#include <unistd.h>

#include "HWMenuRenderer.h"

#include "AClock.h"
#include "cortex.h"

using namespace std;

AudioPlaySdWav           playWav1;
AudioOutputI2S           audioOutput;
AudioConnection          patchCord1(playWav1, 0, audioOutput, 0);
AudioConnection          patchCord2(playWav1, 1, audioOutput, 1);
AudioControlSGTL5000     sgtl5000_1;

IRrecv irrecv(14);
LiquidCrystal lcd(1, 2, 3, 4, 5, 6);
float vol = 0.3f;

volatile int beeping = 0;
volatile int backlight = 1;

IntervalTimer print_time; // Affiche l'heure actuelle toute les secondes
IntervalTimer bloff; // Éteint le rétro-éclairage après 10 secondes d'inactivité

volatile int menu = 0;
Alarm * volatile next_alarm;
volatile int last_alarm_min = -1;

IFDBG(
	short activated[1 << 9];
	__attribute__((used, aligned(1024)))
	void (*VRAMHooks[NVIC_NUM_INTERRUPTS + 16])(void);
	volatile SystickCSR * const systick_csr = (volatile SystickCSR*)&SYST_CSR;
	volatile ICSR * const int_csr = (volatile ICSR*)&SCB_ICSR;
);

void arm_auto_off();
void compute_next_alarm();
void start_alarm();

HWMenuRenderer tmr(move(unique_ptr<vector<Entry>>(root_entries)), lcd);

volatile int lcd_dirty = 1;
void lcd_print_time() {
	if (!lcd_dirty)
		return;

	if (menu) // Le menu est ouvert, ne pas écrire par dessus!
		return;

	lcd_dirty = 0;
	RTC.read(tm);
	int d = dow(tm.Year + 1970, tm.Month, tm.Day);
	lcd.clear();
	lcd.setCursor(0, menu);
	lcd.printf("%02d / %02d / %04d", tm.Day, tm.Month, tm.Year + 1970);
	lcd.setCursor(0, 1);
	lcd.printf("%s %02d:%02d:%02d", days[d], tm.Hour, tm.Minute, tm.Second);
	lcd.setCursor(0, 0);
	DEBUG_LOG("%02d:%02d:%02d\n", tm.Hour, tm.Minute, tm.Second);
}

void check_alarms_and_trigger() {
	// La minute est passée, on peut relaisser les autres alarmes
	// se trigger
	if (last_alarm_min != tm.Minute)
		last_alarm_min = -1;

	if (next_alarm) {
		// Empêche de retrigger la même alarme
		if (last_alarm_min == next_alarm->minutes)
			return;
		int d = dow(tm.Year + 1970, tm.Month, tm.Day);
		// L'alarme n'est pas configurée pour aujourd'hui
		if (!next_alarm->days[d])
			return;
		if (next_alarm->hour == tm.Hour &&
			next_alarm->minutes == tm.Minute) {
			last_alarm_min = next_alarm->minutes;
			start_alarm();
			backlight = 1;
			// Allume le rétro-éclairage de manière inconditionnelle
			digitalWrite(9, 1);
			compute_next_alarm();
		}
	}
}

void start_alarm() {
	DEBUG_LOG("Loading music...\n");
	if (!SD.exists(next_alarm->music.c_str())) {
		DEBUG_LOG("Music %s not found\n", next_alarm->music.c_str());
	} else {
		DEBUG_LOG("Playing %s\n", next_alarm->music.c_str());
		playWav1.play(next_alarm->music.c_str());
	}
}

volatile int stop_pending = 0;
void buttonInt() {
	backlight ^= 1;
	digitalWrite(9, backlight);
	if (playWav1.isPlaying()) {
		stop_pending = 1;
	}
}

IFDBG(
	void debug_exception(void) {
		int excn = int_csr->vectactive;
		// Ignore
		if (excn != 121 && // FlexPWM
			excn != 16 &&  // DMA Ch0
			excn != 86 &&  // SOFTWARE
			excn != 129)   // USB1
			activated[excn] = 1;
		_VectorsRam[excn]();
	}
)

void debug_exception(void);

__attribute__((optimize("no-tree-loop-distribute-patterns")))
void setup() {
	// Me permet de tracer les exceptions
	IFDBG(
		for (int i = 0; i < NVIC_NUM_INTERRUPTS + 16; i++)
			VRAMHooks[i] = &debug_exception;
		SCB_VTOR = (long)VRAMHooks;
	);

	Serial.begin(9600);
	AudioMemory(15);
	DEBUG_LOG("Mem init done %d\n", activated[0]);

	irrecv.enableIRIn();
	lcd.begin(16, 2);
	pinMode(9, OUTPUT);
	digitalWrite(9, 1);
	pinMode(17, INPUT_PULLUP);
	DEBUG_LOG("Pin init done\n");

	DEBUG_LOG("enabling sgtl...\n");
	sgtl5000_1.enable();
	DEBUG_LOG("done, setting volume...\n");
	sgtl5000_1.volume(0.5f);
	DEBUG_LOG("done, setting external level...\n");
	sgtl5000_1.lineOutLevel(31, 31); // son externe

	SPI.setMOSI(11);
	SPI.setSCK(13);
	if (!SD.begin(10)) {
		while (1) {
			Serial.println("Unable to access the SD card");
			delay(500);
		}
	}

	if (!SD.exists(DEFAULT_MUSIC)) {
			lcd.clear();
			lcd.print("Default music");
			lcd.setCursor(0, 1);
			lcd.print("not found.");			

			while(1) {
				yield();
#ifdef __arm__
				__asm__ volatile ("wfi");
#endif
			}
	}

	DEBUG_LOG("Periph init done\n");
	auto cfg = SD.open("/clock.cfg");

	if (cfg) {
		DEBUG_LOG("Config found\n");
		size_t cnt;
		cfg.read(&cnt, sizeof(cnt));
		for (auto i = 0u; i < cnt; ++i) {
			Alarm a;
			cfg.read(a.days, sizeof(a.days));
			cfg.read(&a.hour, sizeof(int));
			cfg.read(&a.minutes, sizeof(int));
			size_t fnl;
			cfg.read(&fnl, sizeof(fnl));
			vector<char> tmp(fnl + 1);
			tmp[fnl] = 0;
			cfg.read(tmp.data(), fnl);
			a.music = tmp.data();
			alarms.push_back(a);
		}
	}

	compute_next_alarm();
	//playWav1.play("/jitter.wav");

	lcd_print_time();

	attachInterrupt(digitalPinToInterrupt(17), buttonInt, FALLING);

	if (!print_time.begin([](){
							  __disable_irq();
							  lcd_dirty = 1;
							  check_alarms_and_trigger();
							  __enable_irq();
						  }, 1000000))
						  DEBUG_LOG("Failed to set int1\n"); // TODO: Attacher au SQW pour peut-etre cacher le drift, plutot qu'utiliser un timer
	arm_auto_off();
}

int getkey(int code) {

	/* 
	   Le layout de la télécommande est de 3*7:

	   P + F
	   < . >
	   v - ^
	   0 = S
	   1 2 3
	   4 5 6
	   7 8 9

	   P = Power
	   F = Func
	   S = Set/Rept
	   = = Eq
	*/
	
	static char keys[] =
		"P+F"
		"<.>"
		"v-^"
		"0=S"
		"123"
		"456"
		"789";
	static uint16_t codes[] = { // Pour ma télécommande...
		0xA25D, 0x629D, 0xE21D,
		0x22DD, 0x02FD, 0xC23D,
		0xE01F, 0xA857, 0x906F,
		0x6897, 0x9867, 0xB04F,
		0x30CF, 0x18E7, 0x7A85,
		0x10EF, 0x38C7, 0x5AA5,
		0x42BD, 0x4AB5, 0x52AD
	};
	static int prev;
	if (code == 0xFFFFFF) // Ce code est envoyé pour indiquer un maintient de la touche précédente
		return prev;
	for (int i = 0; i < 21; ++i)
		if ((0xFF0000 | codes[i]) == code)
			return (prev = keys[i]);
	return -1; // code inconnu/interférences
}

void arm_auto_off() {
	bloff.end();
	if (!bloff.begin([](){
						 __disable_irq();
						 backlight ^= 1;
						 digitalWrite(9, backlight);
						 bloff.end();
						 __enable_irq();
					 }, 10000000)) {
		while(1)
			DEBUG_LOG("Failed to set int2\n");;
	}
}

void compute_next_alarm() {
	next_alarm = 0;
	if (!alarms.size())
		return;
	next_alarm = &alarms[0];
	int dotw = dow(tm.Year + 1970, tm.Month, tm.Day);
	// on cherche tout les réveils à commencer par ceux qui
	// doivent se déclencher aujourd'hui
	for (int j = 0; j < 7; ++j) {
		bool found_better = false;
		for (auto i = 1u; i < alarms.size(); ++i) {
			// cette alarme ne se déclenche pas le jour qui nous intéresse
			if (!alarms[i].days[dotw])
				continue;
			if (alarms[i].hour > tm.Hour) // l'heure est passée
				continue;
			// supériorité non-strict pour passer les alarmes réglées à la même heure exacte
			// pas de skip si la seule alarme réglée est à la même minute tho
			if (alarms[i].minutes >= tm.Minute) // la minute est passée
				continue;
			found_better = true;
			next_alarm = &alarms[i];
		}
		++dotw;
		dotw %= 7;
		if (found_better)
			break;
	}
}

// inutilisé, je  voulais juste voir ce  qui se passe en  cas d'out of
// memory
// et si ya une fuite mémoire dans le code du menu
int freeram() {
  char top;
  // partant  du principe  que la  stack  est en  haut de  la sram  et
  // décroit, et  que le tas est  en haut et croit  vers les addresses
  // hautes
  return &top - (char*)(sbrk(0));
}

// Empeche systick dans la portée où il est déclaré
struct inhibit_systick {
	inhibit_systick() {
		systick_csr->enable = 0;
		systick_csr->tickint = 0;
		// meme avec enable et ticking à 0, il semblerait
		// qu'une interruption soit toujours générée si CVR == 0...
		// ou bien c'est une question d'execution désordonnée
		SYST_CVR = 42;
	}

	~inhibit_systick() {
		systick_csr->enable = 1;
		systick_csr->tickint = 1;
		SYST_CVR = 0;
	}
};

void loop() {
	using namespace std;
	decode_results results;	
	
	// Un de mes tic avec le modèle d'éxecution d'arduino c'est que le
	// code de loop  est executé dans une boucle infinie  alors que le
	// code passe son temps à tester des conditions sur des évènements
	// ça  brule des  cycles pour  rien donc  je me  suis intéressé  à
	// désactiver les interuptions qui  réveillent le processeur alors
	// que rien ne se produit (systick).
	
	// J'ai pas besoin  de delay et de thread,  donc j'empêche systick
	// d'interrompre  la  veille.   Le  processeur sort  de  cet  état
	// seulement quand un  pin, notamment celui de  l'infrarouge ou du
	// bouton, recois  de l'information  ou qu'un timer  (utilisé pour
	// raffraichir  le LCD)  expire  En pratique  le  cpu reste  moins
	// longtemps que nécéssaire en sommeil car le fait d'être connecté
	// en  USB  fait  qu'il  recoit   des  interruptions  liées  à  la
	// communication série.   donc il  devrait encore  moins consommer
	// sur batterie, ou en désactivant l'USB
	{
		__disable_irq();
		inhibit_systick _1;
		IFDBG(activated[15] = 0);
		__enable_irq();
#ifdef __arm__
		__asm__ volatile ("wfi");
#endif
		IFDBG(if (activated[15]) {
				// si l'inhibiteur marche bien, on devrait jamais arriver ici
				activated[15] = 0;
				Serial.printf("Systick interrupted\n");
			});
	}

//	if (lcd_dirty)
//		check_alarms_and_trigger();

	IFDBG(
		for (int i = 0; i < (1 << 9); ++i)
			if (activated[i]) {
				Serial.printf("Exc %d was triggered\n", i);
				activated[i] = 0;
			});

	if (stop_pending) {
		DEBUG_LOG("Stopped playing music.");
		playWav1.stop();
		stop_pending = 0;
	}
	
	if (menu)
		tmr.print_menu();

	// TODO: Réecrire la bibliothèque IRRemote pour se passer de ça dans loop
	if (irrecv.decode(&results)) {

		// Il y a eu une intéraction, reallume le rétro
		// éclairage et réinitialise le timer d'auto-extinction à 10s
		backlight = 0;
		buttonInt();
		arm_auto_off(); //réarme le timer à 10s
		int k = getkey(results.value);
		DEBUG_LOG("Received %c\n", k);
		if (!menu && k == 'P')
			menu = 1;
		if (menu) {
			tmr.exec_input(k);
			if (tmr.exited) {
				menu = 0;
				tmr.exited = 0;
				lcd_print_time();
				compute_next_alarm();
			}
		}
		irrecv.resume();
	}

	lcd_print_time();
}

// TODO: Bouger ça dans le linker script?
unsigned __exidx_start;
unsigned __exidx_end;

