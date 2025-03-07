#include "HighlightingPage.hpp"

#include "Application.hpp"
#include "controllers/highlights/BadgeHighlightModel.hpp"
#include "controllers/highlights/HighlightBlacklistModel.hpp"
#include "controllers/highlights/HighlightModel.hpp"
#include "controllers/highlights/UserHighlightModel.hpp"
#include "singletons/Settings.hpp"
#include "singletons/Theme.hpp"
#include "util/LayoutCreator.hpp"
#include "util/StandardItemHelper.hpp"
#include "widgets/dialogs/BadgePickerDialog.hpp"
#include "widgets/dialogs/ColorPickerDialog.hpp"
#include "widgets/dialogs/SelectChannelHighlightPopup.hpp"

#include <QFileDialog>
#include <QHeaderView>
#include <QListWidget>
#include <QPushButton>
#include <QStandardItemModel>
#include <QTabWidget>
#include <QTableView>
#include <QTextEdit>

#define ENABLE_HIGHLIGHTS "Enable Highlighting"
#define HIGHLIGHT_MSG "Highlight messages containing your name"
#define PLAY_SOUND "Play sound when your name is mentioned"
#define FLASH_TASKBAR "Flash taskbar when your name is mentioned"
#define ALWAYS_PLAY "Play highlight sound even when Chatterino is focused"

