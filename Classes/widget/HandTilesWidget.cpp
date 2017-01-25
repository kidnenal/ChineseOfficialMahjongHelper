﻿#include "HandTilesWidget.h"
#include "../common.h"
#include "../compiler.h"

USING_NS_CC;

bool HandTilesWidget::init() {
    if (UNLIKELY(!ui::Widget::init())) {
        return false;
    }

    // 一张牌的尺寸：27 * 39

    // 14张牌宽度：27 * 14 = 378
    // 加间距：378 + 4 = 382
    Size tilesSize = Size(382 + 4, 39 + 4);
    _standingWidget = ui::Widget::create();
    _standingWidget->setContentSize(tilesSize);
    this->addChild(_standingWidget);
    _standingWidget->setPosition(Vec2(tilesSize.width * 0.5f, tilesSize.height * 0.5f));

    // 高亮方框
    _highlightBox = DrawNode::create();
    _highlightBox->setContentSize(Size(27, 39));
    _highlightBox->setAnchorPoint(Vec2::ANCHOR_MIDDLE);
    _highlightBox->drawLine(Vec2(0, 0), Vec2(27, 0), Color4F::RED);
    _highlightBox->drawLine(Vec2(27, 0), Vec2(27, 39), Color4F::RED);
    _highlightBox->drawLine(Vec2(27, 39), Vec2(0, 39), Color4F::RED);
    _highlightBox->drawLine(Vec2(0, 39), Vec2(0, 0), Color4F::RED);
    _standingWidget->addChild(_highlightBox, 2);

    // 一个直杠的宽度：39 + 27 * 3 = 120
    // 两个直杠的宽度：120 * 2 = 240
    // 加间距：240 + 4 = 244

    // 高度：39 * 2 = 78
    // 加间距：78 + 4 = 82
    Size fixedSize = Size(244, 82);
    _fixedWidget = ui::Widget::create();
    _fixedWidget->setContentSize(fixedSize);
    this->addChild(_fixedWidget);
    _fixedWidget->setPosition(Vec2(tilesSize.width * 0.5f, tilesSize.height + fixedSize.height * 0.5f + 4));

    this->setContentSize(Size(tilesSize.width, tilesSize.height + fixedSize.height + 4));

    _standingTiles.reserve(13);
    _fixedSets.reserve(4);

    reset();

    return true;
}

void HandTilesWidget::reset() {
    // 清空数据
    memset(_usedTilesTable, 0, sizeof(_usedTilesTable));
    memset(_standingTilesTable, 0, sizeof(_standingTilesTable));
    _currentIdx = 0;
    _standingTiles.clear();
    _fixedSets.clear();

    // 清除之前的残留的立牌
    while (!_standingTileButtons.empty()) {
        _standingWidget->removeChild(_standingTileButtons.back());
        _standingTileButtons.pop_back();
    }
    _highlightBox->setPosition(calcStandingTilePos(0));

    // 清除之前残留的副露
    _fixedWidget->removeAllChildren();
}

void HandTilesWidget::setData(const mahjong::SET fixedSets[5], long setCnt, const mahjong::TILE standingTiles[13], long tileCnt, mahjong::TILE winTile) {
    reset();

    for (long i = 0; i < setCnt; ++i) {
        _fixedSets.push_back(fixedSets[i]);
        mahjong::TILE tile = fixedSets[i].mid_tile;
        switch (fixedSets[i].set_type) {
            case mahjong::SET_TYPE::CHOW:
                addFixedChowSet(tile, 0);
                ++_usedTilesTable[tile - 1];
                ++_usedTilesTable[tile];
                ++_usedTilesTable[tile + 1];
                break;
            case mahjong::SET_TYPE::PUNG:
                addFixedPungSet(tile, 0);
                _usedTilesTable[tile] += 3;
                break;
            case mahjong::SET_TYPE::KONG:
                if (fixedSets[i].is_melded) {
                    addFixedMeldedKongSet(tile, 0);
                }
                else {
                    addFixedConcealedKongSet(tile);
                }
                _usedTilesTable[tile] += 4;
                break;

            default:
                break;
        }
    }

    for (long i = 0; i < tileCnt; ++i) {
        mahjong::TILE tile = standingTiles[i];
        addTile(tile);
        ++_usedTilesTable[tile];
    }
    addTile(winTile);
}

