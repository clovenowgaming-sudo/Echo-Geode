#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>

#include "DragButton.hpp"
#include "Echo.hpp"
#include "Panel.hpp"

using namespace geode::prelude;
using namespace echo;

$on_mod(Loaded) {
	Bot::get().loadSettings();
}

// ---------------------------------------------------------------------------
// Core physics hooks: input recording / replay injection / frame advance
// ---------------------------------------------------------------------------
class $modify(EchoGameLayer, GJBaseGameLayer) {
	void update(float dt) {
		auto& bot = Bot::get();
		if (isPlayLayer(this) && bot.frameAdvance) {
			if (!bot.stepRequested) return; // frozen until the Step button is tapped
			bot.stepRequested = false;
			dt = (1.f / bot.tps) * 1.001f; // one physics step
		}
		GJBaseGameLayer::update(dt);
	}

	void processCommands(float dt, bool isHalfTick, bool isLastTick) {
		if (isPlayLayer(this)) Bot::get().beforeStep(this);
		GJBaseGameLayer::processCommands(dt, isHalfTick, isLastTick);
	}

	void handleButton(bool down, int button, bool isPlayer1) {
		auto& bot = Bot::get();
		if (isPlayLayer(this)) {
			// while a macro plays, only the bot may press buttons
			if (bot.state == State::Playing && !bot.injecting) return;
			if (!bot.injecting) bot.onButton(this, down, button, !isPlayer1);
		}
		GJBaseGameLayer::handleButton(down, button, isPlayer1);
	}
};

// ---------------------------------------------------------------------------
// PlayLayer: HUD label, step button, hitboxes, reset handling
// ---------------------------------------------------------------------------
class $modify(EchoPlayLayer, PlayLayer) {
	struct Fields {
		CCLabelBMFont* label = nullptr;
		CCNode* stepButton = nullptr;
	};

	void setupHasCompleted() {
		PlayLayer::setupHasCompleted();

		auto& bot = Bot::get();
		bot.onLevelEnter();

		auto win = CCDirector::get()->getWinSize();

		auto label = CCLabelBMFont::create("", "chatFont.fnt");
		label->setAnchorPoint({0.f, 1.f});
		label->setPosition({6.f, win.height - 4.f});
		label->setScale(0.6f);
		label->setOpacity(170);
		label->setID("echo-label"_spr);
		m_uiLayer->addChild(label, 100);
		m_fields->label = label;

		auto stepSprite = ButtonSprite::create("Step", 40, true, "bigFont.fnt", "GJ_button_01.png", 28.f, 0.5f);
		auto step = DragButton::create(stepSprite, "step-btn", {win.width - 60.f, 50.f}, [] {
			Bot::get().stepRequested = true;
		});
		step->setID("echo-step"_spr);
		step->setVisible(false);
		m_uiLayer->addChild(step, 101);
		m_fields->stepButton = step;
	}

	void resetLevel() {
		PlayLayer::resetLevel();
		Bot::get().onReset(this);
	}

	void postUpdate(float dt) {
		PlayLayer::postUpdate(dt);

		auto& bot = Bot::get();

		if (m_fields->stepButton) m_fields->stepButton->setVisible(bot.frameAdvance);

		if (bot.showHitboxes) {
			m_isDebugDrawEnabled = true;
			if (m_debugDrawNode) m_debugDrawNode->setVisible(true);
			this->updateDebugDraw();
		}

		if (m_fields->label) {
			m_fields->label->setVisible(bot.showLabel);
			if (bot.showLabel) {
				auto text = fmt::format(
					"Echo: {} | frame {} | {} inputs", Bot::stateName(bot.state),
					m_gameState.m_currentProgress, bot.inputs.size()
				);
				m_fields->label->setString(text.c_str());
			}
		}
	}

	void onExit() {
		Bot::get().resetSpeed();
		PlayLayer::onExit();
	}
};

// ---------------------------------------------------------------------------
// Pause menu: draggable button that toggles the Echo menu
// ---------------------------------------------------------------------------
class $modify(EchoPauseLayer, PauseLayer) {
	void customSetup() {
		PauseLayer::customSetup();

		auto win = CCDirector::get()->getWinSize();

		auto spr = CircleButtonSprite::create(
			CCLabelBMFont::create("E", "bigFont.fnt"), CircleBaseColor::Green, CircleBaseSize::Medium
		);
		auto btn = DragButton::create(spr, "pause-btn", {win.width - 34.f, win.height - 34.f}, [] {
			Panel::toggle();
		});
		btn->setID("echo-toggle"_spr);
		this->addChild(btn, 1000);
	}

	void onResume(CCObject* sender) {
		Panel::close();
		PauseLayer::onResume(sender);
		Bot::get().applyPendingRestart();
	}
};
