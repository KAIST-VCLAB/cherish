#ifndef GLWIDGET
#define GLWIDGET

#include <QOpenGLWidget>
#include <QTabletEvent>
#include <QUndoStack>
#include <QStack>
#include <QPixmap>

#include <osg/ref_ptr>
#include <osg/observer_ptr>
#include <osgViewer/GraphicsWindow>
#include <osgViewer/CompositeViewer>
#include <osgViewer/Viewer>

#include "RootScene.h"
#include "Settings.h"
#include "../libSGControls/Manipulator.h"
#include "../libSGControls/EventHandler.h"
#include "../libSGControls/ViewerCommand.h"

class EventHandler;

class GLWidget : public QOpenGLWidget {
    Q_OBJECT
public:
    GLWidget(RootScene* root, QUndoStack* stack, QWidget* parent=0, Qt::WindowFlags f = 0);
    virtual ~GLWidget();

    osg::Camera* getCamera() const;
    void setCameraView();
    void setCameraView(const osg::Vec3d& eye, const osg::Vec3d& center, const osg::Vec3d& up);
    void getCameraView(osg::Vec3d& eye, osg::Vec3d& center, osg::Vec3d& up) const;

    void setMouseMode(const dureu::MOUSE_MODE& mode);

signals:
    void sendAutoSwitchMode(dureu::MOUSE_MODE mode);
    void signalMouseModeSet(dureu::MOUSE_MODE mode);

public slots:
    void getTabletActivity(bool active);
    void onRequestScreenshot(QPixmap& pmap, const osg::Vec3d& eye, const osg::Vec3d& center, const osg::Vec3d& up);
    void onFOVChanged(double fov);
    void onFocalChanged(double focal);
    void onOrthoSet(bool ortho);

protected:
    // Widget's events that need to be over-ridden
    virtual void initializeGL();
    virtual void paintGL();
    virtual void resizeGL(int w, int h);

    virtual void keyPressEvent(QKeyEvent* event);
    virtual void keyReleaseEvent(QKeyEvent* event);

    virtual void mouseMoveEvent(QMouseEvent* event);
    virtual void mousePressEvent(QMouseEvent* event);
    virtual void mouseDoubleClickEvent(QMouseEvent* event);
    virtual void mouseReleaseEvent(QMouseEvent* event);
    virtual void wheelEvent(QWheelEvent* event);
    virtual void tabletEvent(QTabletEvent* event);

    virtual bool event(QEvent* event);

private:
    virtual void onHome();
    virtual void onResize(int w, int h);

    osgGA::EventQueue* getEventQueue() const; // for osg to process mouse and keyboard events
    // for more info see reference osgGA::EventQueue and osgGA::GUIEventAdapter
    // the later's enums are used in EventHandler.h

    osg::ref_ptr<osgViewer::GraphicsWindowEmbedded> m_GraphicsWindow;
    osg::ref_ptr<osgViewer::CompositeViewer> m_Viewer;
    osg::observer_ptr<RootScene> m_RootScene;

    QTabletEvent::TabletDevice m_TabletDevice;

    int m_ModeView; // number of views per CompositeViewer
    bool m_DeviceDown; // pen touches the device?
    bool m_DeviceActive; // pen is in device approximation?

    dureu::MOUSE_MODE m_mouseMode;
    osg::ref_ptr<Manipulator> m_manipulator;
    osg::ref_ptr<EventHandler> m_EH;

    //QStack<osg::Matrixd> m_stackView;

    QUndoStack* m_viewStack;
    osg::Vec3d m_eye, m_center, m_up; /* for prev/next views */
};

#endif // GLWIDGET
