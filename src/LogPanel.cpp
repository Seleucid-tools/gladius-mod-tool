#include "LogPanel.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollBar>
#include <QFont>

LogPanel::LogPanel(QWidget *parent)
    : QWidget(parent)
{
    m_edit = new QPlainTextEdit(this);
    m_edit->setReadOnly(true);
    m_edit->setMaximumBlockCount(10000);

    QFont mono("Monospace");
    mono.setStyleHint(QFont::Monospace);
    mono.setPointSize(9);
    m_edit->setFont(mono);
    m_edit->setLineWrapMode(QPlainTextEdit::NoWrap);

    m_clearBtn = new QPushButton("Clear log", this);
    m_clearBtn->setFixedWidth(90);
    connect(m_clearBtn, &QPushButton::clicked, this, &LogPanel::clear);

    auto *topBar = new QHBoxLayout;
    topBar->addStretch();
    topBar->addWidget(m_clearBtn);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addLayout(topBar);
    layout->addWidget(m_edit);
}

void LogPanel::appendOutput(const QString &text)
{
    // Split by newlines; each line is a plain-text block.
    const QStringList lines = text.split('\n');
    for (const QString &line : lines) {
        if (!line.isEmpty())
            m_edit->appendPlainText(line);
    }
    // Auto-scroll
    m_edit->verticalScrollBar()->setValue(m_edit->verticalScrollBar()->maximum());
}

void LogPanel::appendError(const QString &text)
{
    // Use HTML to colour error output red without switching to rich text mode
    // globally (QPlainTextEdit does not support HTML, so we use a workaround:
    // temporarily switch, append, switch back).
    // Simpler approach: prefix with [ERR] and let monospace colour handle it.
    const QStringList lines = text.split('\n');
    for (const QString &line : lines) {
        if (!line.isEmpty()) {
            m_edit->appendPlainText("[ERR] " + line);
        }
    }
    m_edit->verticalScrollBar()->setValue(m_edit->verticalScrollBar()->maximum());
}

void LogPanel::clear()
{
    m_edit->clear();
}
