#pragma once

#include <cstdint>
#include <string>
#include "planova/common/Dtos.h"

struct Commit {
    std::string hash;
    std::string message;
    std::string author;
    int64_t ts = 0;

    CommitDto toDto() const
    {
        CommitDto dto;
        dto.hash = hash;
        dto.message = message;
        dto.author = author;
        dto.ts = ts;
        return dto;
    }
};

struct Issue {
    int number = 0;
    std::string title;
    bool open = true;

    IssueDto toDto() const
    {
        IssueDto dto;
        dto.number = number;
        dto.title = title;
        dto.open = open;
        return dto;
    }
};