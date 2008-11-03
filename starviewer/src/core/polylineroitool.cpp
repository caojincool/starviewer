/***************************************************************************
 *   Copyright (C) 2005-2007 by Grup de Gràfics de Girona                  *
 *   http://iiia.udg.es/GGG/index.html?langu=uk                            *
 *                                                                         *
 *   Universitat de Girona                                                 *
 ***************************************************************************/
#include "polylineroitool.h"
#include "q2dviewer.h"
#include "logging.h"
#include "series.h"
#include "drawer.h"
#include "drawerpolyline.h"
#include "drawertext.h"
#include "image.h"
#include "series.h"
//vtk
#include <vtkPNGWriter.h>
#include <vtkImageActor.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkCommand.h>
#include <vtkProp.h>
#include <vtkLine.h>
#include <vtkPoints.h>
//Qt

namespace udg {

PolylineROITool::PolylineROITool( QViewer *viewer, QObject *parent )
 : Tool(viewer, parent)
{
    m_toolName = "PolylineROITool";
    m_hasSharedData = false;

    m_2DViewer = qobject_cast<Q2DViewer *>( viewer );
    if( !m_2DViewer )
    {
        DEBUG_LOG(QString("El casting no ha funcionat!!! És possible que viewer no sigui un Q2DViewer!!!-> ")+ viewer->metaObject()->className() );
    }

    m_closingPolyline = NULL;
    m_mainPolyline = NULL;
}

PolylineROITool::~PolylineROITool()
{
    if ( !m_mainPolyline.isNull() )
        delete m_mainPolyline;

    if ( !m_closingPolyline.isNull() )
        delete m_closingPolyline;
}

void PolylineROITool::handleEvent( long unsigned eventID )
{
    switch( eventID )
    {
        case vtkCommand::LeftButtonPressEvent:
            if( m_2DViewer->getInput() )
            {
                this->annotateNewPoint();
                m_2DViewer->getDrawer()->refresh();

                deleteRepeatedPoints();

                if ( m_2DViewer->getInteractor()->GetRepeatCount() == 1 && m_mainPolyline->getNumberOfPoints() > 2 )
                {
                    closeForm();
                }
            }
        break;

        case vtkCommand::MouseMoveEvent:

            if( m_mainPolyline && ( m_mainPolyline->getNumberOfPoints() >= 1 ) )
            {
                this->simulateClosingPolyline();
                m_2DViewer->getDrawer()->refresh();
            }
        break;
    }
}

void PolylineROITool::deleteRepeatedPoints()
{
    /*
        si s'ha anotat el pirmer o el segon punt de la polilinia fent doble clic, aquest punt apareixerà repetit dins de la
        llista de punts de la polilinia, pertant cal truere'ls per tal de tenir un bon funcionament de la tool.
    */
    int i;
    double equals = true;
    double *first = m_mainPolyline->getPoint(0);
    double *second = m_mainPolyline->getPoint(1);

    for ( i = 0; i < 3 && equals; i++ )
    {
        equals = first[i] == second[i];
    }

    if ( equals ) //el primer punt i el segon són el mateix, per tant n'esborrem un
        m_mainPolyline->removePoint(0);

    // ara cal mirar el mateix pel punts segon i tercer
    equals = true;
    first = m_mainPolyline->getPoint(1);
    second = m_mainPolyline->getPoint(2);

    for ( i = 0; i < 3 && equals; i++ )
    {
        equals = first[i] == second[i];
    }

    if ( equals ) //el primer punt i el segon són el mateix, per tant n'esborrem un
        m_mainPolyline->removePoint(1);

}

void PolylineROITool::annotateNewPoint()
{
    if (!m_mainPolyline )
    {
        m_mainPolyline = new DrawerPolyline;
        m_2DViewer->getDrawer()->draw( m_mainPolyline , m_2DViewer->getView(), m_2DViewer->getCurrentSlice() );
    }

    int position[2];
    m_2DViewer->getEventPosition( position );
    double *lastPointInModel = m_2DViewer->pointInModel( position[0], position[1] );

    //afegim el punt
    m_mainPolyline->addPoint( lastPointInModel );

    //actualitzem els atributs de la polilinia
    m_mainPolyline->update( DrawerPrimitive::VTKRepresentation );
}

void PolylineROITool::simulateClosingPolyline( )
{
    if (!m_closingPolyline )
    {
        m_closingPolyline = new DrawerPolyline;
        m_closingPolyline->setLinePattern( DrawerPrimitive::DiscontinuousLinePattern );
        m_2DViewer->getDrawer()->draw( m_closingPolyline , m_2DViewer->getView(), m_2DViewer->getCurrentSlice() );
    }

    m_closingPolyline->deleteAllPoints();

    int position[2];
    m_2DViewer->getEventPosition( position );
    double *lastPointInModel = m_2DViewer->pointInModel( position[0], position[1] );

    //afegim els punts que simulen aquesta polilinia
    m_closingPolyline->addPoint( m_mainPolyline->getPoint( 0 ) );
    m_closingPolyline->addPoint( lastPointInModel );
    m_closingPolyline->addPoint( m_mainPolyline->getPoint( m_mainPolyline->getNumberOfPoints() - 1 ) );

    //actualitzem els atributs de la polilinia
    m_closingPolyline->update( DrawerPrimitive::VTKRepresentation );
}

double PolylineROITool::computeGrayMean()
{
	double mean = 0.0;
    int i;
    int subId;
    int initialPosition;
    int endPosition;
    double intersectPoint[3];
    double *firstIntersection;
    double *secondIntersection;
    double pcoords[3];
    double t;
    double p0[3];
    double p1[3];
    int numberOfVoxels = 0;
    QList<double*> intersectionList;
    QList<int> indexList;
    vtkPoints *auxPoints;
    double rayP1[3];
    double rayP2[3];
    double verticalLimit;
	int currentView = m_2DViewer->getView();

    //el nombre de segments és el mateix que el nombre de punts del polígon
    int numberOfSegments = m_mainPolyline->getNumberOfPoints()-1;

    //taula de punters a vtkLine per a representar cadascun dels segments del polígon
    QVector<vtkLine*> segments;

    //creem els diferents segments
    for ( i = 0; i < numberOfSegments; i++ )
    {
        vtkLine *line = vtkLine::New();
        line->GetPointIds()->SetNumberOfIds(2);
        line->GetPoints()->SetNumberOfPoints(2);

        double *p1 = m_mainPolyline->getPoint( i );
        double *p2 = m_mainPolyline->getPoint( i+1 );

        line->GetPoints()->InsertPoint( 0, p1 );
        line->GetPoints()->InsertPoint( 1, p2 );

        segments << line;
    }

    double *bounds = m_mainPolyline->getPolylineBounds();
	double *spacing = m_2DViewer->getInput()->getSpacing();

	int rayPointIndex;
	int intersectionIndex;
	switch( currentView )
	{
	case Q2DViewer::Axial:
		rayP1[0] = bounds[0];//xmin
		rayP1[1] = bounds[2];//y
		rayP1[2] = bounds[4];//z
		rayP2[0] = bounds[1];//xmax
		rayP2[1] = bounds[2];//y
		rayP2[2] = bounds[4];//z
	
		rayPointIndex = 1;
		intersectionIndex = 0;
		verticalLimit = bounds[3];
	break;

	case Q2DViewer::Sagital:
		rayP1[0] = bounds[0];//xmin
		rayP1[1] = bounds[2];//ymin
		rayP1[2] = bounds[4];//zmin
		rayP2[0] = bounds[0];//xmin
		rayP2[1] = bounds[2];//ymin
		rayP2[2] = bounds[5];//zmax

		rayPointIndex = 1;
		intersectionIndex = 2;		
		verticalLimit = bounds[3];

	break;

	case Q2DViewer::Coronal:
		rayP1[0] = bounds[0];//xmin
		rayP1[1] = bounds[2];//ymin
		rayP1[2] = bounds[4];//zmin
		rayP2[0] = bounds[1];//xmax
		rayP2[1] = bounds[2];//ymin
		rayP2[2] = bounds[4];//zmin

		rayPointIndex = 2;
		intersectionIndex = 0;
		verticalLimit = bounds[5];		
	break;
	}
	
	while( rayP1[rayPointIndex] <= verticalLimit )
    {
        intersectionList.clear();
        indexList.clear();
        for ( i = 0; i < numberOfSegments; i++ )
        {
            auxPoints = segments[i]->GetPoints();
            auxPoints->GetPoint(0,p0);
            auxPoints->GetPoint(1,p1);	
			if( (rayP1[rayPointIndex] <= p0[rayPointIndex] && rayP1[rayPointIndex] >= p1[rayPointIndex]) 
				|| (rayP1[rayPointIndex] >= p0[rayPointIndex] && rayP1[rayPointIndex] <= p1[rayPointIndex]) )
                indexList << i;

        }
        //obtenim les interseccions entre tots els segments de la ROI i el raig actual
        foreach (int segment, indexList)
        {
            if ( segments[segment]->IntersectWithLine(rayP1, rayP2, 0.0001, t, intersectPoint, pcoords, subId) > 0)
            {
                double *findedPoint = new double[3];
                findedPoint[0] = intersectPoint[0];
                findedPoint[1] = intersectPoint[1];
                findedPoint[2] = intersectPoint[2];
                intersectionList.append ( findedPoint );
            }
        }

        if ( (intersectionList.count() % 2)==0 )
        {
            int limit = intersectionList.count()/2;
            for ( i = 0; i < limit; i++ )
            {
                initialPosition = i * 2;
                endPosition = initialPosition + 1;

                firstIntersection = intersectionList.at( initialPosition );
                secondIntersection = intersectionList.at( endPosition );

                //Tractem els dos sentits de les interseccions
                if (firstIntersection[intersectionIndex] <= secondIntersection[intersectionIndex])//d'esquerra cap a dreta
                {
                    while ( firstIntersection[intersectionIndex] <= secondIntersection[intersectionIndex] )
                    {
                        mean += (double)getGrayValue( firstIntersection );
                        numberOfVoxels++;
                        firstIntersection[intersectionIndex] += spacing[0];
                    }
                }
                else //de dreta cap a esquerra
                {
                    while ( firstIntersection[intersectionIndex] >= secondIntersection[intersectionIndex] )
                    {
                        mean += (double)getGrayValue( firstIntersection );
                        numberOfVoxels++;
                        firstIntersection[intersectionIndex] -= spacing[0];
                    }
                }
            }
        }
        else
            DEBUG_LOG( "EL NOMBRE D'INTERSECCIONS ENTRE EL RAIG I LA ROI Ã‰S IMPARELL!!" );

        //fem el següent pas en la coordenada que escombrem
		rayP1[rayPointIndex] += spacing[1];
        rayP2[rayPointIndex] += spacing[1];
    }

	 mean /= numberOfVoxels;

    //destruim els diferents segments que hem creat per simular la roi
    for ( i = 0; i < numberOfSegments; i++ )
        segments[i]->Delete();

    return mean;

}

Volume::VoxelType PolylineROITool::getGrayValue( double *coords )
{
    double *origin = m_2DViewer->getInput()->getOrigin();
	double *spacing = m_2DViewer->getInput()->getSpacing();
    int index[3];

    switch( m_2DViewer->getView() )
    {
        case Q2DViewer::Axial:
            index[0] = (int)((coords[0] - origin[0])/spacing[0]);
            index[1] = (int)((coords[1] - origin[1])/spacing[1]);
            index[2] = m_2DViewer->getCurrentSlice();
            break;

        case Q2DViewer::Sagital:
            index[0] = m_2DViewer->getCurrentSlice();
            index[1] = (int)((coords[1] - origin[1])/spacing[1]);
            index[2] = (int)((coords[2] - origin[2])/spacing[2]);
            break;

        case Q2DViewer::Coronal:
            index[0] = (int)((coords[0] - origin[0])/spacing[0]);
            index[1] = m_2DViewer->getCurrentSlice();
            index[2] = (int)((coords[2] - origin[2])/spacing[2]);
            break;
    }

    if ( m_2DViewer->isThickSlabActive() )
		return *((Volume::VoxelType *)m_2DViewer->getCurrentSlabProjection()->GetScalarPointer(index));
    else
        return *(m_2DViewer->getInput()->getScalarPointer(index));
}

void PolylineROITool::closeForm()
{
    m_mainPolyline->addPoint( m_mainPolyline->getPoint( 0 ) );
    m_mainPolyline->update( DrawerPrimitive::VTKRepresentation );

    double *bounds = m_mainPolyline->getPolylineBounds();
    if( !bounds )
    {
        DEBUG_LOG( "Bounds no definits" );
    }
    else
    {
        double *intersection = new double[3];

        intersection[0] = (bounds[1]+bounds[0])/2.0;
        intersection[1] = (bounds[3]+bounds[2])/2.0;
        intersection[2] = (bounds[5]+bounds[4])/2.0;

        DrawerText * text = new DrawerText;

        const double * pixelSpacing = m_2DViewer->getInput()->getSeries()->getImages().at(0)->getPixelSpacing();

        if ( pixelSpacing[0] == 0.0 && pixelSpacing[1] == 0.0 )
        {
            double * spacing = m_2DViewer->getInput()->getSpacing();
            text->setText( tr("Area: %1 px2\nMean: %2").arg( m_mainPolyline->computeArea( m_2DViewer->getView() , spacing ), 0, 'f', 0 ).arg( this->computeGrayMean(), 0, 'f', 2 ) );
        }
        else
        {
            text->setText( tr("Area: %1 mm2\nMean: %2").arg( m_mainPolyline->computeArea( m_2DViewer->getView() ) ).arg( this->computeGrayMean(), 0, 'f', 2 ) );
        }
        
        text->setAttatchmentPoint( intersection );
        text->update( DrawerPrimitive::VTKRepresentation );
        m_2DViewer->getDrawer()->draw( text , m_2DViewer->getView(), m_2DViewer->getCurrentSlice() );
    }
    delete m_closingPolyline;

    m_closingPolyline=NULL;
    m_mainPolyline=NULL;
    m_2DViewer->getDrawer()->refresh();
}

}
