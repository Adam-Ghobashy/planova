#include <QtTest>
#include <stdexcept>
#include "planova/task/TaskFactory.h"
class TstTaskFactory : public QObject {
    Q_OBJECT
    static NewTask valid() {
        return NewTask{"Design schema", SkillCategory::Storage,
                       Priority::High, 3, 4, {}};
    }
private slots:
    void createsValidTask() {
        Task t = TaskFactory::create("t1", valid());
        QCOMPARE(t.id, std::string("t1"));
        QCOMPARE(t.title, std::string("Design schema"));
        QCOMPARE(t.complexity, 3);
        QCOMPARE(t.estHours, 4);
        QVERIFY(!t.assignee.has_value());
    }
    void statusDefaultsToTodo() {
        Task t = TaskFactory::create("t1", valid());
        QVERIFY(t.status == TaskStatus::ToDo);
    }
    void keepsDependencies() {
        NewTask n = valid();
        n.deps = {"a", "b"};
        Task t = TaskFactory::create("t1", n);
        QCOMPARE(t.deps.size(), size_t(2));
    }
    void rejectsBadComplexity_data() {
        QTest::addColumn<int>("complexity");
        QTest::newRow("zero") << 0;
        QTest::newRow("six") << 6;
        QTest::newRow("negative") << -1;
    }
    void rejectsBadComplexity() {
        QFETCH(int, complexity);
        NewTask n = valid();
        n.complexity = complexity;
        QVERIFY_EXCEPTION_THROWN(TaskFactory::create("t1", n),
                                 std::invalid_argument);
    }
    void rejectsEmptyTitle_data() {
        QTest::addColumn<QString>("title");
        QTest::newRow("empty") << "";
        QTest::newRow("spaces") << "   ";
    }
    void rejectsEmptyTitle() {
        QFETCH(QString, title);
        NewTask n = valid();
        n.title = title.toStdString();
        QVERIFY_EXCEPTION_THROWN(TaskFactory::create("t1", n),
                                 std::invalid_argument);
    }
    void rejectsNonPositiveHours_data() {
        QTest::addColumn<int>("hours");
        QTest::newRow("zero") << 0;
        QTest::newRow("negative") << -2;
    }
    void rejectsNonPositiveHours() {
        QFETCH(int, hours);
        NewTask n = valid();
        n.estHours = hours;
        QVERIFY_EXCEPTION_THROWN(TaskFactory::create("t1", n),
                                 std::invalid_argument);
    }
};
QTEST_APPLESS_MAIN(TstTaskFactory)
#include "tst_TaskFactory.moc"

