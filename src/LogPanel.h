#pragma once
#include <QWidget>
#include <QPlainTextEdit>
#include <QPushButton>

class LogPanel : public QWidget
{
    Q_OBJECT
public:
    explicit LogPanel(QWidget *parent = nullptr);

public slots:
    void appendOutput(const QString &text);   // stdout — white/default
    void appendError(const QString &text);    // stderr — red tinted
    void clear();

private:
    QPlainTextEdit *m_edit;
    QPushButton    *m_clearBtn;
};
