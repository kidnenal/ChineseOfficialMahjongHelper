﻿#include "RecordHistoryScene.h"
#include <array>
#include "Record.h"
#include "../UICommon.h"
#include "../widget/AlertDialog.h"
#include "../widget/LoadingView.h"

USING_NS_CC;

static const Color3B C3B_GRAY = Color3B(96, 96, 96);

static std::vector<Record> g_records;
static std::mutex g_mutex;

static void loadRecords(std::vector<Record> &records) {
    std::lock_guard<std::mutex> lg(g_mutex);

    std::string fileName = FileUtils::getInstance()->getWritablePath();
    fileName.append("history_record.json");
    LoadHistoryRecords(fileName.c_str(), records);
}

static void saveRecords(const std::vector<Record> &records) {
    std::string fileName = FileUtils::getInstance()->getWritablePath();
    fileName.append("history_record.json");
    SaveHistoryRecords(fileName.c_str(), records);
}

#define BUF_SIZE 511

void RecordHistoryScene::updateRecordTexts() {
    _recordTexts.clear();
    _recordTexts.reserve(g_records.size());

    std::transform(g_records.begin(), g_records.end(), std::back_inserter(_recordTexts), [](const Record &record)->std::string {
        char str[BUF_SIZE + 1];
        str[BUF_SIZE] = '\0';

        struct tm ret = *localtime(&record.start_time);
        int len = snprintf(str, BUF_SIZE, "%d年%d月%d日%.2d:%.2d", ret.tm_year + 1900, ret.tm_mon + 1, ret.tm_mday, ret.tm_hour, ret.tm_min);
        if (record.end_time != 0) {
            ret = *localtime(&record.end_time);
            len += snprintf(str + len, static_cast<size_t>(BUF_SIZE - len), "——%d年%d月%d日%.2d:%.2d", ret.tm_year + 1900, ret.tm_mon + 1, ret.tm_mday, ret.tm_hour, ret.tm_min);
        }
        else {
            len += snprintf(str + len, static_cast<size_t>(BUF_SIZE - len), "——(未结束)");
        }

        typedef std::pair<int, int> SeatScore;
        SeatScore seatscore[4];
        seatscore[0].first = 0; seatscore[0].second = 0;
        seatscore[1].first = 1, seatscore[1].second = 0;
        seatscore[2].first = 2, seatscore[2].second = 0;
        seatscore[3].first = 3, seatscore[3].second = 0;
        for (int i = 0; i < 16; ++i) {
            int s[4];
            TranslateDetailToScoreTable(record.detail[i], s);
            seatscore[0].second += s[0];
            seatscore[1].second += s[1];
            seatscore[2].second += s[2];
            seatscore[3].second += s[3];
        }

        std::stable_sort(std::begin(seatscore), std::end(seatscore),
            [](const SeatScore &left, const SeatScore &right) {
            return left.second > right.second;
        });

        static const char *seatText[] = { "东", "南", "西", "北" };
        snprintf(str + len, static_cast<size_t>(BUF_SIZE - len), "\n%s: %s (%+d)\n%s: %s (%+d)\n%s: %s (%+d)\n%s: %s (%+d)",
            seatText[seatscore[0].first], record.name[seatscore[0].first], seatscore[0].second,
            seatText[seatscore[1].first], record.name[seatscore[1].first], seatscore[1].second,
            seatText[seatscore[2].first], record.name[seatscore[2].first], seatscore[2].second,
            seatText[seatscore[3].first], record.name[seatscore[3].first], seatscore[3].second);
        return str;
    });
}

