#include <cassert>
#include <vector>
#include <iostream>
#include <stdexcept>

#include "viewwidget.h"

#include <osg/Camera>
#include <osg/StateSet>
#include <osg/Material>
#include <osgViewer/Viewer>
#include <osgViewer/ViewerEventHandlers>
#include <osgGA/EventQueue>
#include <osgGA/TrackballManipulator>
#include <osg/LineWidth>

#include <QDebug>
#include <QKeyEvent>
#include <QPainter>
#include <QWheelEvent>

osg::Drawable* createAxis(const osg::Vec3& corner,const osg::Vec3& xdir,const osg::Vec3& ydir,const osg::Vec3& zdir)
{
    // set up the Geometry.
    osg::ref_ptr<osg::Geometry> geom = new osg::Geometry;

    osg::Vec3Array* coords = new osg::Vec3Array(6);
    (*coords)[0] = corner;
    (*coords)[1] = corner+xdir;
    (*coords)[2] = corner;
    (*coords)[3] = corner+ydir;
    (*coords)[4] = corner;
    (*coords)[5] = corner+zdir;

    geom->setVertexArray(coords);

    osg::Vec4 x_color(float(38)/255.0f,float(139)/255.0f,float(210)/255.0f,1.0f); // solarized blue
    osg::Vec4 y_color(float(133)/255.0f,float(153)/255.0f,float(0)/255.0f,1.0f); // solarized green
    osg::Vec4 z_color(float(211)/255.0f,float(54)/255.0f,float(130)/255.0f,1.0f); // solarized magenta

    osg::Vec4Array* color = new osg::Vec4Array(6);
    (*color)[0] = x_color;
    (*color)[1] = x_color;
    (*color)[2] = y_color;
    (*color)[3] = y_color;
    (*color)[4] = z_color;
    (*color)[5] = z_color;

    geom->setColorArray(color, osg::Array::BIND_PER_VERTEX);

    geom->addPrimitiveSet(new osg::DrawArrays(osg::PrimitiveSet::LINES,0,6));

    osg::StateSet* stateset = new osg::StateSet;
    osg::LineWidth* linewidth = new osg::LineWidth();
    linewidth->setWidth(4.0f);
    stateset->setAttributeAndModes(linewidth,osg::StateAttribute::ON);
    stateset->setMode(GL_LIGHTING,osg::StateAttribute::OFF);
    geom->setStateSet(stateset);

    return geom.release();
}

ViewWidget::ViewWidget(QWidget* parent, int viewmode):
    QOpenGLWidget(parent),
    _graphicsWindow(new osgViewer::GraphicsWindowEmbedded(this->x(), this->y(), this->width(),this->height()) ),
    _viewer(new osgViewer::CompositeViewer),
    _tabletDevice(QTabletEvent::Stylus), // http://doc.qt.io/qt-5/qtabletevent.html#TabletDevice-enum
    _viewmode(viewmode),
    _deviceDown(false),
    _deviceActive(false)
{
    osg::ref_ptr<osg::Group> root = new osg::Group;
    osg::ref_ptr<osg::Geode> axes = new osg::Geode;
    axes->addDrawable(createAxis(osg::Vec3(0.0f,0.0f,0.0f), osg::Vec3(1.0f,0.0f,0.0f),
                                     osg::Vec3(0.0f,1.0f,0.0f), osg::Vec3(0.0f,0.0f,1.0f)));
    root->addChild(axes.get());

    osg::StateSet* stateSet = root->getOrCreateStateSet();
    osg::Material* material = new osg::Material;
    material->setColorMode(osg::Material::AMBIENT_AND_DIFFUSE);
    stateSet->setAttributeAndModes(material, osg::StateAttribute::ON);
    stateSet->setMode(GL_DEPTH_TEST, osg::StateAttribute::ON);

    float aspectRatio = static_cast<float>( this->width() ) / static_cast<float>( this->height() );
    osg::Camera* camera = new osg::Camera;
    camera->setViewport( 0, 0, this->width(), this->height());
    camera->setProjectionMatrixAsPerspective( 30.f, aspectRatio, 1.f, 1000.f );
    camera->setGraphicsContext( _graphicsWindow );
    camera->setClearColor(osg::Vec4(float(235)/255.0f, float(246)/255.0f,
                                    float(227)/255.0f, 1.0f)); // solarized base3

    osgGA::TrackballManipulator* manipulator = new osgGA::TrackballManipulator;
    manipulator->setAllowThrow( false );

    osgViewer::View* view = new osgViewer::View;
    view->setName("Single view");
    view->setCamera(camera);
    view->setSceneData(root.get());
    view->addEventHandler(new osgViewer::StatsHandler);
    //view->addEventHandler(new PickHandler);
    view->setCameraManipulator(manipulator);

    _viewer->addView(view);
    _viewer->setThreadingModel(osgViewer::CompositeViewer::SingleThreaded);
    _viewer->realize();

    this->setFocusPolicy(Qt::StrongFocus);
    this->setMinimumSize(512, 512);
    this->setMouseTracking(true);
}

