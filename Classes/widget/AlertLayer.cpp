﻿#include "AlertLayer.h"
#include "cocos2d.h"
#include "ui/CocosGUI.h"

#pragma execution_character_set("utf-8")

USING_NS_CC;

void AlertLayer::showWithNode(const std::string &title, cocos2d::Node *node, const std::function<void ()> &confirmCallback, const std::function<void ()> &cancelCallback) {
    AlertLayer *alert = new (std::nothrow) AlertLayer();
    if (alert != nullptr && alert->initWithTitle(title, node, confirmCallback, cancelCallback)) {
        alert->autorelease();
        Director::getInstance()->getRunningScene()->addChild(alert, 100);
    }
}

void AlertLayer::showWithMessage(const std::string &title, const std::string &message, const std::function<void ()> &confirmCallback, const std::function<void ()> &cancelCallback) {
    Size visibleSize = Director::getInstance()->getVisibleSize();
    Label *label = Label::createWithSystemFont(message, "Arail", 12);
    label->setColor(Color3B::BLACK);
    label->setDimensions(visibleSize.width * 0.8f - 10, 0);
    AlertLayer::showWithNode(title, label, confirmCallback, cancelCallback);
}

bool AlertLayer::initWithTitle(const std::string &title, cocos2d::Node *node, const std::function<void ()> &confirmCallback, const std::function<void ()> &cancelCallback) {
    if (!Layer::init()) {
        return false;
    }

    _confirmCallback = confirmCallback;
    _cancelCallback = cancelCallback;

    Size visibleSize = Director::getInstance()->getVisibleSize();
    Vec2 origin = Director::getInstance()->getVisibleOrigin();

    // 监听返回键
    EventListenerKeyboard *keyboardListener = EventListenerKeyboard::create();
    keyboardListener->onKeyReleased = [this](EventKeyboard::KeyCode keyCode, Event *unusedEvent) {
        if (keyCode == EventKeyboard::KeyCode::KEY_BACK) {
            onCancelButton(nullptr);
        }
    };
    _eventDispatcher->addEventListenerWithSceneGraphPriority(keyboardListener, this);

    // 遮罩
    this->addChild(LayerColor::create(Color4B(0, 0, 0, 127)));

    const float width = visibleSize.width * 0.8f;

    const Size &nodeSize = node->getContentSize();
    const float height = nodeSize.height + 78.0f;

    // 背影
    LayerColor *background = LayerColor::create(Color4B::WHITE, width, height);
    this->addChild(background);
    background->ignoreAnchorPointForPosition(false);
    background->setPosition(Vec2(origin.x + visibleSize.width * 0.5f, origin.y + visibleSize.height * 0.5f));

    // 标题
    Label *label = Label::createWithSystemFont(title, "Arail", 14);
    label->setColor(Color3B(0, 114, 197));
    background->addChild(label);
    label->setPosition(Vec2(width * 0.5f, 62.0f + nodeSize.height));

    // 分隔线
    LayerColor *line = LayerColor::create(Color4B(227, 227, 227, 255), width, 2.0f);
    background->addChild(line);
    line->setPosition(Vec2(0.0f, 45.0f + nodeSize.height));

    // 传入的node
    background->addChild(node);
    node->ignoreAnchorPointForPosition(false);
    node->setPosition(Vec2(width * 0.5f, 35.0f + nodeSize.height * 0.5f));

    // 取消按钮
    ui::Button *button = ui::Button::create("source_material/btn_square_disabled.png", "source_material/btn_square_highlighted.png");
    background->addChild(button);
    button->setScale9Enabled(true);
    button->setContentSize(Size(width * 0.5f, 30.0f));
    button->setTitleFontSize(14);
    button->setTitleText("取消");
    button->setPosition(Vec2(width * 0.25f, 10.0f));
    button->addClickEventListener(std::bind(&AlertLayer::onCancelButton, this, std::placeholders::_1));

    // 确定按钮
    button = ui::Button::create("source_material/btn_square_selected.png", "source_material/btn_square_highlighted.png");
    background->addChild(button);
    button->setScale9Enabled(true);
    button->setContentSize(Size(width * 0.5f, 30.0f));
    button->setTitleFontSize(14);
    button->setTitleText("确定");
    button->setPosition(Vec2(width * 0.75f, 10.0f));
    button->addClickEventListener(std::bind(&AlertLayer::onConfirmButton, this, std::placeholders::_1));

    // 触摸监听，点击background以外的部分按按下取消键处理
    EventListenerTouchOneByOne *touchListener = EventListenerTouchOneByOne::create();
    touchListener->setSwallowTouches(true);
    touchListener->onTouchBegan = [this, background](Touch *touch, Event *) {
        Vec2 pos = this->convertTouchToNodeSpace(touch);
        if (background->getBoundingBox().containsPoint(pos)) {
            return true;
        }
        onCancelButton(nullptr);
        return true;
    };
    _eventDispatcher->addEventListenerWithSceneGraphPriority(touchListener, this);
    _touchListener = touchListener;

    return true;
}

void AlertLayer::onCancelButton(cocos2d::Ref *sender) {
    if (_cancelCallback) {
        _cancelCallback();
    }
    this->removeFromParent();
}

void AlertLayer::onConfirmButton(cocos2d::Ref *sender) {
    if (_confirmCallback) {
        _confirmCallback();
    }
    this->removeFromParent();
}