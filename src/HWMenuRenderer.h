#ifndef HWMENURENDERER_H
#define HWMENURENDERER_H

#include "IMenuRenderer.h"
#include <src/LiquidCrystal.h>

struct HWMenuRenderer : IMenuRenderer {
	LiquidCrystal &lcd;
	
	HWMenuRenderer(std::unique_ptr<std::vector<Entry>> &&_entries, LiquidCrystal &_lcd) :
		IMenuRenderer(std::move(_entries)), lcd(_lcd) {
	}

	virtual ~HWMenuRenderer() {
	}

	int x, y;
	void print_menu() {
		if (!dirty)
			return;
		dirty = 0;
		auto &ent = *entries;
		auto p = select - 1 < 0 ? ent[ent.size() - 1].disp : ent[select - 1].disp;
		auto n = select + 1 >=  (long)ent.size() ? ent[0].disp : ent[select + 1].disp;
		auto c = ent[select].disp;
		clear();
		if (locked && ent[select].draw)
			return ent[select].draw(this, &ent[select]);
		if (p == c)
			p = "";
		if (n == c)
			n = "";
		int sx = x, sy = y;
		if (hlsr) {
			setCursor(0, 0);
			print(std::string(" ") + p.c_str());
			setCursor(0, 1);
			print(std::string(">") + c.c_str());
			setCursor(0, 1);
		} else {
			setCursor(0, 0);
			print(std::string(">") + c.c_str());
			setCursor(0, 1);
			print(std::string(" ") + n.c_str());
			setCursor(0, 0);
		}
		if (locked)
			setCursor(sx, sy);
	}

	virtual void setCursor(int x, int y) {
		this->x = x;
		this->y = y;
		lcd.setCursor(x, y);
	}

	virtual void cursor() {
		lcd.cursor();
	}

	virtual void blink() {
		lcd.blink();
	}

	virtual void noBlink() {
		lcd.noBlink();
	}

	virtual void noCursor() {
		lcd.noCursor();
	}

	virtual void clear() {
		lcd.clear();
	}

	virtual void print(const std::string &c) {
		print(c.c_str());
	}

	virtual void print(const char *c) {
		lcd.print(c);
	}
};

#endif /* HWMENURENDERER_H */