namespace chatterino {

namespace {
    // Add additional badges for highlights here
    QList<DisplayBadge> availableBadges = {
        {"Broadcaster", "broadcaster"},
        {"Admin", "admin"},
        {"Staff", "staff"},
        {"Moderator", "moderator"},
        {"Verified", "partner"},
        {"VIP", "vip"},
        {"Predicted Blue", "predictions/blue-1,predictions/blue-2"},
        {"Predicted Pink", "predictions/pink-2,predictions/pink-1"},
    };
}  // namespace

HighlightingPage::HighlightingPage()
{
    LayoutCreator<HighlightingPage> layoutCreator(this);

    auto layout = layoutCreator.emplace<QVBoxLayout>().withoutMargin();
    {
        // GENERAL
        // layout.append(this->createCheckBox(ENABLE_HIGHLIGHTS,
        // getSettings()->enableHighlights));

        // TABS
        auto tabs = layout.emplace<QTabWidget>();
        {
            // HIGHLIGHTS
            auto highlights = tabs.appendTab(new QVBoxLayout, "Messages");
            {
                highlights.emplace<QLabel>(
                    "Play notification sounds and highlight messages based on "
                    "certain patterns.\n"
                    "Message highlights are prioritized over badge highlights, "
                    "but under user highlights");

                auto view =
                    highlights
                        .emplace<EditableModelView>(
                            (new HighlightModel(nullptr))
                                ->initialized(
                                    &getSettings()->highlightedMessages))
                        .getElement();

                view->addSelectChannelHighlight();
                view->addExcludeChannelHighlight();
                view->addRegexHelpLink();
                view->setTitles({"Pattern", "Show in\nMentions",
                                 "Flash\ntaskbar", "Play\nsound",
                                 "Enable\nregex", "Case-\nsensitive",
                                 "Custom\nsound", "Color"});
                view->getTableView()->horizontalHeader()->setSectionResizeMode(
                    QHeaderView::Fixed);
                view->getTableView()->horizontalHeader()->setSectionResizeMode(
                    0, QHeaderView::Stretch);

                // fourtf: make class extrend BaseWidget and add this to
                // dpiChanged
                QTimer::singleShot(1, [view] {
                    view->getTableView()->resizeColumnsToContents();
                    view->getTableView()->setColumnWidth(0, 200);
                });

                view->addButtonPressed.connect([] {
                    getSettings()->highlightedMessages.append(HighlightPhrase{
                        "my phrase", true, true, false, false, false, "",
                        *ColorProvider::instance().color(
                            ColorType::SelfHighlight)});
                });

                view->selectChannelPressed.connect([this, view] {
                    int selected = view->getTableView()
                                       ->selectionModel()
                                       ->currentIndex()
                                       .row() -
                                   5;

                    auto selectUsernameWidget =
                        new SelectChannelWidget(selected, "messages");

                    selectUsernameWidget->show();
                    selectUsernameWidget->raise();
                });

                view->excludeChannelPressed.connect([this, view] {
                    int selected = view->getTableView()
                                       ->selectionModel()
                                       ->currentIndex()
                                       .row() -
                                   5;

                    auto excludeChannelWidget =
                        new ExcludeChannelWidget(selected, "messages");

                    excludeChannelWidget->show();
                    excludeChannelWidget->raise();
                });

                QObject::connect(view->getTableView(), &QTableView::clicked,
                                 [this, view](const QModelIndex &clicked) {
                                     this->tableCellClicked(
                                         clicked, view, HighlightTab::Messages);
                                 });
            }

            auto pingUsers = tabs.appendTab(new QVBoxLayout, "Users");
            {
                pingUsers.emplace<QLabel>(
                    "Play notification sounds and highlight messages from "
                    "certain users.\n"
                    "User highlights are prioritized over message and badge "
                    "highlights.");
                EditableModelView *view =
                    pingUsers
                        .emplace<EditableModelView>(
                            (new UserHighlightModel(nullptr))
                                ->initialized(&getSettings()->highlightedUsers))
                        .getElement();

                view->addSelectChannelHighlight();
                view->addExcludeChannelHighlight();
                view->enableSelectChannelButton();
                view->enableExcludeChannelButton();
                view->addRegexHelpLink();
                view->getTableView()->horizontalHeader()->hideSection(
                    HighlightModel::Column::UseRegex);
                view->getTableView()->horizontalHeader()->hideSection(
                    HighlightModel::Column::CaseSensitive);
                // Case-sensitivity doesn't make sense for user names so it is
                // set to "false" by default & the column is hidden
                view->setTitles({"Username", "Show in\nMentions",
                                 "Flash\ntaskbar", "Play\nsound",
                                 "Enable\nregex", "Case-\nsensitive",
                                 "Custom\nsound", "Color"});
                view->getTableView()->horizontalHeader()->setSectionResizeMode(
                    QHeaderView::Fixed);
                view->getTableView()->horizontalHeader()->setSectionResizeMode(
                    0, QHeaderView::Stretch);

                // fourtf: make class extrend BaseWidget and add this to
                // dpiChanged
                QTimer::singleShot(1, [view] {
                    view->getTableView()->resizeColumnsToContents();
                    view->getTableView()->setColumnWidth(0, 200);
                });

                view->addButtonPressed.connect([] {
                    getSettings()->highlightedUsers.append(HighlightPhrase{
                        "highlighted user", true, true, false, false, false, "",
                        *ColorProvider::instance().color(
                            ColorType::SelfHighlight)});
                });

                view->selectChannelPressed.connect([this, view] {
                    int selected = view->getTableView()
                                       ->selectionModel()
                                       ->currentIndex()
                                       .row();

                    qDebug() << selected;

                    auto selectUsernameWidget =
                        new SelectChannelWidget(selected, "users");

                    selectUsernameWidget->show();
                    selectUsernameWidget->raise();
                });

                view->excludeChannelPressed.connect([this, view] {
                    int selected = view->getTableView()
                                       ->selectionModel()
                                       ->currentIndex()
                                       .row();

                    auto excludeChannelWidget =
                        new ExcludeChannelWidget(selected, "users");

                    excludeChannelWidget->show();
                    excludeChannelWidget->raise();
                });

                QObject::connect(view->getTableView(), &QTableView::clicked,
                                 [this, view](const QModelIndex &clicked) {
                                     this->tableCellClicked(
                                         clicked, view, HighlightTab::Users);
                                 });
            }

            auto badgeHighlights = tabs.appendTab(new QVBoxLayout, "Badges");
            {
                badgeHighlights.emplace<QLabel>(
                    "Play notification sounds and highlight messages based on "
                    "user badges.\n"
                    "Badge highlights are prioritzed under user and message "
                    "highlights.");
                auto view = badgeHighlights
                                .emplace<EditableModelView>(
                                    (new BadgeHighlightModel(nullptr))
                                        ->initialized(
                                            &getSettings()->highlightedBadges))
                                .getElement();
                view->setTitles({"Name", "Show in\nMentions", "Flash\ntaskbar",
                                 "Play\nsound", "Custom\nsound", "Color"});
                view->getTableView()->horizontalHeader()->setSectionResizeMode(
                    QHeaderView::Fixed);
                view->getTableView()->horizontalHeader()->setSectionResizeMode(
                    0, QHeaderView::Stretch);

                // fourtf: make class extrend BaseWidget and add this to
                // dpiChanged
                QTimer::singleShot(1, [view] {
                    view->getTableView()->resizeColumnsToContents();
                    view->getTableView()->setColumnWidth(0, 200);
                });

                view->addButtonPressed.connect([this] {
                    auto d = std::make_shared<BadgePickerDialog>(
                        availableBadges, this);

                    d->setWindowTitle("Choose badge");
                    if (d->exec() == QDialog::Accepted)
                    {
                        auto s = d->getSelection();
                        if (!s)
                        {
                            return;
                        }
                        getSettings()->highlightedBadges.append(
                            HighlightBadge{s->badgeName(), true,
                                           s->displayName(), false, false, "",
                                           *ColorProvider::instance().color(
                                               ColorType::SelfHighlight)});
                    }
                });

                QObject::connect(view->getTableView(), &QTableView::clicked,
                                 [this, view](const QModelIndex &clicked) {
                                     this->tableCellClicked(
                                         clicked, view, HighlightTab::Badges);
                                 });
            }

            auto disabledUsers =
                tabs.appendTab(new QVBoxLayout, "Blacklisted Users");
            {
                disabledUsers.emplace<QLabel>(
                    "Disable notification sounds and highlights from certain "
                    "users (e.g. bots).");
                EditableModelView *view =
                    disabledUsers
                        .emplace<EditableModelView>(
                            (new HighlightBlacklistModel(nullptr))
                                ->initialized(&getSettings()->blacklistedUsers))
                        .getElement();

                view->addRegexHelpLink();
                view->setTitles({"Username", "Enable\nregex"});
                view->getTableView()->horizontalHeader()->setSectionResizeMode(
                    QHeaderView::Fixed);
                view->getTableView()->horizontalHeader()->setSectionResizeMode(
                    0, QHeaderView::Stretch);

                // fourtf: make class extrend BaseWidget and add this to
                // dpiChanged
                QTimer::singleShot(1, [view] {
                    view->getTableView()->resizeColumnsToContents();
                    view->getTableView()->setColumnWidth(0, 200);
                });

                view->addButtonPressed.connect([] {
                    getSettings()->blacklistedUsers.append(
                        HighlightBlacklistUser{"blacklisted user", false});
                });
            }
        }

        // MISC
        auto customSound = layout.emplace<QHBoxLayout>().withoutMargin();
        {
            auto fallbackSound = customSound.append(this->createCheckBox(
                "Fallback sound (played when no other sound is set)",
                getSettings()->customHighlightSound));

            auto getSelectFileText = [] {
                const QString value = getSettings()->pathHighlightSound;
                return value.isEmpty() ? "Select custom fallback sound"
                                       : QUrl::fromLocalFile(value).fileName();
            };

            auto selectFile =
                customSound.emplace<QPushButton>(getSelectFileText());

            QObject::connect(
                selectFile.getElement(), &QPushButton::clicked, this,
                [=]() mutable {
                    auto fileName = QFileDialog::getOpenFileName(
                        this, tr("Open Sound"), "",
                        tr("Audio Files (*.mp3 *.wav)"));

                    getSettings()->pathHighlightSound = fileName;
                    selectFile.getElement()->setText(getSelectFileText());

                    // Set check box according to updated value
                    fallbackSound->setCheckState(
                        fileName.isEmpty() ? Qt::Unchecked : Qt::Checked);
                });
        }

        layout.append(createCheckBox(ALWAYS_PLAY,
                                     getSettings()->highlightAlwaysPlaySound));
        layout.append(createCheckBox(
            "Flash taskbar only stops highlighting when Chatterino is focused",
            getSettings()->longAlerts));
    }

    // ---- misc
    this->disabledUsersChangedTimer_.setSingleShot(true);
}

void HighlightingPage::openSoundDialog(const QModelIndex &clicked,
                                       EditableModelView *view, int soundColumn)
{
    auto fileUrl = QFileDialog::getOpenFileUrl(this, tr("Open Sound"), QUrl(),
                                               tr("Audio Files (*.mp3 *.wav)"));
    view->getModel()->setData(clicked, fileUrl, Qt::UserRole);
    view->getModel()->setData(clicked, fileUrl.fileName(), Qt::DisplayRole);

    // Enable custom sound check box if user set a sound
    if (!fileUrl.isEmpty())
    {
        QModelIndex checkBox = clicked.siblingAtColumn(soundColumn);
        view->getModel()->setData(checkBox, Qt::Checked, Qt::CheckStateRole);
    }
}

void HighlightingPage::openColorDialog(const QModelIndex &clicked,
                                       EditableModelView *view,
                                       HighlightTab tab)
{
    auto initial =
        view->getModel()->data(clicked, Qt::DecorationRole).value<QColor>();

    auto dialog = new ColorPickerDialog(initial, this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
    dialog->closed.connect([=](auto selected) {
        if (selected.isValid())
        {
            view->getModel()->setData(clicked, selected, Qt::DecorationRole);

            if (tab == HighlightTab::Messages)
            {
                /*
                 * For preset highlights in the "Messages" tab, we need to
                 * manually update the color map.
                 */
                auto instance = ColorProvider::instance();
                switch (clicked.row())
                {
                    case 0:
                        instance.updateColor(ColorType::SelfHighlight,
                                             selected);
                        break;
                    case 1:
                        instance.updateColor(ColorType::Whisper, selected);
                        break;
                    case 2:
                        instance.updateColor(ColorType::Subscription, selected);
                        break;
                }
            }
        }
    });
}

void HighlightingPage::tableCellClicked(const QModelIndex &clicked,
                                        EditableModelView *view,
                                        HighlightTab tab)
{
    switch (tab)
    {
        case HighlightTab::Messages:
            if (clicked.row() >= 5)
            {
                view->enableSelectChannelButton();
                view->enableExcludeChannelButton();
            }
            else
            {
                view->disableSelectChannelButton();
                view->disableExcludeChannelButton();
            }
        case HighlightTab::Users: {
            using Column = HighlightModel::Column;
            bool restrictColorRow =
                (tab == HighlightTab::Messages &&
                 clicked.row() == HighlightModel::WHISPER_ROW);
            if (clicked.column() == Column::SoundPath)
            {
                this->openSoundDialog(clicked, view, Column::SoundPath);
            }
            else if (clicked.column() == Column::Color && !restrictColorRow)
            {
                this->openColorDialog(clicked, view, tab);
            }
        }
        break;

        case HighlightTab::Badges: {
            using Column = BadgeHighlightModel::Column;
            if (clicked.column() == Column::SoundPath)
            {
                this->openSoundDialog(clicked, view, Column::SoundPath);
            }
            else if (clicked.column() == Column::Color)
            {
                this->openColorDialog(clicked, view, tab);
            }
        }
        break;
    }
}

}  // namespace chatterino