ViewWidget::~ViewWidget(){}

void ViewWidget::getTabletActivity(bool active){
    _deviceActive = active;
}

void ViewWidget::paintEvent(QPaintEvent *pev){
    this->makeCurrent();
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    this->paintGL();
    //if (_selectionActive && !_selectionFinished){
    //    painter.setPen( Qt::black );
    //    painter.setBrush( Qt::transparent );
    //    painter.drawRect(makeRectangle(_selectionStart, _selectionEnd));
    //}
    painter.end();
    this->doneCurrent();
}

void ViewWidget::paintGL(){
    _viewer->frame();
}

void ViewWidget::resizeGL(int w, int h){
    this->getEventQueue()->windowResize(this->x(), this->y(), w, h);
    _graphicsWindow->resized(this->x(), this->y(), w, h);
    this->onResize(w, h);
}

void ViewWidget::keyPressEvent(QKeyEvent *event){
    QString keystr = event->text();
    const char* keydat = keystr.toLocal8Bit().data();
    if (event->key() == Qt::Key_H) {
        this->onHome();
        return;
    }
    this->getEventQueue()->keyPress(osgGA::GUIEventAdapter::KeySymbol (*keydat));
}

void ViewWidget::keyReleaseEvent(QKeyEvent *event){
    QString keystr = event->text();
    const char* keydat = keystr.toLocal8Bit().data();
    this->getEventQueue()->keyRelease(osgGA::GUIEventAdapter::KeySymbol(*keydat));
}

void ViewWidget::mouseMoveEvent(QMouseEvent *event){
    if (!_deviceDown || !_deviceActive) {
        this->getEventQueue()->mouseMotion(static_cast<float>(event->x()), static_cast<float>(event->y()));
    }
}

void ViewWidget::mousePressEvent( QMouseEvent* event )
{
    if (!_deviceDown || !_deviceActive) {
        unsigned int button = 0;
        switch( event->button() ) {
        case Qt::LeftButton:
            button = 1;
            break;
        case Qt::MiddleButton:
            button = 2;
            break;
        case Qt::RightButton:
            button = 3;
            break;
        default:
            break;
        }
        this->getEventQueue()->mouseButtonPress(static_cast<float>(event->x()), static_cast<float>(event->y()), button);
    }
}

void ViewWidget::mouseReleaseEvent(QMouseEvent* event) {
    if (!_deviceDown || !_deviceActive) {
        unsigned int button = 0;
        switch( event->button() ) {
        case Qt::LeftButton:
            button = 1;
            break;
        case Qt::MiddleButton:
            button = 2;
            break;
        case Qt::RightButton:
            button = 3;
            break;
        default:
            break;
        }
        this->getEventQueue()->mouseButtonRelease(static_cast<float>(event->x()), static_cast<float>(event->y()), button);
    }
}

