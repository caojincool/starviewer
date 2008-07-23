/***************************************************************************
 *   Copyright (C) 2005 by Grup de Gràfics de Girona                       *
 *   http://iiia.udg.es/GGG/index.html?langu=uk                            *
 *                                                                         *
 *   Universitat de Girona                                                 *
 ***************************************************************************/
#include "queryscreen.h"

#include <QMessageBox>
#include <QCloseEvent>
#include <QFileDialog>
#include <QMovie>
#include <QContextMenuEvent>
#include <QShortcut>

#include "processimagesingleton.h"
#include "pacsconnection.h"
#include "qstudytreewidget.h"
#include "dicomseries.h"
#include "querypacs.h"
#include "pacsparameters.h"
#include "pacsserver.h"
#include "qserieslistwidget.h"
#include "pacslist.h"
#include "qpacslist.h"
#include "starviewersettings.h"
#include "cachepool.h"
#include "operation.h"
#include "cachelayer.h"
#include "pacslistdb.h"
#include "logging.h"
#include "status.h"
#include "cachestudydal.h"
#include "cacheseriesdal.h"
#include "cacheimagedal.h"
#include "qchooseoneobjectdialog.h"
#include "dicomdirimporter.h"
#include "patientfillerinput.h"
#include "qcreatedicomdir.h"
#include "dicommask.h"
#include "pacslist.h"
#include "qoperationstatescreen.h"

