#pragma once
#include "planova/common/Dtos.h"
#include <QFrame>

// One workspace in the projects list: name, team, SE model and an Open button.
// The whole card is clickable with the mouse; keyboard users reach the Open button and the "..." menu with Tab.
class ProjectCardWidget : public QFrame {
    Q_OBJECT
public:
    // isCurrent marks the project that is open now (a text chip, so it is not shown by colour alone).
    ProjectCardWidget(const ProjectDto& project, bool isCurrent, QWidget* parent = nullptr);

    QString projectId() const { return id_; }
    QString projectName() const { return name_; }
    // Case-insensitive match on name and team. Empty text matches everything.
    bool matches(const QString& text) const;

signals:
    void openRequested(const QString& projectId);
    void editRequested(const QString& projectId);
    void deleteRequested(const QString& projectId);

protected:
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    QString id_;
    QString name_;
    QString team_;
};
