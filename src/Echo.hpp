#pragma once

#include <Geode/Geode.hpp>
#include <filesystem>
#include <string>
#include <vector>

namespace echo {

enum class State { Idle = 0, Recording = 1, Playing = 2 };

struct Input {
	unsigned frame = 0;
	bool down = false;
	bool player2 = false;
	int button = 1; // 1 = jump, 2 = left, 3 = right (platformer)
};

class Bot {
public:
	static Bot& get();

	// ---- macro ----
	State state = State::Idle;
	std::vector<Input> inputs;
	size_t replayPos = 0;
	bool injecting = false;
	bool pendingRestart = false;
	float tps = 240.f; // macro "fps" (physics steps per second)

	// ---- settings (persisted) ----
	float speed = 1.f;
	bool frameAdvance = false;
	bool stepRequested = false;
	bool showHitboxes = false;
	bool showLabel = true;
	bool autoclicker = false;
	int autoHold = 2;
	int autoRelease = 2;
	bool clickbot = false;
	float clickVolume = 1.f;

	bool autoDown = false;

	void loadSettings();
	void saveSettings();

	// ---- state control ----
	void setState(State s);
	void applyPendingRestart();
	void applySpeed();
	void resetSpeed();

	// ---- gameplay hooks ----
	void beforeStep(GJBaseGameLayer* layer);
	void onButton(GJBaseGameLayer* layer, bool down, int button, bool player2);
	void onReset(GJBaseGameLayer* layer);
	void onLevelEnter();

	// ---- macro files ----
	std::filesystem::path macroDir();
	std::filesystem::path exportDir();
	std::filesystem::path clickDir();
	std::vector<std::filesystem::path> listMacros();
	std::string saveMacro();
	bool loadMacro(std::filesystem::path const& path, std::string& error);
	std::string exportPlainText();

	// ---- clickbot ----
	void scanClicks();
	void playClick(bool down);

	static char const* stateName(State s);

private:
	void record(unsigned frame, bool down, int button, bool player2);
	void truncateFrom(unsigned frame);
	bool loadJson(std::string const& text, std::string& error);
	bool loadBinary(std::filesystem::path const& path, std::string& error);
	bool loadPlain(std::string const& text, std::string& error);
	void finishLoad(std::vector<Input> in, double fileFps);

	std::vector<std::string> m_clicks;
	std::vector<std::string> m_releases;
};

inline bool isPlayLayer(GJBaseGameLayer* layer) {
	auto pl = PlayLayer::get();
	return pl && static_cast<GJBaseGameLayer*>(pl) == layer;
}

} // namespace echo
