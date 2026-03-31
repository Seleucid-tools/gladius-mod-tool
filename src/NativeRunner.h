#pragma once
#include <QObject>
#include <QString>
#include <QStringList>

// Drop-in replacement for PythonRunner.
// Dispatches script names to C++ tool implementations instead of CPython.
// Has the identical signal/slot interface so PipelineTab needs no logic changes.
class NativeRunner : public QObject
{
    Q_OBJECT
public:
    explicit NativeRunner(QObject *parent = nullptr);

public slots:
    void run(const QString &scriptName, const QStringList &args);

signals:
    void output(const QString &text);
    void error(const QString &text);
    void finished(bool success);
};