namespace udg {

QueryScreen::QueryScreen( QWidget *parent )
 : QDialog(parent )
{
    setupUi( this );
    setWindowFlags( Qt::Widget );
    initialize();//inicialitzem les variables necessàries
    //connectem signals i slots
    createConnections();
    //esborrem els estudis vells de la cache
    deleteOldStudies();
    readSettings();
    //fem que per defecte mostri els estudis de la cache
    queryStudy("Cache");
}

QueryScreen::~QueryScreen()
{
    /*sinó fem un this.close i tenim la finestra queryscreen oberta al tancar l'starviewer, l'starviewer no finalitza
     *desapareixen les finestres, però el procés continua viu
     */
    this->close();
}

void QueryScreen::initialize()
{
    //indiquem que la llista de Pacs no es mostra
    m_showPACSNodes = false;
    m_PACSNodes->setVisible(false);

    m_fromStudyDate->setDate( QDate::currentDate() );
    m_toStudyDate->setDate( QDate::currentDate() );

    /* Posem com a pare el pare de la queryscreen, d'aquesta manera quan es tanqui el pare de la queryscreen
     * el QOperationStateScreen també es tancarà
     */
    m_operationStateScreen = new udg::QOperationStateScreen( this );

    m_qcreateDicomdir = new udg::QCreateDicomdir( this );
    m_processImageSingleton = ProcessImageSingleton::getProcessImageSingleton();


    QMovie *operationAnimation = new QMovie(this);
    operationAnimation->setFileName(":/images/loader.gif");
    m_operationAnimation->setMovie(operationAnimation);
    operationAnimation->start();

    m_patientNameText->setFocus();
    m_qwidgetAdvancedSearch->hide();
    m_operationAnimation->hide();
    m_labelOperation->hide();
    refreshTab( LocalDataBaseTab );

    CreateContextMenuQStudyTreeWidgetCache();
    CreateContextMenuQStudyTreeWidgetPacs();
    CreateContextMenuQStudyTreeWidgetDicomdir();

    setQStudyTreeWidgetColumnsWidth();
}

void QueryScreen::CreateContextMenuQStudyTreeWidgetCache()
{
    QAction *action;

    action = m_contextMenuQStudyTreeWidgetCache.addAction( QIcon(":/images/view.png") , tr( "&View" ) , this , SLOT( view() ) , tr("Ctrl+V") );
    (void) new QShortcut( action->shortcut() , this , SLOT( view() ) );

    action = m_contextMenuQStudyTreeWidgetCache.addAction( QIcon(":/images/databaseRemove.png") , tr( "&Delete" ) , this , SLOT( deleteSelectedStudiesInCache() ) , Qt::Key_Delete );
    (void) new QShortcut( action->shortcut() , this , SLOT( deleteSelectedStudiesInCache() ) );

    action = m_contextMenuQStudyTreeWidgetCache.addAction( tr( "Send to DICOMDIR List" ) , this , SLOT( convertToDicomdir() ) , tr( "Ctrl+M" ) );
    (void) new QShortcut( action->shortcut() , this , SLOT( convertToDicomdir() ) );

    action = m_contextMenuQStudyTreeWidgetCache.addAction( QIcon(":/images/store.png") , tr( "Store to PACS" ) , this , SLOT( storeStudiesToPacs() ) , tr( "Ctrl+S" ) );
    (void) new QShortcut( action->shortcut() , this , SLOT( storeStudiesToPacs() ) );

    m_studyTreeWidgetCache->setContextMenu( & m_contextMenuQStudyTreeWidgetCache );//Especifiquem que és el menú per la cache
}

void QueryScreen::CreateContextMenuQStudyTreeWidgetPacs()
{
    QAction *action;

    action = m_contextMenuQStudyTreeWidgetPacs.addAction( QIcon(":/images/view.png") , tr( "&View" ) , this , SLOT( view() ) , tr("Ctrl+V") );
    (void) new QShortcut( action->shortcut() , this , SLOT( view() ) );

    action = m_contextMenuQStudyTreeWidgetPacs.addAction( QIcon(":/images/retrieve.png") , tr("&Retrieve") , this , SLOT( retrieve() ) , tr("Ctrl+R") );
    (void) new QShortcut( action->shortcut() , this , SLOT( retrieve() ) );

    m_studyTreeWidgetPacs->setContextMenu( & m_contextMenuQStudyTreeWidgetPacs ); //Especifiquem que és el menú del PACS
}

void QueryScreen::CreateContextMenuQStudyTreeWidgetDicomdir()
{
    QAction *action;

    action = m_contextMenuQStudyTreeWidgetDicomdir.addAction( QIcon(":/images/view.png") , tr( "&View" ) , this , SLOT( view() ) , tr("Ctrl+V") );
    (void) new QShortcut( action->shortcut() , this , SLOT( view() ) );

    action = m_contextMenuQStudyTreeWidgetDicomdir.addAction( QIcon(":/images/retrieve.png") , tr("&Retrieve") , this , SLOT( importDicomdir() ) , tr("Ctrl+R") );
    (void) new QShortcut( action->shortcut() , this , SLOT( retrieve() ) );

    m_studyTreeWidgetDicomdir->setContextMenu( & m_contextMenuQStudyTreeWidgetDicomdir ); //Especifiquem que es el menu del dicomdir
}

void QueryScreen::setQStudyTreeWidgetColumnsWidth()
{
    StarviewerSettings settings;

    for ( int column = 0; column < m_studyTreeWidgetCache->getNumberOfColumns(); column++)
    {
        m_studyTreeWidgetCache->setColumnWidth( column , settings.getStudyCacheListColumnWidth(column) );
    }

    for ( int column = 0; column < m_studyTreeWidgetPacs->getNumberOfColumns(); column++)
    {
        m_studyTreeWidgetPacs->setColumnWidth( column , settings.getStudyPacsListColumnWidth(column) );
    }

    for ( int column = 0; column < m_studyTreeWidgetDicomdir->getNumberOfColumns(); column++)
    {
        m_studyTreeWidgetDicomdir->setColumnWidth( column , settings.getStudyDicomdirListColumnWidth(column) );
    }
}

void QueryScreen::setSeriesToSeriesListWidgetCache()
{
    QList<DICOMSeries> seriesList;
    CacheSeriesDAL cacheSeriesDAL;
    CacheImageDAL cacheImageDAL;
    int imagesNumber;
    Status state;
    DicomMask mask;

    INFO_LOG( "Cerca de sèries a la cache de l'estudi " + m_studyTreeWidgetCache->getCurrentStudyUID() );

    //preparem la mascara i cerquem les series a la cache
    mask.setStudyUID( m_studyTreeWidgetCache->getCurrentStudyUID() );
    state = cacheSeriesDAL.querySeries( mask , seriesList );
    if ( !state.good() )
    {
        showDatabaseErrorMessage( state );
        return;
    }

    m_seriesListWidgetCache->clear();

    foreach( DICOMSeries series , seriesList )
    {
        //preparem per fer la cerca d'imatges
        mask.setSeriesUID( series.getSeriesUID() );
        state = cacheImageDAL.countImageNumber( mask , imagesNumber );
        series.setImageNumber( imagesNumber );
        if ( !state.good() )
        {
            showDatabaseErrorMessage( state );
            return;
        }
        m_seriesListWidgetCache->insertSeries( &series );
    }
}

void QueryScreen::deleteOldStudies()
{
    Status state;
    CacheLayer cacheLayer;

    state = cacheLayer.deleteOldStudies();

    if ( !state.good() )
    {
        QMessageBox::warning( this , tr( "Starviewer" ) , tr( "Error deleting old studies" ) );
        showDatabaseErrorMessage( state );
    }
}

void QueryScreen::updateOperationsInProgressMessage()
{
    if (m_operationStateScreen->getActiveOperationsCount() > 0)
    {
        m_operationAnimation->show();
        m_labelOperation->show();
    }
    else
    {
        m_operationAnimation->hide();
        m_labelOperation->hide();
    }
}

void QueryScreen::createConnections()
{
    //connectem els butons
    connect( m_searchButton, SIGNAL( clicked() ), SLOT( searchStudy() ) );
    connect( m_clearToolButton, SIGNAL( clicked() ), SLOT( clearTexts() ) );
    connect( m_retrieveButtonPACS, SIGNAL( clicked() ), SLOT( retrieve() ) );
    connect( m_retrieveButtonDICOMDIR, SIGNAL( clicked() ), SLOT( importDicomdir() ) );
    connect( m_operationListToolButton, SIGNAL( clicked() ) , SLOT( showOperationStateScreen() ) );
    connect( m_showPACSNodesToolButton, SIGNAL( toggled(bool) ), m_PACSNodes, SLOT( setVisible(bool) ) );

    connect( m_viewButtonLocal, SIGNAL( clicked() ), SLOT( view() ) );
    connect( m_viewButtonPACS, SIGNAL( clicked() ), SLOT( view() ) );
    connect( m_viewButtonDICOMDIR, SIGNAL( clicked() ), SLOT( view() ) );
    connect( m_createDICOMDIRToolButton, SIGNAL( clicked() ), m_qcreateDicomdir, SLOT( show() ) );

    //connectem Slots dels StudyTreeWidget amb la interficie
    connect( m_studyTreeWidgetPacs, SIGNAL( studyExpanded( QString , QString ) ), SLOT( expandStudy( QString , QString ) ) );
    connect( m_studyTreeWidgetPacs, SIGNAL( seriesExpanded( QString , QString , QString ) ), SLOT( expandSeries( QString , QString , QString ) ) );
    connect( m_studyTreeWidgetPacs, SIGNAL( studyDoubleClicked() ), SLOT( retrieve() ) );
    connect( m_studyTreeWidgetPacs, SIGNAL( seriesDoubleClicked() ), SLOT( retrieve() ) );
    connect( m_studyTreeWidgetPacs, SIGNAL( imageDoubleClicked() ), SLOT( retrieve() ) );

    connect( m_studyTreeWidgetCache, SIGNAL( studyExpanded( QString , QString ) ), SLOT( expandStudy( QString , QString ) ) );
    connect( m_studyTreeWidgetCache, SIGNAL( seriesExpanded( QString , QString , QString ) ), SLOT( expandSeries( QString , QString , QString ) ) );
    connect( m_studyTreeWidgetCache, SIGNAL( studyDoubleClicked() ), SLOT( view() ) );
    connect( m_studyTreeWidgetCache, SIGNAL( seriesDoubleClicked() ), SLOT( view() ) );
    connect( m_studyTreeWidgetCache, SIGNAL( imageDoubleClicked() ), SLOT( view() ) );

    connect( m_studyTreeWidgetDicomdir, SIGNAL( studyExpanded( QString , QString ) ), SLOT( expandStudy( QString , QString ) ) );
    connect( m_studyTreeWidgetDicomdir, SIGNAL( seriesExpanded( QString , QString , QString ) ), SLOT( expandSeries( QString , QString , QString ) ) );
    connect( m_studyTreeWidgetDicomdir, SIGNAL( studyDoubleClicked() ), SLOT( view() ) );
    connect( m_studyTreeWidgetDicomdir, SIGNAL( seriesDoubleClicked() ), SLOT( view() ) );
    connect( m_studyTreeWidgetDicomdir, SIGNAL( imageDoubleClicked() ), SLOT( view() ) );

    //es canvia de pestanya del TAB
    connect( m_tab , SIGNAL( currentChanged( int ) ), SLOT( refreshTab( int ) ) );

    //connectem els signes del SeriesIconView StudyListView
    connect( m_seriesListWidgetCache, SIGNAL( selectedSeriesIcon(QString) ), m_studyTreeWidgetCache, SLOT( setCurrentSeries(QString) ) );
    connect( m_seriesListWidgetCache, SIGNAL( viewSeriesIcon() ), SLOT( viewFromQSeriesListWidget() ) );
    connect( m_studyTreeWidgetCache, SIGNAL( currentStudyChanged() ), SLOT( setSeriesToSeriesListWidgetCache() ) );
    connect( m_studyTreeWidgetCache, SIGNAL( currentSeriesChanged(QString) ), m_seriesListWidgetCache, SLOT( setCurrentSeries(QString) ) );

    //connecta el signal que emiteix qexecuteoperationthread, per visualitzar un estudi amb aquesta classe
    connect( &m_qexecuteOperationThread, SIGNAL( viewStudy( QString , QString , QString ) ), SLOT( studyRetrievedView( QString , QString , QString ) ) , Qt::QueuedConnection );

    //connecta els signals el qexecute operation thread amb els de qretrievescreen, per coneixer quant s'ha descarregat una imatge, serie, estudi, si hi ha error, etc..
    connect( &m_qexecuteOperationThread, SIGNAL( setErrorOperation( QString ) ), m_operationStateScreen, SLOT(  setErrorOperation( QString ) ) );
    connect( &m_qexecuteOperationThread, SIGNAL( setOperationFinished( QString ) ), m_operationStateScreen, SLOT(  setOperationFinished( QString ) ) );

    connect( &m_qexecuteOperationThread, SIGNAL( setOperating( QString ) ), m_operationStateScreen, SLOT(  setOperating( QString ) ) );
    connect( &m_qexecuteOperationThread, SIGNAL( imageCommit( QString , int) ), m_operationStateScreen, SLOT(  imageCommit( QString , int ) ) );
    connect( &m_qexecuteOperationThread, SIGNAL( seriesCommit( QString ) ), m_operationStateScreen, SLOT(  seriesCommit( QString ) ) );
    connect( &m_qexecuteOperationThread, SIGNAL( newOperation( Operation * ) ), m_operationStateScreen, SLOT(  insertNewOperation( Operation *) ) );

    // Label d'informació (cutre-xapussa)
    connect( &m_qexecuteOperationThread, SIGNAL( setErrorOperation(QString) ), SLOT( updateOperationsInProgressMessage() ));
    connect( &m_qexecuteOperationThread, SIGNAL( setOperationFinished(QString) ), SLOT( updateOperationsInProgressMessage() ));
    connect( &m_qexecuteOperationThread, SIGNAL( newOperation(Operation *) ), SLOT( updateOperationsInProgressMessage() ));

    //connect tracta els errors de connexió al PACS
    connect ( &multipleQueryStudy, SIGNAL( errorConnectingPacs( int ) ), SLOT( errorConnectingPacs( int ) ) );
    connect ( &multipleQueryStudy, SIGNAL( errorQueringStudiesPacs( int ) ), SLOT( errorQueringStudiesPacs( int ) ) );

    //connect tracta els errors de connexió al PACS, al descarregar imatges
    connect ( &m_qexecuteOperationThread, SIGNAL( errorConnectingPacs( int ) ), SLOT( errorConnectingPacs( int ) ) );
    connect( &m_qexecuteOperationThread, SIGNAL( setRetrieveFinished( QString ) ), SLOT( studyRetrieveFinished ( QString ) ) );

    //Amaga o ensenya la cerca avançada
    connect( m_advancedSearchToolButton, SIGNAL( toggled( bool ) ), SLOT( setAdvancedSearchVisible( bool ) ) );

    connect( m_textOtherModality, SIGNAL( editingFinished () ), SLOT( textOtherModalityEdited() ) );

    connect( m_fromStudyDate, SIGNAL( dateChanged( QDate ) ), SLOT( checkNewFromDate( QDate ) ) );
    connect( m_toStudyDate, SIGNAL( dateChanged( QDate ) ), SLOT( checkNewToDate( QDate ) ) );

    foreach(QLineEdit *lineEdit, m_qwidgetAdvancedSearch->findChildren<QLineEdit*>())
    {
        connect(lineEdit, SIGNAL(textChanged(const QString &)), SLOT(updateAdvancedSearchModifiedStatus()));
    }
}

void QueryScreen::checkNewFromDate( QDate date )
{
    if( date > m_toStudyDate->date() )
    {
        m_toStudyDate->setDate( date );
    }
}

void QueryScreen::checkNewToDate( QDate date )
{
    if( date < m_fromStudyDate->date()   )
    {
        m_fromStudyDate->setDate( date );
    }
}

void QueryScreen::setAdvancedSearchVisible(bool visible)
{
    m_qwidgetAdvancedSearch->setVisible(visible);

    if (visible)
    {
        m_advancedSearchToolButton->setText( m_advancedSearchToolButton->text().replace(">>","<<") );
    }
    else
    {
        m_advancedSearchToolButton->setText( m_advancedSearchToolButton->text().replace("<<",">>") );
    }
}

void QueryScreen::updateAdvancedSearchModifiedStatus()
{
    for(int i = 0; i < m_qwidgetAdvancedSearch->count(); i++)
    {
        bool hasModifiedLineEdit = false;
        QWidget *tab = m_qwidgetAdvancedSearch->widget(i);

        foreach(QLineEdit *lineEdit, tab->findChildren<QLineEdit*>())
        {
            if (lineEdit->text() != "")
            {
                hasModifiedLineEdit = true;
                break;
            }
        }
        QString tabText = m_qwidgetAdvancedSearch->tabText(i).remove(QRegExp("\\*$"));
        if (hasModifiedLineEdit)
        {
            tabText += "*";
        }
        m_qwidgetAdvancedSearch->setTabText(i, tabText);
    }
}

void QueryScreen::readSettings()
{
    StarviewerSettings settings;
    move( settings.getQueryScreenWindowPositionX() , settings.getQueryScreenWindowPositionY() );
    resize( settings.getQueryScreenWindowWidth() , settings.getQueryScreenWindowHeight() );
    if ( !settings.getQueryScreenStudyTreeSeriesListQSplitterState().isEmpty() )
    {
        m_StudyTreeSeriesListQSplitter->restoreState( settings.getQueryScreenStudyTreeSeriesListQSplitterState() );
    }
    //carreguem el processImageSingleton
    m_processImageSingleton->setPath( settings.getCacheImagePath() );
}

void QueryScreen::clearTexts()
{
    m_studyIDText->clear();
    m_patientIDText->clear();
    m_patientNameText->clear();
    m_accessionNumberText->clear();
    m_referringPhysiciansNameText->clear();
    m_studyUIDText->clear();
    m_seriesUIDText->clear();
    m_requestedProcedureIDText->clear();
    m_scheduledProcedureStepIDText->clear();
    m_SOPInstanceUIDText->clear();
    m_instanceNumberText->clear();
    m_PPStartDateText->clear();
    m_PPStartTimeText->clear();
    m_seriesNumberText->clear();
    m_studyModalityText->clear();
    m_studyTimeText->clear();

    clearCheckedModality();

    m_anyDateRadioButton->setChecked( true );
}

void QueryScreen::clearCheckedModality()
{
    m_checkAll->setChecked( true );
    m_textOtherModality->clear();
}

void QueryScreen::textOtherModalityEdited()
{
    if ( m_textOtherModality->text().isEmpty() )
    {
        m_checkAll->setChecked(true);
    }
}

void QueryScreen::updateConfiguration(const QString &configuration)
{
    if (configuration == "Pacs/ListChanged")
    {
        m_PACSNodes->refresh();
    }
    else if (configuration == "Pacs/CacheCleared")
    {
        m_studyTreeWidgetCache->clear();
        m_seriesListWidgetCache->clear();
    }
}

void QueryScreen::bringToFront()
{
    this->show();
    this->raise();
    this->activateWindow();
}

void QueryScreen::searchStudy()
{
    switch ( m_tab->currentIndex() )
    {
        case LocalDataBaseTab:
            queryStudy("Cache");
            break;

        case PACSQueryTab:
            queryStudyPacs();
        break;

        case DICOMDIRTab:
            queryStudy("DICOMDIR");
            break;
    }
}

Status QueryScreen::preparePacsServerConnection(QString AETitlePACS, PacsServer *pacsConnection )
{
    PacsParameters pacs;
    PacsListDB pacsListDB;
    Status state;
    StarviewerSettings settings;

    state = pacsListDB.queryPacs( &pacs, AETitlePACS );//cerquem els paràmetres del Pacs al qual s'han de cercar les dades
    if ( !state.good() )
    {
        showDatabaseErrorMessage( state );
        return state;
    }

    pacs.setAELocal( settings.getAETitleMachine() ); //especifiquem el nostres AE
    pacs.setTimeOut( settings.getTimeout().toInt( NULL , 10 ) ); //li especifiquem el TimeOut

    pacsConnection->setPacs( pacs );

    return state;
}

void QueryScreen::queryStudyPacs()
{
    QList<PacsParameters> selectedPacsList;
    PacsParameters pa;
    QString result;
    StarviewerSettings settings;

    INFO_LOG( "Cerca d'estudis als PACS amb paràmetres " + buildQueryParametersString() );

    m_PACSNodes->getSelectedPacs(selectedPacsList); //Emplemen el pacsList amb les pacs seleccionats al QPacsList

    if (selectedPacsList.isEmpty()) //es comprova que hi hagi pacs seleccionats
    {
        QMessageBox::warning( this , tr( "Starviewer" ) , tr( "Please select a PACS to query" ) );
        return;
    }

    multipleQueryStudy.setPacsList( selectedPacsList ); //indiquem a quins Pacs Cercar

    DicomMask searchMask = buildDicomMask();
    bool stopQuery = false;
    if ( searchMask.isAHeavyQuery() )
    {
        //0 -> Yes; 1->No
        switch( QMessageBox::information( 0 , tr( "Warning" ) , tr( "This query can take a long time.\nDo you want continue?" ), tr( "&Yes" ) , tr( "&No" ) , 0 , 1 ) )
        {
            case 1:
                stopQuery = true;
            break;

            default:
            break;
        }
    }
    if( !stopQuery )
    {
        QApplication::setOverrideCursor( QCursor( Qt::WaitCursor ) );
        multipleQueryStudy.setMask( searchMask ); //assignem la mascara

        Status queryStatus = multipleQueryStudy.StartQueries();

        if( !queryStatus.good() )  //no fem la query
        {
            m_studyTreeWidgetPacs->clear();
            QApplication::restoreOverrideCursor();
            QMessageBox::information( this , tr( "Starviewer" ) , tr( "ERROR QUERING!." ) );
            return;
        }

        if ( multipleQueryStudy.getStudyList().isEmpty() )
        {
            m_studyTreeWidgetPacs->clear();
            QApplication::restoreOverrideCursor();
            QMessageBox::information( this , tr( "Starviewer" ) , tr( "No study match found." ) );
            return;
        }

        m_studyListQueriedPacs = multipleQueryStudy.getStudyList(); //Guardem una còpia en local de la llista d'estudis trobats al PACS

        m_studyTreeWidgetPacs->insertStudyList( multipleQueryStudy.getStudyList() ); //fem que es visualitzi l'studyView seleccionat
        m_studyTreeWidgetPacs->insertSeriesList( multipleQueryStudy.getSeriesList() );
        m_studyTreeWidgetPacs->insertImageList( multipleQueryStudy.getImageList() );
        m_studyTreeWidgetPacs->setSortColumn( QStudyTreeWidget::ObjectName );

        QApplication::restoreOverrideCursor();
    }
}

void QueryScreen::queryStudy( QString source )
{
    CacheStudyDAL cacheStudyDAL;
    QList<DICOMStudy> studyListResultQuery;
    Status state;

    INFO_LOG( "Cerca d'estudis a la font" + source + " amb paràmetres " + buildQueryParametersString() );
    QApplication::setOverrideCursor( QCursor( Qt::WaitCursor ) );

    if( source == "Cache" )
    {
        m_seriesListWidgetCache->clear();
        state = cacheStudyDAL.queryStudy( buildDicomMask() , studyListResultQuery ); //busquem els estudis a la cache
        if ( !state.good() )
        {
            m_studyTreeWidgetCache->clear();
            QApplication::restoreOverrideCursor();
            showDatabaseErrorMessage( state );
            return;
        }
    }
    else if( source == "DICOMDIR" )
    {
        state = m_readDicomdir.readStudies( studyListResultQuery , buildDicomMask() );
        if ( !state.good() )
        {
            QApplication::restoreOverrideCursor();
            if ( state.code() == 1302 ) //Aquest és l'error quan no tenim un dicomdir obert l'ig
            {
                QMessageBox::warning( this , tr( "Starviewer" ) , tr( "Error, not opened Dicomdir" ) );
                ERROR_LOG( "No s'ha obert cap directori dicomdir " + state.text() );
            }
            else
            {
                QMessageBox::warning( this , tr( "Starviewer" ) , tr( "Error quering in dicomdir" ) );
                ERROR_LOG( "Error cercant estudis al dicomdir " + state.text() );
            }
            return;
        }
    }
    else
    {
        QApplication::restoreOverrideCursor();
        DEBUG_LOG( "Unrecognised source: " + source );
        return;
    }

    /* Aquest mètode a part de ser cridada quan l'usuari fa click al botó search, també es cridada al
     * constructor d'aquesta classe, per a que al engegar l'aplicació ja es mostri la llista d'estudis
     * que hi ha a la base de dades local. Si el mètode no troba cap estudi a la base de dades local
     * es llença el missatge que no s'han trobat estudis, però com que no és idonii, en el cas aquest que es
     * crida des del constructor que es mostri el missatge de que no s'han trobat estudis al engegar l'aplicació, el que
     * es fa és que per llançar el missatge es comprovi que la finestra estigui activa. Si la finestra no està activa
     * vol dir que el mètode ha estat invocat des del constructor
     */
    if ( studyListResultQuery.isEmpty() && isActiveWindow() )
    {
        //no hi ha estudis
        if( source == "Cache" )
            m_studyTreeWidgetCache->clear();
        else if( source == "DICOMDIR" )
            m_studyTreeWidgetDicomdir->clear();

        QApplication::restoreOverrideCursor();
        QMessageBox::information( this , tr( "Starviewer" ) , tr( "No study match found." ) );
    }
    else
    {
        if( source == "Cache" )
        {
            m_studyTreeWidgetCache->insertStudyList( studyListResultQuery );//es mostra la llista d'estudis
            m_studyTreeWidgetCache->setSortColumn( QStudyTreeWidget::ObjectName ); //ordenem pel nom
        }
        else if( source == "DICOMDIR" )
        {
            m_studyTreeWidgetDicomdir->clear();
            m_studyTreeWidgetDicomdir->insertStudyList( studyListResultQuery );
            m_studyTreeWidgetDicomdir->setSortColumn( QStudyTreeWidget::ObjectName );//ordenem pel nom
        }
        QApplication::restoreOverrideCursor();
    }
}

void QueryScreen::expandStudy( QString studyUID , QString pacsAETitle )
{
    QApplication::setOverrideCursor( QCursor( Qt::WaitCursor ) );

    switch ( m_tab->currentIndex() )
    {
        case 0 : // si estem a la pestanya de la cache
            querySeries( studyUID, "Cache" );
            break;
        case 1 :  //si estem la pestanya del PACS fem query al Pacs
            querySeriesPacs(studyUID, pacsAETitle);
            break;
        case 2 : //si estem a la pestanya del dicomdir, fem query al dicomdir
            querySeries( studyUID, "DICOMDIR" );
            break;
    }

    QApplication::restoreOverrideCursor();
}

/* AQUESTA ACCIO ES CRIDADA DES DEL STUDYLISTVIEW*/
void QueryScreen::expandSeries( QString studyUID , QString seriesUID , QString pacsAETitle )
{
    QApplication::setOverrideCursor( QCursor( Qt::WaitCursor ) );

    switch ( m_tab->currentIndex() )
    {
        case 0 : // si estem a la pestanya de la cache
            queryImage( studyUID , seriesUID, "Cache" );
            break;

        case 1 :  //si estem la pestanya del PACS fem query al Pacs
            queryImagePacs( studyUID , seriesUID , pacsAETitle );
            break;

        case 2 : //si estem a la pestanya del dicomdir, fem query al dicomdir
            queryImage( studyUID , seriesUID, "DICOMDIR" );
            break;
    }

    QApplication::restoreOverrideCursor();
}

void QueryScreen::querySeriesPacs(QString studyUID , QString pacsAETitle)
{
    DICOMSeries serie;
    Status state;
    QString text;
    PacsServer pacsConnection;
    QueryPacs querySeriesPacs;

    INFO_LOG( "Cercant informacio de les sèries de l'estudi" + studyUID + " del PACS " + pacsAETitle );

    if ( ! preparePacsServerConnection( pacsAETitle, &pacsConnection ).good() )
        return;

    state = pacsConnection.connect(PacsServer::query,PacsServer::seriesLevel);
    if ( !state.good() )
    {
        //Error al connectar
        ERROR_LOG( "Error al connectar al pacs " + pacsAETitle + ". PACS ERROR : " + state.text() );
        errorConnectingPacs ( pacsConnection.getPacs().getPacsID() );
        return;
    }

    querySeriesPacs.setConnection( pacsConnection.getConnection() );
    state = querySeriesPacs.query( buildSeriesDicomMask( studyUID ) );
    pacsConnection.disconnect();

    if ( !state.good() )
    {
        //Error a la query
        ERROR_LOG( "QueryScreen::QueryPacs : Error cercant les sèries al PACS " + pacsAETitle + ". PACS ERROR : " + state.text() );

        text = tr( "Error! Can't query series to PACS named %1" ).arg( pacsAETitle );
        QMessageBox::warning( this , tr( "Starviewer" ) , text );
        return;
    }

    if ( querySeriesPacs.getQueryResultsAsSeriesList().isEmpty() )
    {
        QMessageBox::information( this , tr( "Starviewer" ) , tr( "No series match for this study.\n" ) );
        return;
    }

    m_studyTreeWidgetPacs->insertSeriesList( querySeriesPacs.getQueryResultsAsSeriesList() );
}

void QueryScreen::querySeries( QString studyUID, QString source )
{
    QList<DICOMSeries> seriesListQueryResults;
    CacheSeriesDAL cacheSeriesDAL;
    Status state;
    DicomMask mask;

    INFO_LOG( "Cerca de sèries a la font " + source +" de l'estudi " + studyUID );
    if( source == "Cache" )
    {
        //preparem la mascara i cerquem les series a la cache
        mask.setStudyUID( studyUID );
        state = cacheSeriesDAL.querySeries( mask , seriesListQueryResults );
        if ( !state.good() )
        {
            showDatabaseErrorMessage( state );
            return;
        }
    }
    else if( source == "DICOMDIR" )
    {
        m_readDicomdir.readSeries( studyUID , "" , seriesListQueryResults ); //"" pq no busquem cap serie en concret
    }
    else
    {
        DEBUG_LOG( "Unrecognised source: " + source );
        return;
    }

    if ( seriesListQueryResults.isEmpty() )
    {
        QMessageBox::information( this , tr( "Starviewer" ) , tr( "No series match for this study.\n" ) );
        return;
    }

    if( source == "Cache" )
    {
        m_studyTreeWidgetCache->insertSeriesList( seriesListQueryResults );//inserim la informació de les sèries al llist
    }
    else if( source == "DICOMDIR" )
        m_studyTreeWidgetDicomdir->insertSeriesList( seriesListQueryResults );//inserim la informació de la sèrie al llistat
}

void QueryScreen::queryImagePacs( QString studyUID , QString seriesUID , QString AETitlePACS )
{
    DICOMSeries serie;
    Status state;
    QString text;
    PacsServer pacsConnection;
    QueryPacs queryImages;
    DicomMask dicomMask;

    QApplication::setOverrideCursor( QCursor( Qt::WaitCursor ) );

    INFO_LOG( "Cercant informacio de les imatges de l'estudi" + studyUID + " serie " + seriesUID + " del PACS " + AETitlePACS );

    dicomMask.setStudyUID( studyUID );
    dicomMask.setSeriesUID( seriesUID );
    dicomMask.setImageNumber( "" );
    dicomMask.setSOPInstanceUID( "" );

    if ( ! preparePacsServerConnection( AETitlePACS, &pacsConnection ).good() )
    {
        QApplication::restoreOverrideCursor();
        return;
    }

    state = pacsConnection.connect(PacsServer::query,PacsServer::imageLevel);
    if ( !state.good() )
    {   //Error al connectar
        QApplication::restoreOverrideCursor();
        ERROR_LOG( "Error al connectar al pacs " + AETitlePACS + ". PACS ERROR : " + state.text() );
        errorConnectingPacs ( pacsConnection.getPacs().getPacsID() );
        return;
    }

    queryImages.setConnection( pacsConnection.getConnection() );

    state = queryImages.query( dicomMask );
    if ( !state.good() )
    {
        //Error a la query
        QApplication::restoreOverrideCursor();
        ERROR_LOG( "QueryScreen::QueryPacs : Error cercant les images al PACS " + AETitlePACS + ". PACS ERROR : " + state.text() );

        text = tr( "Error! Can't query images to PACS named %1 " ).arg(AETitlePACS);
        QMessageBox::warning( this , tr( "Starviewer" ) , text );
        return;
    }

    pacsConnection.disconnect();

    if ( queryImages.getQueryResultsAsImageList().isEmpty() )
    {
        QApplication::restoreOverrideCursor();
        QMessageBox::information( this , tr( "Starviewer" ) , tr( "No images match for this series.\n" ) );
        return;
    }
    
    m_studyTreeWidgetPacs->insertImageList( queryImages.getQueryResultsAsImageList() );

    QApplication::restoreOverrideCursor();
}

void QueryScreen::retrieve()
{
    retrievePacs( false );
}

void QueryScreen::queryImage(QString studyUID, QString seriesUID, QString source )
{
    CacheImageDAL cacheImageDAL;
    DicomMask mask;
    QList<DICOMImage> imageListQueryResults;

    INFO_LOG( "Cerca d'imatges a la font " + source + " de l'estudi " + studyUID + " i serie " + seriesUID );

    if( source == "Cache" )
    {
        mask.setStudyUID( studyUID );
        mask.setSeriesUID( seriesUID );
        cacheImageDAL.queryImages( mask , imageListQueryResults );
    }
    else if( source == "DICOMDIR" )
    {
        m_readDicomdir.readImages( seriesUID , "", imageListQueryResults );
    }
    else
    {
        DEBUG_LOG( "Unrecognised source: " + source );
        return;
    }

    if (imageListQueryResults.isEmpty())
    {
        QMessageBox::information( this , tr( "Starviewer" ) , tr( "No images match for this study.\n" ) );
        return;
    }

    if( source == "Cache" )
        m_studyTreeWidgetCache->insertImageList( imageListQueryResults );//inserim la informació de la sèrie al llistat
    else if( source == "DICOMDIR" )
        m_studyTreeWidgetDicomdir->insertImageList( imageListQueryResults );//inserim la informació de la sèrie al llistat
}

void QueryScreen::retrievePacs( bool view )
{
    QApplication::setOverrideCursor( QCursor ( Qt::WaitCursor ) );

    QStringList selectedStudiesUIDList = m_studyTreeWidgetPacs->getSelectedStudiesUID();
    if( selectedStudiesUIDList.isEmpty() )
    {
        QApplication::restoreOverrideCursor();
        if( view )
            QMessageBox::warning( this , tr( "Starviewer" ) , tr( "Select a study to view " ) );
        else
            QMessageBox::warning( this , tr( "Starviewer" ) , tr( "Select a study to download " ) );

        return;
    }

    StarviewerSettings settings;
    bool ok;

    foreach( QString currentStudyUID, selectedStudiesUIDList )
    {
        DicomMask mask;
        QString defaultSeriesUID;
        Status state;
        Operation operation;
        PacsParameters pacs;
        PacsListDB pacsListDB;
        QString pacsAETitle;

        //Busquem en quina posició de la llista on guardem els estudis trobats al PACS en quina posició està per poder-lo recuperar 
        //TODO no hauria de tenir la responsabilitat de retornar l'estudi al QStudyTreeView no la pròpia QueryScreen
        int indexStudyInList = getStudyPositionInStudyListQueriedPacs( currentStudyUID , 
        m_studyTreeWidgetPacs->getStudyPACSAETitleFromSelectedStudies( currentStudyUID ) );
        ok = true;
        //Tenim l'informació de l'estudi a descarregar a la llista d'estudis cercats del pacs, el busquem a la llista a través d'aquest mètode
        if ( indexStudyInList == -1 ) 
        {//Es comprova que existeixi l'estudi a la llista d'estudis de la última query que s'ha fet al PACS
         //TODO Arreglar missatge d'error
            QApplication::restoreOverrideCursor();
            QMessageBox::warning( this , tr( "Starviewer" ) , tr( "Internal Error : " ) );
        }
        else
        {
            DICOMStudy studyToRetrieve =  m_studyListQueriedPacs.value( indexStudyInList );

            pacsAETitle = m_studyTreeWidgetPacs->getStudyPACSAETitleFromSelectedStudies(currentStudyUID);

            //Inserim l'informació de l'estudi a la caché!
            state = insertStudyCache( studyToRetrieve );

            if( !state.good() )
            {
                if ( state.code() != 2019 ) // si hi ha l'error 2019, indica que l'estudi ja existeix a la base de dades, per tant estar parcialment o totalment descarregat, de totes maneres el tornem a descarregar
                {
                    //TODO no entenc que es fa aquí, el comentari anterior no sembla gaire coherent
                    QApplication::restoreOverrideCursor();
                    showDatabaseErrorMessage( state );
                    ok = false;
                }
                else // si l'estudi ja existeix actualizem la seva informació també
                {
                    CacheStudyDAL cacheStudyDAL; //Actualitzem la informació
                    cacheStudyDAL.updateStudy( studyToRetrieve );
                }
            }

            if( ok )
            {
                mask.setStudyUID( currentStudyUID );//definim la màscara per descarregar l'estudi

                // TODO aquí només tenim en compte l'última sèrie o imatge seleccionada
                // per tant si seleccionem més d'una sèrie/imatge només s'en baixarà una
                // Caldria fer possible que es baixi tants com en seleccionem
                if ( !m_studyTreeWidgetPacs->getCurrentSeriesUID().isEmpty() )
                    mask.setSeriesUID( m_studyTreeWidgetPacs->getCurrentSeriesUID() );

                if ( !m_studyTreeWidgetPacs->getCurrentImageUID().isEmpty() )
                    mask.setSOPInstanceUID( m_studyTreeWidgetPacs->getCurrentImageUID() );

                //busquem els paràmetres del pacs del qual volem descarregar l'estudi
                state = pacsListDB.queryPacs( &pacs , pacsAETitle );

                if ( !state.good() )
                {
                    QApplication::restoreOverrideCursor();
                    showDatabaseErrorMessage( state );
                    // TODO potser l'error no s'hauria de mostrar aquí, es podria fer un missatge de
                    // "hi ha hagut errors en alguns estudis" o algo per l'estil
                }
                else
                {
                    //emplanem els parametres amb dades del starviewersettings
                    pacs.setAELocal( settings.getAETitleMachine() );
                    pacs.setTimeOut( settings.getTimeout().toInt( NULL , 10 ) );
                    pacs.setLocalPort( settings.getLocalPort() );

                    //definim l'operacio
                    operation.setPacsParameters( pacs );
                    operation.setDicomMask( mask );
                    if ( view )
                    {
                        operation.setOperation( Operation::View );
                        operation.setPriority( Operation::Medium );//Té priorita mitjà per passar al davant de les operacions de Retrieve
                    }
                    else
                    {
                        operation.setOperation( Operation::Retrieve );
                        operation.setPriority( Operation::Low );
                    }
                    //emplenem les dades de l'operació
                    operation.setPatientName( studyToRetrieve.getPatientName() );
                    operation.setPatientID( studyToRetrieve.getPatientId() );
                    operation.setStudyID( studyToRetrieve.getStudyId() );
                    operation.setStudyUID( studyToRetrieve.getStudyUID() );

                    m_qexecuteOperationThread.queueOperation( operation );
                }
            }
        }
    }
    QApplication::restoreOverrideCursor();
}

Status QueryScreen::insertStudyCache( DICOMStudy stu )
{
    QString absPath;
    Status state;
    DICOMStudy study = stu;
    StarviewerSettings settings;
    CacheStudyDAL cacheStudyDAL;

    //creem el path absolut de l'estudi
    absPath = settings.getCacheImagePath() + study.getStudyUID() + "/";
    //inserim l'estudi a la caché
    study.setAbsPath(absPath);
    state = cacheStudyDAL.insertStudy( &study );

    return state;
}

void QueryScreen::studyRetrievedView( QString studyUID , QString seriesUID , QString sopInstanceUID )
{
    retrieve( studyUID , seriesUID , sopInstanceUID, "Cache" );
}

void QueryScreen::refreshTab( int index )
{
    switch ( index )
    {
        case LocalDataBaseTab:
                m_buttonGroupModality->setEnabled(false);
                clearCheckedModality();
                m_qwidgetAdvancedSearch->setEnabled(false);
                break;

        case PACSQueryTab:
                m_buttonGroupModality->setEnabled(true);
                clearCheckedModality();
                m_qwidgetAdvancedSearch->setEnabled( true );
                break;

        case DICOMDIRTab:
                m_buttonGroupModality->setEnabled(false);
                clearCheckedModality();
                m_qwidgetAdvancedSearch->setEnabled(false);
                break;
    }
}

void QueryScreen::view()
{
    switch ( m_tab->currentIndex() )
    {
        case 0 :
            loadStudies( m_studyTreeWidgetCache->getSelectedStudiesUID(), m_studyTreeWidgetCache->getCurrentSeriesUID(), m_studyTreeWidgetCache->getCurrentImageUID(), "Cache" );
            break;

        case 1 :
            retrievePacs( true );
           break;

        case 2 :
            loadStudies( m_studyTreeWidgetDicomdir->getSelectedStudiesUID(), m_studyTreeWidgetDicomdir->getCurrentSeriesUID(), m_studyTreeWidgetDicomdir->getCurrentImageUID(), "DICOMDIR" );
            break;

        default :
            break;
    }
}

void QueryScreen::viewFromQSeriesListWidget()
{
    QStringList studyUIDList;

    studyUIDList << m_seriesListWidgetCache->getCurrentStudyUID();//Agafem l'estudi uid de la sèrie seleccionada
    loadStudies( studyUIDList, m_seriesListWidgetCache->getCurrentSeriesUID(), "", "Cache" );
}

void QueryScreen::retrieve( QString studyUID , QString seriesUID , QString sopInstanceUID, QString source )
{
    CacheStudyDAL cacheStudyDAL;

    if ( studyUID.isEmpty() )
    {
        QMessageBox::warning( this , tr( "Starviewer" ) , tr( "Select a study to view " ) );
        return;
    }

    QStringList files;
    if( source == "Cache" )
    {
        files = cacheStudyDAL.getFiles( studyUID );
        cacheStudyDAL.updateStudyAccTime( studyUID );
    }
    else if( source == "DICOMDIR" )
    {
        files = m_readDicomdir.getFiles( studyUID );
    }
    else
    {
        DEBUG_LOG("Unrecognized source: " + source );
        return;
    }

    this->close();//s'amaga per poder visualitzar la serie
    if ( m_operationStateScreen->isVisible() )
    {
        m_operationStateScreen->close();//s'amaga per poder visualitzar la serie
    }

    // enviem la informació a processar
    emit processFiles( files, studyUID, seriesUID, sopInstanceUID );
}

void QueryScreen::loadStudies( QStringList studiesUIDList, QString defaultSeriesUID , QString defaultSOPInstanceUID, QString source )
{
    if( studiesUIDList.isEmpty() )
    {
        QMessageBox::warning( this , tr( "Starviewer" ) , tr( "Select at least one study to view" ) );
        return;
    }

    CacheStudyDAL cacheStudyDAL;
    QStringList files;
    if( source == "Cache" )
    {
        foreach(QString studyUID, studiesUIDList )
        {
            files += cacheStudyDAL.getFiles( studyUID );
            cacheStudyDAL.updateStudyAccTime( studyUID );
        }
    }
    else if( source == "DICOMDIR" )
    {
        foreach(QString studyUID, studiesUIDList )
        {
            files += m_readDicomdir.getFiles( studyUID );
        }
    }
    else
    {
        DEBUG_LOG("Unrecognized source: " + source );
        return;
    }

    this->close();//s'amaga per poder visualitzar la serie
    if ( m_operationStateScreen->isVisible() )
    {
        m_operationStateScreen->close();//s'amaga per poder visualitzar la serie
    }

    // enviem la informació a processar
    emit processFiles( files, studiesUIDList.first(), defaultSeriesUID, defaultSOPInstanceUID );
}

void QueryScreen::importDicomdir()
{
    DICOMDIRImporter importDicom;
    Status state;
    int failedStudies = 0;

    QApplication::setOverrideCursor( QCursor( Qt::WaitCursor ) );

//     importDicom.import( m_readDicomdir.getDicomdirPath() , m_studyTreeWidgetDicomdir->getSelectedStudyUID() , m_studyTreeWidgetDicomdir->getSelectedSeriesUID() ,  m_studyTreeWidgetDicomdir->getSelectedImageUID() );
    // TODO ara només permetrem importar estudis sencers
    foreach( QString studyUID, m_studyTreeWidgetDicomdir->getSelectedStudiesUID() )
    {
        state = importDicom.import( m_readDicomdir.getDicomdirFilePath(), studyUID, QString(), QString() );

        if (!state.good()) failedStudies++;
    }

    queryStudy("Cache"); //Actualitzem la llista tenint en compte el criteri de cerca

    QApplication::restoreOverrideCursor();

    if ( failedStudies > 0 ) //si ha fallat algun estudi
    {
        if ( failedStudies == m_studyTreeWidgetDicomdir->getSelectedStudiesUID().count() ) //si han fallat tots els estudis
            QMessageBox::critical( this , tr( "Starviewer" ) , tr( "Error: Can't import selected studies" ) );
        else
            QMessageBox::warning( this , tr( "Starviewer" ) , tr( "Error: Some studies can't be imported" ) );
    }
}

void QueryScreen::deleteSelectedStudiesInCache()
{
    QStringList studiesList = m_studyTreeWidgetCache->getSelectedStudiesUID();
    if( studiesList.isEmpty() )
    {
        QMessageBox::information( this , tr( "Starviewer" ) , tr( "Please select at least one study to delete" ) );
    }
    else
    {
        switch( QMessageBox::information( this , tr( "Starviewer" ) ,
                        tr( "Are you sure you want to delete the selected Studies?" ) ,
                        tr( "&Yes" ) , tr( "&No" ) ,
                        0, 1 )
                )
        {
            case 0:
                //Posem el cursor en espera
                QApplication::setOverrideCursor(Qt::WaitCursor);

                Status state;
                CacheStudyDAL cacheStudyDAL;
                foreach(QString studyUID, studiesList)
                {
                    if( m_qcreateDicomdir->studyExists( studyUID ) )
                    {
                        QMessageBox::warning( this , tr( "Starviewer" ) ,
                        tr( "The study with UID: %1 is in use by the DICOMDIR List. If you want to delete this study you should remove it from the DICOMDIR List first." ).arg(studyUID) );
                    }
                    else
                    {
                        state = cacheStudyDAL.delStudy( studyUID );
                        if ( state.good() )
                        {
                            INFO_LOG( "S'esborra de la cache l'estudi " + studyUID );
                            m_studyTreeWidgetCache->removeStudy( studyUID );
                            m_seriesListWidgetCache->clear();
                        }
                        else
                        {
                            // TODO potser s'hauria de fer al final, recollir per quins hi ha hagut error i mostrar-ho
                            showDatabaseErrorMessage( state );
                        }
                    }
                }

                QApplication::restoreOverrideCursor();
        }
    }
}

void QueryScreen::studyRetrieveFinished( QString studyUID )
{
    DICOMStudy study;
    CacheStudyDAL cacheStudyDAL;
    Status state;

    state = cacheStudyDAL.queryStudy( studyUID , study );

    if ( state.good() )
    {
        m_studyTreeWidgetCache->insertStudy( &study );
        m_studyTreeWidgetCache->sort();
    }
    else
        showDatabaseErrorMessage( state );
}

void QueryScreen::closeEvent( QCloseEvent* event )
{
    writeSettings(); // guardem els settings

    m_operationStateScreen->close(); //Tanquem la QOperationStateScreen al tancar la QueryScreen

    event->accept();
    m_qcreateDicomdir->clearTemporaryDir();
}

void QueryScreen::writeSettings()
{
    /* Només guardem els settings quan la interfície ha estat visible, ja que hi ha settings com el QSplitter que si la primera vegada
     * que executem l'starviewer no obrim la QueryScreen retorna un valors incorrecte per això, el que fem és comprova que la QueryScreen
     * hagi estat visible per guardar el settings
     */
    if (this->isVisible())
    {
        StarviewerSettings settings;

        saveQStudyTreeWidgetColumnsWidth();

        //guardem l'estat del QSplitter que divideix el StudyTree del QSeries
        settings.setQueryScreenStudyTreeSeriesListQSplitterState( m_StudyTreeSeriesListQSplitter->saveState() );

        //guardem la posició en que es troba la pantalla
        settings.setQueryScreenWindowPositionX( x() );
        settings.setQueryScreenWindowPositionY( y() );

        //guardem les dimensions de la pantalla
        settings.setQueryScreenWindowHeight( height() );
        settings.setQueryScreenWindowWidth( width() );
    }
}

void QueryScreen::saveQStudyTreeWidgetColumnsWidth()
{
    StarviewerSettings settings;

    for ( int column = 0; column < m_studyTreeWidgetPacs->getNumberOfColumns(); column++ )
    {
        settings.setStudyPacsListColumnWidth( column , m_studyTreeWidgetPacs->getColumnWidth( column ) );
    }

    for ( int column = 0; column < m_studyTreeWidgetCache->getNumberOfColumns(); column++ )
    {
        settings.setStudyCacheListColumnWidth( column , m_studyTreeWidgetCache->getColumnWidth( column ) );
    }

    for ( int column = 0; column < m_studyTreeWidgetDicomdir->getNumberOfColumns(); column++ )
    {
        settings.setStudyDicomdirListColumnWidth( column , m_studyTreeWidgetDicomdir->getColumnWidth( column ) );
    }
}

void QueryScreen::showOperationStateScreen()
{
    if ( !m_operationStateScreen->isVisible() )
    {
        m_operationStateScreen->setVisible( true );
    }
    else
    {
        m_operationStateScreen->raise();
        m_operationStateScreen->activateWindow();
    }
}

void QueryScreen::convertToDicomdir()
{
    CacheStudyDAL cacheStudyDAL;
    DICOMStudy study;
    QStringList studiesUIDList = m_studyTreeWidgetCache->getSelectedStudiesUID();

    foreach(QString studyUID, studiesUIDList )
    {
        //busquem la informació de l'estudi
        cacheStudyDAL.queryStudy( studyUID , study );

        //afegim l'estudi a la llista d'estudis pendents per crear el Dicomdir
        m_qcreateDicomdir->addStudy( study );
    }
}

void QueryScreen::openDicomdir()
{
	StarviewerSettings settings;
    QFileDialog *dlg = new QFileDialog( 0 , QFileDialog::tr( "Open" ) , settings.getLastOpenedDICOMDIRPath(), "DICOMDIR" );
    QString path, dicomdirPath;

    dlg->setFileMode( QFileDialog::ExistingFile );
    Status state;

    if ( dlg->exec() == QDialog::Accepted )
    {
        if ( !dlg->selectedFiles().empty() )
            dicomdirPath = dlg->selectedFiles().takeFirst();

        state = m_readDicomdir.open ( dicomdirPath );//Obrim el dicomdir

        if ( !state.good() )
        {
            QMessageBox::warning( this , tr( "Starviewer" ) , tr( "Error openning dicomdir" ) );
            ERROR_LOG( "Error al obrir el dicomdir " + dicomdirPath + state.text() );
        }
        else
        {
            INFO_LOG( "Obert el dicomdir " + dicomdirPath );
			settings.setLastOpenedDICOMDIRPath( QFileInfo(dicomdirPath).dir().path() );
            this->bringToFront();
            m_tab->setCurrentIndex( 2 ); // mostre el tab del dicomdir
        }

        clearTexts();//Netegem el filtre de cerca al obrir el dicomdir
        //cerquem els estudis al dicomdir per a que es mostrin
        queryStudy("DICOMDIR");
    }

    delete dlg;
}

void QueryScreen::storeStudiesToPacs()
{
    QList<PacsParameters> selectedPacsList;
    QStringList studiesUIDList = m_studyTreeWidgetCache->getSelectedStudiesUID();
    QApplication::setOverrideCursor( QCursor( Qt::WaitCursor ) );

    m_PACSNodes->getSelectedPacs( selectedPacsList ); //Emplemen el pacsList amb les pacs seleccionats al QPacsList

    switch( selectedPacsList.size() )
    {
        case  0 :
            QApplication::restoreOverrideCursor();
            QMessageBox::warning( this , tr( "Starviewer" ) , tr( "You have to select a PACS to store the study in" ));
            break;

        case 1 :
        {
            StarviewerSettings settings;
            foreach( QString studyUID, studiesUIDList )
            {
                CacheStudyDAL cacheStudy;
                PacsListDB pacsListDB;
                PacsParameters pacs;
                Operation storeStudyOperation;
                DicomMask dicomMask;
                Status state;
                DICOMStudy study;

                cacheStudy.queryStudy( studyUID, study );
                dicomMask.setStudyUID( studyUID );
                storeStudyOperation.setPatientName( study.getPatientName() );
                storeStudyOperation.setStudyUID( study.getStudyUID() );
                storeStudyOperation.setPriority( Operation::Low );
                storeStudyOperation.setOperation( Operation::Move );
                storeStudyOperation.setDicomMask( dicomMask );
                storeStudyOperation.setPatientID( study.getPatientId() );
                storeStudyOperation.setStudyID( study.getStudyId() );

                state = pacsListDB.queryPacs( &pacs, selectedPacsList.value(0).getAEPacs() );//cerquem els par�etres del Pacs al qual s'han de cercar les dades
                if ( state.good() )
                {
                    storeStudyOperation.setPacsParameters( pacs );

                    m_qexecuteOperationThread.queueOperation( storeStudyOperation );
                }
                else
                {
                    QApplication::restoreOverrideCursor();
                    // TODO potser s'haurien de recollir si hi ha hagut errors i al final fer el sumari si calgues
                    showDatabaseErrorMessage( state );
                }
            }
            break;
        }

        default :
            QApplication::restoreOverrideCursor();
            QMessageBox::warning( this , tr( "Starviewer" ) , tr( "The studies can only be stored to one PACS" ));
            break;
    }

    QApplication::restoreOverrideCursor();
}

void QueryScreen::errorConnectingPacs( int IDPacs )
{
    PacsListDB pacsListDB;
    PacsParameters errorPacs;
    QString errorMessage;

    pacsListDB.queryPacs( &errorPacs, IDPacs );

    errorMessage = tr( "Can't connect to PACS %1 from %2\nBe sure that the IP and AETitle of the PACS are correct" )
        .arg( errorPacs.getAEPacs() )
        .arg( errorPacs.getInstitution()
    );

    QMessageBox::critical( this , tr( "Starviewer" ) , errorMessage );
}

void QueryScreen::errorQueringStudiesPacs( int PacsID )
{
    PacsListDB pacsListDB;
    PacsParameters errorPacs;
    QString errorMessage;

    pacsListDB.queryPacs( &errorPacs, PacsID );
    errorMessage = tr( "Can't query PACS %1 from %2\nBe sure that the IP and AETitle of this PACS are correct" )
        .arg( errorPacs.getAEPacs() )
        .arg( errorPacs.getInstitution()
    );

    QMessageBox::critical( this , tr( "Starviewer" ) , errorMessage );
}

DicomMask QueryScreen::buildSeriesDicomMask( QString studyUID )
{
    DicomMask mask;

    mask.setStudyUID( studyUID );
    mask.setSeriesDate( "" );
    mask.setSeriesTime( "" );
    mask.setSeriesModality( "" );
    mask.setSeriesNumber( "" );
    mask.setSeriesBodyPartExaminated( "" );
    mask.setSeriesUID( "" );
    mask.setPPSStartDate( "" );
    mask.setPPStartTime( "" );
    mask.setRequestAttributeSequence( "" , "" );

    return mask;
}

QString QueryScreen::getStudyDatesStringMask()
{
    if (m_anyDateRadioButton->isChecked())
    {
        return "";
    }
    else if (m_todayRadioButton->isChecked())
    {
        return QDate::currentDate().toString("yyyyMMdd");
    }
    else if (m_yesterdayRadioButton->isChecked())
    {
        return QDate::currentDate().addDays(-1).toString("yyyyMMdd");
    }
    else if (m_lastWeekRadioButton->isChecked())
    {
        return QDate::currentDate().addDays(-7).toString("yyyyMMdd") + "-" + QDate::currentDate().toString("yyyyMMdd");
    }
    else if (m_customDateRadioButton->isChecked())
    {
        QString date;

        if ( m_fromDateCheck->isChecked() && m_toDateCheck->isChecked() )
        {
            if ( m_fromStudyDate->date() == m_toStudyDate->date() )
            {
                date = m_fromStudyDate->date().toString( "yyyyMMdd" );
            }
            else
            {
                date = m_fromStudyDate->date().toString( "yyyyMMdd" ) + "-" + m_toStudyDate->date().toString( "yyyyMMdd" );
            }
        }
        else
        {
            if ( m_fromDateCheck->isChecked() )
            {
                // indiquem que volem buscar tots els estudis d'aquella data en endavant
                date = m_fromStudyDate->date().toString( "yyyyMMdd" ) + "-";
            }
            else if ( m_toDateCheck->isChecked() )
            {
                //indiquem que volem buscar tots els estudis que no superin aquesta data
                date = "-"+ m_toStudyDate->date().toString( "yyyyMMdd" );
            }
        }

        return date;
    }
    return "";
}

DicomMask QueryScreen::buildDicomMask()
{
    /*Per fer cerques entre valors consultat el capítol 4 de DICOM punt C.2.2.2.5*/
    /*Per defecte si passem un valor buit a la màscara,farà una cerca per tots els els valor d'aquella camp*/
    /*En aquí hem de fer un set a tots els camps que volem cercar */
    DicomMask mask;
    QString modalityMask;

    //S'afegeix '*' al patientId i patientName automàticament
    QString patientID = m_patientIDText->text();
    if ( ! patientID.startsWith("*") ) patientID = "*" + patientID;
    if ( ! patientID.endsWith("*") ) patientID = patientID + "*";
    mask.setPatientId(patientID);

    QString patientName = m_patientNameText->text();
    if ( ! patientName.startsWith("*") ) patientName = "*" + patientName;
    if ( ! patientName.endsWith("*") ) patientName = patientName + "*";
    mask.setPatientName(patientName);

    mask.setStudyId( m_studyIDText->text()  );
    mask.setStudyDate( getStudyDatesStringMask() );
    mask.setStudyDescription( "" );
    mask.setStudyTime( m_studyTimeText->text() );
    mask.setStudyUID( m_studyUIDText->text() );
    mask.setInstitutionName( "" );
    mask.setStudyModality( m_studyModalityText->text() );
    mask.setPatientAge( "" );
    mask.setAccessionNumber( m_accessionNumberText->text() );
    mask.setReferringPhysiciansName( m_referringPhysiciansNameText->text() );
    mask.setPatientSex( "" );
    mask.setPatientBirth( "" );

    //si hem de filtrar per un camp a nivell d'imatge o serie activem els filtres de serie
    if (!m_seriesUIDText->text().isEmpty() || !m_scheduledProcedureStepIDText->text().isEmpty() ||
        !m_requestedProcedureIDText->text().isEmpty() || !m_checkAll->isChecked() ||
        !m_SOPInstanceUIDText->text().isEmpty() || !m_instanceNumberText->text().isEmpty() ||
        !m_PPStartDateText->text().isEmpty() || !m_PPStartTimeText->text().isEmpty() ||
        !m_seriesNumberText->text().isEmpty()
       )
    {
        mask.setSeriesDate( "" );
        mask.setSeriesTime( "" );
        mask.setSeriesModality( "" );
        mask.setSeriesNumber( m_seriesNumberText->text() );
        mask.setSeriesBodyPartExaminated( "" );
        mask.setSeriesUID( m_seriesUIDText->text() );
        mask.setRequestAttributeSequence( m_requestedProcedureIDText->text() , m_scheduledProcedureStepIDText->text() );
        mask.setPPSStartDate( m_PPStartDateText->text() );
        mask.setPPStartTime( m_PPStartTimeText->text() );

        if ( m_buttonGroupModality->isEnabled() )
        { //es crea una sentencia per poder fer un in
            if ( m_checkCT->isChecked() )
            {
                mask.setSeriesModality( "CT" );
            }
            else if ( m_checkCR->isChecked() )
            {
                mask.setSeriesModality( "CR" );
            }
            else if ( m_checkDX->isChecked() )
            {
                mask.setSeriesModality( "DX" );
            }
            else if ( m_checkES->isChecked() )
            {
                mask.setSeriesModality( "ES" );
            }
            else if ( m_checkMG->isChecked() )
            {
                mask.setSeriesModality( "MG" );
            }
            else if ( m_checkMR->isChecked() )
            {
                mask.setSeriesModality( "MR" );
            }
            else if ( m_checkNM->isChecked() )
            {
                mask.setSeriesModality( "NM" );
            }
            else if ( m_checkDT->isChecked() )
            {
                mask.setSeriesModality( "DT" );
            }
            else if ( m_checkPT->isChecked() )
            {
                mask.setSeriesModality(  "PT" );
            }
            else if ( m_checkRF->isChecked() )
            {
                mask.setSeriesModality(  "RF" );
            }
            else if ( m_checkSC->isChecked() )
            {
                mask.setSeriesModality(  "SC" );
            }
            else if ( m_checkUS->isChecked() )
            {
                mask.setSeriesModality(  "US" );
            }
            else if ( m_checkXA->isChecked() )
            {
                mask.setSeriesModality(  "XA" );
            }
            else if ( m_checkOtherModality->isChecked() )
            {
                mask.setSeriesModality( m_textOtherModality->text() );
            }
        }

        if ( !m_SOPInstanceUIDText->text().isEmpty() || !m_instanceNumberText->text().isEmpty() )
        {
            mask.setImageNumber( m_instanceNumberText->text() );
            mask.setSOPInstanceUID( m_SOPInstanceUIDText->text() );
        }

    }

    return mask;
}

void QueryScreen::addModalityStudyMask( DicomMask* mask, QString modality )
{
    QString studyModalities;

    if ( mask->getStudyModality().length() > 0 ) // ja hi ha una altra modalitat
        studyModalities = mask->getStudyModality() + "," + modality;
    else
        studyModalities = modality;

    mask->setStudyModality( studyModalities );
}

int QueryScreen::getStudyPositionInStudyListQueriedPacs( QString studyUID , QString pacsAETitle )
{
    int index = 0;
    bool studyUIDisTheSame = false , pacsAETitleIsTheSame = false;

    while ( index < m_studyListQueriedPacs.count() && ( !studyUIDisTheSame || !pacsAETitleIsTheSame ) )
    {
        studyUIDisTheSame = m_studyListQueriedPacs.value( index ).getStudyUID() == studyUID;
        pacsAETitleIsTheSame = m_studyListQueriedPacs.value( index ).getPacsAETitle() == pacsAETitle;
        
        if (!studyUIDisTheSame || !pacsAETitleIsTheSame ) index++;
    } 

    return index < m_studyListQueriedPacs.count() ? index : -1;
}

QString QueryScreen::buildQueryParametersString()
{
	QString logMessage;

    logMessage = "PATIENT_ID=[" + m_patientIDText->text() + "] "
        + "PATIENT_NAME=[" + m_patientNameText->text() + "] "
        + "STUDY_ID=[" + m_studyIDText->text() + "] "
        + "DATES_MASK=[" + getStudyDatesStringMask() + "] "
        + "ACCESSION_NUMBER=[" + m_accessionNumberText->text() + "]";

    return logMessage;
}

void QueryScreen::showDatabaseErrorMessage( const Status &state )
{
    QString message;
    if (state.code() == 2006)
    {
        message = tr("The Cache is blocked by another Starviewer window."
                     "\nTry to close all the others Starviewer windows and try again."
                     "\n\nIf you want to open different Starviewer's windows always choose the 'New' option from the File menu.");
    }
    else
    {
        message = state.text() + tr("\nError Number: %1").arg(state.code());
    }
    QMessageBox::critical( this , tr( "Starviewer" ) , message );
}

};

