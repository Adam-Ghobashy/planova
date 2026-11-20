#include "slicer/WeightSlidersPanel.h"
#include <QLabel>
#include <QSignalSpy>
#include <QSlider>
#include <QTest>

using namespace planova::slicer;

class tst_WeightSlidersPanel : public QObject {
    Q_OBJECT
private slots:
    void startsAt503020WithSum100() {
        WeightSlidersPanel panel;
        QCOMPARE(panel.weights().skill, 50);
        QCOMPARE(panel.weights().workload, 30);
        QCOMPARE(panel.weights().priority, 20);
        QCOMPARE(panel.findChild<QLabel*>("sumLabel")->text(), QString("Sum: 100%"));
    }
    void movingOneSliderKeepsTheSumAt100() {
        WeightSlidersPanel panel;
        auto sliders = panel.findChildren<QSlider*>();
        QCOMPARE(sliders.size(), 3);
        sliders[0]->setValue(80);
        const ScoringWeights w = panel.weights();
        QCOMPARE(w.skill, 80);
        QCOMPARE(w.skill + w.workload + w.priority, 100);
        QVERIFY(w.valid());
        QCOMPARE(panel.findChild<QLabel*>("sumLabel")->text(), QString("Sum: 100%"));
    }
    void aKeyboardStepCommitsAtOnce() {
        WeightSlidersPanel panel;
        QSignalSpy committed(&panel, &WeightSlidersPanel::weightsCommitted);
        panel.findChildren<QSlider*>()[1]->setValue(60);
        QCOMPARE(committed.count(), 1);
    }
    void draggingCommitsOnlyOnRelease() {
        WeightSlidersPanel panel;
        QSignalSpy changed(&panel, &WeightSlidersPanel::weightsChanged);
        QSignalSpy committed(&panel, &WeightSlidersPanel::weightsCommitted);
        QSlider* slider = panel.findChildren<QSlider*>()[0];

        slider->setSliderDown(true);
        slider->setValue(70);
        slider->setValue(65);
        QVERIFY(changed.count() >= 2);   // live updates while dragging
        QCOMPARE(committed.count(), 0);  // but nothing final yet

        slider->setSliderDown(false);    // release
        QCOMPARE(committed.count(), 1);
    }
};

QTEST_MAIN(tst_WeightSlidersPanel)
#include "tst_WeightSlidersPanel.moc"
