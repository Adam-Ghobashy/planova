#pragma once
#include <QDialog>
#include <optional>
#include <string>
#include <vector>

#include "planova/pins/PinBoardApi.h"

class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QShowEvent;

namespace planova::s5 {

// Create a pin ("Leave a Pin") or edit one ("Edit Pin"). No colours here, see pins.qss.
class LeavePinDialog : public QDialog {
    Q_OBJECT
public:
    // Create mode.
    LeavePinDialog(const std::vector<TaskDto>& tasks, const std::vector<MemberDto>& members,
                   const Id& defaultAuthorId, QWidget* parent = nullptr);
    // Edit mode: prefilled from the pin, author locked.
    LeavePinDialog(const PinView& pin, const std::vector<TaskDto>& tasks,
                   const std::vector<MemberDto>& members, QWidget* parent = nullptr);

    PinCategory category() const;
    QString text() const;                       // trimmed
    std::vector<std::string> tags() const;      // lowercase, no '#', no duplicates
    std::optional<Id> taskId() const;
    Id authorId() const;

    // Pure helper, exposed for tests: "#A, b  A" -> {"a","b"}.
    static std::vector<std::string> parseTags(const QString& raw);

protected:
    void showEvent(QShowEvent* e) override;

private:
    void build(const std::vector<TaskDto>& tasks, const std::vector<MemberDto>& members, bool edit);
    void updateOk();

    QComboBox* author_ = nullptr;
    QComboBox* category_ = nullptr;
    QComboBox* task_ = nullptr;
    QPlainTextEdit* note_ = nullptr;
    QLineEdit* tags_ = nullptr;
    QLabel* hint_ = nullptr;
    QPushButton* ok_ = nullptr;
    bool fadedIn_ = false;
};

}  // namespace planova::s5
