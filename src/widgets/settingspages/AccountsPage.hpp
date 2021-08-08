#pragma once

#include "widgets/AccountSwitchWidget.hpp"
#include "widgets/settingspages/SettingsPage.hpp"

#include <QPushButton>

namespace chatterino {

class AccountsPage : public SettingsPage
{
public:
    AccountsPage();

    void refreshButtons();
    void AnimatedSave(auto label);

    QPushButton* clearFieldsButton;
    QPushButton* accountsSettingsButton;
    QComboBox* combo;

    QLineEdit followHashInput;
    QLineEdit unfollowHashInput;
    QLineEdit OAuthTokenInput;

private:
    QPushButton *addButton_;
    QPushButton *removeButton_;
    AccountSwitchWidget *accountSwitchWidget_;
};

}  // namespace chatterino
