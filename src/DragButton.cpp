#include "DragButton.hpp"

using namespace geode::prelude;

namespace echo {

DragButton* DragButton::create(CCNode* sprite, std::string key, CCPoint defaultPos, std::function<void()> onTap) {
	auto ret = new DragButton();
	if (ret->init(sprite, std::move(key), defaultPos, std::move(onTap))) {
		ret->autorelease();
		return ret;
	}
	delete ret;
	return nullptr;
}

bool DragButton::init(CCNode* sprite, std::string key, CCPoint defaultPos, std::function<void()> onTap) {
	if (!CCLayer::init()) return false;

	m_sprite = sprite;
	m_key = std::move(key);
	m_onTap = std::move(onTap);
	m_baseScale = sprite->getScale();

	sprite->setPosition({0.f, 0.f});
	this->addChild(sprite);

	auto mod = Mod::get();
	this->setPosition({
		mod->getSavedValue<float>(m_key + "-x", defaultPos.x),
		mod->getSavedValue<float>(m_key + "-y", defaultPos.y),
	});
	this->clampToScreen();

	// must sit above the pause menu / gameplay touch handlers
	this->setTouchMode(kCCTouchesOneByOne);
	this->setTouchPriority(-1100);
	this->setTouchEnabled(true);
	return true;
}

void DragButton::clampToScreen() {
	auto win = CCDirector::get()->getWinSize();
	auto half = m_sprite->boundingBox().size * 0.5f;
	auto pos = this->getPosition();
	pos.x = std::clamp(pos.x, half.width, std::max(half.width, win.width - half.width));
	pos.y = std::clamp(pos.y, half.height, std::max(half.height, win.height - half.height));
	this->setPosition(pos);
}

bool DragButton::ccTouchBegan(CCTouch* touch, CCEvent*) {
	if (!this->isVisible() || !nodeIsVisible(this)) return false;

	auto local = this->convertToNodeSpace(touch->getLocation());
	if (!m_sprite->boundingBox().containsPoint(local)) return false;

	m_dragging = false;
	m_startTouch = touch->getLocation();
	auto parentPoint = this->getParent() ? this->getParent()->convertToNodeSpace(touch->getLocation())
	                                     : touch->getLocation();
	m_grabOffset = this->getPosition() - parentPoint;
	m_sprite->setScale(m_baseScale * 0.9f);
	return true;
}

void DragButton::ccTouchMoved(CCTouch* touch, CCEvent*) {
	auto loc = touch->getLocation();
	if (!m_dragging && ccpDistance(loc, m_startTouch) > 8.f) m_dragging = true;
	if (!m_dragging) return;

	auto parentPoint = this->getParent() ? this->getParent()->convertToNodeSpace(loc) : loc;
	this->setPosition(parentPoint + m_grabOffset);
	this->clampToScreen();
}

void DragButton::ccTouchEnded(CCTouch*, CCEvent*) {
	m_sprite->setScale(m_baseScale);
	if (m_dragging) {
		m_dragging = false;
		auto mod = Mod::get();
		mod->setSavedValue<float>(m_key + "-x", this->getPositionX());
		mod->setSavedValue<float>(m_key + "-y", this->getPositionY());
	} else if (m_onTap) {
		m_onTap();
	}
}

void DragButton::ccTouchCancelled(CCTouch*, CCEvent*) {
	m_sprite->setScale(m_baseScale);
	m_dragging = false;
}

} // namespace echo