mahjong::TILE HandTilesWidget::getWinTile() const {
    if (_standingTiles.empty()) {
        return 0;
    }

    size_t maxCnt = 13 - _fixedSets.size() * 3;  // 立牌数最大值（不包括和牌）
    if (_standingTiles.size() < maxCnt) {
        return 0;
    }
    return _standingTiles.back();
}

bool HandTilesWidget::isFixedSetsContainsKong() const {
    return std::any_of(_fixedSets.begin(), _fixedSets.end(),
        [](const mahjong::SET &set) { return set.set_type == mahjong::SET_TYPE::KONG; });
}

bool HandTilesWidget::isStandingTilesContainsWinTile() const {
    mahjong::TILE winTile = getWinTile();
    if (winTile == 0) {
        return false;
    }
    return std::any_of(_standingTiles.begin(), _standingTiles.end(),
        [winTile](mahjong::TILE tile) { return tile == winTile; });
}

bool HandTilesWidget::isFixedSetsContainsWinTile() const {
    mahjong::TILE winTile = getWinTile();
    if (winTile == 0) {
        return false;
    }
    return std::any_of(_fixedSets.begin(), _fixedSets.end(),
        std::bind(&mahjong::is_set_contains_tile, std::placeholders::_1, winTile));
}

void HandTilesWidget::sort() {
    if (!_standingTiles.empty()) {
        std::sort(_standingTiles.begin(), _standingTiles.end() - 1);
        refreshStandingTiles();
    }
}

cocos2d::Vec2 HandTilesWidget::calcStandingTilePos(size_t idx) const {
    Vec2 pos;
    pos.y = 19.5f + 2;
    switch (_fixedSets.size()) {
    default: pos.x = 27 * (idx + 0.5f) + 2; break;
    case 1: pos.x = 27 * (idx + 2) + 2; break;
    case 2: pos.x = 27 * (idx + 3.5f) + 2; break;
    case 3: pos.x = 27 * (idx + 5) + 2; break;
    case 4: pos.x = 27 * (idx + 6.5f) + 2; break;
    }
    return pos;
}

cocos2d::Vec2 HandTilesWidget::calcFixedSetPos(size_t idx) const {
    const Size &fixedSize = _fixedWidget->getContentSize();
    Vec2 pos = Vec2(fixedSize.width * 0.25f, fixedSize.height * 0.25f);
    switch (idx) {
        case 1: pos.y *= 3; break;
        case 2: pos.x *= 3; pos.y *= 3; break;
        case 3: break;
        case 4: pos.x *= 3; break;
        default: return Vec2::ZERO;
    }
    return pos;
}

void HandTilesWidget::onTileButton(cocos2d::Ref *sender) {
    ui::Button *button = (ui::Button *)sender;
    _currentIdx = reinterpret_cast<size_t>(button->getUserData());
    refreshHighlightPos();
    if (_currentIdxChangedCallback) {
        _currentIdxChangedCallback();
    }
}

// 添加一张牌
void HandTilesWidget::addTile(mahjong::TILE tile) {
    ui::Button *button = ui::Button::create(tilesImageName[tile]);
    button->setScale(27 / button->getContentSize().width);
    _standingWidget->addChild(button);

    size_t tilesCnt = _standingTiles.size();
    button->setUserData(reinterpret_cast<void *>(tilesCnt));
    button->addClickEventListener(std::bind(&HandTilesWidget::onTileButton, this, std::placeholders::_1));

    Vec2 pos = calcStandingTilePos(tilesCnt);
    size_t maxCnt = 13 - _fixedSets.size() * 3;  // 立牌数最大值（不包括和牌）
    if (tilesCnt < maxCnt) {
        button->setPosition(pos);
        _currentIdx = tilesCnt + 1;
        ++_standingTilesTable[tile];
    }
    else {
        button->setPosition(Vec2(pos.x + 4, pos.y));  // 和牌张与立牌间隔4像素
    }

    _standingTileButtons.push_back(button);
    _standingTiles.push_back(tile);
}