bool RecordHistoryScene::initWithCallback(const ViewCallback &viewCallback) {
    if (UNLIKELY(!BaseScene::initWithTitle("历史记录"))) {
        return false;
    }

    _viewCallback = viewCallback;

    Size visibleSize = Director::getInstance()->getVisibleSize();
    Vec2 origin = Director::getInstance()->getVisibleOrigin();

    // 个人汇总按钮
    ui::Button *button = UICommon::createButton();
    this->addChild(button);
    button->setScale9Enabled(true);
    button->setContentSize(Size(55.0f, 20.0f));
    button->setTitleFontSize(12);
    button->setTitleText("个人汇总");
    button->setPosition(Vec2(origin.x + visibleSize.width * 0.25f, origin.y + visibleSize.height - 45.0f));
    button->addClickEventListener(std::bind(&RecordHistoryScene::onSummaryButton, this, std::placeholders::_1));

    // 批量删除按钮
    button = UICommon::createButton();
    this->addChild(button);
    button->setScale9Enabled(true);
    button->setContentSize(Size(55.0f, 20.0f));
    button->setTitleFontSize(12);
    button->setTitleText("批量删除");
    button->setPosition(Vec2(origin.x + visibleSize.width * 0.75f, origin.y + visibleSize.height - 45.0f));
    button->addClickEventListener(std::bind(&RecordHistoryScene::onBatchDeleteButton, this, std::placeholders::_1));

    cw::TableView *tableView = cw::TableView::create();
    tableView->setDirection(ui::ScrollView::Direction::VERTICAL);
    tableView->setScrollBarPositionFromCorner(Vec2(2.0f, 2.0f));
    tableView->setScrollBarWidth(4.0f);
    tableView->setScrollBarOpacity(0x99);
    tableView->setContentSize(Size(visibleSize.width - 5.0f, visibleSize.height - 65.0f));
    tableView->setDelegate(this);
    tableView->setVerticalFillOrder(cw::TableView::VerticalFillOrder::TOP_DOWN);

    tableView->setAnchorPoint(Vec2::ANCHOR_MIDDLE);
    tableView->setPosition(Vec2(origin.x + visibleSize.width * 0.5f, origin.y + visibleSize.height * 0.5f - 27.5f));
    this->addChild(tableView);
    _tableView = tableView;

    tableView->setOnEnterCallback([this]() {
        if (LIKELY(!g_records.empty())) {
            updateRecordTexts();
            _tableView->reloadDataInplacement();
        }
    });

    if (UNLIKELY(g_records.empty())) {
        this->scheduleOnce([this](float) {
            Vec2 origin = Director::getInstance()->getVisibleOrigin();

            LoadingView *loadingView = LoadingView::create();
            this->addChild(loadingView);
            loadingView->setPosition(origin);

            auto thiz = makeRef(this);  // 保证线程回来之前不析构
            std::thread([thiz, loadingView]() {
                auto temp = std::make_shared<std::vector<Record> >();
                loadRecords(*temp);

                // 切换到cocos线程
                Director::getInstance()->getScheduler()->performFunctionInCocosThread([thiz, loadingView, temp]() {
                    g_records.swap(*temp);

                    if (LIKELY(thiz->isRunning())) {
                        thiz->updateRecordTexts();
                        loadingView->removeFromParent();
                        thiz->_tableView->reloadData();
                    }
                });
            }).detach();
        }, 0.0f, "load_records");
    }

    return true;
}

ssize_t RecordHistoryScene::numberOfCellsInTableView(cw::TableView *) {
    return _recordTexts.size();
}

float RecordHistoryScene::tableCellSizeForIndex(cw::TableView *, ssize_t) {
    return 70.0f;
}

cw::TableViewCell *RecordHistoryScene::tableCellAtIndex(cw::TableView *table, ssize_t idx) {
    typedef cw::TableViewCellEx<std::array<LayerColor *, 2>, Label *, ui::Button *> CustomCell;
    CustomCell *cell = (CustomCell *)table->dequeueCell();

    if (cell == nullptr) {
        cell = CustomCell::create();

        Size visibleSize = Director::getInstance()->getVisibleSize();
        const float width = visibleSize.width - 5.0f;

        CustomCell::ExtDataType &ext = cell->getExtData();
        std::array<LayerColor *, 2> &layerColors = std::get<0>(ext);
        Label *&label = std::get<1>(ext);
        ui::Button *&delBtn = std::get<2>(ext);

        layerColors[0] = LayerColor::create(Color4B(0x10, 0x10, 0x10, 0x10), width, 70.0f);
        cell->addChild(layerColors[0]);

        layerColors[1] = LayerColor::create(Color4B(0xC0, 0xC0, 0xC0, 0x10), width, 70.0f);
        cell->addChild(layerColors[1]);

        label = Label::createWithSystemFont("", "Arail", 10);
        label->setColor(Color3B::BLACK);
        cell->addChild(label);
        label->setPosition(Vec2(2.0f, 35.0f));
        label->setAnchorPoint(Vec2::ANCHOR_MIDDLE_LEFT);

        delBtn = ui::Button::create("drawable/btn_trash_bin.png");
        delBtn->setScale(Director::getInstance()->getContentScaleFactor() * 0.5f);
        delBtn->addClickEventListener(std::bind(&RecordHistoryScene::onDeleteButton, this, std::placeholders::_1));
        cell->addChild(delBtn);
        delBtn->setPosition(Vec2(width - 20.0f, 35.0f));

        cell->setContentSize(Size(width, 70.0f));
        cell->setTouchEnabled(true);
        cell->addClickEventListener(std::bind(&RecordHistoryScene::onCellClicked, this, std::placeholders::_1));
    }

    const CustomCell::ExtDataType &ext = cell->getExtData();
    const std::array<LayerColor *, 2> &layerColors = std::get<0>(ext);
    Label *label = std::get<1>(ext);
    ui::Button *delBtn = std::get<2>(ext);

    layerColors[0]->setVisible(!(idx & 1));
    layerColors[1]->setVisible(!!(idx & 1));

    delBtn->setUserData(reinterpret_cast<void *>(idx));

    label->setString(_recordTexts[idx]);

    return cell;
}

