#pragma once
#include "planova/pins/PinBoardApi.h"
#include "widgets/ModalDialog.h"

class QPlainTextEdit;
class QPushButton;

namespace planova::s5 {

// Asks for a reason, then calls IConflictCommands::decline. Opened by S1's MainWindow
// (entry points live in S3 board cards and S2 team rows, which only emit a signal).
class DeclineDialog : public ModalDialog {
    Q_OBJECT
public:
    DeclineDialog(const Id& taskId, const Id& memberId, const QString& taskTitle,
                  const QString& memberName, IConflictCommands& conflicts, QWidget* parent = nullptr);

signals:
    void openSlicerRequested();

public slots:
    void accept() override;

private:
    Id taskId_;
    Id memberId_;
    IConflictCommands& conflicts_;
    QPlainTextEdit* reason_ = nullptr;
    QPushButton* ok_ = nullptr;
};

}  // namespace planova::s5