// 替换一张牌
void HandTilesWidget::replaceTile(mahjong::TILE tile) {
    ui::Button *button = ui::Button::create(tilesImageName[tile]);
    button->setScale(27 / button->getContentSize().width);
    _standingWidget->addChild(button);

    button->setUserData(reinterpret_cast<void *>(_currentIdx));
    button->addClickEventListener(std::bind(&HandTilesWidget::onTileButton, this, std::placeholders::_1));

    button->setPosition(_standingTileButtons[_currentIdx]->getPosition());
    _standingWidget->removeChild(_standingTileButtons[_currentIdx]);
    _standingTileButtons[_currentIdx] = button;

    size_t maxCnt = 13 - _fixedSets.size() * 3;  // 立牌数最大值（不包括和牌）
    if (_currentIdx < maxCnt) {
        mahjong::TILE prevTile = _standingTiles[_currentIdx];
        --_standingTilesTable[prevTile];

        _standingTiles[_currentIdx] = tile;
        ++_standingTilesTable[tile];

        ++_currentIdx;
    }
    else {
        _standingTiles[_currentIdx] = tile;
    }
}

mahjong::TILE HandTilesWidget::putTile(mahjong::TILE tile) {
    mahjong::TILE prevTile = 0;
    if (_currentIdx >= _standingTiles.size()) {  // 新增牌
        addTile(tile);
        ++_usedTilesTable[tile];
    }
    else {  // 修改牌
        prevTile = _standingTiles[_currentIdx];  // 此位置之前的牌
        if (prevTile != tile) {  // 新选的牌与之前的牌不同，更新相关信息
            replaceTile(tile);
            --_usedTilesTable[prevTile];
            ++_usedTilesTable[tile];
        }
        else {
            size_t maxCnt = 13 - _fixedSets.size() * 3;  // 立牌数最大值（不包括和牌）
            if (_currentIdx < maxCnt) {
                ++_currentIdx;
            }
        }
    }

    refreshHighlightPos();
    return prevTile;
}

void HandTilesWidget::refreshHighlightPos() {
    size_t maxCnt = 13 - _fixedSets.size() * 3;  // 立牌数最大值（不包括和牌）
    Vec2 pos = calcStandingTilePos(_currentIdx);
    if (_currentIdx < maxCnt) {
        _highlightBox->setPosition(pos);
    }
    else {
        _highlightBox->setPosition(Vec2(pos.x + 4, pos.y));
    }
}

// 刷新立牌
void HandTilesWidget::refreshStandingTiles() {
    // 移除所有的立牌
    while (!_standingTileButtons.empty()) {
        _standingWidget->removeChild(_standingTileButtons.back());
        _standingTileButtons.pop_back();
    }

    size_t prevIndex = _currentIdx;

    // 重新一张一张添加立牌
    std::vector<mahjong::TILE> temp;
    temp.swap(_standingTiles);  // 这里要保存一下旧的数据，因为addTile会在standingTiles添加
    memset(_standingTilesTable, 0, sizeof(_standingTilesTable));
    std::for_each(temp.begin(), temp.end(), std::bind(&HandTilesWidget::addTile, this, std::placeholders::_1));

    _currentIdx = _standingTiles.size();
    if (prevIndex < _currentIdx) {
        _currentIdx = prevIndex;
    }
    refreshHighlightPos();
}

// 添加一组吃
void HandTilesWidget::addFixedChowSet(mahjong::TILE tile, int meldedIdx) {
    Vec2 center = calcFixedSetPos(_fixedSets.size());

    // 一张牌的尺寸：27 * 39
    const char *image[3];
    switch (meldedIdx) {
    default:
        image[0] = tilesImageName[tile - 1];
        image[1] = tilesImageName[tile];
        image[2] = tilesImageName[tile + 1];
        break;
    case 1:
        image[0] = tilesImageName[tile];
        image[1] = tilesImageName[tile - 1];
        image[2] = tilesImageName[tile + 1];
        break;
    case 2:
        image[0] = tilesImageName[tile + 1];
        image[1] = tilesImageName[tile - 1];
        image[2] = tilesImageName[tile];
        break;
    }

    Vec2 pos[3];
    pos[0] = Vec2(center.x - 27, center.y - 6);
    pos[1] = Vec2(center.x + 6, center.y);
    pos[2] = Vec2(center.x + 33, center.y);

    for (int i = 0; i < 3; ++i) {
        Sprite *sprite = Sprite::create(image[i]);
        sprite->setScale(27 / sprite->getContentSize().width);
        _fixedWidget->addChild(sprite);
        sprite->setPosition(pos[i]);
        if (i == 0) {
            sprite->setRotation(-90);
        }
    }
}

