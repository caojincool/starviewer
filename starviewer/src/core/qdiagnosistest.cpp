#include "qdiagnosistest.h"

#include <QTableWidget>
#include <QTableWidgetItem>
#include <QThread>
#include <QMovie>
#include <QFileDialog>

#include "diagnosistestfactory.h"
#include "diagnosistest.h"
#include "rundiagnosistest.h"
#include "diagnosistestresultwriter.h"

#include "qdiagnosistestresultwidget.h"

namespace udg {

QDiagnosisTest::QDiagnosisTest(QWidget *parent)
 : QDialog(parent)
{
    setupUi(this);

    m_threadRunningDiagnosisTest = new QThread();
    m_threadRunningDiagnosisTest->start();

    m_runDiagnosisTest = new RunDiagnosisTest(getDiagnosisTestsToRun());
    m_runDiagnosisTest->moveToThread(m_threadRunningDiagnosisTest);

    createConnections();

    QMovie *operationAnimation = new QMovie(this);
    operationAnimation->setFileName(":/images/loader.gif");
    m_animationInProgressLabel->setMovie(operationAnimation);
    operationAnimation->start();

    //Treiem icona amb ? que apareix al costat del bot� de tancar
    this->setWindowFlags(this->windowFlags() & ~Qt::WindowContextHelpButtonHint);
    m_qDiagnosisTestResultWidgetExpanded = NULL;
}

QDiagnosisTest::~QDiagnosisTest()
{
    qDeleteAll(m_runDiagnosisTest->getDiagnosisTestToRun());
    delete m_runDiagnosisTest;

    m_threadRunningDiagnosisTest->terminate();
    m_threadRunningDiagnosisTest->wait();
    delete m_threadRunningDiagnosisTest;
}

void QDiagnosisTest::resizeEvent(QResizeEvent *)
{
    m_testsResultsTable->setColumnWidth(0, m_testsResultsTable->width());
    if (m_qDiagnosisTestResultWidgetExpanded)
    {
        // Si es fa un rezise i hi ha algun resultat expanded, potser que canv�in el nombre de l�nies del QLabel, i per tant cal recalcular
        // la seva mida vertical.
        // Primer de tot li diem quina mida horitzontal tindr�.
        m_qDiagnosisTestResultWidgetExpanded->setParentWidgetWidth(m_testsResultsTable->width());
        // El for�em a calcular la nova mida.
        m_qDiagnosisTestResultWidgetExpanded->expand();
        // I ajustem la mida de la taula.
        m_testsResultsTable->resizeRowsToContents();
    }
    
}

void QDiagnosisTest::execAndRunDiagnosisTest()
{
    this->show();
    runDiagnosisTest();
    this->exec();
}

void QDiagnosisTest::createConnections()
{
    connect(m_succeededTestsToolButton, SIGNAL(clicked()), SLOT(fillDiagnosisTestsResultTable()));
    connect(m_warningTestsToolButton, SIGNAL(clicked()), SLOT(fillDiagnosisTestsResultTable()));
    connect(m_errorTestsToolButton, SIGNAL(clicked()), SLOT(fillDiagnosisTestsResultTable()));
    connect(m_saveResultsButton, SIGNAL(clicked()), SLOT(saveDiagnosisTestResultsAsFile()));
    connect(m_closeButton, SIGNAL(clicked()), SLOT(accept()));
    
    connect(m_viewTestsLabel, SIGNAL(linkActivated(QString)), SLOT(viewTestsLabelClicked()));

    connect(this, SIGNAL(start()), m_runDiagnosisTest, SLOT(run()));
    connect(m_runDiagnosisTest, SIGNAL(runningDiagnosisTest(DiagnosisTest *)), this, SLOT(updateRunningDiagnosisTestProgress(DiagnosisTest *)));
    connect(m_runDiagnosisTest, SIGNAL(finished()), this, SLOT(finishedRunningDiagnosisTest()));
}

void QDiagnosisTest::qdiagnosisTestResultWidgetClicked(QDiagnosisTestResultWidget *qDiagnosisTestResultWidgetClicked)
{
    if (!qDiagnosisTestResultWidgetClicked->isExpandable())
    {
        return;
    }

    if (m_qDiagnosisTestResultWidgetExpanded)
    {
        m_qDiagnosisTestResultWidgetExpanded->contract();
    }

    if (qDiagnosisTestResultWidgetClicked != m_qDiagnosisTestResultWidgetExpanded)
    {
        // Per tal de que es calculi b� la mida del resultat de test quan est� expanded, cal passar-li la mida horitzontal de la taula.
        // D'aquesta manera pot saber quantes l�nies de text ocupen la descripci� i la soluci�.
        qDiagnosisTestResultWidgetClicked->setParentWidgetWidth(m_testsResultsTable->width());
        qDiagnosisTestResultWidgetClicked->expand();
        m_qDiagnosisTestResultWidgetExpanded = qDiagnosisTestResultWidgetClicked;
    }
    else
    {
        //Si ens han clickat el mateix que element que ja estava expandit, nom�s s'ha de contraure no l'hem de tornar a expandir
        m_qDiagnosisTestResultWidgetExpanded = NULL;
    }

    m_testsResultsTable->resizeRowsToContents();
}

void QDiagnosisTest::runDiagnosisTest()
{
    updateWidgetToRunDiagnosisTest();

    m_testsProgressBar->setValue(0);
    m_testsProgressBar->setMaximum(m_runDiagnosisTest->getDiagnosisTestToRun().count());

    emit start();
} 

void QDiagnosisTest::updateRunningDiagnosisTestProgress(DiagnosisTest * diagnosisTest)
{
    m_executingTestLabel->setText(tr("Running test: ") + diagnosisTest->getDescription());
    m_testsProgressBar->setValue(m_testsProgressBar->value() + 1);
}

void QDiagnosisTest::finishedRunningDiagnosisTest()
{
    groupDiagnosisTestFromRunDiagnosisTestByState();

    m_errorTestsToolButton->setText(tr("%1 errors").arg(m_errorExecutedDiagnosisTests.count()));
    m_succeededTestsToolButton->setText(tr("%1 Ok").arg(m_okExecutedDiagnosisTests.count()));
    m_warningTestsToolButton->setText(tr("%1 warnings").arg(m_warningExecutedDiagnosisTests.count()));

    if (allDiagnosisTestResultAreOk())
    {
        m_allTestSuccededFrame->setVisible(true);
    }
    else
    {
        //Marquem els botons de warning i error perqu� en el llistat apareguin nom�s els testos que han fallat 
        m_warningTestsToolButton->setChecked(true);
        m_errorTestsToolButton->setChecked(true);
        
        m_someTestsFailedFrame->setVisible(true);
        m_diagnosisTestsResultsFrame->setVisible(true);
        fillDiagnosisTestsResultTable();
    }

    m_progressBarFrame->setVisible(false);
    m_closeButtonFrame->setVisible(true);

    this->adjustSize();
}

void QDiagnosisTest::viewTestsLabelClicked()
{
    m_diagnosisTestsResultsFrame->setVisible(true);

    m_succeededTestsToolButton->setChecked(true);
    fillDiagnosisTestsResultTable();

    this->adjustSize();
}

void QDiagnosisTest::fillDiagnosisTestsResultTable()
{
    m_qDiagnosisTestResultWidgetExpanded = NULL;

    m_testsResultsTable->setColumnWidth(0, m_testsResultsTable->width());

    m_testsResultsTable->clear();
    m_testsResultsTable->setRowCount(0);

    QList<QPair<DiagnosisTest *,DiagnosisTestResult> > testsToShow = getDiagnosisTestsToShowInDiagnosisTestsResultTable();

    for (int index = 0; index < testsToShow.count(); index++)
    {
        QPair<DiagnosisTest *, DiagnosisTestResult> executedTest = testsToShow.at(index);
        addDiagnosisTestResultToTable(executedTest.first, executedTest.second);
    }
    
    m_testsResultsTable->resizeRowsToContents();
}

void QDiagnosisTest::addDiagnosisTestResultToTable(DiagnosisTest *diagnosisTest, DiagnosisTestResult diagnosisTestResult)
{
    QDiagnosisTestResultWidget *qdiagnosisTestResultWidget = new QDiagnosisTestResultWidget(diagnosisTest, diagnosisTestResult);
    connect(qdiagnosisTestResultWidget, SIGNAL(clicked(QDiagnosisTestResultWidget*)), SLOT(qdiagnosisTestResultWidgetClicked(QDiagnosisTestResultWidget*)));
    qdiagnosisTestResultWidget->resize(m_testsResultsTable->width(), qdiagnosisTestResultWidget->height());

    m_testsResultsTable->setRowCount(m_testsResultsTable->rowCount() + 1);
    m_testsResultsTable->setCellWidget(m_testsResultsTable->rowCount() -1, 0, qdiagnosisTestResultWidget);
}

void QDiagnosisTest::updateWidgetToRunDiagnosisTest()
{
    m_diagnosisTestsResultsFrame->setVisible(false);
    m_allTestSuccededFrame->setVisible(false);
    m_closeButtonFrame->setVisible(false);
    m_someTestsFailedFrame->setVisible(false);

    this->adjustSize();
}

void QDiagnosisTest::saveDiagnosisTestResultsAsFile()
{
    QString pathFile = QFileDialog::getSaveFileName(this, tr("Save diagnosis test results"), QDir::homePath(), tr("Images (*.txt)"));

    if (!pathFile.isEmpty())
    {
        DiagnosisTestResultWriter diagnosisTestResultWriter;
        
        diagnosisTestResultWriter.setDiagnosisTests(m_errorExecutedDiagnosisTests + m_warningExecutedDiagnosisTests + m_okExecutedDiagnosisTests);
        diagnosisTestResultWriter.write(pathFile);
    }
}

bool QDiagnosisTest::allDiagnosisTestResultAreOk()
{
    return m_errorExecutedDiagnosisTests.count() == 0 && m_warningExecutedDiagnosisTests.count() == 0;
}

QList<DiagnosisTest*> QDiagnosisTest::getDiagnosisTestsToRun() const
{
    QList<DiagnosisTest*> diagnosisTestsToRun;

    foreach(QString className, DiagnosisTestFactory::instance()->getFactoryNamesList())
    {
        diagnosisTestsToRun.append(DiagnosisTestFactory::instance()->create(className));
    }

    return diagnosisTestsToRun;
}

QList<QPair<DiagnosisTest *,DiagnosisTestResult> > QDiagnosisTest::getDiagnosisTestsToShowInDiagnosisTestsResultTable()
{
    QList<QPair<DiagnosisTest *,DiagnosisTestResult> > testsToShow;
    
    if (m_succeededTestsToolButton->isChecked())
    {
        testsToShow.append(m_okExecutedDiagnosisTests);
    }

    if (m_errorTestsToolButton->isChecked())
    {
        testsToShow.append(m_errorExecutedDiagnosisTests);
    }
    
    if (m_warningTestsToolButton->isChecked())
    {
        testsToShow.append(m_warningExecutedDiagnosisTests);
    }

    return testsToShow;
}

void QDiagnosisTest::groupDiagnosisTestFromRunDiagnosisTestByState()
{
    QList<QPair<DiagnosisTest *,DiagnosisTestResult> >  runDiagnosisTests = m_runDiagnosisTest->getRunTests();

    for (int index = 0; index < runDiagnosisTests.count(); index++)
    {
        QPair<DiagnosisTest *, DiagnosisTestResult> runDiagnosisTest = runDiagnosisTests.at(index);

        if (runDiagnosisTest.second.getState() == DiagnosisTestResult::Ok)
        {
            m_okExecutedDiagnosisTests.append(runDiagnosisTest);
        }
        else if (runDiagnosisTest.second.getState() == DiagnosisTestResult::Warning)
        {
            m_warningExecutedDiagnosisTests.append(runDiagnosisTest);
        }
        else
        {
            m_errorExecutedDiagnosisTests.append(runDiagnosisTest);
        }
    }
}

}