#pragma once
#include "planova/PlanovaCore.h"
#include <QWizard>
#include <string>
#include <vector>

// A team member typed in step 4. Saved only when the wizard finishes.
struct PendingMember {
    std::string name;
    std::string role;
    bool isLead = false;
};

class SetupWizard : public QWizard {
    Q_OBJECT
public:
    explicit SetupWizard(PlanovaCore& core, QWidget* parent = nullptr);
    void accept() override;

private:
    PlanovaCore& core_;
    std::vector<PendingMember> pendingMembers_;
};