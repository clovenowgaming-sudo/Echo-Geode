#include "Echo.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <random>
#include <sstream>

using namespace geode::prelude;
namespace fs = std::filesystem;

namespace echo {

Bot& Bot::get() {
	static Bot bot;
	return bot;
}

char const* Bot::stateName(State s) {
	switch (s) {
		case State::Recording: return "Record";
		case State::Playing: return "Play";
		default: return "Idle";
	}
}

// ------------------------------------------------------------------ settings

void Bot::loadSettings() {
	auto m = Mod::get();
	speed = m->getSavedValue<float>("speed", 1.f);
	frameAdvance = false; // never start a session frozen
	showHitboxes = m->getSavedValue<bool>("show-hitboxes", false);
	showLabel = m->getSavedValue<bool>("show-label", true);
	autoclicker = false;
	autoHold = m->getSavedValue<int>("auto-hold", 2);
	autoRelease = m->getSavedValue<int>("auto-release", 2);
	clickbot = m->getSavedValue<bool>("clickbot", false);
	clickVolume = m->getSavedValue<float>("click-volume", 1.f);
}

void Bot::saveSettings() {
	auto m = Mod::get();
	m->setSavedValue<float>("speed", speed);
	m->setSavedValue<bool>("show-hitboxes", showHitboxes);
	m->setSavedValue<bool>("show-label", showLabel);
	m->setSavedValue<int>("auto-hold", autoHold);
	m->setSavedValue<int>("auto-release", autoRelease);
	m->setSavedValue<bool>("clickbot", clickbot);
	m->setSavedValue<float>("click-volume", clickVolume);
}

// ------------------------------------------------------------------ state

void Bot::setState(State s) {
	state = s;
	replayPos = 0;
	autoDown = false;
	if (s == State::Recording) inputs.clear();
	// Recording / playback must begin on frame 0, so the level is restarted
	// as soon as the pause menu is closed.
	pendingRestart = (s != State::Idle);
}

void Bot::applyPendingRestart() {
	if (!pendingRestart) return;
	pendingRestart = false;
	if (auto pl = PlayLayer::get()) pl->resetLevelFromStart();
}

void Bot::applySpeed() {
	CCDirector::get()->getScheduler()->setTimeScale(std::clamp(speed, 0.05f, 10.f));
}

void Bot::resetSpeed() {
	CCDirector::get()->getScheduler()->setTimeScale(1.f);
}

void Bot::onLevelEnter() {
	applySpeed();
	autoDown = false;
	replayPos = 0;
	if (clickbot) scanClicks();
}

// ------------------------------------------------------------------ gameplay

void Bot::record(unsigned frame, bool down, int button, bool player2) {
	for (auto it = inputs.rbegin(); it != inputs.rend(); ++it) {
		if (it->player2 == player2 && it->button == button) {
			if (it->down == down) return; // no state change
			break;
		}
	}
	Input in;
	in.frame = frame;
	in.down = down;
	in.player2 = player2;
	in.button = button;
	inputs.push_back(in);
}

void Bot::truncateFrom(unsigned frame) {
	inputs.erase(
		std::remove_if(inputs.begin(), inputs.end(), [frame](Input const& i) { return i.frame >= frame; }),
		inputs.end()
	);
	// close any hold that is still open so the macro stays consistent
	for (int p = 0; p < 2; ++p) {
		for (int b = 1; b <= 3; ++b) {
			bool held = false;
			for (auto const& i : inputs) {
				if (i.player2 == (p == 1) && i.button == b) held = i.down;
			}
			if (held) {
				Input in;
				in.frame = frame;
				in.down = false;
				in.player2 = (p == 1);
				in.button = b;
				inputs.push_back(in);
			}
		}
	}
}

void Bot::onButton(GJBaseGameLayer* layer, bool down, int button, bool player2) {
	if (state == State::Recording) {
		record(layer->m_gameState.m_currentProgress, down, button, player2);
	}
	playClick(down);
}

void Bot::beforeStep(GJBaseGameLayer* layer) {
	unsigned frame = layer->m_gameState.m_currentProgress;

	if (state == State::Playing) {
		while (replayPos < inputs.size() && inputs[replayPos].frame <= frame) {
			auto const in = inputs[replayPos++];
			injecting = true;
			layer->handleButton(in.down, in.button, !in.player2);
			injecting = false;
			playClick(in.down);
		}
		return;
	}

	if (autoclicker) {
		unsigned hold = static_cast<unsigned>(std::max(1, autoHold));
		unsigned rel = static_cast<unsigned>(std::max(1, autoRelease));
		bool want = (frame % (hold + rel)) < hold;
		if (want != autoDown) {
			autoDown = want;
			layer->handleButton(want, 1, true); // goes through the recorder if recording
		}
	} else if (autoDown) {
		autoDown = false;
		layer->handleButton(false, 1, true);
	}
}

void Bot::onReset(GJBaseGameLayer* layer) {
	unsigned frame = layer->m_gameState.m_currentProgress;
	autoDown = false;

	if (state == State::Recording) {
		truncateFrom(frame);
	} else if (state == State::Playing) {
		replayPos = std::lower_bound(
			inputs.begin(), inputs.end(), frame,
			[](Input const& i, unsigned f) { return i.frame < f; }
		) - inputs.begin();
	}
}

// ------------------------------------------------------------------ files

fs::path Bot::macroDir() {
	auto p = Mod::get()->getSaveDir() / "macros";
	std::error_code ec;
	fs::create_directories(p, ec);
	return p;
}

fs::path Bot::exportDir() {
	auto p = Mod::get()->getSaveDir() / "exported";
	std::error_code ec;
	fs::create_directories(p, ec);
	return p;
}

fs::path Bot::clickDir() {
	auto p = Mod::get()->getSaveDir() / "clicks";
	std::error_code ec;
	fs::create_directories(p, ec);
	return p;
}

std::vector<fs::path> Bot::listMacros() {
	std::vector<fs::path> out;
	std::error_code ec;
	for (auto const& e : fs::directory_iterator(macroDir(), ec)) {
		if (!e.is_regular_file()) continue;
		auto name = e.path().filename().string();
		if (name.ends_with(".echo.json") || name.ends_with(".echo") || name.ends_with(".txt")) {
			out.push_back(e.path());
		}
	}
	std::sort(out.begin(), out.end());
	return out;
}

static std::string sanitize(std::string s) {
	for (auto& c : s) {
		if (!std::isalnum(static_cast<unsigned char>(c)) && c != '-' && c != '_') c = '_';
	}
	if (s.empty()) s = "macro";
	return s;
}

std::string Bot::saveMacro() {
	std::string base = "macro";
	if (auto pl = PlayLayer::get()) {
		if (pl->m_level) {
			base = sanitize(std::string(pl->m_level->m_levelName)) + "_" + std::to_string(pl->m_level->m_levelID.value());
		}
	}

	auto dir = macroDir();
	std::string name;
	fs::path path;
	for (int i = 1; i < 10000; ++i) {
		name = base + "_" + std::to_string(i);
		path = dir / (name + ".echo.json");
		if (!fs::exists(path)) break;
	}

	std::ofstream f(path, std::ios::binary);
	if (!f.is_open()) return "";

	// Same layout as the original Echo ".echo.json" macros (+ "button" for platformer)
	f << "{\n    \"version\": \"SIMPLE\",\n    \"fps\": " << tps << ",\n    \"end_xpos\": 0,\n    \"inputs\": [\n";
	for (size_t i = 0; i < inputs.size(); ++i) {
		auto const& in = inputs[i];
		f << "        {\"frame\": " << in.frame << ", \"holding\": " << (in.down ? "true" : "false");
		if (in.player2) f << ", \"player_2\": true";
		if (in.button != 1) f << ", \"button\": " << in.button;
		f << "}" << (i + 1 < inputs.size() ? "," : "") << "\n";
	}
	f << "    ]\n}\n";
	f.close();
	return name;
}

std::string Bot::exportPlainText() {
	auto dir = exportDir();
	std::string name;
	fs::path path;
	for (int i = 1; i < 10000; ++i) {
		name = "macro_" + std::to_string(i);
		path = dir / (name + ".txt");
		if (!fs::exists(path)) break;
	}
	std::ofstream f(path);
	if (!f.is_open()) return "";
	f << tps << "\n";
	for (auto const& in : inputs) {
		f << in.frame << " " << (in.down ? 1 : 0) << " " << (in.player2 ? 1 : 0) << "\n";
	}
	return name;
}

void Bot::finishLoad(std::vector<Input> in, double fileFps) {
	double scale = (fileFps > 0.0) ? (tps / fileFps) : 1.0;
	if (std::abs(scale - 1.0) > 1e-6) {
		for (auto& i : in) i.frame = static_cast<unsigned>(std::llround(i.frame * scale));
	}
	std::stable_sort(in.begin(), in.end(), [](Input const& a, Input const& b) { return a.frame < b.frame; });
	inputs = std::move(in);
	replayPos = 0;
}

bool Bot::loadJson(std::string const& text, std::string& error) {
	auto parsed = matjson::parse(text);
	if (parsed.isErr()) {
		error = "Invalid JSON";
		return false;
	}
	auto root = parsed.unwrap();

	double fileFps = root["fps"].asDouble().unwrapOr(0.0);
	auto arr = root["inputs"].asArray();
	if (arr.isErr()) {
		error = "No inputs found";
		return false;
	}

	std::vector<Input> in;
	for (auto& v : arr.unwrap()) {
		Input i;
		i.frame = static_cast<unsigned>(std::max<int64_t>(0, v["frame"].asInt().unwrapOr(0)));
		i.down = v["holding"].asBool().unwrapOr(false);
		i.player2 = v["player_2"].asBool().unwrapOr(false);
		i.button = static_cast<int>(v["button"].asInt().unwrapOr(1));
		in.push_back(i);
	}
	finishLoad(std::move(in), fileFps);
	return true;
}

bool Bot::loadBinary(fs::path const& path, std::string& error) {
	std::ifstream f(path, std::ios::binary);
	if (!f.is_open()) {
		error = "Cannot open file";
		return false;
	}

	char header[24] = {};
	f.read(header, sizeof(header));
	std::string fmt;
	for (size_t i = 0; i < 16 && std::isalpha(static_cast<unsigned char>(header[i])); i++) fmt += header[i];

	bool debug = false, meta = false;
	if (fmt == "DEBUG") debug = true;
	else if (fmt == "META") meta = true;
	else if (fmt == "METADBG") { debug = true; meta = true; }
	else if (fmt != "SIMPLE") {
		f.clear();
		f.seekg(0);
	}

	float fps = 0.f, endX = 0.f;
	f.read(reinterpret_cast<char*>(&fps), sizeof(fps));
	f.read(reinterpret_cast<char*>(&endX), sizeof(endX));
	if (meta) {
		double secs = 0;
		int clicks = 0, attempts = 0;
		f.read(reinterpret_cast<char*>(&secs), sizeof(secs));
		f.read(reinterpret_cast<char*>(&clicks), sizeof(clicks));
		f.read(reinterpret_cast<char*>(&attempts), sizeof(attempts));
	}
	if (!f || !(fps > 0.f)) {
		error = "Invalid .echo file";
		return false;
	}

	std::vector<Input> in;
	while (true) {
		int number = 0;
		bool down = false, p2 = false;
		f.read(reinterpret_cast<char*>(&number), sizeof(number));
		f.read(reinterpret_cast<char*>(&down), sizeof(down));
		f.read(reinterpret_cast<char*>(&p2), sizeof(p2));
		if (debug) {
			float xPos, rot, yPos;
			double yVel, xVel;
			f.read(reinterpret_cast<char*>(&xPos), sizeof(xPos));
			f.read(reinterpret_cast<char*>(&yVel), sizeof(yVel));
			f.read(reinterpret_cast<char*>(&xVel), sizeof(xVel));
			f.read(reinterpret_cast<char*>(&yPos), sizeof(yPos));
			f.read(reinterpret_cast<char*>(&rot), sizeof(rot));
		}
		if (!f) break;
		Input i;
		i.frame = static_cast<unsigned>(std::max(0, number));
		i.down = down;
		i.player2 = p2;
		in.push_back(i);
	}
	finishLoad(std::move(in), fps);
	return true;
}

bool Bot::loadPlain(std::string const& text, std::string& error) {
	std::istringstream s(text);
	double fps = 0;
	if (!(s >> fps) || !(fps > 0)) {
		error = "Invalid plain text macro";
		return false;
	}
	std::vector<Input> in;
	long long frame;
	int down, p2;
	while (s >> frame >> down >> p2) {
		Input i;
		i.frame = static_cast<unsigned>(std::max<long long>(0, frame));
		i.down = down != 0;
		i.player2 = p2 != 0;
		in.push_back(i);
	}
	finishLoad(std::move(in), fps);
	return true;
}

bool Bot::loadMacro(fs::path const& path, std::string& error) {
	auto name = path.filename().string();
	if (name.ends_with(".echo") && !name.ends_with(".echo.json")) {
		return loadBinary(path, error);
	}
	std::ifstream f(path, std::ios::binary);
	if (!f.is_open()) {
		error = "Cannot open file";
		return false;
	}
	std::stringstream ss;
	ss << f.rdbuf();
	if (name.ends_with(".txt")) return loadPlain(ss.str(), error);
	return loadJson(ss.str(), error);
}

// ------------------------------------------------------------------ clickbot

void Bot::scanClicks() {
	m_clicks.clear();
	m_releases.clear();
	std::error_code ec;
	for (auto const& e : fs::directory_iterator(clickDir(), ec)) {
		if (!e.is_regular_file()) continue;
		auto ext = e.path().extension().string();
		std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return std::tolower(c); });
		if (ext != ".ogg" && ext != ".mp3" && ext != ".wav") continue;
		auto stem = e.path().stem().string();
		std::transform(stem.begin(), stem.end(), stem.begin(), [](unsigned char c) { return std::tolower(c); });
		if (stem.rfind("click", 0) == 0) m_clicks.push_back(e.path().string());
		else if (stem.rfind("release", 0) == 0) m_releases.push_back(e.path().string());
	}
}

void Bot::playClick(bool down) {
	if (!clickbot) return;
	auto& list = down ? m_clicks : m_releases;
	if (list.empty()) return;
	static std::mt19937 rng{std::random_device{}()};
	auto& path = list[rng() % list.size()];
	FMODAudioEngine::sharedEngine()->playEffect(path, 1.f, 0.f, clickVolume);
}

} // namespace echo
