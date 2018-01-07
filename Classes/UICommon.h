#ifndef __UI_COMMON_H__
#define __UI_COMMON_H__

#include "cocos2d.h"
#include "ui/CocosGUI.h"

namespace UICommon {

    static inline cocos2d::ui::Button *createButton() {
        return cocos2d::ui::Button::create("source_material/btn_square_highlighted.png", "source_material/btn_square_selected.png");
    }

    static inline cocos2d::ui::CheckBox *createCheckBox() {
        return cocos2d::ui::CheckBox::create("source_material/btn_square_normal.png", "source_material/btn_square_highlighted.png");
    }

    static inline cocos2d::ui::RadioButton *createRadioButton() {
        return cocos2d::ui::RadioButton::create("source_material/btn_square_normal.png", "source_material/btn_square_highlighted.png");
    }
}

#endif