void RecordHistoryScene::onDeleteButton(cocos2d::Ref *sender) {
    ui::Button *button = (ui::Button *)sender;
    size_t idx = reinterpret_cast<size_t>(button->getUserData());

    AlertDialog::Builder(this)
        .setTitle("删除记录")
        .setMessage("删除后无法找回，确认删除？")
        .setNegativeButton("取消", nullptr)
        .setPositiveButton("确定", [this, idx](AlertDialog *, int) {
        LoadingView *loadingView = LoadingView::create();
        this->addChild(loadingView);
        loadingView->setPosition(Director::getInstance()->getVisibleSize());

        g_records.erase(g_records.begin() + idx);

        auto thiz = makeRef(this);  // 保证线程回来之前不析构
        auto temp = std::make_shared<std::vector<Record> >(g_records);
        std::thread([thiz, temp, loadingView](){
            saveRecords(*temp);

            // 切换到cocos线程
            Director::getInstance()->getScheduler()->performFunctionInCocosThread([thiz, loadingView]() {
                if (LIKELY(thiz->isRunning())) {
                    thiz->updateRecordTexts();
                    loadingView->removeFromParent();
                    thiz->_tableView->reloadDataInplacement();
                }
            });
        }).detach();
        return true;
    }).create()->show();
}

namespace {
    struct RecordsStatistic {
        size_t rank[4];
        float standard_score;
        int competition_score;
        uint16_t max_fan;
        size_t win;
        size_t self_drawn;
        size_t claim;
        size_t win_fan;
        size_t claim_fan;
    };
}

static void SummarizeRecords(const std::vector<int8_t> &flags, const std::vector<Record> &records, RecordsStatistic *result) {
    memset(result, 0, sizeof(*result));

    for (size_t i = 0, cnt = std::min<size_t>(flags.size(), records.size()); i < cnt; ++i) {
        const Record &record = records[i];
        if (record.end_time == 0) {
            continue;
        }

        int8_t idx = flags[i];
        if (idx == -1) {
            continue;
        }

        int totalScores[4] = { 0 };

        for (int k = 0; k < 16; ++k) {
            const Record::Detail &detail = record.detail[k];

            int scoreTable[4];
            TranslateDetailToScoreTable(detail, scoreTable);
            for (int n = 0; n < 4; ++n) {
                totalScores[n] += scoreTable[n];
            }

            uint8_t wf = detail.win_flag;
            uint8_t cf = detail.claim_flag;
            int winIndex = WIN_CLAIM_INDEX(wf);
            int claimIndex = WIN_CLAIM_INDEX(cf);
            if (winIndex == idx) {
                ++result->win;
                if (claimIndex == idx) {
                    ++result->self_drawn;
                }
                result->win_fan += detail.fan;
                result->max_fan = std::max<uint16_t>(result->max_fan, detail.fan);
            }
            else if (claimIndex == idx) {
                ++result->claim;
                result->claim_fan += detail.fan;
            }
        }

        unsigned ranks[4];
        CalculateRankFromScore(totalScores, ranks);
        ++result->rank[ranks[idx]];
        result->competition_score += totalScores[idx];

        float ss[4];
        RankToStandardScore(ranks, ss);
        result->standard_score += ss[idx];
    }
}

