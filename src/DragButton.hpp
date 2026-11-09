#pragma once

#include <Geode/Geode.hpp>
#include <functional>
#include <string>

namespace echo {

// A button that can be tapped (runs a callback) or dragged around the screen.
// Its position is saved, so it stays where the user left it.
class DragButton : public cocos2d::CCLayer {
public:
	static DragButton* create(
		cocos2d::CCNode* sprite, std::string key, cocos2d::CCPoint defaultPos, std::function<void()> onTap
	);

	bool ccTouchBegan(cocos2d::CCTouch* touch, cocos2d::CCEvent* event) override;
	void ccTouchMoved(cocos2d::CCTouch* touch, cocos2d::CCEvent* event) override;
	void ccTouchEnded(cocos2d::CCTouch* touch, cocos2d::CCEvent* event) override;
	void ccTouchCancelled(cocos2d::CCTouch* touch, cocos2d::CCEvent* event) override;

private:
	bool init(cocos2d::CCNode* sprite, std::string key, cocos2d::CCPoint defaultPos, std::function<void()> onTap);
	void clampToScreen();

	cocos2d::CCNode* m_sprite = nullptr;
	std::string m_key;
	std::function<void()> m_onTap;
	bool m_dragging = false;
	cocos2d::CCPoint m_startTouch;
	cocos2d::CCPoint m_grabOffset;
	float m_baseScale = 1.f;
};

} // namespace echo
