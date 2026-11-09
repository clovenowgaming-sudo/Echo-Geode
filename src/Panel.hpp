#pragma once

#include <Geode/Geode.hpp>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace echo {

// The Echo menu. Built from plain cocos nodes with its own touch handling so it can
// stay open next to the pause menu and be toggled by the draggable pause-menu button.
class Panel : public cocos2d::CCLayer {
public:
	static void toggle();
	static void close();
	static bool isOpen();

	bool ccTouchBegan(cocos2d::CCTouch* touch, cocos2d::CCEvent* event) override;
	void ccTouchEnded(cocos2d::CCTouch* touch, cocos2d::CCEvent* event) override;
	void ccTouchCancelled(cocos2d::CCTouch* touch, cocos2d::CCEvent* event) override;
	void update(float dt) override;

private:
	struct Hit {
		cocos2d::CCRect rect;
		std::function<void()> fn;
	};

	bool init() override;
	void buildFrame();
	void buildTab(int tab);
	void refresh();
	void tell(std::string const& text);

	cocos2d::CCPoint rowPos(int row) const;
	void addButtonRow(int row, std::string const& text, std::function<void()> fn);
	void addToggleRow(int row, std::string const& text, std::function<bool()> get, std::function<void(bool)> set);
	void addStepperRow(
		int row, std::string const& text, std::function<std::string()> value, std::function<void(int)> step
	);
	void addInfoRow(int row, std::function<std::string()> text);

	cocos2d::CCSize m_size;
	cocos2d::CCNode* m_content = nullptr;
	std::vector<cocos2d::CCLayerColor*> m_tabBgs;
	std::vector<Hit> m_tabHits;
	std::vector<Hit> m_hits;
	std::vector<std::function<void()>> m_refresh;
	int m_tab = 0;
	int m_pressed = -1;
	bool m_pressedTab = false;
	bool m_pressedClose = false;
	cocos2d::CCRect m_closeRect;

	std::vector<std::filesystem::path> m_files;
	int m_fileIdx = 0;
};

} // namespace echo