static cocos2d::Node *createStatisticNode(const RecordsStatistic &rs) {
    const float width = AlertDialog::maxWidth();
    const float height = 8 * 20;

    DrawNode *drawNode = DrawNode::create();
    drawNode->setContentSize(Size(width, height));

    // 横线
    for (int i = 0; i < 9; ++i) {
        drawNode->drawLine(Vec2(0, 20.0f * i), Vec2(width, 20.0f * i), Color4F::BLACK);
    }

    // 竖线
    drawNode->drawLine(Vec2(0.0f, 0.0f), Vec2(0.0f, 160.0f), Color4F::BLACK);
    for (int i = 0; i < 4; ++i) {
        const float x = width * 0.2f * (i + 1);
        drawNode->drawLine(Vec2(x, 80.0f), Vec2(x, 160.0f), Color4F::BLACK);
    }
    for (int i = 0; i < 3; ++i) {
        const float x = width * 0.25f * (i + 1);
        drawNode->drawLine(Vec2(x, 0.0f), Vec2(x, 80.0f), Color4F::BLACK);
    }
    drawNode->drawLine(Vec2(width, 0.0f), Vec2(width, 160.0f), Color4F::BLACK);

    static const char *titleText[4][5] = {
        { "一位", "二位", "三位", "四位", "标准分" },
        { "一位率", "二位率", "三位率", "四位率", "均标准分" },
        { "比赛分", "均比赛分", "均和牌番", "均点炮番", "" },
        { "和牌率", "点炮率", "自摸率", "最大番", "" }
    };

    std::string contentText[4][5];
    size_t sum = rs.rank[0] + rs.rank[1] + rs.rank[2] + rs.rank[3];

    for (int i = 0; i < 4; ++i) {
        contentText[0][i] = std::to_string(rs.rank[i]);
        contentText[1][i] = sum > 0 ? Common::format("%.2f%%", rs.rank[i] * 100 / static_cast<float>(sum)) : std::string("0.00%");
    }
    contentText[0][4] = Common::format("%.2f", rs.standard_score);
    contentText[1][4] = sum > 0 ? Common::format("%.2f", rs.standard_score / static_cast<float>(sum)) : std::string("0.00%");

    contentText[2][0] = std::to_string(rs.competition_score);
    contentText[2][1] = sum > 0 ? Common::format("%.2f", rs.competition_score / static_cast<float>(sum)) : std::string("0.00");
    contentText[2][2] = rs.win > 0 ? Common::format("%.2f", rs.win_fan / static_cast<float>(rs.win)) : std::string("0.00");
    contentText[2][3] = rs.claim > 0 ? Common::format("%.2f", rs.claim_fan / static_cast<float>(rs.claim)) : std::string("0.00");

    contentText[3][0] = sum > 0 ? Common::format("%.2f%%", rs.win * 100 / static_cast<float>(sum * 16)) : std::string("0.00");
    contentText[3][1] = sum > 0 ? Common::format("%.2f%%", rs.claim * 100 / static_cast<float>(sum * 16)) : std::string("0.00");
    contentText[3][2] = rs.win > 0 ? Common::format("%.2f%%", rs.self_drawn * 100 / static_cast<float>(rs.win)) : std::string("0.00");
    contentText[3][3] = std::to_string(rs.max_fan);

    for (int n = 0; n < 4; ++n) {
        int cnt = n < 2 ? 5 : 4;
        float colWidth = width / cnt;
        for (int i = 0; i < cnt; ++i) {
            const float posX = colWidth * (i + 0.5f);
            const float posY = 150.0f - n * 40.0f;
            Label *label = Label::createWithSystemFont(titleText[n][i], "Arail", 12);
            label->setColor(Color3B::BLACK);
            label->setPosition(Vec2(posX, posY));
            drawNode->addChild(label);
            cw::scaleLabelToFitWidth(label, colWidth - 4.0f);

            label = Label::createWithSystemFont(contentText[n][i], "Arail", 12);
            label->setColor(C3B_GRAY);
            label->setPosition(Vec2(posX, posY - 20.0f));
            drawNode->addChild(label);
            cw::scaleLabelToFitWidth(label, colWidth - 4.0f);
        }
    }

    return drawNode;
}

namespace {

    class SummaryTableNode : public Node, cw::TableViewDelegate {
    private:
        std::vector<std::string> _titles;
        std::vector<int8_t> _currentFlags;

    public:
        const std::vector<int8_t> &getCurrentFlags() { return _currentFlags; }

        CREATE_FUNC(SummaryTableNode);

        virtual bool init() override;

