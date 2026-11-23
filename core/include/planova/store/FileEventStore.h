#pragma once
#include "planova/common/IEventStore.h"
#include <filesystem>
#include <vector>

class FileEventStore : public IEventStore
{
public:
    explicit FileEventStore(std::filesystem::path dir);
    void append(const Event& event) override;
    std::vector<Event> readAll() const override;
    void setDirectory(std::filesystem::path dir);
    // Files the most recent readAll() could not open or parse.
    std::vector<std::filesystem::path> brokenFiles() const;

private:
    std::filesystem::path dir_;
    mutable std::vector<std::filesystem::path> broken_;
};