// 添加一组碰
void HandTilesWidget::addFixedPungSet(mahjong::TILE tile, int meldedIdx) {
    Vec2 center = calcFixedSetPos(_fixedSets.size());

    // 一张牌的尺寸：27 * 39，横放一张
    Vec2 pos[3];
    switch (meldedIdx) {
    default:
        pos[0] = Vec2(center.x - 27, center.y - 6);
        pos[1] = Vec2(center.x + 6, center.y);
        pos[2] = Vec2(center.x + 33, center.y);
        break;
    case 1:
        pos[0] = Vec2(center.x - 33, center.y);
        pos[1] = Vec2(center.x, center.y - 6);
        pos[2] = Vec2(center.x + 33, center.y);
        break;
    case 2:
        pos[0] = Vec2(center.x - 33, center.y);
        pos[1] = Vec2(center.x - 6, center.y);
        pos[2] = Vec2(center.x + 27, center.y - 6);
        break;
    }

    for (int i = 0; i < 3; ++i) {
        Sprite *sprite = Sprite::create(tilesImageName[tile]);
        sprite->setScale(27 / sprite->getContentSize().width);
        _fixedWidget->addChild(sprite);
        sprite->setPosition(pos[i]);
        if (i == meldedIdx) {
            sprite->setRotation(-90);
        }
    }
}

// 添加一组明杠
void HandTilesWidget::addFixedMeldedKongSet(mahjong::TILE tile, int meldedIdx) {
    Vec2 center = calcFixedSetPos(_fixedSets.size());

    // 一张牌的尺寸：27 * 39，横放一张
    Vec2 pos[4];
    switch (meldedIdx) {
    default:
        pos[0] = Vec2(center.x - 40.5f, center.y - 6);
        pos[1] = Vec2(center.x -  7.5f, center.y);
        pos[2] = Vec2(center.x + 19.5f, center.y);
        pos[3] = Vec2(center.x + 46.5f, center.y);
        break;
    case 1:
        pos[0] = Vec2(center.x - 46.5f, center.y);
        pos[1] = Vec2(center.x - 13.5f, center.y - 6);
        pos[2] = Vec2(center.x + 19.5f, center.y);
        pos[3] = Vec2(center.x + 46.5f, center.y);
        break;
    case 2:
        pos[0] = Vec2(center.x - 46.5f, center.y);
        pos[1] = Vec2(center.x - 19.5f, center.y);
        pos[2] = Vec2(center.x + 13.5f, center.y - 6);
        pos[3] = Vec2(center.x + 46.5f, center.y);
        break;
    case 3:
        pos[0] = Vec2(center.x - 46.5f, center.y);
        pos[1] = Vec2(center.x - 19.5f, center.y);
        pos[2] = Vec2(center.x +  7.5f, center.y);
        pos[3] = Vec2(center.x + 40.5f, center.y - 6);
        break;
    }

    for (int i = 0; i < 4; ++i) {
        Sprite *sprite = Sprite::create(tilesImageName[tile]);
        sprite->setScale(27 / sprite->getContentSize().width);
        _fixedWidget->addChild(sprite);
        sprite->setPosition(pos[i]);
        if (i == meldedIdx) {
            sprite->setRotation(-90);
        }
    }
}

// 添加一组暗杠
void HandTilesWidget::addFixedConcealedKongSet(mahjong::TILE tile) {
    Vec2 center = calcFixedSetPos(_fixedSets.size());

    // 一张牌的尺寸：27 * 39，横放一张
    const char *image[4];
    image[0] = "tiles/bg.png";
    image[1] = tilesImageName[tile];
    image[2] = tilesImageName[tile];
    image[3] = "tiles/bg.png";

    Vec2 pos[4];
    pos[0] = Vec2(center.x - 40.5f, center.y);
    pos[1] = Vec2(center.x - 13.5f, center.y);
    pos[2] = Vec2(center.x + 13.5f, center.y);
    pos[3] = Vec2(center.x + 40.5f, center.y);

    for (int i = 0; i < 4; ++i) {
        Sprite *sprite = Sprite::create(image[i]);
        sprite->setScale(27 / sprite->getContentSize().width);
        _fixedWidget->addChild(sprite);
        sprite->setPosition(pos[i]);
    }
}