        virtual ssize_t numberOfCellsInTableView(cw::TableView *table) override;
        virtual float tableCellSizeForIndex(cw::TableView *table, ssize_t idx) override;
        virtual cw::TableViewCell *tableCellAtIndex(cw::TableView *table, ssize_t idx) override;

        void onRadioButtonGroup(ui::RadioButton *radioButton, int index, ui::RadioButtonGroup::EventType event);
    };

    bool SummaryTableNode::init() {
        if (UNLIKELY(!Node::init())) {
            return false;
        }

        _currentFlags.assign(g_records.size(), -1);

        _titles.reserve(g_records.size());
        std::transform(g_records.begin(), g_records.end(), std::back_inserter(_titles), [](const Record &record)->std::string {
            char str[BUF_SIZE + 1];
            str[BUF_SIZE] = '\0';

            struct tm ret = *localtime(&record.start_time);
            int len = snprintf(str, BUF_SIZE, "%d年%d月%d日%.2d:%.2d", ret.tm_year + 1900, ret.tm_mon + 1, ret.tm_mday, ret.tm_hour, ret.tm_min);
            if (record.end_time != 0) {
                ret = *localtime(&record.end_time);
                len += snprintf(str + len, static_cast<size_t>(BUF_SIZE - len), "——%d年%d月%d日%.2d:%.2d", ret.tm_year + 1900, ret.tm_mon + 1, ret.tm_mday, ret.tm_hour, ret.tm_min);
            }
            else {
                len += snprintf(str + len, static_cast<size_t>(BUF_SIZE - len), "——(未结束)");
            }
            return str;
        });

        Size visibleSize = Director::getInstance()->getVisibleSize();
        const float width = AlertDialog::maxWidth();
        const float height = visibleSize.height * 0.8f - 80.0f;

        this->setContentSize(Size(width, height));

        // 表格
        cw::TableView *tableView = cw::TableView::create();
        tableView->setDirection(ui::ScrollView::Direction::VERTICAL);
        tableView->setScrollBarPositionFromCorner(Vec2(2.0f, 2.0f));
        tableView->setScrollBarWidth(4.0f);
        tableView->setScrollBarOpacity(0x99);
        tableView->setContentSize(Size(width, height));
        tableView->setDelegate(this);
        tableView->setVerticalFillOrder(cw::TableView::VerticalFillOrder::TOP_DOWN);

        tableView->setAnchorPoint(Vec2::ANCHOR_MIDDLE);
        tableView->setPosition(Vec2(width * 0.5f, height * 0.5f));
        tableView->reloadData();
        this->addChild(tableView);

        // 使表格跳到第一个未选择的选项处
        for (size_t i = 0, cnt = g_records.size(); i < cnt; ++i) {
            if (_currentFlags[i] == -1) {
                tableView->jumpToCell(i);
                break;
            }
        }

        return true;
    }

    ssize_t SummaryTableNode::numberOfCellsInTableView(cw::TableView *) {
        return g_records.size();
    }

    float SummaryTableNode::tableCellSizeForIndex(cw::TableView *, ssize_t) {
        return 40.0f;
    }

