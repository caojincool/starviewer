/***************************************************************************
 *   Copyright (C) 2007 by Grup de Gràfics de Girona                       *
 *   http://iiia.udg.es/GGG/index.html?langu=uk                            *
 *                                                                         *
 *   Universitat de Girona                                                 *
 ***************************************************************************/



#include "qdifuperfuextension.h"


// Qt
#include <QMessageBox>
#include <QSettings>
#include <QtDebug>


// VTK
#include <vtkActor.h>
#include <vtkCellType.h>
#include <vtkCommand.h>
#include <vtkDataSetMapper.h>
#include <vtkImageActor.h>
#include <vtkImageCast.h>
#include <vtkImageMapToWindowLevelColors.h>
#include <vtkImageThreshold.h>
#include <vtkImageViewer2.h>
#include <vtkLookupTable.h>
#include <vtkPoints.h>
#include <vtkProperty.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkRenderer.h>
#include <vtkUnstructuredGrid.h>


#include "strokesegmentationmethod.h"
#include "toolsactionfactory.h"
#include "volumecalculator.h"
#include "volumesourceinformation.h"








//prova recte
#include "itkRescaleIntensityImageFilter.h"
#include "itkCurvatureAnisotropicDiffusionImageFilter.h"
#include "itkExtractImageFilter.h"



#include "itkRegistre3DAffine.h"
#include "udgPerfusionEstimator.h"
#include "udgBinaryMaker.h"



// PROVES!!!!
// #include "q2dviewerextension.h"
// PROVES!!!!




