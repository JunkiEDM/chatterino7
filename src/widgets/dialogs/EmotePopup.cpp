#include "EmotePopup.hpp"

#include "Application.hpp"
#include "common/CompletionModel.hpp"
#include "common/QLogging.hpp"
#include "controllers/accounts/AccountController.hpp"
#include "controllers/hotkeys/HotkeyController.hpp"
#include "debug/Benchmark.hpp"
#include "messages/Message.hpp"
#include "messages/MessageBuilder.hpp"
#include "providers/twitch/TwitchChannel.hpp"
#include "providers/twitch/TwitchIrcServer.hpp"
#include "singletons/Emotes.hpp"
#include "singletons/WindowManager.hpp"
#include "widgets/Notebook.hpp"
#include "widgets/Scrollbar.hpp"
#include "widgets/helper/ChannelView.hpp"

#include <QHBoxLayout>
#include <QTabWidget>

namespace chatterino {
namespace {
    auto makeTitleMessage(const QString &title)
    {
        MessageBuilder builder;
        builder.emplace<TextElement>(title, MessageElementFlag::Text);
        builder->flags.set(MessageFlag::Centered);
        return builder.release();
    }
    auto makeEmoteMessage(const EmoteMap &map,
                          const MessageElementFlag &emoteFlag)
    {
        MessageBuilder builder;
        builder->flags.set(MessageFlag::Centered);
        builder->flags.set(MessageFlag::DisableCompactEmotes);

        if (map.empty())
        {
            builder.emplace<TextElement>("no emotes available",
                                         MessageElementFlag::Text,
                                         MessageColor::System);
            return builder.release();
        }

        std::vector<std::pair<EmoteName, EmotePtr>> vec(map.begin(), map.end());
        std::sort(vec.begin(), vec.end(),
                  [](const std::pair<EmoteName, EmotePtr> &l,
                     const std::pair<EmoteName, EmotePtr> &r) {
                      return CompletionModel::compareStrings(l.first.string,
                                                             r.first.string);
                  });
        for (const auto &emote : vec)
        {
            builder
                .emplace<EmoteElement>(
                    emote.second,
                    MessageElementFlags{MessageElementFlag::AlwaysShow,
                                        emoteFlag})
                ->setLink(Link(Link::InsertText, emote.first.string));
        }

        return builder.release();
    }
    void addEmoteSets(
        std::vector<std::shared_ptr<TwitchAccount::EmoteSet>> sets,
        Channel &globalChannel, Channel &subChannel, QString currentChannelName)
    {
        QMap<QString, QPair<bool, std::vector<MessagePtr>>> mapOfSets;

        for (const auto &set : sets)
        {
            // Some emotes (e.g. follower ones) are only available in their origin channel
            if (set->local && currentChannelName != set->channelName)
            {
                continue;
            }

            // TITLE
            auto channelName = set->channelName;
            auto text = set->text.isEmpty() ? "Twitch" : set->text;

            // EMOTES
            MessageBuilder builder;
            builder->flags.set(MessageFlag::Centered);
            builder->flags.set(MessageFlag::DisableCompactEmotes);

            // If value of map is empty, create init pair and add title.
            if (mapOfSets.find(channelName) == mapOfSets.end())
            {
                std::vector<MessagePtr> b;
                b.push_back(makeTitleMessage(text));
                mapOfSets[channelName] = qMakePair(set->key == "0", b);
            }

            for (const auto &emote : set->emotes)
            {
                builder
                    .emplace<EmoteElement>(
                        getApp()->emotes->twitch.getOrCreateEmote(emote.id,
                                                                  emote.name),
                        MessageElementFlags{MessageElementFlag::AlwaysShow,
                                            MessageElementFlag::TwitchEmote})
                    ->setLink(Link(Link::InsertText, emote.name.string));
            }

            mapOfSets[channelName].second.push_back(builder.release());
        }

        // Output to channel all created messages,
        // That contain title or emotes.
        // Put current channel emotes at the top
        auto currentChannelPair = mapOfSets[currentChannelName];
        for (auto message : currentChannelPair.second)
        {
            subChannel.addMessage(message);
        }
        mapOfSets.remove(currentChannelName);

        foreach (auto pair, mapOfSets)
        {
            auto &channel = pair.first ? globalChannel : subChannel;
            for (auto message : pair.second)
            {
                channel.addMessage(message);
            }
        }
    }
}  // namespace

EmotePopup::EmotePopup(QWidget *parent)
    : BasePopup(BaseWindow::EnableCustomFrame, parent)
{
    this->setStayInScreenRect(true);
    this->moveTo(this, getApp()->windows->emotePopupPos(), false);

    auto layout = new QVBoxLayout(this);
    this->getLayoutContainer()->setLayout(layout);

    this->notebook_ = new Notebook(this);
    layout->addWidget(this->notebook_);
    layout->setMargin(0);

    auto clicked = [this](const Link &link) {
        this->linkClicked.invoke(link);
    };

    auto makeView = [&](QString tabTitle) {
        auto view = new ChannelView();

        view->setOverrideFlags(MessageElementFlags{
            MessageElementFlag::Default, MessageElementFlag::AlwaysShow,
            MessageElementFlag::EmoteImages});
        view->setEnableScrollingToBottom(false);
        this->notebook_->addPage(view, tabTitle);
        view->linkClicked.connect(clicked);

        return view;
    };

    this->subEmotesView_ = makeView("Subs");
    this->channelEmotesView_ = makeView("Channel");
    this->globalEmotesView_ = makeView("Global");
    this->viewEmojis_ = makeView("Emojis");

    this->loadEmojis();
    this->addShortcuts();
    this->signalHolder_.managedConnect(getApp()->hotkeys->onItemsUpdated,
                                       [this]() {
                                           this->clearShortcuts();
                                           this->addShortcuts();
                                       });
}
void EmotePopup::addShortcuts()
{
    HotkeyController::HotkeyMap actions{
        {"openTab",  // CTRL + 1-8 to open corresponding tab.
         [this](std::vector<QString> arguments) -> QString {
             if (arguments.size() == 0)
             {
                 qCWarning(chatterinoHotkeys)
                     << "openTab shortcut called without arguments. Takes "
                        "only one argument: tab specifier";
                 return "openTab shortcut called without arguments. "
                        "Takes only one argument: tab specifier";
             }
             auto target = arguments.at(0);
             if (target == "last")
             {
                 this->notebook_->selectLastTab();
             }
             else if (target == "next")
             {
                 this->notebook_->selectNextTab();
             }
             else if (target == "previous")
             {
                 this->notebook_->selectPreviousTab();
             }
             else
             {
                 bool ok;
                 int result = target.toInt(&ok);
                 if (ok)
                 {
                     this->notebook_->selectIndex(result);
                 }
                 else
                 {
                     qCWarning(chatterinoHotkeys)
                         << "Invalid argument for openTab shortcut";
                     return QString("Invalid argument for openTab "
                                    "shortcut: \"%1\". Use \"last\", "
                                    "\"next\", \"previous\" or an integer.")
                         .arg(target);
                 }
             }
             return "";
         }},
        {"delete",
         [this](std::vector<QString>) -> QString {
             this->close();
             return "";
         }},
        {"scrollPage",
         [this](std::vector<QString> arguments) -> QString {
             if (arguments.size() == 0)
             {
                 qCWarning(chatterinoHotkeys)
                     << "scrollPage hotkey called without arguments!";
                 return "scrollPage hotkey called without arguments!";
             }
             auto direction = arguments.at(0);
             auto channelView = dynamic_cast<ChannelView *>(
                 this->notebook_->getSelectedPage());

             auto &scrollbar = channelView->getScrollBar();
             if (direction == "up")
             {
                 scrollbar.offset(-scrollbar.getLargeChange());
             }
             else if (direction == "down")
             {
                 scrollbar.offset(scrollbar.getLargeChange());
             }
             else
             {
                 qCWarning(chatterinoHotkeys) << "Unknown scroll direction";
             }
             return "";
         }},

        {"reject", nullptr},
        {"accept", nullptr},
        {"search", nullptr},
    };

    this->shortcuts_ = getApp()->hotkeys->shortcutsForCategory(
        HotkeyCategory::PopupWindow, actions, this);
}

void EmotePopup::loadChannel(ChannelPtr _channel)
{
    BenchmarkGuard guard("loadChannel");

    this->setWindowTitle("Emotes in #" + _channel->getName());

    auto twitchChannel = dynamic_cast<TwitchChannel *>(_channel.get());
    if (twitchChannel == nullptr)
        return;

    auto addEmotes = [&](Channel &channel, const EmoteMap &map,
                         const QString &title,
                         const MessageElementFlag &emoteFlag) {
        channel.addMessage(makeTitleMessage(title));
        channel.addMessage(makeEmoteMessage(map, emoteFlag));
    };

    auto subChannel = std::make_shared<Channel>("", Channel::Type::None);
    auto globalChannel = std::make_shared<Channel>("", Channel::Type::None);
    auto channelChannel = std::make_shared<Channel>("", Channel::Type::None);

    // global
    if (getSettings()->enableHomiesGlobalEmotes)
    {
        addEmotes(*globalChannel,
                  *getApp()->twitch2->getHomiesEmotes().emotes(), "Homies",
                  MessageElementFlag::HomiesEmote);
    }

    if (getSettings()->enable7TVGlobalEmotes)
    {
        addEmotes(*globalChannel,
                  *getApp()->twitch2->getSeventvEmotes().emotes(), "7TV",
                  MessageElementFlag::SeventvEmote);
    }

    if (getSettings()->enableBTTVGlobalEmotes)
    {
        addEmotes(*globalChannel, *getApp()->twitch2->getBttvEmotes().emotes(),
                  "BetterTTV", MessageElementFlag::BttvEmote);
    }

    if (getSettings()->enableFFZGlobalEmotes)
    {
        addEmotes(*globalChannel, *getApp()->twitch2->getFfzEmotes().emotes(),
                  "FrankerFaceZ", MessageElementFlag::FfzEmote);
    }

    // twitch
    addEmoteSets(
        getApp()->accounts->twitch.getCurrent()->accessEmotes()->emoteSets,
        *globalChannel, *subChannel, _channel->getName());

    // channel
    addEmotes(*channelChannel, *twitchChannel->homiesEmotes(), "Homies",
              MessageElementFlag::HomiesEmote);
    addEmotes(*channelChannel, *twitchChannel->seventvEmotes(), "7TV",
              MessageElementFlag::SeventvEmote);
    addEmotes(*channelChannel, *twitchChannel->bttvEmotes(), "BetterTTV",
              MessageElementFlag::BttvEmote);
    addEmotes(*channelChannel, *twitchChannel->ffzEmotes(), "FrankerFaceZ",
              MessageElementFlag::FfzEmote);

    this->globalEmotesView_->setChannel(globalChannel);
    this->subEmotesView_->setChannel(subChannel);
    this->channelEmotesView_->setChannel(channelChannel);

    if (subChannel->getMessageSnapshot().size() == 0)
    {
        MessageBuilder builder;
        builder->flags.set(MessageFlag::Centered);
        builder->flags.set(MessageFlag::DisableCompactEmotes);
        builder.emplace<TextElement>("no subscription emotes available",
                                     MessageElementFlag::Text,
                                     MessageColor::System);
        subChannel->addMessage(builder.release());
    }
}

void EmotePopup::loadEmojis()
{
    auto &emojis = getApp()->emotes->emojis.emojis;

    ChannelPtr emojiChannel(new Channel("", Channel::Type::None));

    // emojis
    MessageBuilder builder;
    builder->flags.set(MessageFlag::Centered);
    builder->flags.set(MessageFlag::DisableCompactEmotes);

    emojis.each([&builder](const auto &key, const auto &value) {
        builder
            .emplace<EmoteElement>(
                value->emote,
                MessageElementFlags{MessageElementFlag::AlwaysShow,
                                    MessageElementFlag::EmojiAll})
            ->setLink(
                Link(Link::Type::InsertText, ":" + value->shortCodes[0] + ":"));
    });
    emojiChannel->addMessage(builder.release());

    this->viewEmojis_->setChannel(emojiChannel);
}

void EmotePopup::closeEvent(QCloseEvent *event)
{
    getApp()->windows->setEmotePopupPos(this->pos());
    QWidget::closeEvent(event);
}
}  // namespace chatterino
