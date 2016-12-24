﻿#ifndef _CW_EDIT_BOX_DELETATE_H_
#define _CW_EDIT_BOX_DELETATE_H_

#include "ui/UIEditBox/UIEditBox.h"

namespace cw {
    class EditBoxDelegate : public cocos2d::ui::EditBoxDelegate {
    public:
        EditBoxDelegate(const std::function<void (cocos2d::ui::EditBox *editBox)> &editBoxReturn)
            : _editBoxReturn(editBoxReturn) { }

        EditBoxDelegate(std::function<void (cocos2d::ui::EditBox *editBox)> &&editBoxReturn)
            : _editBoxReturn(editBoxReturn) { }

        EditBoxDelegate(const EditBoxDelegate &other)
            : _editBoxReturn(other._editBoxReturn) { }

        EditBoxDelegate(EditBoxDelegate &&other)
            : _editBoxReturn(std::move(other._editBoxReturn)) { }

        EditBoxDelegate &operator=(const EditBoxDelegate &other) {
            _editBoxReturn = other._editBoxReturn;
            return *this;
        }

        EditBoxDelegate &operator=(EditBoxDelegate &&other) {
            _editBoxReturn = std::move(other._editBoxReturn);
            return *this;
        }

        virtual ~EditBoxDelegate() { CCLOG("%s", __FUNCTION__); }

    private:
        virtual void editBoxReturn(cocos2d::ui::EditBox *editBox) override {
            _editBoxReturn(editBox);
        }
        std::function<void (cocos2d::ui::EditBox *editBox)> _editBoxReturn;
    };
}

#endif