    cw::TableViewCell *SummaryTableNode::tableCellAtIndex(cw::TableView *table, ssize_t idx) {
        typedef cw::TableViewCellEx<Label *, ui::RadioButtonGroup *, std::array<ui::RadioButton *, 4>, std::array<Label *, 4>, std::array<LayerColor *, 2> > CustomCell;
        CustomCell *cell = (CustomCell *)table->dequeueCell();

        const float cellWidth = AlertDialog::maxWidth();

        if (cell == nullptr) {
            cell = CustomCell::create();

            CustomCell::ExtDataType &ext = cell->getExtData();
            Label *&titleLabel = std::get<0>(ext);
            ui::RadioButtonGroup *&radioGroup = std::get<1>(ext);
            std::array<ui::RadioButton *, 4> &radioButtons = std::get<2>(ext);
            std::array<Label *, 4> &labels = std::get<3>(ext);
            std::array<LayerColor *, 2> &layerColors = std::get<4>(ext);

            // 背景色
            layerColors[0] = LayerColor::create(Color4B(0x10, 0x10, 0x10, 0x10), cellWidth, 40.0f);
            cell->addChild(layerColors[0]);

            layerColors[1] = LayerColor::create(Color4B(0xC0, 0xC0, 0xC0, 0x10), cellWidth, 40.0f);
            cell->addChild(layerColors[1]);

            // 标题
            Label *label = Label::createWithSystemFont("", "Arail", 10);
            label->setColor(Color3B::BLACK);
            cell->addChild(label);
            label->setAnchorPoint(Vec2::ANCHOR_MIDDLE_LEFT);
            label->setPosition(Vec2(2.0f, 30.0f));
            titleLabel = label;

            // 4个RadioButton和人名文本
            radioGroup = ui::RadioButtonGroup::create();
            cell->addChild(radioGroup);
            radioGroup->setAllowedNoSelection(true);
            for (int i = 0; i < 4; ++i) {
                ui::RadioButton *radioButton = UICommon::createRadioButton();
                radioButton->setZoomScale(0.0f);
                radioButton->ignoreContentAdaptWithSize(false);
                radioButton->setContentSize(Size(15.0f, 15.0f));
                radioButton->setPosition(Vec2(cellWidth * 0.25f * i + 10.0f, 10.0f));
                cell->addChild(radioButton);
                radioButtons[i] = radioButton;

                radioGroup->addRadioButton(radioButton);

                label = Label::createWithSystemFont("", "Arail", 10);
                label->setColor(Color3B::BLACK);
                cell->addChild(label);
                label->setAnchorPoint(Vec2::ANCHOR_MIDDLE_LEFT);
                label->setPosition(Vec2(cellWidth * 0.25f * i + 20.0f, 10.0f));
                labels[i] = label;
            }
            radioGroup->addEventListener(std::bind(&SummaryTableNode::onRadioButtonGroup, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

            // 清除按钮
            ui::Button *button = UICommon::createButton();
            cell->addChild(button);
            button->setScale9Enabled(true);
            button->setContentSize(Size(30.0f, 15.0f));
            button->setTitleFontSize(10);
            button->setTitleText("清除");
            button->setPosition(Vec2(cellWidth - 17.0f, 30.0f));
            button->addClickEventListener([this, radioGroup](Ref *) {
                // 如果有选中，则清空
                int selectedButton = radioGroup->getSelectedButtonIndex();
                if (selectedButton != -1) {
                    ssize_t idx = reinterpret_cast<ssize_t>(radioGroup->getRadioButtonByIndex(selectedButton)->getUserData());
                    _currentFlags[idx] = -1;
                    radioGroup->setSelectedButton(nullptr);
                }
            });
        }

        const CustomCell::ExtDataType &ext = cell->getExtData();
        Label *titleLabel = std::get<0>(ext);
        ui::RadioButtonGroup *radioGroup = std::get<1>(ext);
        const std::array<ui::RadioButton *, 4> &radioButtons = std::get<2>(ext);
        const std::array<Label *, 4> &labels = std::get<3>(ext);
        const std::array<LayerColor *, 2> &layerColors = std::get<4>(ext);

        layerColors[0]->setVisible(!(idx & 1));
        layerColors[1]->setVisible(!!(idx & 1));

        const Record &record = g_records[idx];
        bool finished = record.end_time != 0;

        // 标题
        titleLabel->setString(_titles[idx]);
        cw::scaleLabelToFitWidth(titleLabel, cellWidth - 35.0f - 2.0f);

        // 人名文本
        for (int i = 0; i < 4; ++i) {
            radioButtons[i]->setEnabled(finished);
            radioButtons[i]->setUserData(reinterpret_cast<void *>(idx));
            labels[i]->setString(record.name[i]);
            cw::scaleLabelToFitWidth(labels[i], cellWidth * 0.25f - 20.0f - 2.0f);
        }

        // 设置选中
        int8_t flag = _currentFlags[idx];
        radioGroup->setSelectedButton(flag == -1 ? nullptr : radioButtons[flag]);

        return cell;
    }

    void SummaryTableNode::onRadioButtonGroup(ui::RadioButton *radioButton, int index, ui::RadioButtonGroup::EventType) {
        if (radioButton == nullptr) {
            return;
        }

        ssize_t idx = reinterpret_cast<ssize_t>(radioButton->getUserData());
        _currentFlags[idx] = static_cast<int8_t>(index);
    }
}

void RecordHistoryScene::onSummaryButton(cocos2d::Ref *) {
    SummaryTableNode *rootNode = SummaryTableNode::create();
    AlertDialog::Builder(this)
        .setTitle("选择要汇总的对局")
        .setContentNode(rootNode)
        .setNegativeButton("取消", nullptr)
        .setPositiveButton("确定", [this, rootNode](AlertDialog *, int) {
        auto& currentFlags = rootNode->getCurrentFlags();

        RecordsStatistic rs;
        SummarizeRecords(currentFlags, g_records, &rs);

        Node *node = createStatisticNode(rs);
        AlertDialog::Builder(this)
            .setTitle("汇总")
            .setContentNode(node)
            .setPositiveButton("确定", nullptr)
            .create()->show();
        return true;
    }).create()->show();
}

namespace {

    class BatchDeleteTableNode : public Node, cw::TableViewDelegate {
    private:
        std::vector<std::string> _texts;
        std::vector<bool> _currentFlags;

    public:
        const std::vector<bool> &getCurrentFlags() { return _currentFlags; }

        CREATE_FUNC_WITH_PARAM_1(BatchDeleteTableNode, initWithText, const std::vector<std::string> &, texts);

        bool initWithText(const std::vector<std::string> &texts);

        virtual ssize_t numberOfCellsInTableView(cw::TableView *table) override;
        virtual float tableCellSizeForIndex(cw::TableView *table, ssize_t idx) override;
        virtual cw::TableViewCell *tableCellAtIndex(cw::TableView *table, ssize_t idx) override;
    };

    bool BatchDeleteTableNode::initWithText(const std::vector<std::string> &texts) {
        if (UNLIKELY(!Node::init())) {
            return false;
        }

        _currentFlags.assign(g_records.size(), 0);

        _texts.reserve(texts.size());
        std::copy(texts.begin(), texts.end(), std::back_inserter(_texts));

        Size visibleSize = Director::getInstance()->getVisibleSize();
        const float width = AlertDialog::maxWidth();
        const float height = visibleSize.height * 0.8f - 80.0f;

        this->setContentSize(Size(width, height));

        // 表格
        cw::TableView *tableView = cw::TableView::create();
        tableView->setDirection(ui::ScrollView::Direction::VERTICAL);
        tableView->setScrollBarPositionFromCorner(Vec2(2.0f, 2.0f));
        tableView->setScrollBarWidth(4.0f);
        tableView->setScrollBarOpacity(0x99);
        tableView->setContentSize(Size(width, height));
        tableView->setDelegate(this);
        tableView->setVerticalFillOrder(cw::TableView::VerticalFillOrder::TOP_DOWN);

        tableView->setAnchorPoint(Vec2::ANCHOR_MIDDLE);
        tableView->setPosition(Vec2(width * 0.5f, height * 0.5f));
        tableView->reloadData();
        this->addChild(tableView);

        return true;
    }

    ssize_t BatchDeleteTableNode::numberOfCellsInTableView(cw::TableView *) {
        return g_records.size();
    }

    float BatchDeleteTableNode::tableCellSizeForIndex(cw::TableView *, ssize_t) {
        return 70.0f;
    }

    cw::TableViewCell *BatchDeleteTableNode::tableCellAtIndex(cw::TableView *table, ssize_t idx) {
        typedef cw::TableViewCellEx<Label *, ui::CheckBox *, std::array<LayerColor *, 2> > CustomCell;
        CustomCell *cell = (CustomCell *)table->dequeueCell();

        const float cellWidth = AlertDialog::maxWidth();

        if (cell == nullptr) {
            cell = CustomCell::create();

            CustomCell::ExtDataType &ext = cell->getExtData();
            Label *&label = std::get<0>(ext);
            ui::CheckBox *&checkBox = std::get<1>(ext);
            std::array<LayerColor *, 2> &layerColors = std::get<2>(ext);

            // 背景色
            layerColors[0] = LayerColor::create(Color4B(0x10, 0x10, 0x10, 0x10), cellWidth, 70.0f);
            cell->addChild(layerColors[0]);

            layerColors[1] = LayerColor::create(Color4B(0xC0, 0xC0, 0xC0, 0x10), cellWidth, 70.0f);
            cell->addChild(layerColors[1]);

            label = Label::createWithSystemFont("", "Arail", 10);
            label->setColor(Color3B::BLACK);
            cell->addChild(label);
            label->setAnchorPoint(Vec2::ANCHOR_MIDDLE_LEFT);
            label->setPosition(Vec2(2.0f, 35.0f));

            checkBox = UICommon::createCheckBox();
            cell->addChild(checkBox);
            checkBox->setZoomScale(0.0f);
            checkBox->ignoreContentAdaptWithSize(false);
            checkBox->setContentSize(Size(15.0f, 15.0f));
            checkBox->setPosition(Vec2(cellWidth - 20.0f, 30.0f));
            checkBox->addEventListener([this](Ref *sender, ui::CheckBox::EventType) {
                ui::CheckBox *checkBox = (ui::CheckBox *)sender;
                ssize_t idx = reinterpret_cast<ssize_t>(checkBox->getUserData());
                _currentFlags[idx] = checkBox->isSelected();
            });
        }

        const CustomCell::ExtDataType &ext = cell->getExtData();
        Label *label = std::get<0>(ext);
        ui::CheckBox *checkBox = std::get<1>(ext);
        const std::array<LayerColor *, 2> &layerColors = std::get<2>(ext);

        layerColors[0]->setVisible(!(idx & 1));
        layerColors[1]->setVisible(!!(idx & 1));

        const Record &record = g_records[idx];
        bool finished = record.end_time != 0;

        label->setString(_texts[idx]);
        cw::scaleLabelToFitWidth(label, cellWidth - 4.0f);

        // 设置选中
        checkBox->setUserData(reinterpret_cast<void *>(idx));
        checkBox->setSelected(_currentFlags[idx]);
        checkBox->setEnabled(finished);

        return cell;
    }
}

void RecordHistoryScene::onBatchDeleteButton(cocos2d::Ref *) {
    BatchDeleteTableNode *rootNode = BatchDeleteTableNode::create(_recordTexts);
    AlertDialog::Builder(this)
        .setTitle("选择要删除的对局")
        .setContentNode(rootNode)
        .setNegativeButton("取消", nullptr)
        .setPositiveButton("确定", [this, rootNode](AlertDialog *dlg, int) {
        auto currentFlags = std::make_shared<std::vector<bool> >(rootNode->getCurrentFlags());

        AlertDialog::Builder(this)
            .setTitle("删除记录")
            .setMessage("删除后无法找回，确认删除？")
            .setNegativeButton("取消", nullptr)
            .setPositiveButton("确定", [this, currentFlags, dlg](AlertDialog *, int) {
            LoadingView *loadingView = LoadingView::create();
            this->addChild(loadingView);
            loadingView->setPosition(Director::getInstance()->getVisibleSize());

            for (size_t i = currentFlags->size(); i-- > 0; ) {
                if (currentFlags->at(i)) {
                    g_records.erase(g_records.begin() + i);
                }
            }

            auto thiz = makeRef(this);  // 保证线程回来之前不析构
            auto temp = std::make_shared<std::vector<Record> >(g_records);
            std::thread([thiz, temp, loadingView](){
                saveRecords(*temp);

                // 切换到cocos线程
                Director::getInstance()->getScheduler()->performFunctionInCocosThread([thiz, loadingView]() {
                    if (LIKELY(thiz->isRunning())) {
                        thiz->updateRecordTexts();
                        loadingView->removeFromParent();
                        thiz->_tableView->reloadDataInplacement();
                    }
                });
            }).detach();
            dlg->dismiss();
            return true;
        }).create()->show();
        return false;
    }).create()->show();
}

void RecordHistoryScene::onCellClicked(cocos2d::Ref *sender) {
    cw::TableViewCell *cell = (cw::TableViewCell *)sender;
    _viewCallback(&g_records[cell->getIdx()]);
}

void RecordHistoryScene::modifyRecord(const Record *record) {
    if (UNLIKELY(g_records.empty())) {  // 如果当前没有加载过历史记录
        auto r = std::make_shared<Record>(*record);  // 复制当前记录
        // 子线程中加载、修改、保存
        std::thread([r]() {
            auto records = std::make_shared<std::vector<Record> >();
            loadRecords(*records);
            ModifyRecordInHistory(*records, r.get());
            saveRecords(*records);

            // 切换到主线程，覆盖整个历史记录
            Director::getInstance()->getScheduler()->performFunctionInCocosThread([records]() {
                g_records.swap(*records);
            });
        }).detach();
    }
    else {  // 如果当前加载过历史记录
        // 直接修改
        ModifyRecordInHistory(g_records, record);

        // 子线程中保存
        auto temp = std::make_shared<std::vector<Record> >(g_records);
        std::thread([temp]() {
            saveRecords(*temp);
        }).detach();
    }
}
