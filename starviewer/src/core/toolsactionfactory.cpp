/***************************************************************************
 *   Copyright (C) 2005-2006 by Grup de Gràfics de Girona                  *
 *   http://iiia.udg.es/GGG/index.html?langu=uk                            *
 *                                                                         *
 *   Universitat de Girona                                                 *
 ***************************************************************************/
#include "toolsactionfactory.h"
#include <QAction>
#include <QSignalMapper>

namespace udg {

ToolsActionFactory::ToolsActionFactory( QWidget *parent )
 : QObject( parent )
{
    m_signalMapper = new QSignalMapper( this );

    m_slicingAction = new QAction( 0 );
    m_slicingAction->setText( tr("Slicer") );
    m_slicingAction->setStatusTip( tr("Enable/Disable slicing tool") );
    m_slicingAction->setIcon( QIcon(":/images/slicing.png") );
    m_slicingAction->setCheckable( true );
    m_signalMapper->setMapping( m_slicingAction , "SlicingTool" );
    connect( m_slicingAction , SIGNAL( triggered() ) , m_signalMapper , SLOT( map() ) );

    m_windowLevelAction = new QAction( 0 );
    m_windowLevelAction->setText( tr("Window Level") );
    m_windowLevelAction->setStatusTip( tr("Enable/Disable Window Level tool") );
    m_windowLevelAction->setIcon( QIcon(":/images/windowLevel.png") );
    m_windowLevelAction->setCheckable( true );
    m_signalMapper->setMapping( m_windowLevelAction , "WindowLevelTool" );
    connect( m_windowLevelAction , SIGNAL( triggered() ) , m_signalMapper , SLOT( map() ) );

    m_zoomAction = new QAction( 0 );
    m_zoomAction->setText( tr("Zoom") );
    m_zoomAction->setStatusTip( tr("Enable/Disable Zoom tool") );
    m_zoomAction->setIcon( QIcon(":/images/zoom.png") );
    m_zoomAction->setCheckable( true );
    m_signalMapper->setMapping( m_zoomAction , "ZoomTool" );
    connect( m_zoomAction , SIGNAL( triggered() ) , m_signalMapper , SLOT( map() ) );

    m_moveAction = new QAction( 0 );
    m_moveAction->setText( tr("Move") );
    m_moveAction->setStatusTip( tr("Enable/Disable Move tool") );
    m_moveAction->setIcon( QIcon(":/images/move.png") );
    m_moveAction->setCheckable( true );
    m_signalMapper->setMapping( m_moveAction , "TranslateTool" );
    connect( m_moveAction , SIGNAL( triggered() ) , m_signalMapper , SLOT( map() ) );

    m_screenShotAction = new QAction( 0 );
    m_screenShotAction->setText( tr("Screen Shot") );
    m_screenShotAction->setStatusTip( tr("Enable/Disable Screen shot tool") );
    m_screenShotAction->setIcon( QIcon(":/images/photo.png") );
    m_screenShotAction->setCheckable( true );
    m_signalMapper->setMapping( m_screenShotAction , "ScreenShotTool" );
    connect( m_screenShotAction , SIGNAL( triggered() ) , m_signalMapper , SLOT( map() ) );

    m_rotate3dAction = new QAction( 0 );
    m_rotate3dAction->setText( tr("3D Rotation") );
    m_rotate3dAction->setStatusTip( tr("Enable/Disable 3D Rotation tool") );
    m_rotate3dAction->setIcon( QIcon(":/images/rotate3d.png") );
    m_rotate3dAction->setCheckable( true );
    m_signalMapper->setMapping( m_rotate3dAction , "3DRotationTool" );
    connect( m_rotate3dAction , SIGNAL( triggered() ) , m_signalMapper , SLOT( map() ) );

    connect( m_signalMapper, SIGNAL( mapped(QString) ), this , SIGNAL( triggeredTool(QString) ) );

    m_availableToolActions.clear();
    m_availableToolActions["SlicingTool"] = m_slicingAction;
    m_availableToolActions["WindowLevelTool"] = m_windowLevelAction;
    m_availableToolActions["ZoomTool"] = m_zoomAction;
    m_availableToolActions["TranslateTool"] = m_moveAction;
    m_availableToolActions["ScreenShotTool"] = m_screenShotAction;
    m_availableToolActions["3DRotationTool"] = m_rotate3dAction;
}

ToolsActionFactory::~ToolsActionFactory()
{
}

QAction *ToolsActionFactory::getActionFrom( QString toolName )
{
    if( m_availableToolActions.find( toolName ) != m_availableToolActions.end() )
        return m_availableToolActions[toolName];
    else return 0;
}

}
