#include "emotemanager.hpp"
#include "resources.hpp"
#include "util/urlfetch.hpp"
#include "windowmanager.hpp"

#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>

#include <memory>

#define TWITCH_EMOTE_TEMPLATE "https://static-cdn.jtvnw.net/emoticons/v1/{id}/{scale}.0"

using namespace chatterino::messages;

namespace chatterino {

EmoteManager::EmoteManager(WindowManager &_windowManager, Resources &_resources)
    : windowManager(_windowManager)
    , resources(_resources)
{
    // Note: Do not use this->resources in ctor
}

void EmoteManager::loadGlobalEmotes()
{
    this->loadEmojis();
    this->loadBTTVEmotes();
    this->loadFFZEmotes();
}

void EmoteManager::reloadBTTVChannelEmotes(const QString &channelName,
                                           BTTVEmoteMap &channelEmoteMap)
{
    printf("[EmoteManager] Reload BTTV Channel Emotes for channel %s\n", qPrintable(channelName));

    QString url("https://api.betterttv.net/2/channels/" + channelName);
    util::urlFetchJSON(url, [this, &channelEmoteMap](QJsonObject &rootNode) {
        channelEmoteMap.clear();

        auto emotesNode = rootNode.value("emotes").toArray();

        QString linkTemplate = "https:" + rootNode.value("urlTemplate").toString();

        for (const QJsonValue &emoteNode : emotesNode) {
            QJsonObject emoteObject = emoteNode.toObject();

            QString id = emoteObject.value("id").toString();
            QString code = emoteObject.value("code").toString();
            // emoteObject.value("imageType").toString();

            QString link = linkTemplate;
            link.detach();

            link = link.replace("{{id}}", id).replace("{{image}}", "1x");

            auto emote = this->getBTTVChannelEmoteFromCaches().getOrAdd(id, [this, &code, &link] {
                return new LazyLoadedImage(*this, this->windowManager, link, 1, code,
                                           code + "\nChannel BTTV Emote");
            });

            this->bttvChannelEmotes.insert(code, emote);
            channelEmoteMap.insert(code, emote);
        }
    });
}

void EmoteManager::reloadFFZChannelEmotes(
    const QString &channelName,
    ConcurrentMap<QString, messages::LazyLoadedImage *> &channelEmoteMap)
{
    printf("[EmoteManager] Reload FFZ Channel Emotes for channel %s\n", qPrintable(channelName));

    QString url("http://api.frankerfacez.com/v1/room/" + channelName);

    util::urlFetchJSON(url, [this, &channelEmoteMap](QJsonObject &rootNode) {
        channelEmoteMap.clear();

        auto setsNode = rootNode.value("sets").toObject();

        for (const QJsonValue &setNode : setsNode) {
            auto emotesNode = setNode.toObject().value("emoticons").toArray();

            for (const QJsonValue &emoteNode : emotesNode) {
                QJsonObject emoteObject = emoteNode.toObject();

                // margins
                int id = emoteObject.value("id").toInt();
                QString code = emoteObject.value("name").toString();

                QJsonObject urls = emoteObject.value("urls").toObject();
                QString url1 = "http:" + urls.value("1").toString();

                auto emote =
                    this->getFFZChannelEmoteFromCaches().getOrAdd(id, [this, &code, &url1] {
                        return new LazyLoadedImage(*this, this->windowManager, url1, 1, code,
                                                   code + "\nGlobal FFZ Emote");
                    });

                this->ffzChannelEmotes.insert(code, emote);
                channelEmoteMap.insert(code, emote);
            }
        }
    });
}

ConcurrentMap<QString, twitch::EmoteValue *> &EmoteManager::getTwitchEmotes()
{
    return _twitchEmotes;
}

ConcurrentMap<QString, messages::LazyLoadedImage *> &EmoteManager::getBTTVEmotes()
{
    return _bttvEmotes;
}

ConcurrentMap<QString, messages::LazyLoadedImage *> &EmoteManager::getFFZEmotes()
{
    return _ffzEmotes;
}

ConcurrentMap<QString, messages::LazyLoadedImage *> &EmoteManager::getChatterinoEmotes()
{
    return _chatterinoEmotes;
}

ConcurrentMap<QString, messages::LazyLoadedImage *> &EmoteManager::getBTTVChannelEmoteFromCaches()
{
    return _bttvChannelEmoteFromCaches;
}

ConcurrentMap<int, messages::LazyLoadedImage *> &EmoteManager::getFFZChannelEmoteFromCaches()
{
    return _ffzChannelEmoteFromCaches;
}

ConcurrentMap<long, messages::LazyLoadedImage *> &EmoteManager::getTwitchEmoteFromCache()
{
    return _twitchEmoteFromCache;
}

ConcurrentMap<QString, messages::LazyLoadedImage *> &EmoteManager::getMiscImageFromCache()
{
    return _miscImageFromCache;
}

void EmoteManager::loadEmojis()
{
    QFile file(":/emojidata.txt");
    file.open(QFile::ReadOnly);
    QTextStream in(&file);

    uint unicodeBytes[4];

    while (!in.atEnd()) {
        // Line example: sunglasses 1f60e
        QString line = in.readLine();

        if (line.at(0) == '#') {
            // Ignore lines starting with # (comments)
            continue;
        }

        QStringList parts = line.split(' ');
        if (parts.length() < 2) {
            continue;
        }

        QString shortCode = parts[0];
        QString code = parts[1];

        QStringList unicodeCharacters = code.split('-');
        if (unicodeCharacters.length() < 1) {
            continue;
        }

        int numUnicodeBytes = 0;

        for (const QString &unicodeCharacter : unicodeCharacters) {
            unicodeBytes[numUnicodeBytes++] = QString(unicodeCharacter).toUInt(nullptr, 16);
        }

        EmojiData emojiData{
            QString::fromUcs4(unicodeBytes, numUnicodeBytes),  //
            code,                                              //
        };

        shortCodeToEmoji.insert(shortCode, emojiData);
        emojiToShortCode.insert(emojiData.value, shortCode);
    }

    /*
    for (auto const &emoji : shortCodeToEmoji.toStdMap()) {
        auto iter = firstEmojiChars.find(emoji.first.at(0));

        if (iter != firstEmojiChars.end()) {
            iter.value().insert(emoji.second.value, emoji.second.value);
            continue;
        }

        firstEmojiChars.insert(emoji.first.at(0),
                               QMap<QString, QString>{{emoji.second.value, emoji.second.code}});
    }
    */
}

void EmoteManager::parseEmojis(
    std::vector<std::tuple<messages::LazyLoadedImage *, QString>> &vector, const QString &text)
{
    // TODO(pajlada): Add this method to EmoteManager instead
    long lastSlice = 0;

    for (auto i = 0; i < text.length() - 1; i++) {
        if (!text.at(i).isLowSurrogate()) {
            auto iter = firstEmojiChars.find(text.at(i));

            if (iter != firstEmojiChars.end()) {
                for (auto j = std::min(8, text.length() - i); j > 0; j--) {
                    QString emojiString = text.mid(i, 2);
                    auto emojiIter = iter.value().find(emojiString);

                    if (emojiIter != iter.value().end()) {
                        QString url = "https://cdnjs.cloudflare.com/ajax/libs/"
                                      "emojione/2.2.6/assets/png/" +
                                      emojiIter.value() + ".png";

                        if (i - lastSlice != 0) {
                            vector.push_back(std::tuple<messages::LazyLoadedImage *, QString>(
                                nullptr, text.mid(lastSlice, i - lastSlice)));
                        }

                        vector.push_back(std::tuple<messages::LazyLoadedImage *, QString>(
                            emojis.getOrAdd(url,
                                            [this, &url] {
                                                return new LazyLoadedImage(
                                                    *this, this->windowManager, url, 0.35);  //
                                            }),
                            QString()));

                        i += j - 1;

                        lastSlice = i + 1;

                        break;
                    }
                }
            }
        }
    }

    if (lastSlice < text.length()) {
        vector.push_back(
            std::tuple<messages::LazyLoadedImage *, QString>(nullptr, text.mid(lastSlice)));
    }
}

void EmoteManager::loadBTTVEmotes()
{
    // bttv
    QNetworkAccessManager *manager = new QNetworkAccessManager();

    QUrl url("https://api.betterttv.net/2/emotes");
    QNetworkRequest request(url);

    QNetworkReply *reply = manager->get(request);

    QObject::connect(reply, &QNetworkReply::finished, [=] {
        if (reply->error() == QNetworkReply::NetworkError::NoError) {
            QByteArray data = reply->readAll();
            QJsonDocument jsonDoc(QJsonDocument::fromJson(data));
            QJsonObject root = jsonDoc.object();

            auto emotes = root.value("emotes").toArray();

            QString linkTemplate = "https:" + root.value("urlTemplate").toString();

            for (const QJsonValue &emote : emotes) {
                QString id = emote.toObject().value("id").toString();
                QString code = emote.toObject().value("code").toString();
                // emote.value("imageType").toString();

                QString tmp = linkTemplate;
                tmp.detach();
                QString url = tmp.replace("{{id}}", id).replace("{{image}}", "1x");

                EmoteManager::getBTTVEmotes().insert(
                    code, new LazyLoadedImage(*this, this->windowManager, url, 1, code,
                                              code + "\nGlobal BTTV Emote"));
            }
        }

        reply->deleteLater();
        manager->deleteLater();
    });
}

void EmoteManager::loadFFZEmotes()
{
    // ffz
    QNetworkAccessManager *manager = new QNetworkAccessManager();

    QUrl url("https://api.frankerfacez.com/v1/set/global");
    QNetworkRequest request(url);

    QNetworkReply *reply = manager->get(request);

    QObject::connect(reply, &QNetworkReply::finished, [=] {
        if (reply->error() == QNetworkReply::NetworkError::NoError) {
            QByteArray data = reply->readAll();
            QJsonDocument jsonDoc(QJsonDocument::fromJson(data));
            QJsonObject root = jsonDoc.object();

            auto sets = root.value("sets").toObject();

            for (const QJsonValue &set : sets) {
                auto emoticons = set.toObject().value("emoticons").toArray();

                for (const QJsonValue &emote : emoticons) {
                    QJsonObject object = emote.toObject();

                    // margins

                    // int id = object.value("id").toInt();
                    QString code = object.value("name").toString();

                    QJsonObject urls = object.value("urls").toObject();
                    QString url1 = "http:" + urls.value("1").toString();

                    EmoteManager::getBTTVEmotes().insert(
                        code, new LazyLoadedImage(*this, this->windowManager, url1, 1, code,
                                                  code + "\nGlobal FFZ Emote"));
                }
            }
        }

        reply->deleteLater();
        manager->deleteLater();
    });
}

LazyLoadedImage *EmoteManager::getTwitchEmoteById(const QString &name, long id)
{
    return EmoteManager::_twitchEmoteFromCache.getOrAdd(id, [this, &name, &id] {
        qDebug() << "added twitch emote: " << id;
        qreal scale;
        QString url = getTwitchEmoteLink(id, scale);
        return new LazyLoadedImage(*this, this->windowManager, url, scale, name,
                                   name + "\nTwitch Emote");
    });
}

QString EmoteManager::getTwitchEmoteLink(long id, qreal &scale)
{
    scale = .5;

    QString value = TWITCH_EMOTE_TEMPLATE;

    value.detach();

    return value.replace("{id}", QString::number(id)).replace("{scale}", "2");
}

LazyLoadedImage *EmoteManager::getCheerImage(long long amount, bool animated)
{
    // TODO: fix this xD
    return this->getCheerBadge(amount);
}

LazyLoadedImage *EmoteManager::getCheerBadge(long long amount)
{
    if (amount >= 100000) {
        return this->resources.cheerBadge100000;
    } else if (amount >= 10000) {
        return this->resources.cheerBadge10000;
    } else if (amount >= 5000) {
        return this->resources.cheerBadge5000;
    } else if (amount >= 1000) {
        return this->resources.cheerBadge1000;
    } else if (amount >= 100) {
        return this->resources.cheerBadge100;
    } else {
        return this->resources.cheerBadge1;
    }
}

boost::signals2::signal<void()> &EmoteManager::getGifUpdateSignal()
{
    if (!_gifUpdateTimerInitiated) {
        _gifUpdateTimerInitiated = true;

        _gifUpdateTimer.setInterval(30);
        _gifUpdateTimer.start();

        QObject::connect(&_gifUpdateTimer, &QTimer::timeout, [this] {
            _gifUpdateTimerSignal();
            this->windowManager.repaintGifEmotes();
        });
    }

    return _gifUpdateTimerSignal;
}

}  // namespace chatterino
