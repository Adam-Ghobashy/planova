#include "planova/store/FileEventStore.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>
#include <stdexcept>

namespace {

QJsonObject toJson(const Event& e)
{
    QJsonObject o;
    o["id"] = QString::fromStdString(e.id);
    o["ts"] = static_cast<double>(e.ts);
    o["author"] = QString::fromStdString(e.author);
    o["type"] = QString::fromStdString(e.type);
    o["payload"] = e.payload;
    return o;
}

Event fromJson(const QJsonObject& o)
{
    Event e;
    e.id = o["id"].toString().toStdString();
    e.ts = static_cast<int64_t>(o["ts"].toDouble());
    e.author = o["author"].toString().toStdString();
    e.type = o["type"].toString().toStdString();
    e.payload = o["payload"].toObject();
    return e;
}

} // namespace

FileEventStore::FileEventStore(std::filesystem::path dir)
    : dir_(std::move(dir))
{
}

void FileEventStore::setDirectory(std::filesystem::path dir)
{
    dir_ = std::move(dir);
}

void FileEventStore::append(const Event& event)
{
    std::filesystem::create_directories(dir_);

    const auto tmpPath = dir_ / (event.id + ".json.tmp");
    const auto finalPath = dir_ / (event.id + ".json");

    QFile tmp(QString::fromStdString(tmpPath.string()));
    if (!tmp.open(QIODevice::WriteOnly))
        throw std::runtime_error("Cannot write event file");

    const QByteArray data = QJsonDocument(toJson(event)).toJson(QJsonDocument::Compact);
    if (tmp.write(data) != data.size() || !tmp.flush()) {
        tmp.close();
        tmp.remove();
        throw std::runtime_error("Cannot write event file");
    }
    tmp.close();

    std::filesystem::rename(tmpPath, finalPath);
}

std::vector<std::filesystem::path> FileEventStore::brokenFiles() const
{
    return broken_;
}

std::vector<Event> FileEventStore::readAll() const
{
    broken_.clear();
    std::vector<Event> events;
    if (!std::filesystem::exists(dir_))
        return events;

    for (const auto& entry : std::filesystem::directory_iterator(dir_)) {
        if (entry.path().extension() != ".json")
            continue;

        QFile file(QString::fromStdString(entry.path().string()));
        if (!file.open(QIODevice::ReadOnly)) {
            broken_.push_back(entry.path());
            continue;
        }

        const auto doc = QJsonDocument::fromJson(file.readAll());
        if (doc.isObject())
            events.push_back(fromJson(doc.object()));
        else
            broken_.push_back(entry.path());
    }

    std::sort(events.begin(), events.end(), [](const Event& a, const Event& b) {
        if (a.ts != b.ts)
            return a.ts < b.ts;
        return a.id < b.id;
    });

    return events;
}