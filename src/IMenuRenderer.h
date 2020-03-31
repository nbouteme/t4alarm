#ifndef IMENURENDERER_H
#define IMENURENDERER_H

#include <vector>
#include <memory>
#include <functional>

struct IMenuRenderer;

struct Entry {
	std::string disp;
	std::function<void(IMenuRenderer*, Entry *self)> act;
	std::function<void(IMenuRenderer*, Entry *self, char)> handle_input;
	std::function<void(IMenuRenderer*, Entry *self)> draw;
	Entry(const std::string &a,
		  std::function<void(IMenuRenderer*, Entry*)> b,
		  std::function<void(IMenuRenderer*, Entry*, char)> c = std::function<void(IMenuRenderer*, Entry*, char)>(),
		  std::function<void(IMenuRenderer*, Entry*)> d = std::function<void(IMenuRenderer*, Entry*)>()
		) : disp(a),
			act(b),
			handle_input(c),
			draw(d)
		{}
};

struct IMenuRenderer {
	int select = 0;
	int hlsr = 0;
	std::vector<std::unique_ptr<std::vector<Entry>>> history;
	std::vector<std::pair<int, int>> state_history;
	std::unique_ptr<std::vector<Entry>> entries;
	bool exited = false;

	IMenuRenderer(std::unique_ptr<std::vector<Entry>> &&_entries) :
		entries(std::move(_entries)) {
	}

	virtual void navigate(std::unique_ptr<std::vector<Entry>> &&_entries) {
		history.emplace_back(std::move(entries));
		state_history.push_back(std::make_pair(select, hlsr));
		entries = std::move(_entries);
		select = 0;
		hlsr = 0;
	}

	virtual void back() {
		if (history.size() == 0) {
			exited = true;
			return;
		}
		entries = std::move(history.back());
		history.pop_back();
		auto &p = state_history.back();
		select = p.first;
		hlsr = p.second;
		state_history.pop_back();
	};

	bool locked = false;
	virtual void lock_input() { locked = true; }
	virtual void unlock_input()  { locked = false; }

	int dirty = 0;
	void exec_input(int c) {
		auto &ent = *entries;
		dirty = 1;
		if (locked) {
			return ent[select].handle_input(this, &ent[select], c);
		}
		switch(c) {
		case '2':
		case '^':
		case '+':
			dmove(-1);
			break;
		case '-':
		case 'v':
		case '8':
			dmove(1);
			break;
		case '<':
		case '4':
			back();
			break;
		case '>':
		case '.':
		case '5':
		case '6':
			ent[select].act(this, &ent[select]);
			break;
		}
	}

	void dmove(int dir) {
		auto &ent = *entries;
		hlsr = dir > 0;
		select += dir;
		if (select < 0 || select >= (long)ent.size()) {
			select += ent.size();
			select %= ent.size();
			hlsr = !hlsr;
		}
	}

	virtual void cursor() = 0;
	virtual void noBlink() = 0;
	virtual void blink() = 0;
	virtual void print(const char *) = 0;
	virtual void setCursor(int, int) = 0;
};

#endif /* IMENURENDERER_H */