bool HandTilesWidget::canChow_XX() {
    if (_currentIdx >= _standingTiles.size()) {
        return false;
    }

    mahjong::TILE tile = _standingTiles[_currentIdx];
    return (!mahjong::is_honor(tile)
        && _standingTilesTable[tile] > 0
        && _standingTilesTable[tile + 1] > 0
        && _standingTilesTable[tile + 2] > 0);
}

bool HandTilesWidget::canChowX_X() {
    if (_currentIdx >= _standingTiles.size()) {
        return false;
    }

    mahjong::TILE tile = _standingTiles[_currentIdx];
    return (!mahjong::is_honor(tile)
        && _standingTilesTable[tile - 1] > 0
        && _standingTilesTable[tile] > 0
        && _standingTilesTable[tile + 1] > 0);
}

bool HandTilesWidget::canChowXX_() {
    if (_currentIdx >= _standingTiles.size()) {
        return false;
    }

    mahjong::TILE tile = _standingTiles[_currentIdx];
    return (!mahjong::is_honor(tile)
        && _standingTilesTable[tile - 2] > 0
        && _standingTilesTable[tile - 1] > 0
        && _standingTilesTable[tile] > 0);
}

bool HandTilesWidget::canPung() {
    if (_currentIdx >= _standingTiles.size()) {
        return false;
    }

    mahjong::TILE tile = _standingTiles[_currentIdx];
    return (_standingTilesTable[tile] >= 3);
}

bool HandTilesWidget::canKong() {
    if (_currentIdx >= _standingTiles.size()) {
        return false;
    }

    mahjong::TILE tile = _standingTiles[_currentIdx];
    return (_standingTilesTable[tile] >= 4);
}

bool HandTilesWidget::makeFixedChow_XXSet() {
    if (UNLIKELY(_currentIdx >= _standingTiles.size())) {
        return false;
    }

    // _XX 23吃1
    mahjong::TILE tile = _standingTiles[_currentIdx];
    if (UNLIKELY(mahjong::is_honor(tile)
        || _standingTilesTable[tile] == 0
        || _standingTilesTable[tile + 1] == 0
        || _standingTilesTable[tile + 2] == 0)) {
        return false;
    }

    mahjong::SET set;
    set.is_melded = true;
    set.set_type = mahjong::SET_TYPE::CHOW;
    set.mid_tile = tile + 1;
    _fixedSets.push_back(set);

    std::vector<mahjong::TILE>::iterator it;
    it = std::find(_standingTiles.begin(), _standingTiles.end(), tile);
    it = _standingTiles.erase(it);
    it = std::find(it, _standingTiles.end(), tile + 1);
    it = _standingTiles.erase(it);
    it = std::find(it, _standingTiles.end(), tile + 2);
    _standingTiles.erase(it);
    --_standingTilesTable[tile];
    --_standingTilesTable[tile + 1];
    --_standingTilesTable[tile + 2];

    addFixedChowSet(tile, 0);
    refreshStandingTiles();
    return true;
}

bool HandTilesWidget::makeFixedChowX_XSet() {
    if (UNLIKELY(_currentIdx >= _standingTiles.size())) {
        return false;
    }

    // X_X 13吃2
    mahjong::TILE tile = _standingTiles[_currentIdx];
    if (UNLIKELY(mahjong::is_honor(tile)
        || _standingTilesTable[tile - 1] == 0
        || _standingTilesTable[tile] == 0
        || _standingTilesTable[tile + 1] == 0)) {
        return false;
    }

    mahjong::SET set;
    set.is_melded = true;
    set.set_type = mahjong::SET_TYPE::CHOW;
    set.mid_tile = tile;
    _fixedSets.push_back(set);

    std::vector<mahjong::TILE>::iterator it;
    it = std::find(_standingTiles.begin(), _standingTiles.end(), tile - 1);
    it = _standingTiles.erase(it);
    it = std::find(it, _standingTiles.end(), tile);
    it = _standingTiles.erase(it);
    it = std::find(it, _standingTiles.end(), tile + 1);
    _standingTiles.erase(it);
    --_standingTilesTable[tile - 1];
    --_standingTilesTable[tile];
    --_standingTilesTable[tile + 1];

    addFixedChowSet(tile, 1);
    refreshStandingTiles();
    return true;
}


