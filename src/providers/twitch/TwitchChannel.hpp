#pragma once

#include "common/Aliases.hpp"
#include "common/Atomic.hpp"
#include "common/Channel.hpp"
#include "common/ChannelChatters.hpp"
#include "common/ChatterSet.hpp"
#include "common/Outcome.hpp"
#include "common/UniqueAccess.hpp"
#include "providers/twitch/ChannelPointReward.hpp"
#include "providers/twitch/TwitchEmotes.hpp"
#include "providers/twitch/api/Helix.hpp"
#include "util/QStringHash.hpp"

#include <QColor>
#include <QElapsedTimer>
#include <QRegularExpression>
#include <boost/optional.hpp>
#include <pajlada/signals/signalholder.hpp>

#include <mutex>
#include <unordered_map>

namespace chatterino {

enum class HighlightState;

struct Emote;
using EmotePtr = std::shared_ptr<const Emote>;
class EmoteMap;

class TwitchBadges;
class SeventvEmotes;
class HomiesEmotes;
class FfzEmotes;
class BttvEmotes;

class TwitchIrcServer;

class TwitchChannel : public Channel, public ChannelChatters
{
public:
    struct StreamStatus {
        bool live = false;
        bool rerun = false;
        unsigned viewerCount = 0;
        QString title;
        QString game;
        QString gameId;
        QString uptime;
        QString streamType;
    };

    struct RoomModes {
        bool submode = false;
        bool r9k = false;
        bool emoteOnly = false;
        int followerOnly = -1;
        int slowMode = 0;
    };

    void initialize();

    // Channel methods
    virtual bool isEmpty() const override;
    virtual bool canSendMessage() const override;
    virtual void sendMessage(const QString &message) override;
    virtual bool isMod() const override;
    bool isVip() const;
    bool isStaff() const;
    virtual bool isBroadcaster() const override;
    virtual bool hasHighRateLimit() const override;
    virtual bool canReconnect() const override;
    virtual void reconnect() override;
    void refreshTitle();
    void createClip();

    // Data
    const QString &subscriptionUrl();
    const QString &channelUrl();
    const QString &popoutPlayerUrl();
    int chatterCount();
    virtual bool isLive() const override;
    QString roomId() const;
    SharedAccessGuard<const RoomModes> accessRoomModes() const;
    SharedAccessGuard<const StreamStatus> accessStreamStatus() const;

    // Emotes
    boost::optional<EmotePtr> seventvEmote(const EmoteName &name) const;
    boost::optional<EmotePtr> homiesEmote(const EmoteName &name) const;
    boost::optional<EmotePtr> bttvEmote(const EmoteName &name) const;
    boost::optional<EmotePtr> ffzEmote(const EmoteName &name) const;
    std::shared_ptr<const EmoteMap> seventvEmotes() const;
    std::shared_ptr<const EmoteMap> homiesEmotes() const;
    std::shared_ptr<const EmoteMap> bttvEmotes() const;
    std::shared_ptr<const EmoteMap> ffzEmotes() const;

    virtual void refreshBadgesProviders();
    virtual void refresh7TVChannelEmotes(bool manualRefresh);
    virtual void refreshBTTVChannelEmotes(bool manualRefresh);
    virtual void refreshFFZChannelEmotes(bool manualRefresh);
    virtual void refreshHomiesChannelEmotes(bool manualRefresh);

    // Badges
    boost::optional<EmotePtr> ffzCustomModBadge() const;
    boost::optional<EmotePtr> ffzCustomVipBadge() const;
    boost::optional<EmotePtr> twitchBadge(const QString &set,
                                          const QString &version) const;

    // Cheers
    boost::optional<CheerEmote> cheerEmote(const QString &string);

    // Signals
    pajlada::Signals::NoArgSignal roomIdChanged;
    pajlada::Signals::NoArgSignal userStateChanged;
    pajlada::Signals::NoArgSignal liveStatusChanged;
    pajlada::Signals::NoArgSignal roomModesChanged;

    // Channel point rewards
    pajlada::Signals::SelfDisconnectingSignal<ChannelPointReward>
        channelPointRewardAdded;
    void addChannelPointReward(const ChannelPointReward &reward);
    bool isChannelPointRewardKnown(const QString &rewardId);
    boost::optional<ChannelPointReward> channelPointReward(
        const QString &rewardId) const;

private:
    struct NameOptions {
        QString displayName;
        QString localizedName;
    } nameOptions;

protected:
    explicit TwitchChannel(const QString &channelName);

private:
    // Methods
    void refreshLiveStatus();
    void parseLiveStatus(bool live, const HelixStream &stream);
    void refreshPubsub();
    void refreshChatters();
    void refreshBadges();
    void refreshCheerEmotes();
    void loadRecentMessages();
    void fetchDisplayName();

    void setLive(bool newLiveStatus);
    void setMod(bool value);
    void setVIP(bool value);
    void setStaff(bool value);
    void setRoomId(const QString &id);
    void setRoomModes(const RoomModes &roomModes_);
    void setDisplayName(const QString &name);
    void setLocalizedName(const QString &name);

    const QString &getDisplayName() const override;
    const QString &getLocalizedName() const override;

    // Data
    const QString subscriptionUrl_;
    const QString channelUrl_;
    const QString popoutPlayerUrl_;
    int chatterCount_;
    UniqueAccess<StreamStatus> streamStatus_;
    UniqueAccess<RoomModes> roomModes_;

protected:
    Atomic<std::shared_ptr<const EmoteMap>> seventvEmotes_;
    Atomic<std::shared_ptr<const EmoteMap>> homiesEmotes_;
    Atomic<std::shared_ptr<const EmoteMap>> bttvEmotes_;
    Atomic<std::shared_ptr<const EmoteMap>> ffzEmotes_;
    Atomic<boost::optional<EmotePtr>> ffzCustomModBadge_;
    Atomic<boost::optional<EmotePtr>> ffzCustomVipBadge_;

private:
    // Badges
    UniqueAccess<std::map<QString, std::map<QString, EmotePtr>>>
        badgeSets_;  // "subscribers": { "0": ... "3": ... "6": ...
    UniqueAccess<std::vector<CheerEmoteSet>> cheerEmoteSets_;
    UniqueAccess<std::map<QString, ChannelPointReward>> channelPointRewards_;

    bool mod_ = false;
    bool vip_ = false;
    bool staff_ = false;
    UniqueAccess<QString> roomID_;

    // --
    QString lastSentMessage_;
    QObject lifetimeGuard_;
    QTimer liveStatusTimer_;
    QTimer chattersListTimer_;
    QElapsedTimer titleRefreshedTimer_;
    QElapsedTimer clipCreationTimer_;
    bool isClipCreationInProgress{false};

    pajlada::Signals::SignalHolder signalHolder_;

    friend class TwitchIrcServer;
    friend class TwitchMessageBuilder;
    friend class IrcMessageHandler;
};

}  // namespace chatterino
