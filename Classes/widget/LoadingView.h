﻿#ifndef __LOADING_VIEW_H__
#define __LOADING_VIEW_H__

#include "cocos2d.h"
#include "../utils/compiler.h"

class LoadingView : public cocos2d::Layer {
public:
    CREATE_FUNC(LoadingView);

    virtual bool init() override {
        if (UNLIKELY(!cocos2d::Layer::init())) {
            return false;
        }

        cocos2d::Size visibleSize = cocos2d::Director::getInstance()->getVisibleSize();
        float height = visibleSize.height - 30;

        this->setContentSize(cocos2d::Size(visibleSize.width, height));
        this->setAnchorPoint(cocos2d::Vec2::ANCHOR_BOTTOM_LEFT);

        // 转圈的sprite
        cocos2d::Sprite *sprite = cocos2d::Sprite::create("drawable/spinner_white_48.png");
        this->addChild(sprite);
        sprite->setScale(CC_CONTENT_SCALE_FACTOR() * 0.5f);
        sprite->setPosition(cocos2d::Vec2(visibleSize.width * 0.5f, height * 0.5f));
        sprite->runAction(cocos2d::RepeatForever::create(cocos2d::RotateBy::create(0.5f, 180.0f)));

        // 触摸监听
        auto touchListener = cocos2d::EventListenerTouchOneByOne::create();
        touchListener->setSwallowTouches(true);
        touchListener->onTouchBegan = [this, height](cocos2d::Touch *touch, cocos2d::Event *event) {
            cocos2d::Vec2 pos = this->convertTouchToNodeSpace(touch);
            if (pos.y >= height) {
                return false;
            }
            event->stopPropagation();
            return true;
        };
        _eventDispatcher->addEventListenerWithSceneGraphPriority(touchListener, this);
        _touchListener = touchListener;

        return true;
    }
};

#endif
