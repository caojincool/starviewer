#include "qcudarenderwindow.h"

#include <iostream>


namespace udg {


QCudaRenderWindow::QCudaRenderWindow( QColor backgroundColor, int renderSize )
 : QGLWidget( QGLFormat( QGL::AlphaChannel ) ), m_backgroundColor( backgroundColor ), m_renderSize( renderSize )
{
#ifdef CUDA_AVAILABLE
    makeCurrent();

    GLenum glew = glewInit();
    if ( glew != GLEW_OK ) std::cout << "EEeeeeeEEEEEEEEE" << std::endl;

    // crear el pbo
    glGenBuffersARB( 1, &m_pixelBufferObject );
    glBindBufferARB( GL_PIXEL_UNPACK_BUFFER_ARB, m_pixelBufferObject );
    glBufferDataARB( GL_PIXEL_UNPACK_BUFFER_ARB, renderSize * renderSize * sizeof(uint), 0, GL_STREAM_DRAW_ARB );
    glBindBufferARB( GL_PIXEL_UNPACK_BUFFER_ARB, 0 );

    // crear la textura
    glGenTextures( 1, &m_texture );
    glBindTexture( GL_TEXTURE_2D, m_texture );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP );
    glTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL );
    glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA8, renderSize, renderSize, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0 );
    glBindTexture( GL_TEXTURE_2D, 0 );
#endif
}


QCudaRenderWindow::~QCudaRenderWindow()
{
#ifdef CUDA_AVAILABLE
    glDeleteBuffersARB( 1, &m_pixelBufferObject );
    glDeleteTextures( 1, &m_texture );
#endif
}


GLuint QCudaRenderWindow::pixelBufferObject() const
{
    return m_pixelBufferObject;
}


void QCudaRenderWindow::initializeGL()
{
#ifdef CUDA_AVAILABLE
    qglClearColor( m_backgroundColor );
    glDisable( GL_DEPTH_TEST );
    glEnable( GL_TEXTURE_2D );
#endif
}


void QCudaRenderWindow::resizeGL( int width, int height )
{
#ifdef CUDA_AVAILABLE
    glViewport( 0, 0, width, height );
#endif
}


void QCudaRenderWindow::paintGL()
{
#ifdef CUDA_AVAILABLE
    glBindTexture( GL_TEXTURE_2D, m_texture );
    glBindBufferARB( GL_PIXEL_UNPACK_BUFFER_ARB, m_pixelBufferObject );
    glTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, m_renderSize, m_renderSize, GL_RGBA, GL_UNSIGNED_BYTE, 0 );
    glBindBufferARB( GL_PIXEL_UNPACK_BUFFER_ARB, 0 );

    glClear( GL_COLOR_BUFFER_BIT );

    qglColor( m_backgroundColor );

    glBegin( GL_QUADS );
    {
        glTexCoord2f( 0.0f, 0.0f );
        glVertex2f( -1.0f, -1.0f );
        glTexCoord2f( 0.0f, 1.0f );
        glVertex2f( -1.0f, 1.0f );
        glTexCoord2f( 1.0f, 1.0f );
        glVertex2f( 1.0f, 1.0f );
        glTexCoord2f( 1.0f, 0.0f );
        glVertex2f( 1.0f, -1.0f );
    }
    glEnd();

    glBindTexture( GL_TEXTURE_2D, 0 );
#endif
}


}