void ViewWidget::wheelEvent( QWheelEvent* event )
{
    event->accept();
    int delta = event->delta();
    osgGA::GUIEventAdapter::ScrollingMotion motion = delta > 0 ?   osgGA::GUIEventAdapter::SCROLL_UP
                                                                 : osgGA::GUIEventAdapter::SCROLL_DOWN;
    this->getEventQueue()->mouseScroll( motion );
}

void ViewWidget::tabletEvent(QTabletEvent *event){
    unsigned int button = 0;
    int osg_rotate = 1, osg_pan = 2, osg_zoom = 3;
    switch (event->type()){
    case QEvent::TabletPress:
        switch(event->buttons()) {
        ///case 1: // touch and no buttons are pressed, default: rotate
        case 2: // no touch and lower button pressed, default: zoom
            button = osg_rotate;
            break;
        case 3: // touch and lower button pressed, default: pan
            button = osg_rotate;
            break;
        case 4: // no touch and upper button pressed, default: rotate
            button = osg_pan;
            break;
        case 5: // touch and upper button pressed, default: rotate
            button = osg_pan;
            break;
        default:
            break;
        }
        this->getEventQueue()->mouseButtonPress(static_cast<float>(event->x()), static_cast<float>(event->y()), button);
        break;
    case QEvent::TabletRelease:
        switch(event->button()) {
        /// buttons() will not work here, so have to use button()
        /// 1: no button released, whether in the air or in touch
        /// 2: lower button released, whether in the air or in touch
        /// 4: top button released, whether in the air or in touch
        case 2:
            button = osg_rotate;
            break;
        case 4:
            button = osg_pan;
            break;
        default:
            break;
        }
        this->getEventQueue()->mouseButtonRelease(static_cast<float>(event->x()), static_cast<float>(event->y()), button);
        break;
    case QEvent::TabletMove:
        this->getEventQueue()->mouseMotion(static_cast<float>(event->x()), static_cast<float>(event->y()));
        break;
    }
}

bool ViewWidget::event( QEvent* event ) {
    bool handled = QOpenGLWidget::event( event );
    switch( event->type() ) {
    case QEvent::TabletMove:
        event->accept();
        this->update();
        break;
    case QEvent::TabletPress:
        event->accept();
        _deviceDown = true;
        this->update();
        break;
    case QEvent::TabletRelease:
        event->accept();
        this->update();
        _deviceDown = false;
        break;
    case QEvent::KeyPress:
    case QEvent::KeyRelease:
    case QEvent::MouseButtonDblClick:
    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonRelease:
    case QEvent::MouseMove:
    case QEvent::Wheel:
        this->update();
        break;
    default:
        break;
    }
    return handled;
}

void ViewWidget::onHome() {
    osgViewer::ViewerBase::Views views;
    _viewer->getViews( views );
    for( std::size_t i = 0; i < views.size(); i++ ) {
        osgViewer::View* view = views.at(i);
        view->home();
    }
}

void ViewWidget::onResize( int w, int h ) {
    std::vector<osg::Camera*> cameras;
    _viewer->getCameras( cameras );
    assert( cameras.size() == (unsigned int)_viewmode);
    //cameras[0]->setViewport( 0, 0, this->width(), this->height() );
    cameras[0]->setViewport( 0, 0, this->width() / _viewmode, this->height() );
    if (_viewmode == 2)
        cameras[1]->setViewport( this->width() / _viewmode, 0, this->width() / _viewmode, this->height() );
}

osgGA::EventQueue* ViewWidget::getEventQueue() const {
    osgGA::EventQueue* eventQueue = _graphicsWindow->getEventQueue();
    if( eventQueue )
        return eventQueue;
    else
        throw std::runtime_error( "Unable to obtain valid event queue");
}

