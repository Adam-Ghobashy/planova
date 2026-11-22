#pragma once
#include <QString>
#include <string>
#include <vector>

// Text of the start-up notice about event files that could not be read: how many, up to five names
// ("and N more" for the rest) and the folder to look in.
QString damagedFilesMessage(const std::vector<std::string>& fileNames, const QString& folder);