bool HandTilesWidget::makeFixedChowXX_Set() {
    if (UNLIKELY(_currentIdx >= _standingTiles.size())) {
        return false;
    }

    // XX_ 12吃3
    mahjong::TILE tile = _standingTiles[_currentIdx];
    if (UNLIKELY(mahjong::is_honor(tile)
        || _standingTilesTable[tile - 2] == 0
        || _standingTilesTable[tile - 1] == 0
        || _standingTilesTable[tile] == 0)) {
        return false;
    }

    mahjong::SET set;
    set.is_melded = true;
    set.set_type = mahjong::SET_TYPE::CHOW;
    set.mid_tile = tile - 1;
    _fixedSets.push_back(set);

    std::vector<mahjong::TILE>::iterator it;
    it = std::find(_standingTiles.begin(), _standingTiles.end(), tile - 2);
    it = _standingTiles.erase(it);
    it = std::find(it, _standingTiles.end(), tile - 1);
    it = _standingTiles.erase(it);
    it = std::find(it, _standingTiles.end(), tile);
    _standingTiles.erase(it);
    --_standingTilesTable[tile - 2];
    --_standingTilesTable[tile - 1];
    --_standingTilesTable[tile];

    addFixedChowSet(tile, 2);
    refreshStandingTiles();
    return true;
}

bool HandTilesWidget::makeFixedPungSet() {
    if (UNLIKELY(_currentIdx >= _standingTiles.size())) {
        return false;
    }

    mahjong::TILE tile = _standingTiles[_currentIdx];
    if (UNLIKELY(_standingTilesTable[tile] < 3)) {
        return false;
    }

    mahjong::SET set;
    set.is_melded = true;
    set.set_type = mahjong::SET_TYPE::PUNG;
    set.mid_tile = tile;
    _fixedSets.push_back(set);

    std::vector<mahjong::TILE>::iterator it = _standingTiles.begin();
    for (int i = 0; i < 3; ++i) {
        it = std::find(it, _standingTiles.end(), tile);
        it = _standingTiles.erase(it);
    }
    _standingTilesTable[tile] -= 3;

    it = std::find(_standingTiles.begin(), _standingTiles.end(), tile);
    int meldedIdx = std::distance(it, std::begin(_standingTiles) + _currentIdx);
    addFixedPungSet(tile, meldedIdx);

    refreshStandingTiles();
    return true;
}

bool HandTilesWidget::makeFixedMeldedKongSet() {
    if (UNLIKELY(_currentIdx >= _standingTiles.size())) {
        return false;
    }

    mahjong::TILE tile = _standingTiles[_currentIdx];
    if (UNLIKELY(_standingTilesTable[tile] < 4)) {
        return false;
    }

    mahjong::SET set;
    set.is_melded = true;
    set.set_type = mahjong::SET_TYPE::KONG;
    set.mid_tile = tile;
    _fixedSets.push_back(set);

    std::vector<mahjong::TILE>::iterator it = _standingTiles.begin();
    for (int i = 0; i < 4; ++i) {
        it = std::find(it, _standingTiles.end(), tile);
        it = _standingTiles.erase(it);
    }
    _standingTilesTable[tile] -= 4;

    it = std::find(_standingTiles.begin(), _standingTiles.end(), tile);
    int meldedIdx = std::distance(it, std::begin(_standingTiles) + _currentIdx);
    if (meldedIdx == 2) {
        meldedIdx = 1;
    }
    addFixedMeldedKongSet(tile, meldedIdx);
    refreshStandingTiles();
    return true;
}

bool HandTilesWidget::makeFixedConcealedKongSet() {
    if (UNLIKELY(_currentIdx >= _standingTiles.size())) {
        return false;
    }

    mahjong::TILE tile = _standingTiles[_currentIdx];
    if (UNLIKELY(_standingTilesTable[tile] < 4)) {
        return false;
    }

    mahjong::SET set;
    set.is_melded = false;
    set.set_type = mahjong::SET_TYPE::KONG;
    set.mid_tile = tile;
    _fixedSets.push_back(set);

    std::vector<mahjong::TILE>::iterator it = _standingTiles.begin();
    for (int i = 0; i < 4; ++i) {
        it = std::find(it, _standingTiles.end(), tile);
        it = _standingTiles.erase(it);
    }
    _standingTilesTable[tile] -= 4;

    addFixedConcealedKongSet(tile);
    refreshStandingTiles();
    return true;
}