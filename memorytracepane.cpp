#include "memorytracepane.h"
#include "ui_memorytracepane.h"
#include "sim.h"

MemoryTracePane::MemoryTracePane(QWidget *parent) :
    QWidget(parent),
    m_ui(new Ui::MemoryTracePane)
{
    m_ui->setupUi(this);

    connect(m_ui->pepMemSingleStepPushButton, SIGNAL(clicked()), this, SLOT(singleStep()));

    connect(m_ui->pepMemResumePushButton, SIGNAL(clicked()), this, SLOT(resumeExecution()));

}

MemoryTracePane::~MemoryTracePane()
{
    delete m_ui;
}

void MemoryTracePane::highlightOnFocus()
{
    if (m_ui->pepStackTraceGraphicsView->hasFocus()) {
        m_ui->pepMemoryTraceLabel->setAutoFillBackground(true);
    }
    else {
        m_ui->pepMemoryTraceLabel->setAutoFillBackground(false);
    }
}

bool MemoryTracePane::hasFocus()
{
    return m_ui->pepStackTraceGraphicsView->hasFocus();
}

void MemoryTracePane::undo()
{
    // does nothing with our current implementation
}

void MemoryTracePane::redo()
{
    // does nothing with our current implementation
}

void MemoryTracePane::cut()
{
    // does nothing with our current implementation
}

void MemoryTracePane::copy()
{
    // does nothing with our current implementation
}

void MemoryTracePane::paste()
{
    // does nothing with our current implementation
}

void MemoryTracePane::singleStep()
{
    if (Sim::vonNeumannStep()) {

    }
    else {

    }

}

void MemoryTracePane::resumeExecution()
{
    while (Sim::instructionSpecifier) {
        Sim::vonNeumannStep();
    }
}

