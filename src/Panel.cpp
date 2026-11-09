#include "Panel.hpp"

#include "Echo.hpp"

#include <cstdio>
#include <cmath>

using namespace geode::prelude;
namespace fs = std::filesystem;

namespace echo {

static Panel* s_instance = nullptr;

static constexpr float kHeader = 38.f;
static constexpr float kTabH = 24.f;
static constexpr float kRowH = 30.f;
static constexpr float kPad = 10.f;

static std::string fmtNum(char const* f, double v) {
	char buf[64];
	std::snprintf(buf, sizeof(buf), f, v);
	return buf;
}

// ------------------------------------------------------------------ static API

bool Panel::isOpen() {
	return s_instance != nullptr;
}

void Panel::close() {
	if (!s_instance) return;
	auto p = s_instance;
	s_instance = nullptr;
	p->removeFromParentAndCleanup(true);
}

void Panel::toggle() {
	if (s_instance) {
		close();
		return;
	}
	auto scene = CCDirector::get()->getRunningScene();
	if (!scene) return;

	auto p = new Panel();
	if (p && p->init()) {
		p->autorelease();
		s_instance = p;
		scene->addChild(p, 100000);
	} else {
		delete p;
	}
}

// ------------------------------------------------------------------ setup

bool Panel::init() {
	if (!CCLayer::init()) return false;

	m_size = {390.f, 284.f};
	this->setContentSize(m_size);
	auto win = CCDirector::get()->getWinSize();
	this->setPosition({(win.width - m_size.width) / 2.f, (win.height - m_size.height) / 2.f});

	this->setTouchMode(kCCTouchesOneByOne);
	this->setTouchPriority(-1000);
	this->setTouchEnabled(true);
	this->scheduleUpdate();

	m_files = Bot::get().listMacros();

	buildFrame();
	buildTab(0);
	return true;
}

void Panel::update(float) {
	// the menu only lives while the pause menu is open
	auto scene = CCDirector::get()->getRunningScene();
	if (!scene || !scene->getChildByType<PauseLayer>(0)) {
		Panel::close();
	}
}

void Panel::buildFrame() {
	auto bg = CCScale9Sprite::create("GJ_square02.png");
	bg->setContentSize(m_size);
	bg->setAnchorPoint({0.f, 0.f});
	bg->setOpacity(245);
	this->addChild(bg, -2);

	auto title = CCLabelBMFont::create("Echo", "goldFont.fnt");
	title->setScale(0.7f);
	title->setPosition({m_size.width / 2.f, m_size.height - 20.f});
	this->addChild(title);

	auto x = CCLabelBMFont::create("X", "bigFont.fnt");
	x->setScale(0.6f);
	x->setPosition({m_size.width - 20.f, m_size.height - 20.f});
	this->addChild(x);
	m_closeRect = CCRect(m_size.width - 42.f, m_size.height - 40.f, 40.f, 40.f);

	char const* names[3] = {"Macro", "Hacks", "Bot"};
	float tabW = (m_size.width - kPad * 2.f) / 3.f;
	for (int i = 0; i < 3; ++i) {
		float x0 = kPad + tabW * i;
		float y0 = m_size.height - kHeader - kTabH;
		auto bgc = CCLayerColor::create({0, 0, 0, 90}, tabW - 4.f, kTabH);
		bgc->setPosition({x0, y0});
		this->addChild(bgc, -1);
		m_tabBgs.push_back(bgc);

		auto lbl = CCLabelBMFont::create(names[i], "bigFont.fnt");
		lbl->setScale(0.45f);
		lbl->setPosition({x0 + (tabW - 4.f) / 2.f, y0 + kTabH / 2.f});
		this->addChild(lbl);

		m_tabHits.push_back({CCRect(x0, y0, tabW - 4.f, kTabH), [this, i] { buildTab(i); }});
	}

	m_content = CCNode::create();
	this->addChild(m_content);
}

void Panel::tell(std::string const& text) {
	Notification::create(text, NotificationIcon::Info, 1.5f)->show();
}

// ------------------------------------------------------------------ rows

CCPoint Panel::rowPos(int row) const {
	float top = m_size.height - kHeader - kTabH - 6.f;
	return {kPad, top - kRowH * (row + 1)};
}

void Panel::addButtonRow(int row, std::string const& text, std::function<void()> fn) {
	auto p = rowPos(row);
	float w = m_size.width - kPad * 2.f;

	auto bg = CCLayerColor::create({40, 140, 60, 130}, w, kRowH - 4.f);
	bg->setPosition(p);
	m_content->addChild(bg);

	auto lbl = CCLabelBMFont::create(text.c_str(), "bigFont.fnt");
	lbl->setPosition({p.x + w / 2.f, p.y + (kRowH - 4.f) / 2.f});
	lbl->limitLabelWidth(w - 20.f, 0.5f, 0.2f);
	m_content->addChild(lbl);

	m_hits.push_back({CCRect(p.x, p.y, w, kRowH - 4.f), std::move(fn)});
}

void Panel::addToggleRow(int row, std::string const& text, std::function<bool()> get, std::function<void(bool)> set) {
	auto p = rowPos(row);
	float w = m_size.width - kPad * 2.f;

	auto bg = CCLayerColor::create({0, 0, 0, 90}, w, kRowH - 4.f);
	bg->setPosition(p);
	m_content->addChild(bg);

	auto lbl = CCLabelBMFont::create(text.c_str(), "bigFont.fnt");
	lbl->setAnchorPoint({0.f, 0.5f});
	lbl->setPosition({p.x + 8.f, p.y + (kRowH - 4.f) / 2.f});
	lbl->limitLabelWidth(w - 90.f, 0.5f, 0.2f);
	m_content->addChild(lbl);

	auto val = CCLabelBMFont::create("OFF", "bigFont.fnt");
	val->setAnchorPoint({1.f, 0.5f});
	val->setPosition({p.x + w - 10.f, p.y + (kRowH - 4.f) / 2.f});
	val->setScale(0.5f);
	m_content->addChild(val);

	m_refresh.push_back([val, get] {
		bool on = get();
		val->setString(on ? "ON" : "OFF");
		val->setColor(on ? ccColor3B{90, 255, 90} : ccColor3B{255, 90, 90});
	});
	m_hits.push_back({CCRect(p.x, p.y, w, kRowH - 4.f), [get, set] { set(!get()); }});
}

void Panel::addStepperRow(
	int row, std::string const& text, std::function<std::string()> value, std::function<void(int)> step
) {
	auto p = rowPos(row);
	float w = m_size.width - kPad * 2.f;
	float h = kRowH - 4.f;

	auto bg = CCLayerColor::create({0, 0, 0, 90}, w, h);
	bg->setPosition(p);
	m_content->addChild(bg);

	auto lbl = CCLabelBMFont::create(text.c_str(), "bigFont.fnt");
	lbl->setAnchorPoint({0.f, 0.5f});
	lbl->setPosition({p.x + 8.f, p.y + h / 2.f});
	lbl->limitLabelWidth(w - 190.f, 0.5f, 0.2f);
	m_content->addChild(lbl);

	float cx = p.x + w - 78.f;
	auto minus = CCLabelBMFont::create("<", "bigFont.fnt");
	minus->setScale(0.6f);
	minus->setPosition({cx - 58.f, p.y + h / 2.f});
	m_content->addChild(minus);
	auto plus = CCLabelBMFont::create(">", "bigFont.fnt");
	plus->setScale(0.6f);
	plus->setPosition({cx + 58.f, p.y + h / 2.f});
	m_content->addChild(plus);

	auto val = CCLabelBMFont::create("", "bigFont.fnt");
	val->setPosition({cx, p.y + h / 2.f});
	m_content->addChild(val);

	m_refresh.push_back([val, value] {
		val->setString(value().c_str());
		val->limitLabelWidth(88.f, 0.5f, 0.15f);
	});
	m_hits.push_back({CCRect(cx - 80.f, p.y, 40.f, h), [step] { step(-1); }});
	m_hits.push_back({CCRect(cx + 40.f, p.y, 40.f, h), [step] { step(1); }});
}

void Panel::addInfoRow(int row, std::function<std::string()> text) {
	auto p = rowPos(row);
	float w = m_size.width - kPad * 2.f;

	auto lbl = CCLabelBMFont::create("", "chatFont.fnt");
	lbl->setAnchorPoint({0.f, 0.5f});
	lbl->setPosition({p.x + 8.f, p.y + (kRowH - 4.f) / 2.f});
	m_content->addChild(lbl);

	m_refresh.push_back([lbl, text, w] {
		lbl->setString(text().c_str());
		lbl->limitLabelWidth(w - 16.f, 0.7f, 0.3f);
	});
}

// ------------------------------------------------------------------ tabs

void Panel::buildTab(int tab) {
	m_tab = tab;
	m_content->removeAllChildren();
	m_hits.clear();
	m_refresh.clear();

	for (int i = 0; i < 3; ++i) {
		m_tabBgs[i]->setColor(i == tab ? ccColor3B{60, 160, 80} : ccColor3B{0, 0, 0});
		m_tabBgs[i]->setOpacity(i == tab ? 200 : 90);
	}

	auto& bot = Bot::get();

	if (tab == 0) {
		// ---- Macro ----
		m_files = bot.listMacros();
		if (m_fileIdx >= static_cast<int>(m_files.size())) m_fileIdx = 0;

		addStepperRow(
			0, "Mode", [] { return std::string(Bot::stateName(Bot::get().state)); },
			[this](int d) {
				int s = (static_cast<int>(Bot::get().state) + d + 3) % 3;
				Bot::get().setState(static_cast<State>(s));
				if (s != 0) this->tell("Resume the game: the level restarts");
			}
		);
		addInfoRow(1, [] {
			auto& b = Bot::get();
			unsigned last = b.inputs.empty() ? 0 : b.inputs.back().frame;
			return fmtNum("%.0f", (double)b.inputs.size()) + " inputs, last frame " + fmtNum("%.0f", (double)last);
		});
		addButtonRow(2, "Save Macro", [this] {
			if (Bot::get().inputs.empty()) return this->tell("Macro is empty");
			auto name = Bot::get().saveMacro();
			if (name.empty()) return this->tell("Could not save macro");
			m_files = Bot::get().listMacros();
			this->tell("Saved " + name);
		});
		addStepperRow(
			3, "File",
			[this] {
				if (m_files.empty()) return std::string("none");
				return m_files[m_fileIdx].filename().string();
			},
			[this](int d) {
				if (m_files.empty()) return;
				int n = static_cast<int>(m_files.size());
				m_fileIdx = (m_fileIdx + d + n) % n;
			}
		);
		addButtonRow(4, "Load Selected", [this] {
			if (m_files.empty()) return this->tell("No macro files");
			std::string err;
			if (Bot::get().loadMacro(m_files[m_fileIdx], err)) {
				this->tell("Loaded " + std::to_string(Bot::get().inputs.size()) + " inputs");
			} else {
				this->tell(err);
			}
		});
		addButtonRow(5, "Export Plain Text", [this] {
			auto name = Bot::get().exportPlainText();
			this->tell(name.empty() ? "Could not export" : "Exported " + name + ".txt");
		});
		addButtonRow(6, "Clear Macro", [this] {
			Bot::get().inputs.clear();
			Bot::get().replayPos = 0;
			this->tell("Macro cleared");
		});
	} else if (tab == 1) {
		// ---- Hacks ----
		addStepperRow(
			0, "Speed", [] { return fmtNum("%.2fx", Bot::get().speed); },
			[](int d) {
				auto& b = Bot::get();
				b.speed = std::clamp(std::round((b.speed + d * 0.05f) * 100.f) / 100.f, 0.05f, 10.f);
				b.applySpeed();
				b.saveSettings();
			}
		);
		addToggleRow(
			1, "Frame Advance", [] { return Bot::get().frameAdvance; },
			[](bool v) {
				Bot::get().frameAdvance = v;
				Bot::get().stepRequested = false;
			}
		);
		addToggleRow(
			2, "Show Hitboxes", [] { return Bot::get().showHitboxes; },
			[](bool v) {
				Bot::get().showHitboxes = v;
				Bot::get().saveSettings();
			}
		);
		addToggleRow(
			3, "Info Label", [] { return Bot::get().showLabel; },
			[](bool v) {
				Bot::get().showLabel = v;
				Bot::get().saveSettings();
			}
		);
		addInfoRow(4, [] { return std::string("Frame Advance shows a draggable Step button in the level."); });
	} else {
		// ---- Bot ----
		addToggleRow(
			0, "Autoclicker", [] { return Bot::get().autoclicker; },
			[](bool v) { Bot::get().autoclicker = v; }
		);
		addStepperRow(
			1, "Hold (steps)", [] { return std::to_string(Bot::get().autoHold); },
			[](int d) {
				auto& b = Bot::get();
				b.autoHold = std::clamp(b.autoHold + d, 1, 240);
				b.saveSettings();
			}
		);
		addStepperRow(
			2, "Release (steps)", [] { return std::to_string(Bot::get().autoRelease); },
			[](int d) {
				auto& b = Bot::get();
				b.autoRelease = std::clamp(b.autoRelease + d, 1, 240);
				b.saveSettings();
			}
		);
		addToggleRow(
			3, "Clickbot", [] { return Bot::get().clickbot; },
			[](bool v) {
				auto& b = Bot::get();
				b.clickbot = v;
				if (v) b.scanClicks();
				b.saveSettings();
			}
		);
		addStepperRow(
			4, "Click Volume", [] { return fmtNum("%.0f%%", Bot::get().clickVolume * 100.0); },
			[](int d) {
				auto& b = Bot::get();
				b.clickVolume = std::clamp(std::round((b.clickVolume + d * 0.1f) * 10.f) / 10.f, 0.f, 2.f);
				b.saveSettings();
			}
		);
		addInfoRow(5, [] { return std::string("Put click*.ogg / release*.ogg in the mod's 'clicks' folder."); });
	}

	refresh();
}

void Panel::refresh() {
	for (auto& f : m_refresh) f();
}

// ------------------------------------------------------------------ touches

bool Panel::ccTouchBegan(CCTouch* touch, CCEvent*) {
	auto p = this->convertToNodeSpace(touch->getLocation());
	if (!CCRect(0.f, 0.f, m_size.width, m_size.height).containsPoint(p)) return false;

	m_pressed = -1;
	m_pressedTab = false;
	m_pressedClose = false;

	if (m_closeRect.containsPoint(p)) {
		m_pressedClose = true;
		return true;
	}
	for (size_t i = 0; i < m_tabHits.size(); ++i) {
		if (m_tabHits[i].rect.containsPoint(p)) {
			m_pressed = static_cast<int>(i);
			m_pressedTab = true;
			return true;
		}
	}
	for (size_t i = 0; i < m_hits.size(); ++i) {
		if (m_hits[i].rect.containsPoint(p)) {
			m_pressed = static_cast<int>(i);
			break;
		}
	}
	return true; // swallow touches on the panel
}

void Panel::ccTouchEnded(CCTouch* touch, CCEvent*) {
	auto p = this->convertToNodeSpace(touch->getLocation());

	if (m_pressedClose) {
		if (m_closeRect.containsPoint(p)) Panel::close();
		return;
	}
	if (m_pressed < 0) return;

	if (m_pressedTab) {
		auto hit = m_tabHits[m_pressed];
		if (hit.rect.containsPoint(p)) hit.fn(); // rebuilds the tab
		return;
	}

	if (m_pressed < static_cast<int>(m_hits.size())) {
		auto hit = m_hits[m_pressed];
		if (hit.rect.containsPoint(p)) {
			hit.fn();
			refresh();
		}
	}
}

void Panel::ccTouchCancelled(CCTouch*, CCEvent*) {
	m_pressed = -1;
}

} // namespace echo