namespace udg {



QDifuPerfuSegmentationExtension::QDifuPerfuSegmentationExtension( QWidget * parent )
 : QWidget( parent )
{
    setupUi( this );


    m_diffusionInputVolume = 0;
    m_perfusionInputVolume = 0;
    m_diffusionMainVolume = 0;
    m_perfusionMainVolume = 0;
    m_diffusionRescaledVolume = 0;
    m_perfusionRescaledVolume = 0;

    m_activedMaskVolume = 0;
    m_strokeMaskVolume = 0;
    m_ventriclesMaskVolume = 0;

    m_blackpointEstimatedVolume = 0;
    m_penombraMaskVolume = 0;

    m_perfusionOverlay = 0;

    m_strokeSegmentationMethod = 0;
    m_strokeVolume = 0.0;

    m_registerTransform = 0;

    m_penombraVolume = 0.0;

    m_isLeftButtonPressed = false;

    m_squareActor = vtkActor::New();

    m_actionFactory = 0;


    createActions();
    createToolBars();
    createConnections();


    readSettings();
}



QDifuPerfuSegmentationExtension::~QDifuPerfuSegmentationExtension()
{
    writeSettings();


    delete m_diffusionInputVolume;  // potser no s'ha d'esborrar
    delete m_perfusionInputVolume;  // potser no s'ha d'esborrar
    delete m_diffusionMainVolume;
    delete m_perfusionMainVolume;
    delete m_diffusionRescaledVolume;
    delete m_perfusionRescaledVolume;

    delete m_strokeMaskVolume;
    delete m_ventriclesMaskVolume;

    delete m_blackpointEstimatedVolume;
    delete m_penombraMaskVolume;

    if ( m_perfusionOverlay ) m_perfusionOverlay->Delete();

    delete m_strokeSegmentationMethod;

    m_squareActor->Delete();

    delete m_actionFactory;
}



void QDifuPerfuSegmentationExtension::createActions()
{
    m_rotateClockWiseAction = new QAction( this );
    m_rotateClockWiseAction->setText( tr("Rotate Clockwise") );
    m_rotateClockWiseAction->setShortcut( Qt::CTRL + Qt::Key_Plus );
    m_rotateClockWiseAction->setStatusTip( tr("Rotate the image in clockwise direction") );
    m_rotateClockWiseAction->setIcon( QIcon( ":/images/rotateClockWise.png" ) );
    m_rotateClockWiseToolButton->setDefaultAction( m_rotateClockWiseAction );

    connect( m_rotateClockWiseAction, SIGNAL( triggered() ),
             m_diffusion2DView, SLOT( rotateClockWise() ) );
    connect( m_rotateClockWiseAction, SIGNAL( triggered() ),
             m_perfusion2DView, SLOT( rotateClockWise() ) );


    m_editorAction = new QAction( this );
    m_editorAction->setText( tr("EditorTool") );
    m_editorAction->setStatusTip( tr("Enable/disable editor tool") );
    m_editorAction->setIcon( QIcon( ":/images/pencil.png" ) );
    m_editorAction->setCheckable( true );
    m_editorAction->setEnabled( false );
    m_editorToolButton->setDefaultAction( m_editorAction );


    // Pseudo-tool
    // TODO ara mateix no ho integrem dins del framework de tools, però potser que més endavant sí
    m_voxelInformationAction = new QAction( this );
    m_voxelInformationAction->setText( tr("Voxel Information") );
    m_voxelInformationAction->setShortcut( tr("Ctrl+I") );
    m_voxelInformationAction->setStatusTip( tr("Enable voxel information over cursor") );
    m_voxelInformationAction->setIcon( QIcon( ":/images/voxelInformation.png" ) );
    m_voxelInformationAction->setCheckable( true );
    m_voxelInformationToolButton->setDefaultAction( m_voxelInformationAction );

    connect( m_voxelInformationAction, SIGNAL( triggered(bool) ),
             m_diffusion2DView, SLOT( setVoxelInformationCaptionEnabled(bool) ) );
    connect( m_voxelInformationAction, SIGNAL( triggered(bool) ),
             m_perfusion2DView, SLOT( setVoxelInformationCaptionEnabled(bool) ) );


    // Tools

    m_actionFactory = new ToolsActionFactory();

    m_slicingAction = m_actionFactory->getActionFrom( "SlicingTool" );
    m_slicingToolButton->setDefaultAction( m_slicingAction );

    m_windowLevelAction = m_actionFactory->getActionFrom( "WindowLevelTool" );
    m_windowLevelToolButton->setDefaultAction( m_windowLevelAction );

    m_zoomAction = m_actionFactory->getActionFrom( "ZoomTool" );
    m_zoomToolButton->setDefaultAction( m_zoomAction );

    m_moveAction = m_actionFactory->getActionFrom( "TranslateTool" );
    m_moveToolButton->setDefaultAction( m_moveAction );

    m_seedAction = m_actionFactory->getActionFrom( "SeedTool" );
    m_seedAction->setIcon( QIcon( ":/images/seed.png" ) );
    m_seedToolButton->setDefaultAction( m_seedAction );

    connect( m_actionFactory, SIGNAL( triggeredTool(QString) ),
             m_diffusion2DView, SLOT( setTool(QString) ) );
    connect( m_actionFactory, SIGNAL( triggeredTool(QString) ),
             m_perfusion2DView, SLOT( setTool(QString) ) );


    m_toolsActionGroup = new QActionGroup( this );
    m_toolsActionGroup->setExclusive( true );
    m_toolsActionGroup->addAction( m_slicingAction );
    m_toolsActionGroup->addAction( m_windowLevelAction );
    m_toolsActionGroup->addAction( m_zoomAction );
    m_toolsActionGroup->addAction( m_moveAction );
    m_toolsActionGroup->addAction( m_editorAction );
    m_toolsActionGroup->addAction( m_seedAction );

    // activem per defecte una tool
    // TODO podríem posar algun mecanisme especial per escollir la tool per defecte?
    m_seedAction->trigger();


    m_lesionViewAction = new QAction( this );
    m_lesionViewAction->setText( tr("Lesion Overlay") );
    m_lesionViewAction->setStatusTip( tr("Enable/disable lesion view overlay") );
    m_lesionViewAction->setCheckable( true );
    m_lesionViewAction->setEnabled( false );
    m_lesionViewToolButton->setDefaultAction( m_lesionViewAction );

    m_ventriclesViewAction = new QAction( this );
    m_ventriclesViewAction->setText( tr("Ventricles Overlay") );
    m_ventriclesViewAction->setStatusTip( tr("Enable/disable ventricles view overlay") );
    m_ventriclesViewAction->setCheckable( true );
    m_ventriclesViewAction->setEnabled( false );
    m_ventriclesViewToolButton->setDefaultAction( m_ventriclesViewAction );

    m_viewOverlayActionGroup = new QActionGroup( this );
    m_viewOverlayActionGroup->setExclusive( true );
    m_viewOverlayActionGroup->addAction( m_ventriclesViewAction );
    m_viewOverlayActionGroup->addAction( m_lesionViewAction );


    m_paintEditorAction = new QAction( 0 );
    m_paintEditorAction->setText( tr("Paint Editor Tool") );
    m_paintEditorAction->setStatusTip( tr("Enable/Disable Paint Editor") );
    m_paintEditorAction->setIcon( QIcon(":/images/airbrush.png") );
    m_paintEditorAction->setCheckable( true );
    m_paintEditorAction->setEnabled( false );
    m_paintButton->setDefaultAction( m_paintEditorAction );

    m_eraseEditorAction = new QAction( 0 );
    m_eraseEditorAction->setText( tr("Erase Editor Tool") );
    m_eraseEditorAction->setStatusTip( tr("Enable/Disable Erase Editor") );
    m_eraseEditorAction->setIcon( QIcon(":/images/eraser.png") );
    m_eraseEditorAction->setCheckable( true );
    m_eraseEditorAction->setEnabled( false );
    m_eraseButton->setDefaultAction( m_eraseEditorAction );

    m_eraseSliceEditorAction = new QAction( 0 );
    m_eraseSliceEditorAction->setText( tr("Erase Slice Editor Tool") );
    m_eraseSliceEditorAction->setStatusTip( tr("Enable/Disable Erase Slice Editor") );
    m_eraseSliceEditorAction->setIcon( QIcon(":/images/axial.png") );
    m_eraseSliceEditorAction->setCheckable( true );
    m_eraseSliceEditorAction->setEnabled( false );
    m_eraseSliceButton->setDefaultAction( m_eraseSliceEditorAction );

    m_eraseRegionEditorAction = new QAction( 0 );
    m_eraseRegionEditorAction->setText( tr("Erase Region Editor Tool") );
    m_eraseRegionEditorAction->setStatusTip( tr("Enable/Disable Erase Region Editor") );
    m_eraseRegionEditorAction->setIcon( QIcon(":/images/move.png") );//A falta d'una millor!!!
    m_eraseRegionEditorAction->setCheckable( true );
    m_eraseRegionEditorAction->setEnabled( false );
    m_eraseRegionButton->setDefaultAction( m_eraseRegionEditorAction );

    m_editorToolActionGroup = new QActionGroup( 0 );
    m_editorToolActionGroup->setExclusive( true );
    m_editorToolActionGroup->addAction( m_paintEditorAction );
    m_editorToolActionGroup->addAction( m_eraseEditorAction );
    m_editorToolActionGroup->addAction( m_eraseSliceEditorAction );
    m_editorToolActionGroup->addAction( m_eraseRegionEditorAction );


}



void QDifuPerfuSegmentationExtension::createToolBars()
{
}



void QDifuPerfuSegmentationExtension::createConnections()
{
    connect( m_totalDiffusionImagesSpinBox, SIGNAL( valueChanged(int) ),
             this, SLOT( setMaxDiffusionImage(int) ) );
    connect( m_selectedDiffusionImageSpinBox, SIGNAL( valueChanged(int) ),
             this, SLOT( setDiffusionImage(int) ) );

    connect( m_openPerfusionImagePushButton, SIGNAL( clicked() ),
             this, SIGNAL( openPerfusionImage() ) );
    connect( m_totalPerfusionImagesSpinBox, SIGNAL( valueChanged(int) ),
             this, SLOT( setMaxPerfusionImage(int) ) );
    connect( m_selectedPerfusionImageSpinBox, SIGNAL( valueChanged(int) ),
             this, SLOT( setPerfusionImage(int) ) );

    connect( m_strokeLowerValueSlider, SIGNAL( valueChanged(int) ),
             this, SLOT( setStrokeLowerValue(int) ) );
    connect( m_strokeUpperValueSlider, SIGNAL( valueChanged(int) ),
             this, SLOT( setStrokeUpperValue(int) ) );
    connect( m_strokeViewThresholdsPushButton, SIGNAL( clicked() ),
             this, SLOT( viewThresholds() ) );
    connect( m_diffusion2DView, SIGNAL( seedChanged() ), this, SLOT( setSeedPosition() ) );
    connect( m_strokeApplyPushButton, SIGNAL( clicked() ),
             this, SLOT( applyStrokeSegmentation() ) );
    connect( m_strokeVolumeUpdatePushButton, SIGNAL( clicked() ),
             this, SLOT( updateStrokeVolume() ) );

    connect( m_ventriclesApplyPushButton, SIGNAL( clicked() ),
             this, SLOT( applyVentriclesMethod() ) );

    connect( m_applyRegistrationPushButton, SIGNAL( clicked() ),
             this, SLOT( applyRegistration() ) );

    connect( m_computeBlackpointEstimationPushButton, SIGNAL( clicked() ),
             this, SLOT( computeBlackpointEstimation() ) );

    connect( m_penombraApplyPushButton, SIGNAL( clicked() ),
             this, SLOT( applyPenombraSegmentation() ) );

    connect( m_filterDiffusionPushButton, SIGNAL( clicked() ),
             this, SLOT( applyFilterDiffusionImage() ) );



    connect( m_diffusion2DView , SIGNAL( eventReceived( unsigned long ) ) , this , SLOT( strokeEventHandler(unsigned long) ) );
  // caldria pel perfusion?????


    connect( m_synchroCheckBox, SIGNAL( toggled(bool) ), this, SLOT( synchronizeSlices(bool) ) );


    // potser és millor fer-ho amb l'acció ( signal triggered() )
  connect( m_lesionViewToolButton , SIGNAL( clicked() ) , this , SLOT( viewLesionOverlay() ) );


  // potser és millor fer-ho amb l'acció ( signal triggered() )
  connect( m_ventriclesViewToolButton , SIGNAL( clicked() ) , this , SLOT( viewVentriclesOverlay() ) );

  connect( m_eraseButton , SIGNAL( clicked() ) , this , SLOT( setErase() ) );

  connect( m_eraseSliceButton , SIGNAL( clicked() ) , this , SLOT( setEraseSlice() ) );

  connect( m_paintButton , SIGNAL( clicked() ) , this , SLOT( setPaint() ) );

  connect( m_eraseRegionButton , SIGNAL( clicked() ) , this , SLOT( setEraseRegion() ) );

  connect( m_splitterLeftButton, SIGNAL( clicked() ), this, SLOT( moveViewerSplitterToLeft() ) );
  connect( m_splitterCenterButton, SIGNAL( clicked() ), this, SLOT( moveViewerSplitterToCenter() ) );
  connect( m_splitterRightButton, SIGNAL( clicked() ), this, SLOT( moveViewerSplitterToRight() ) );



  connect( m_diffusionSliceSlider, SIGNAL( valueChanged(int) ) , m_diffusion2DView , SLOT( setSlice(int) ) );
  connect( m_perfusionSliceSlider, SIGNAL( valueChanged(int) ) , m_perfusion2DView , SLOT( setSlice(int) ) );

  connect( m_diffusion2DView, SIGNAL( sliceChanged(int) ), m_diffusionSliceSlider, SLOT( setValue(int) ) );
  connect( m_perfusion2DView, SIGNAL( sliceChanged(int) ), m_perfusionSliceSlider, SLOT( setValue(int) ) );

  connect( m_perfusion2DView, SIGNAL( sliceChanged(int) ), this, SLOT( setPerfusionSlice(int) ) );



  connect( m_diffusionOpacitySlider, SIGNAL( valueChanged(int) ), this, SLOT( setDiffusionOpacity(int) ) );
  connect( m_perfusionOpacitySlider, SIGNAL( valueChanged(int) ), this, SLOT( setPerfusionOpacity(int) ) );


}



void QDifuPerfuSegmentationExtension::readSettings()
{
    QSettings settings( "GGG", "StarViewer-App-DiffusionPerfusionSegmentation" );
    settings.beginGroup("StarViewer-App-DiffusionPerfusionSegmentation");

    m_horizontalSplitter->restoreState( settings.value( "horizontalSplitter" ).toByteArray() );
    m_viewerSplitter->restoreState( settings.value( "viewerSplitter" ).toByteArray() );

    settings.endGroup();
}



void QDifuPerfuSegmentationExtension::writeSettings()
{
    QSettings settings( "GGG", "StarViewer-App-DiffusionPerfusionSegmentation" );
    settings.beginGroup( "StarViewer-App-DiffusionPerfusionSegmentation" );

    settings.setValue( "horizontalSplitter", m_horizontalSplitter->saveState() );
    ///Movem l'splitter a la dreta pq quan es torni obrir només es vegi la difu
    this->moveViewerSplitterToRight();
    settings.setValue( "viewerSplitter", m_viewerSplitter->saveState() );

    settings.endGroup();
}



void QDifuPerfuSegmentationExtension::setDiffusionInput( Volume * input )
{
    if ( !input )
    {
        qCritical() << "setDiffusionInput: null input";
        return;
    }

    if ( m_diffusionInputVolume )
    {
        qWarning() << "setDiffusionInput: second input has no effect";
        return;
    }
    m_diffusionInputVolume = input;

    ItkImageType::Pointer inputImage = input->getItkData();
    ItkImageType::RegionType region = inputImage->GetLargestPossibleRegion();
    ItkImageType::RegionType::SizeType size = region.GetSize();
    m_totalDiffusionImagesSpinBox->setMaximum( size[2] );

    setDiffusionImage( m_selectedDiffusionImageSpinBox->value() );

}

void QDifuPerfuSegmentationExtension::moveViewerSplitterToLeft(  )
{
    std::cout<<"Move L"<<std::endl;
    QList<int> splitterSize = m_viewerSplitter->sizes();
    int suma = splitterSize[0]+splitterSize[1];
    splitterSize[0]=0;
    splitterSize[1]=suma;
    m_viewerSplitter->setSizes(splitterSize);
}

void QDifuPerfuSegmentationExtension::moveViewerSplitterToRight(  )
{
    std::cout<<"Move R"<<std::endl;
    QList<int> splitterSize = m_viewerSplitter->sizes();
    int suma = splitterSize[0]+splitterSize[1];
    splitterSize[0]=suma;
    splitterSize[1]=0;
    m_viewerSplitter->setSizes(splitterSize);
}

void QDifuPerfuSegmentationExtension::moveViewerSplitterToCenter(  )
{
    std::cout<<"Move C"<<std::endl;
    QList<int> splitterSize = m_viewerSplitter->sizes();
    int suma = splitterSize[0]+splitterSize[1];
    splitterSize[0]=suma/2;
    splitterSize[1]=suma - suma/2;
    m_viewerSplitter->setSizes(splitterSize);
}


void QDifuPerfuSegmentationExtension::setMaxDiffusionImage( int max )
{
    m_selectedDiffusionImageSpinBox->setMaximum( max - 1 );
}



void QDifuPerfuSegmentationExtension::setDiffusionImage( int index )
{
    int total = m_totalDiffusionImagesSpinBox->value();


    ItkImageType::Pointer inputImage = m_diffusionInputVolume->getItkData();

    ItkImageType::RegionType region = inputImage->GetLargestPossibleRegion();
    ItkImageType::RegionType::SizeType size = region.GetSize();
    size[2] /= total;
//     std::cout << "size: " << size << std::endl;
//     std::cout << "region: " << region << std::endl;
    region.SetSize( size );

    /// \warning Posem l'espaiat en Z a 7!!
    ItkImageType::SpacingType spacing = inputImage->GetSpacing();
//     std::cout << "spacing before: " << spacing << std::endl;
//     spacing[2] *= total;
    spacing[2] = 7.0;
//     std::cout << "spacing after: " << spacing << std::endl;


    ItkImageType::Pointer diffusionImage = ItkImageType::New();
    diffusionImage->SetRegions( region );
//     diffusionImage->SetSpacing( inputImage->GetSpacing() );
    diffusionImage->SetSpacing( spacing );
    diffusionImage->SetOrigin( inputImage->GetOrigin() );
    diffusionImage->Allocate();


    itk::ImageRegionConstIteratorWithIndex< ItkImageType > itInput(
            inputImage, inputImage->GetBufferedRegion() );
    itk::ImageRegionIterator< ItkImageType > itDiffusion(
            diffusionImage, diffusionImage->GetBufferedRegion() );

    itInput.GoToBegin();
    itDiffusion.GoToBegin();

    unsigned int sliceSize = size[0] * size[1];
    unsigned int slice = index;

    for ( unsigned int k = 0; k < size[2]; k++ )
    {
        ItkImageType::IndexType imageIndex = { { 0, 0, slice } };
        itInput.SetIndex( imageIndex );

        for ( unsigned int i = 0; i < sliceSize; i++ )
        {
            itDiffusion.Set( itInput.Get() );
            ++itInput;
            ++itDiffusion;
        }

        slice += total;
    }


    delete m_diffusionMainVolume;
    m_diffusionMainVolume = new Volume();
    m_diffusionMainVolume->setData( diffusionImage );

    //Posem la informació de la imatge original, ja que tot el tema de DICOM (orientació, w/l,...) serà el mateix
    m_diffusionMainVolume->setVolumeSourceInformation( m_diffusionInputVolume->getVolumeSourceInformation() );


    // TODO ara ho fem "a saco" però s'hauria de millorar
    m_diffusion2DView->setInput( m_diffusionMainVolume );
    m_diffusion2DView->setView( Q2DViewer::Axial );
    m_diffusion2DView->removeAnnotation( Q2DViewer::NoAnnotation );
    m_diffusion2DView->resetWindowLevelToDefault();


    int * dim = m_diffusionMainVolume->getDimensions();
    m_diffusionSliceSlider->setMinimum( 0 );
    m_diffusionSliceSlider->setMaximum( dim[2] - 1 );
    m_diffusionSliceSpinBox->setMinimum( 0 );
    m_diffusionSliceSpinBox->setMaximum( dim[2] - 1 );

    m_diffusionSliceSlider->setValue( m_diffusion2DView->getSlice() );


    // Trobem els valors de propietat mínim i màxim

    itk::ImageRegionConstIterator< ItkImageType > it(
            diffusionImage, diffusionImage->GetBufferedRegion() );

    it.GoToBegin();
    m_diffusionMinValue = m_diffusionMaxValue = it.Get();

    ItkImageType::PixelType value;

    while ( !it.IsAtEnd() )
    {
        value = it.Get();

        if ( value < m_diffusionMinValue ) m_diffusionMinValue = value;
        if ( value > m_diffusionMaxValue ) m_diffusionMaxValue = value;

        ++it;
    }


    m_strokeLowerValueSpinBox->setMinimum( m_diffusionMinValue );
    m_strokeLowerValueSpinBox->setMaximum( m_diffusionMaxValue );
    m_strokeUpperValueSpinBox->setMinimum( m_diffusionMinValue );
    m_strokeUpperValueSpinBox->setMaximum( m_diffusionMaxValue );
    m_strokeLowerValueSlider->setMinimum( m_diffusionMinValue );
    m_strokeLowerValueSlider->setMaximum( m_diffusionMaxValue );
    m_strokeUpperValueSlider->setMinimum( m_diffusionMinValue );
    m_strokeUpperValueSlider->setMaximum( m_diffusionMaxValue );

    m_strokeLowerValueSlider->setValue( 150 );
    m_strokeUpperValueSlider->setValue( m_diffusionMaxValue );


    m_ventriclesLowerValueSpinBox->setMinimum( m_diffusionMinValue );
    m_ventriclesLowerValueSpinBox->setMaximum( m_diffusionMaxValue );
    m_ventriclesLowerValueSlider->setMinimum( m_diffusionMinValue );
    m_ventriclesLowerValueSlider->setMaximum( m_diffusionMaxValue );

    m_ventriclesLowerValueSlider->setValue( 50 );


    m_filterDiffusionPushButton->setEnabled( true );


    m_diffusion2DView->render();
}



void QDifuPerfuSegmentationExtension::setPerfusionInput( Volume * input )
{
    if ( !input )
    {
        qCritical() << "setPerfusionInput: null input";
        return;
    }

    if ( m_perfusionInputVolume )
    {
        qWarning() << "setPerfusionInput: second input has no effect";
        return;
    }

    m_perfusionImageStackedWidget->setCurrentWidget( m_selectPerfusionImagePage );

    m_perfusionInputVolume = input;

    ItkImageType::Pointer inputImage = input->getItkData();
    ItkImageType::RegionType region = inputImage->GetLargestPossibleRegion();
    ItkImageType::RegionType::SizeType size = region.GetSize();
    m_totalPerfusionImagesSpinBox->setMaximum( size[2] );

    setPerfusionImage( m_selectedPerfusionImageSpinBox->value() );

    m_applyRegistrationPushButton->setEnabled( true );

    this->moveViewerSplitterToCenter();

}



void QDifuPerfuSegmentationExtension::setMaxPerfusionImage( int max )
{
    m_selectedPerfusionImageSpinBox->setMaximum( max - 1 );
}



void QDifuPerfuSegmentationExtension::setPerfusionImage( int index )
{
    int total = m_totalPerfusionImagesSpinBox->value();


    ItkImageType::Pointer inputImage = m_perfusionInputVolume->getItkData();

    ItkImageType::RegionType region = inputImage->GetLargestPossibleRegion();
    ItkImageType::RegionType::SizeType size = region.GetSize();
    size[2] /= total;
//     std::cout << "size: " << size << std::endl;
//     std::cout << "region: " << region << std::endl;
    region.SetSize( size );


    ItkImageType::Pointer perfusionImage = ItkImageType::New();
    perfusionImage->SetRegions( region );
    perfusionImage->SetSpacing( inputImage->GetSpacing() );
    perfusionImage->SetOrigin( inputImage->GetOrigin() );
    perfusionImage->Allocate();


    ItkImageType::IndexType imageIndex = { { 0, 0, index * size[2] } };
    itk::ImageRegionConstIterator< ItkImageType > itInput(
            inputImage, ItkImageType::RegionType( imageIndex, size ) );
    itk::ImageRegionIterator< ItkImageType > itPerfusion(
            perfusionImage, perfusionImage->GetBufferedRegion() );

    itInput.GoToBegin();
    itPerfusion.GoToBegin();

    unsigned int regionSize = size[0] * size[1] * size[2];

    for ( unsigned int i = 0; i < regionSize; i++ )
    {
        itPerfusion.Set( itInput.Get() );
        ++itInput;
        ++itPerfusion;
    }


    delete m_perfusionMainVolume;
    m_perfusionMainVolume = new Volume();
    m_perfusionMainVolume->setData( perfusionImage );

    //Posem la informació de la imatge original, ja que tot el tema de DICOM (orientació, w/l,...) serà el mateix
    m_perfusionMainVolume->setVolumeSourceInformation( m_perfusionInputVolume->getVolumeSourceInformation() );


    // TODO ara ho fem "a saco" però s'hauria de millorar
    m_perfusion2DView->setInput( m_perfusionMainVolume );
    m_perfusion2DView->setView( Q2DViewer::Axial );
    m_perfusion2DView->removeAnnotation( Q2DViewer::NoAnnotation );
    m_perfusion2DView->resetWindowLevelToDefault();


    int * dim = m_perfusionMainVolume->getDimensions();
    m_perfusionSliceSlider->setMinimum( 0 );
    m_perfusionSliceSlider->setMaximum( dim[2] - 1 );
    m_perfusionSliceSpinBox->setMinimum( 0 );
    m_perfusionSliceSpinBox->setMaximum( dim[2] - 1 );

    m_perfusionSliceSlider->setValue( m_perfusion2DView->getSlice() );


    // Trobem els valors de propietat mínim i màxim

    itk::ImageRegionConstIterator< ItkImageType > it(
            perfusionImage, perfusionImage->GetBufferedRegion() );

    it.GoToBegin();
    m_perfusionMinValue = m_perfusionMaxValue = it.Get();

    ItkImageType::PixelType value;

    while ( !it.IsAtEnd() )
    {
        value = it.Get();

        if ( value < m_perfusionMinValue ) m_perfusionMinValue = value;
        if ( value > m_perfusionMaxValue ) m_perfusionMaxValue = value;

        ++it;
    }


    vtkLookupTable * hueLut = vtkLookupTable::New();
    hueLut->SetTableRange( m_perfusionMinValue, m_perfusionMaxValue );
    hueLut->SetHueRange( 1.0, 0.0 );
    hueLut->SetSaturationRange( 1.0, 1.0 );
    hueLut->SetValueRange( 1.0, 1.0 );
    hueLut->SetAlphaRange( 1.0, 1.0 );
    hueLut->SetRampToLinear();
    hueLut->Build();    //effective built
    int nvalues=hueLut->GetNumberOfTableValues();
    double* tvalue= new double[4];
    for(int i=0;i<(nvalues/3);i++)
    {
        tvalue=hueLut->GetTableValue(i);
        tvalue[0]=0.0;  //Posem els valors transparents
        tvalue[1]=0.0;  //Posem els valors transparents
        tvalue[2]=0.0;  //Posem els valors transparents
        tvalue[3]=0.0;  //Posem els valors transparents
        hueLut->SetTableValue(i, tvalue);
    }
    hueLut->Build();    //effective built

    vtkUnsignedCharArray * table = hueLut->GetTable();
    unsigned char tuple[4] = { 0, 0, 0, 0 };
    table->SetTupleValue( 0, tuple );
    table->SetTupleValue( table->GetNumberOfTuples() - 1, tuple );

    m_perfusion2DView->getWindowLevelMapper()->SetLookupTable( hueLut );
    m_perfusion2DView->updateWindowLevelAnnotation();
    m_perfusion2DView->setWindowLevel( m_perfusionMaxValue - m_perfusionMinValue, 0.0 );


    m_perfusion2DView->render();
}



void QDifuPerfuSegmentationExtension::setStrokeLowerValue( int x )
{
    if ( x > m_strokeUpperValueSlider->value() ) m_strokeUpperValueSlider->setValue( x );
}



void QDifuPerfuSegmentationExtension::setStrokeUpperValue( int x )
{
    if ( x < m_strokeLowerValueSlider->value() ) m_strokeLowerValueSlider->setValue( x );
}



void QDifuPerfuSegmentationExtension::viewThresholds()
{
    if ( !m_strokeMaskVolume ) m_strokeMaskVolume = new Volume();

    m_activedMaskVolume = m_strokeMaskVolume;

    vtkImageThreshold * imageThreshold = vtkImageThreshold::New();
    imageThreshold->SetInput( m_diffusionMainVolume->getVtkData() );
    imageThreshold->ThresholdBetween( m_strokeLowerValueSlider->value(),
                                      m_strokeUpperValueSlider->value() );
    imageThreshold->SetInValue( m_diffusionMaxValue );
    imageThreshold->SetOutValue( m_diffusionMinValue );
    imageThreshold->Update();

    m_strokeMaskVolume->setData( imageThreshold->GetOutput() );

    m_diffusion2DView->setOverlayToBlend();
    m_diffusion2DView->setOpacityOverlay( m_diffusionOpacitySlider->value() / 100.0 );
    m_diffusion2DView->setOverlayInput( m_strokeMaskVolume );

    m_diffusionOpacityLabel->setEnabled( true );
    m_diffusionOpacitySlider->setEnabled( true );

    m_diffusion2DView->getInteractor()->Render();
}



void QDifuPerfuSegmentationExtension::setSeedPosition()
{
    m_diffusion2DView->getSeedPosition( m_seedPosition );

    m_strokeSeedXLineEdit->setText( QString::number( m_seedPosition[0], 'f', 1 ) );
    m_strokeSeedYLineEdit->setText( QString::number( m_seedPosition[1], 'f', 1 ) );
    m_strokeSeedZLineEdit->setText( QString::number( m_seedPosition[2], 'f', 1 ) );

    m_strokeApplyPushButton->setEnabled( true );
}



void QDifuPerfuSegmentationExtension::applyStrokeSegmentation()
{
    QApplication::setOverrideCursor( Qt::WaitCursor );


    if ( !m_strokeSegmentationMethod ) m_strokeSegmentationMethod = new StrokeSegmentationMethod();

    m_strokeSegmentationMethod->setVolume( m_diffusionMainVolume );

    if ( !m_strokeMaskVolume ) m_strokeMaskVolume = new Volume();

    m_strokeSegmentationMethod->setMask( m_strokeMaskVolume );
    m_strokeSegmentationMethod->setInsideMaskValue ( m_diffusionMaxValue );
    m_strokeSegmentationMethod->setOutsideMaskValue( m_diffusionMinValue );
    m_strokeSegmentationMethod->setHistogramLowerLevel( m_strokeLowerValueSlider->value() );
    m_strokeSegmentationMethod->setHistogramUpperLevel( m_strokeUpperValueSlider->value() );
    m_strokeSegmentationMethod->setSeedPosition(
            m_seedPosition[0], m_seedPosition[1], m_seedPosition[2] );

    m_strokeVolume = m_strokeSegmentationMethod->applyMethod();


    m_diffusion2DView->setOverlayToBlend();
    m_diffusion2DView->setOpacityOverlay( m_diffusionOpacitySlider->value() / 100.0 );
    m_diffusion2DView->setOverlayInput( m_strokeMaskVolume );

    m_diffusionOpacityLabel->setEnabled( true );
    m_diffusionOpacitySlider->setEnabled( true );


    m_strokeVolumeLineEdit->setText( QString::number( m_strokeVolume, 'f', 2 ) );
    m_strokeVolumeLabel->setEnabled( true );
    m_strokeVolumeLineEdit->setEnabled( true );
    m_strokeVolumeUpdatePushButton->setEnabled( true );


    m_editorAction->trigger();
    m_diffusion2DView->disableTools();
    m_editorAction->setEnabled( true );

    m_paintEditorAction->setEnabled(true);
    m_eraseEditorAction->setEnabled(true);
    m_eraseSliceEditorAction->setEnabled(true);
    m_eraseRegionEditorAction->setEnabled(true);
    m_eraseEditorAction->trigger();
    m_editorTool = QDifuPerfuSegmentationExtension::Erase;
    m_editorSize->setEnabled(true);


    m_lesionViewAction->setEnabled( true );
    m_lesionViewAction->trigger();
    this->viewLesionOverlay();


    QApplication::restoreOverrideCursor();
}



void QDifuPerfuSegmentationExtension::updateStrokeVolume()
{
    m_strokeVolume = this->calculateStrokeVolume();
    m_strokeVolumeLineEdit->setText( QString::number( m_strokeVolume, 'f', 2 ) );
}



double QDifuPerfuSegmentationExtension::calculateStrokeVolume()
{
    VolumeCalculator volumeCalculator;

    volumeCalculator.setInput( m_strokeMaskVolume );
    volumeCalculator.setInsideValue( m_diffusionMaxValue );

    return volumeCalculator.getVolume();
}



void QDifuPerfuSegmentationExtension::applyVentriclesMethod()
{
    if ( !m_ventriclesMaskVolume ) m_ventriclesMaskVolume = new Volume();

    vtkImageThreshold * imageThreshold = vtkImageThreshold::New();
    imageThreshold->SetInput( m_diffusionMainVolume->getVtkData() );
    imageThreshold->ThresholdBetween( m_ventriclesLowerValueSlider->value(), m_diffusionMaxValue );
    // Inverse mask --> we want < lower or > upper
    imageThreshold->SetInValue( m_diffusionMinValue );
    imageThreshold->SetOutValue( m_diffusionMaxValue );
    imageThreshold->Update();

    m_ventriclesMaskVolume->setData( imageThreshold->GetOutput() );


    m_ventriclesViewAction->setEnabled( true );
    m_ventriclesViewAction->trigger();
    this->viewVentriclesOverlay();


    m_diffusionOpacityLabel->setEnabled( true );
    m_diffusionOpacitySlider->setEnabled( true );
}



void QDifuPerfuSegmentationExtension::applyRegistration()
{
    QApplication::setOverrideCursor( Qt::WaitCursor );


    ItkImageType::Pointer fixedImage = m_diffusionMainVolume->getItkData();
    ItkImageType::Pointer movingImage = m_perfusionMainVolume->getItkData();

//     ItkImageType::SpacingType fixedSpacing = fixedImage->GetSpacing();
//     std::cout << "fixed spacing = " << fixedSpacing << std::endl;
//     ItkImageType::SpacingType movingSpacing = movingImage->GetSpacing();
//     std::cout << "moving spacing = " << movingSpacing << std::endl;

    itkRegistre3DAffine< ItkImageType, ItkImageType > registre;
    registre.SetInputImages( fixedImage, movingImage );
    registre.SetParamatersMetric( REGISTRATION_FIXED_STANDARD_DEVIATION,
                                  REGISTRATION_MOVING_STANDARD_DEVIATION,
                                  REGISTRATION_NUMBER_OF_SPACIAL_SAMPLES );
    registre.SetParamatersGaussian( REGISTRATION_FIXED_VARIANCE, REGISTRATION_MOVING_VARIANCE );
    registre.SetParamatresOptimizer( REGISTRATION_MAXIMUM_STEP, REGISTRATION_MINIMUM_STEP,
                                     REGISTRATION_NUMBER_OF_ITERATIONS );

    if ( registre.applyMethod() )
    {
        typedef TransformType::InputPointType TransformPointType;
        typedef itk::ResampleImageFilter< ItkImageType, ItkImageType > ResampleGrisFilterType;
        typedef itk::RescaleIntensityImageFilter< ItkImageType, ItkImageType > RescaleFilterType;

        itkRegistre3DAffine< ItkImageType, ItkImageType >::OptimizerParametersType finalParameters;
        finalParameters = registre.getFinalParameters();

        if ( !m_registerTransform ) m_registerTransform = TransformType::New();
        m_registerTransform->SetParameters( finalParameters );

        // Recalculem el centre -> No ho fa bé! (???????)
        ItkImageType::SpacingType fixedSpacing = fixedImage->GetSpacing();
        ItkImageType::PointType fixedOrigin = fixedImage->GetOrigin();
        ItkImageType::SizeType fixedSize = fixedImage->GetLargestPossibleRegion().GetSize();
        TransformPointType centerFixed;
        centerFixed[0] = fixedOrigin[0] + fixedSpacing[0] * fixedSize[0] / 2.0;
        centerFixed[1] = fixedOrigin[1] + fixedSpacing[1] * fixedSize[1] / 2.0;
        centerFixed[2] = fixedOrigin[2] + fixedSpacing[2] * fixedSize[2] / 2.0;

        m_registerTransform->SetCenter( centerFixed );

        ResampleGrisFilterType::Pointer resample = ResampleGrisFilterType::New();
        resample->SetTransform( m_registerTransform );
        resample->SetInput( movingImage );
        resample->SetSize( fixedImage->GetLargestPossibleRegion().GetSize() );
        resample->SetOutputOrigin( fixedImage->GetOrigin() );
        resample->SetOutputSpacing( fixedImage->GetSpacing() );
        resample->SetDefaultPixelValue( 0 );
        resample->Update();

        RescaleFilterType::Pointer rescalerPerfusion = RescaleFilterType::New();
        rescalerPerfusion->SetInput( resample->GetOutput() );
        rescalerPerfusion->SetOutputMinimum( 0 );
        rescalerPerfusion->SetOutputMaximum( 255 );
        rescalerPerfusion->Update();

        if ( !m_perfusionRescaledVolume ) m_perfusionRescaledVolume = new Volume();

        m_perfusionRescaledVolume->setData( rescalerPerfusion->GetOutput() );
        m_perfusionRescaledVolume->setVolumeSourceInformation( m_perfusionInputVolume->getVolumeSourceInformation() );


        RescaleFilterType::Pointer rescalerDiffusion = RescaleFilterType::New();
        rescalerDiffusion->SetInput( m_diffusionMainVolume->getItkData() );
        rescalerDiffusion->SetOutputMinimum( 0 );
        rescalerDiffusion->SetOutputMaximum( 255 );
        rescalerDiffusion->Update();

        if ( !m_diffusionRescaledVolume ) m_diffusionRescaledVolume = new Volume();

        m_diffusionRescaledVolume->setData( rescalerDiffusion->GetOutput() );
        m_diffusionRescaledVolume->setVolumeSourceInformation( m_diffusionInputVolume->getVolumeSourceInformation() );

        m_perfusion2DView->setInput( m_perfusionRescaledVolume );
        m_perfusion2DView->resetWindowLevelToDefault();

        vtkLookupTable * hueLut = vtkLookupTable::New();
        hueLut->SetTableRange( 0.0, 255.0 );
        hueLut->SetHueRange( 1.0, 0.0 );
        hueLut->SetSaturationRange( 1.0, 1.0 );
        hueLut->SetValueRange( 1.0, 1.0 );
        hueLut->SetAlphaRange( 1.0, 1.0 );
        hueLut->SetRampToLinear();
        hueLut->Build();    //effective built
        int nvalues=hueLut->GetNumberOfTableValues();
        double* tvalue= new double[4];
        for(int i=0;i<(nvalues/3);i++)
        {
            tvalue=hueLut->GetTableValue(i);
            tvalue[0]=0.0;  //Posem els valors transparents
            tvalue[1]=0.0;  //Posem els valors transparents
            tvalue[2]=0.0;  //Posem els valors transparents
            tvalue[3]=0.0;  //Posem els valors transparents
            hueLut->SetTableValue(i, tvalue);
        }
        hueLut->Build();    //effective built

        vtkUnsignedCharArray * table = hueLut->GetTable();
        unsigned char tuple[4] = { 0, 0, 0, 0 };
        table->SetTupleValue( 0, tuple );
        table->SetTupleValue( table->GetNumberOfTuples() - 1, tuple );

        m_perfusion2DView->getWindowLevelMapper()->SetLookupTable( hueLut );

        vtkImageCast * imageCast = vtkImageCast::New();
        imageCast->SetInput( m_diffusionRescaledVolume->getVtkData() );
        imageCast->SetOutputScalarTypeToUnsignedChar();

        if ( !m_perfusionOverlay ) m_perfusionOverlay = vtkImageActor::New();
        m_perfusionOverlay->SetInput( imageCast->GetOutput() );
        m_perfusionOverlay->SetOpacity( m_perfusionOpacitySlider->value() / 100.0 );
        m_perfusionOverlay->SetZSlice( m_perfusionSliceSlider->value() );
        // el posem una mica més endavant per assegurar que es vegi
        m_perfusionOverlay->SetPosition( 0.0, 0.0, -1.0 );

        if ( !m_perfusion2DView->getRenderer()->HasViewProp( m_perfusionOverlay ) )
            m_perfusion2DView->getRenderer()->AddViewProp( m_perfusionOverlay );

        m_perfusion2DView->updateWindowLevelAnnotation();
        //m_perfusion2DView->setWindowLevel( m_diffusion2DView->getCurrentColorWindow(), m_diffusion2DView->getCurrentColorLevel() );
        m_perfusion2DView->setWindowLevel( 255.0, 0.0);

        m_perfusion2DView->render();

        m_computeBlackpointEstimationPushButton->setEnabled( true );

        m_perfusionOpacityLabel->setEnabled( true );
        m_perfusionOpacitySlider->setEnabled( true );

        imageCast->Delete();

    }
    else
    {
        QMessageBox::warning( this, tr("Registration failed!"), tr("Registration failed!") );
    }


    QApplication::restoreOverrideCursor();
}



void QDifuPerfuSegmentationExtension::computeBlackpointEstimation()
{
    QApplication::setOverrideCursor(Qt::WaitCursor);

    typedef itk::RescaleIntensityImageFilter< Volume::ItkImageType, Volume::ItkImageType > RescaleFilterType;

    RescaleFilterType::Pointer rescaler2 = RescaleFilterType::New();
    RescaleFilterType::Pointer rescaler3 = RescaleFilterType::New();

    rescaler2->SetInput( m_ventriclesMaskVolume->getItkData() );
    rescaler2->SetOutputMinimum( 0 );
    rescaler2->SetOutputMaximum( 255 );
    rescaler2->Update();
//     Volume * r2 = new Volume();
//     r2->setData( rescaler2->GetOutput() );
//     Q2DViewerExtension * win2 = new Q2DViewerExtension();
//     win2->setInput( r2 );
//     win2->show();

    rescaler3->SetInput( m_strokeMaskVolume->getItkData() );
    rescaler3->SetOutputMinimum( 0 );
    rescaler3->SetOutputMaximum( 255 );
    rescaler3->Update();
//     Volume * r3 = new Volume();
//     r3->setData( rescaler3->GetOutput() );
//     Q2DViewerExtension * win3 = new Q2DViewerExtension();
//     win3->setInput( r3 );
//     win3->show();

    udgPerfusionEstimator< Volume::ItkImageType, Volume::ItkImageType, TransformType> blackpointEstimator;

    blackpointEstimator.SetPerfuImage( m_perfusionRescaledVolume->getItkData() );
    blackpointEstimator.SetVentricleMask( rescaler2->GetOutput() );
    blackpointEstimator.SetStrokeMask( rescaler3->GetOutput() );
    std::cout << "pas1" << std::endl;

    // Aquí apliquem la transformació que ens ha donat el registre
    blackpointEstimator.SetTransform( m_registerTransform );
    std::cout << "pas2" << std::endl;
    blackpointEstimator.ComputeEstimation();
    std::cout << "pas3" << std::endl;
    Volume::ItkImageTypePointer perfuEstimatorImageResult = blackpointEstimator.GetEstimatedImage();
    std::cout << "pas4" << std::endl;


    m_blackpointEstimatedVolume = new Volume();
    m_blackpointEstimatedVolume->setData( perfuEstimatorImageResult );



//     vtkImageCast * imageCast = vtkImageCast::New();
//     imageCast->SetInput( m_blackpointEstimatedVolume->getVtkData() );
//     imageCast->SetOutputScalarTypeToUnsignedChar();
//     m_perfusionOverlay->SetInput( imageCast->GetOutput() );
//     m_perfusion2DView->setWindowLevel( 255.0, 0.0 );
//     m_perfusion2DView->render();


//     Volume * resultat = new Volume();
//     std::cout << "volum creat" << std::endl;
//     resultat->setData( perfuEstimatorImageResult );
//     std::cout << "data set" << std::endl;
//     Q2DViewerExtension * win = new Q2DViewerExtension();
//     std::cout << "extensió creada" << std::endl;
//     win->setInput( resultat );
//     std::cout << "input set" << std::endl;
//     win->show();
//     std::cout << "showed" << std::endl;

    QApplication::restoreOverrideCursor();

    m_penombraApplyPushButton->setEnabled( true );
}



void QDifuPerfuSegmentationExtension::applyPenombraSegmentation()
{
    QApplication::setOverrideCursor( Qt::WaitCursor );


    ItkImageType::PointType seedPoint( m_seedPosition );
    ItkImageType::IndexType seedIndex;
    m_blackpointEstimatedVolume->getItkData()->TransformPhysicalPointToIndex( seedPoint, seedIndex );
    std::cout << "seedPosition: [" << m_seedPosition[0] << ", " << m_seedPosition[1] << ", "
              << m_seedPosition[2] << "]" << std::endl;
    std::cout << "seedPoint: " << seedPoint << std::endl;
    std::cout << "seedIndex: " << seedIndex << std::endl;

    udgBinaryMaker< ItkImageType, ItkImageType > binaritzador;
    ItkImageType::Pointer penombraMask = ItkImageType::New();
    binaritzador.SetInsideValue( 254 );
    // el tercer paràmetre (strokelevel) ha de ser el valor mínim de la zona a segmentar
    // el quart paràmetre (max) ha de ser el valor màxim de la zona a segmentar (255)
    binaritzador.PenombraSegmentation( m_blackpointEstimatedVolume->getItkData(), penombraMask,
                                       m_penombraLowerValueSlider->value(), 255, seedIndex );

    delete m_penombraMaskVolume;
    m_penombraMaskVolume = new Volume();
    m_penombraMaskVolume->setData( penombraMask );


    vtkImageCast * imageCast = vtkImageCast::New();
    imageCast->SetInput( m_penombraMaskVolume->getVtkData() );
    imageCast->SetOutputScalarTypeToUnsignedChar();
    m_perfusionOverlay->SetInput( imageCast->GetOutput() );
    imageCast->Delete();


    m_perfusion2DView->render();


    VolumeCalculator volumeCalculator;
    volumeCalculator.setInput( m_penombraMaskVolume );
    volumeCalculator.setInsideValue( 254 );
    m_penombraVolumeLabel->setEnabled( true );
    m_penombraVolumeLineEdit->setEnabled( true );
    m_penombraVolumeLineEdit->setText( QString::number( volumeCalculator.getVolume(), 'f', 2 ) );


    QApplication::restoreOverrideCursor();
}



void QDifuPerfuSegmentationExtension::applyFilterDiffusionImage()
{
    QApplication::setOverrideCursor( Qt::WaitCursor );


    if ( !m_strokeSegmentationMethod ) m_strokeSegmentationMethod = new StrokeSegmentationMethod();

    m_strokeSegmentationMethod->setVolume( m_diffusionMainVolume );
    Volume * filteredVolume = new Volume();
    m_strokeSegmentationMethod->applyFilter( filteredVolume );

    delete m_diffusionMainVolume;
    m_diffusionMainVolume = filteredVolume;

    m_diffusion2DView->setInput( m_diffusionMainVolume );
    m_diffusion2DView->getInteractor()->Render();

    m_filterDiffusionPushButton->setEnabled( false );


    QApplication::restoreOverrideCursor();
}





void QDifuPerfuSegmentationExtension::strokeEventHandler( unsigned long id )
{
    switch( id )
    {
    case vtkCommand::MouseMoveEvent:
        setPaintCursor();
    break;

    case vtkCommand::LeftButtonPressEvent:
        leftButtonEventHandler();
    break;

    case vtkCommand::LeftButtonReleaseEvent:
        setLeftButtonOff();
    break;

    case vtkCommand::RightButtonPressEvent:
    break;

    default:
    break;
    }
}

void QDifuPerfuSegmentationExtension::leftButtonEventHandler( )
{
    m_isLeftButtonPressed = true;

    if(m_editorToolButton->isChecked())
    {
        //std::cout<<"Editor Tool"<<std::endl;
        m_diffusion2DView->disableTools();
        setEditorPoint(  );
    }
    else
    {
        m_diffusion2DView->enableTools();
    }
}



void QDifuPerfuSegmentationExtension::setEditorPoint(  )
{
    double pos[3];
    if(m_editorTool != QDifuPerfuSegmentationExtension::NoEditor)
    {
        m_diffusion2DView->updateVoxelInformation();
        m_diffusion2DView->getCurrentCursorPosition(pos);

        // quan dona una posici�� de (-1, -1, -1) � que estem fora de l'actor
        if(!( pos[0] == -1 && pos[1] == -1 && pos[2] == -1) )
        {
            switch( m_editorTool)
            {
                case Erase:
                {
                    this->eraseMask(m_editorSize->value());
                    break;
                }
                case Paint:
                {
                    this->paintMask(m_editorSize->value());
                    break;
                }
                case EraseSlice:
                {
                    this->eraseSliceMask();
                    break;
                }
                case EraseRegion:
                {
                    this->eraseRegionMask();
                    break;
                }
            }
            m_strokeVolumeLineEdit->clear();
            m_strokeVolumeLineEdit->insert(QString("%1").arg(m_strokeVolume, 0, 'f', 2));
            m_diffusion2DView->setOverlayInput(m_activedMaskVolume);
            m_diffusion2DView->getInteractor()->Render();
        }
    }
}

void QDifuPerfuSegmentationExtension::setLeftButtonOff( )
{
    m_isLeftButtonPressed = false;
}



void QDifuPerfuSegmentationExtension::setDiffusionOpacity( int opacity )
{
    if ( m_activedMaskVolume != 0 )
    {
        m_diffusion2DView->setOpacityOverlay(((double)opacity)/100.0);
        m_diffusion2DView->setOverlayInput(m_activedMaskVolume);
        m_diffusion2DView->getInteractor()->Render();
    }
}



void QDifuPerfuSegmentationExtension::setPerfusionOpacity( int opacity )
{
    m_perfusionOverlay->SetOpacity( opacity / 100.0 );
    m_perfusion2DView->getInteractor()->Render();
}



void QDifuPerfuSegmentationExtension::setErase()
{
    m_editorTool = QDifuPerfuSegmentationExtension::Erase;
}

void QDifuPerfuSegmentationExtension::setPaint()
{
    m_editorTool = QDifuPerfuSegmentationExtension::Paint;
}

void QDifuPerfuSegmentationExtension::setEraseSlice()
{
    m_editorTool = QDifuPerfuSegmentationExtension::EraseSlice;
}

void QDifuPerfuSegmentationExtension::setEraseRegion()
{
    m_editorTool = QDifuPerfuSegmentationExtension::EraseRegion;
}

void QDifuPerfuSegmentationExtension::setPaintCursor()
{
    if(m_editorToolButton->isChecked())    //Nom� en cas que estiguem en l'editor
    {
        if(m_isLeftButtonPressed)
        {
            setEditorPoint();
        }

        double pos[3];
        m_diffusion2DView->getCurrentCursorPosition(pos);

        if((m_editorTool == QDifuPerfuSegmentationExtension::Erase || m_editorTool == QDifuPerfuSegmentationExtension::Paint)&&(!( pos[0] == -1 && pos[1] == -1 && pos[2] == -1) ))
        {
            int size = m_editorSize->value();
            double spacing[3];
            m_strokeMaskVolume->getSpacing(spacing);

            vtkPoints *points = vtkPoints::New();
            points->SetNumberOfPoints(4);

            double sizeView[2];
            sizeView[0]=(double)size*spacing[0];
            sizeView[1]=(double)size*spacing[1];

            points->SetPoint(0, pos[0] - sizeView[0], pos[1] - sizeView[1], pos[2]-1);
            points->SetPoint(1, pos[0] + sizeView[0], pos[1] - sizeView[1], pos[2]-1);
            points->SetPoint(2, pos[0] + sizeView[0], pos[1] + sizeView[1], pos[2]-1);
            points->SetPoint(3, pos[0] - sizeView[0], pos[1] + sizeView[1], pos[2]-1);


            vtkIdType pointIds[4];

            pointIds[0] = 0;
            pointIds[1] = 1;
            pointIds[2] = 2;
            pointIds[3] = 3;


            vtkUnstructuredGrid*    grid = vtkUnstructuredGrid::New();

            grid->Allocate(1);
            grid->SetPoints(points);

            grid->InsertNextCell(VTK_QUAD,4,pointIds);

            m_squareActor -> GetProperty()->SetColor(0.15, 0.83, 0.26);
            m_squareActor -> GetProperty()->SetOpacity(0.2);

            vtkDataSetMapper *squareMapper = vtkDataSetMapper::New();
            squareMapper->SetInput( grid );

            m_squareActor->SetMapper( squareMapper );

            m_diffusion2DView->getRenderer()-> AddActor( m_squareActor );
            m_diffusion2DView->getInteractor()->Render();

            m_squareActor->VisibilityOn();

            squareMapper-> Delete();
            points      -> Delete();
            grid        -> Delete();
        }
        else
        {
            m_squareActor->VisibilityOff();
        }
    }
    else
    {
        m_squareActor->VisibilityOff();
    }
}

void QDifuPerfuSegmentationExtension::eraseMask(int size)
{
    int i,j;
    int* value;
    double pos[3];
    double origin[3];
    double spacing[3];
    int centralIndex[3];
    int index[3];
    m_diffusion2DView->getCurrentCursorPosition(pos);
    m_activedMaskVolume->getVtkData()->GetSpacing(spacing[0],spacing[1],spacing[2]);
    m_activedMaskVolume->getVtkData()->GetOrigin(origin[0],origin[1],origin[2]);
    centralIndex[0]=(int)((((double)pos[0]-origin[0])/spacing[0])+0.5);
    centralIndex[1]=(int)((((double)pos[1]-origin[1])/spacing[1])+0.5);
    //index[2]=(int)(((double)pos[2]-origin[2])/spacing[2]);
    index[2]=m_diffusion2DView->getSlice();

    for(i=-size;i<=size;i++)
    {
        for(j=-size;j<=size;j++)
        {
            index[0]=centralIndex[0]+i;
            index[1]=centralIndex[1]+j;
            value=(int*)m_activedMaskVolume->getVtkData()->GetScalarPointer(index);
            if((*value) != m_diffusionMinValue)
            {
                (*value) = m_diffusionMinValue;
            }
        }
    }
    //m_strokeMaskVolume->getVtkData()->Update();
    //m_2DView->getInteractor()->Render();
}

void QDifuPerfuSegmentationExtension::paintMask(int size)
{
    int i,j;
    int* value;
    double pos[3];
    double origin[3];
    double spacing[3];
    int centralIndex[3];
    int index[3];
    m_diffusion2DView->getCurrentCursorPosition(pos);
    m_activedMaskVolume->getVtkData()->GetSpacing(spacing[0],spacing[1],spacing[2]);
    m_activedMaskVolume->getVtkData()->GetOrigin(origin[0],origin[1],origin[2]);
    centralIndex[0]=(int)((((double)pos[0]-origin[0])/spacing[0])+0.5);
    centralIndex[1]=(int)((((double)pos[1]-origin[1])/spacing[1])+0.5);
    //index[2]=(int)(((double)pos[2]-origin[2])/spacing[2]);
    index[2]=m_diffusion2DView->getSlice();
    for(i=-size;i<=size;i++)
    {
        for(j=-size;j<=size;j++)
        {
            index[0]=centralIndex[0]+i;
            index[1]=centralIndex[1]+j;
            value=(int*)m_activedMaskVolume->getVtkData()->GetScalarPointer(index);
            if((*value) == m_diffusionMinValue)
            {
                (*value) = m_diffusionMaxValue;
            }
        }
    }
    //m_strokeMaskVolume->getVtkData()->Update();
    //m_2DView->getInteractor()->Render();
}

void QDifuPerfuSegmentationExtension::eraseSliceMask()
{
    int i,j;
    int* value;
    double pos[3];
    double origin[3];
    double spacing[3];
    int centralIndex[3];
    int index[3];
    int ext[6];
    m_strokeMaskVolume->getVtkData()->GetExtent(ext);
    m_diffusion2DView->getCurrentCursorPosition(pos);
    m_activedMaskVolume->getVtkData()->GetSpacing(spacing[0],spacing[1],spacing[2]);
    m_activedMaskVolume->getVtkData()->GetOrigin(origin[0],origin[1],origin[2]);
    centralIndex[0]=(int)(((double)pos[0]-origin[0])/spacing[0]);
    centralIndex[1]=(int)(((double)pos[1]-origin[1])/spacing[1]);
    //index[2]=(int)(((double)pos[2]-origin[2])/spacing[2]);
    index[2]=m_diffusion2DView->getSlice();
    //std::cout<<"Esborrant llesca "<<index[2]<<std::endl;
    for(i=ext[0];i<=ext[1];i++)
    {
        for(j=ext[2];j<=ext[3];j++)
        {
            index[0]=i;
            index[1]=j;
            value=(int*)m_activedMaskVolume->getVtkData()->GetScalarPointer(index);
            if((*value) != m_diffusionMinValue)
            {
                (*value) = m_diffusionMinValue;
            }
        }
    }
    //m_strokeMaskVolume->getVtkData()->Update();
    //m_2DView->getInteractor()->Render();
}




void QDifuPerfuSegmentationExtension::eraseRegionMask()
{
    double pos[3];
    double origin[3];
    double spacing[3];
    int index[3];
    m_diffusion2DView->getCurrentCursorPosition(pos);
    m_activedMaskVolume->getVtkData()->GetSpacing(spacing[0],spacing[1],spacing[2]);
    m_activedMaskVolume->getVtkData()->GetOrigin(origin[0],origin[1],origin[2]);
    index[0]=(int)((((double)pos[0]-origin[0])/spacing[0])+0.5);
    index[1]=(int)((((double)pos[1]-origin[1])/spacing[1])+0.5);
    index[2]=m_diffusion2DView->getSlice();
    eraseRegionMaskRecursive(index[0],index[1],index[2]);
}

void QDifuPerfuSegmentationExtension::eraseRegionMaskRecursive(int a, int b, int c)
{
    int ext[6];
    m_activedMaskVolume->getVtkData()->GetExtent(ext);
    if((a>=ext[0])&&(a<=ext[1])&&(b>=ext[2])&&(b<=ext[3])&&(c>=ext[4])&&(c<=ext[5]))
    {
        int index[3];
        index[0]=a;
        index[1]=b;
        index[2]=c;
        int* value=(int*)m_activedMaskVolume->getVtkData()->GetScalarPointer(index);
        if ((*value) != m_diffusionMinValue)
        {
            //std::cout<<m_outsideValue<<" "<<m_insideValue<<"->"<<(*value)<<std::endl;
            (*value)= m_diffusionMinValue;
            //(*m_activedCont)--;
            eraseRegionMaskRecursive( a+1, b, c);
            eraseRegionMaskRecursive( a-1, b, c);
            eraseRegionMaskRecursive( a, b+1, c);
            eraseRegionMaskRecursive( a, b-1, c);
        }
    }
}


void QDifuPerfuSegmentationExtension::viewLesionOverlay()
{
    if(m_strokeMaskVolume != 0)
    {
        m_activedMaskVolume = m_strokeMaskVolume;
        m_diffusion2DView->setOverlayToBlend();
        m_diffusion2DView->setOpacityOverlay(((double)m_diffusionOpacitySlider->value())/100.0);
        m_diffusion2DView->setOverlayInput(m_strokeMaskVolume);
        m_diffusion2DView->getInteractor()->Render();
    }
}

void QDifuPerfuSegmentationExtension::viewVentriclesOverlay()
{
    if(m_ventriclesMaskVolume != 0)
    {
        m_activedMaskVolume = m_ventriclesMaskVolume;
        m_diffusion2DView->setOverlayToBlend();
        m_diffusion2DView->setOpacityOverlay(((double)m_diffusionOpacitySlider->value())/100.0);
        m_diffusion2DView->setOverlayInput(m_ventriclesMaskVolume);
        m_diffusion2DView->getInteractor()->Render();
    }
}









void QDifuPerfuSegmentationExtension::synchronizeSlices( bool sync )
{
    if ( sync )
    {
        connect( m_diffusionSliceSlider, SIGNAL( valueChanged(int) ), m_perfusionSliceSlider, SLOT( setValue(int) ) );
        connect( m_perfusionSliceSlider, SIGNAL( valueChanged(int) ), m_diffusionSliceSlider, SLOT( setValue(int) ) );
    }
    else
    {
        disconnect( m_diffusionSliceSlider, SIGNAL( valueChanged(int) ), m_perfusionSliceSlider, SLOT( setValue(int) ) );
        disconnect( m_perfusionSliceSlider, SIGNAL( valueChanged(int) ), m_diffusionSliceSlider, SLOT( setValue(int) ) );
    }
}



void QDifuPerfuSegmentationExtension::setPerfusionSlice( int slice )
{
    if ( m_perfusionOverlay ) m_perfusionOverlay->SetZSlice( slice );
}



}
