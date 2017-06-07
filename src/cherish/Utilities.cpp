#include "Utilities.h"

#include <QDebug>
#include <QtGlobal>

#include "Settings.h"
#include "Data.h"

//#include <opencv2/core.hpp>
//#include <opencv2/calib3d.hpp>
#include "vector"

QColor Utilities::getQColor(const osg::Vec4f &color)
{
    return QColor(color.r()*255, color.g()*255, color.b()*255, color.a()*255);
}

osg::Vec4f Utilities::getOsgColor(const QColor &color)
{
    return osg::Vec4f(float(color.red())/255.f,
                      float(color.green())/255.f,
                      float(color.blue())/255.f,
                      float(color.alpha())/255.f);
}

bool Utilities::areStrokesProjectable(const std::vector<entity::Stroke *> &strokes, entity::Canvas *source, entity::Canvas *target, osg::Camera *camera)
{
    if (!target || !source || strokes.empty() || !camera) {
        qWarning( "push strokes: one of the pointers is NULL " );
        return false;
    }
    osg::Matrix M = source->getTransform()->getMatrix();
    osg::Matrix m = target->getTransform()->getMatrix();
    osg::Matrix invM;
    if (!invM.invert(m)){
        qWarning( "push strokes: could not invert model matrix" );
        return false;
    }
    const osg::Plane plane = target->getPlane();
    const osg::Vec3 center = target->getCenter();

    /* get camera matrices and other data */
    osg::Vec3f eye, c, u;
    camera->getViewMatrixAsLookAt(eye, c, u);
    osg::Matrix VPW = camera->getViewMatrix() * camera->getProjectionMatrix() * camera->getViewport()->computeWindowMatrix();
    osg::Matrix invVPW;
    if (!invVPW.invert(VPW)){
        qWarning( "areStrokesProjectable(): could not invert View-projection-world matrix for ray casting" );
        return false;
    }

    std::vector<osg::Vec3f> ray(2);

    for (unsigned int i=0; i<strokes.size(); ++i){
        entity::Stroke* s0 = strokes.at(i);
        osg::Vec3Array* verts = static_cast<osg::Vec3Array*>(s0->getVertexArray());
        for (unsigned int j=0; j<verts->size(); ++j){
            osg::Vec3 p = (*verts)[j];
            osg::Vec3 P = p * M;
            osg::Vec3 dir = P - eye;
            osg::Vec3f screen = P * VPW;
            osg::Vec3f nearPoint = osg::Vec3f(screen.x(), screen.y(), 0.f) * invVPW;
            osg::Vec3f farPoint = osg::Vec3f(screen.x(), screen.y(), 1.f) * invVPW;
            ray[0] = nearPoint;
            ray[1] = farPoint;

            if (plane.intersect(ray)){
                qWarning("push strokes: some point projections do not intersect the target canvas plane."
                          "To resolve, change the camera view");
                return false;
            }

            if (! plane.dotProductNormal(dir)){ // denominator
                qWarning( "push strokes: one of the points of projected stroke forms parallel projection to the canvas plane."
                             "To resolve, change camera view.");
                return false;
            }
            if (! plane.dotProductNormal(center-P)){
                qWarning( "push strokes: plane contains the projected stroke or its part, so no single intersection can be defined."
                             "To resolve, change camera view.");
                return false;
            }
            double len = plane.dotProductNormal(center-P) / plane.dotProductNormal(dir);
            osg::Vec3 P_ = dir * len + P;
            osg::Vec3 p_ = P_ * invM;
            if (std::fabs(p_.z())>cher::EPSILON){
                qWarning( "push strokes: error while projecting point from global 3D to local 3D, z-coordinate is not zero.");
                outLogVec("P_", P_.x(), P_.y(), P_.z());
                outLogVec("p_", p_.x(), p_.y(), p_.z());
                return false;
            }
        }
    }
    qDebug("strokes are projectable");
    return true;
}

bool Utilities::getViewProjectionWorld(osgGA::GUIActionAdapter &aa, osg::Matrix &VPW, osg::Matrix &invVPW)
{
    osgViewer::View* viewer = dynamic_cast<osgViewer::View*>(&aa);
    if (!viewer){
        qWarning("getVPW: could not dynamic_cast to View*");
        return false;
    }

    osg::Camera* camera = viewer->getCamera();
    if (!camera){
        qWarning("getVPW: could not obtain camera");
        return false;
    }

    if (!camera->getViewport()){
        qWarning("getVPW: could not obtain viewport");
        return false;
    }

    VPW = camera->getViewMatrix()
            * camera->getProjectionMatrix()
            * camera->getViewport()->computeWindowMatrix();

    if (!invVPW.invert(VPW)){
        qWarning("getVPW: could not invert VPW matrix");
        return false;
    }

    return true;
}

void Utilities::getFarNear(double x, double y, const osg::Matrix &invVPW, osg::Vec3f &near, osg::Vec3f &far)
{
    near = osg::Vec3f(x, y, 0.f) * invVPW;
    far = osg::Vec3f(x, y, 1.f) * invVPW;
}

bool Utilities::getRayPlaneIntersection(const osg::Plane &plane, const osg::Vec3f &center, const osg::Vec3f &nearPoint, const osg::Vec3f &farPoint, osg::Vec3f &P, bool isLine)
{
    if (!plane.valid()){
        qWarning("rayPlaneIntersection: plane is not valid");
        return false;
    }

    if (!isLine) {
        std::vector<osg::Vec3f> ray(2);
        ray[0] = nearPoint;
        ray[1] = farPoint;
        if (plane.intersect(ray)) { // 1 or -1 means no intersection
            qWarning("rayPlaneIntersection: no intersection with ray");
            return false;
        }
    }

    osg::Vec3f dir = farPoint-nearPoint;
    if (!plane.dotProductNormal(dir)){
        qWarning("rayPlaneIntersection: projected ray is almost parallel to plane. "
                  "Change view point.");
        return false;
    }

    if (! plane.dotProductNormal(center-nearPoint)){
        qWarning("rayPlaneIntersection: plane contains the line. "
                  "Change view point");
        return false;
    }

    double len = plane.dotProductNormal(center-nearPoint) / plane.dotProductNormal(dir);
    P = dir * len + nearPoint;

    return true;
}

bool Utilities::getModel(entity::Canvas *canvas, osg::Matrix &M, osg::Matrix &invM)
{
    if (!canvas){
        qWarning("getModel: canvas is NULL");
        return false;
    }

    M = canvas->getTransform()->getMatrix();

    if (!invM.invert(M)){
        qWarning("getModel: could not invert M");
        return false;
    }

    return true;
}

bool Utilities::getLocalFromGlobal(const osg::Vec3f &P, const osg::Matrix &invM, osg::Vec3f &p)
{
    p = P * invM;
    if (std::fabs(p.z())>cher::EPSILON){
        qWarning("getLocalFromGlobal: local point's z-coordinate is not zero");
        return false;
    }

    return true;
}

bool Utilities::getGlobalFromLocal(const osg::Vec3f &p, const osg::Matrix &M, osg::Vec3f &P)
{
    if (std::fabs(p.z())>cher::EPSILON){
        qWarning("getGlobalFromLocal: local point's z-coord is not close to zero");
        return false;
    }
    P = p * M;
    return true;
}

double Utilities::getAngleTwoVectors(const osg::Vec2f &p1, const osg::Vec2f &p2, const osg::Vec2f &p3, const osg::Vec2f &p4)
{
    double theta = 0.0;

    osg::Vec2f v1 = p2-p1;
    osg::Vec2f v2 = p4-p3;

    theta = std::atan2(v2.y(), v2.x()) - std::atan2(v1.y(), v1.x());

    return theta;
}

osg::Vec3f Utilities::rotate2DPointAround(const osg::Vec3f &center, float theta, const osg::Vec3f &original)
{
    osg::Vec3f result = original;
    if (theta != 0){
        auto P = original - center;
        result = center + osg::Vec3f(P.x() * std::cos(theta) - P.y() * std::sin(theta),
                                     P.x() * std::sin(theta) + P.y() * std::cos(theta),
                                     0);
    }
    return result;
}

bool Utilities::getSkewLinesProjection(const osg::Vec3f &center, const osg::Vec3f &farPoint, const osg::Vec3f &nearPoint, const osg::Vec3f &normal, osg::Vec3f &X1)
{
    osg::Vec3f P1 = center;
    osg::Vec3f P2 = nearPoint;
    osg::Vec3f d = P2 - P1;
    osg::Vec3f u1 = normal;
    u1.normalize();
    osg::Vec3f u2 = farPoint - nearPoint;
    u2.normalize();
    osg::Vec3f u3 = u1^u2;

    if (std::fabs(u3.x())<=cher::EPSILON && std::fabs(u3.y())<=cher::EPSILON && std::fabs(u3.z())<=cher::EPSILON){
        qWarning("getSkewLinesProjection: cast ray and normal are almost parallel."
                  "Switch view point.");
        return false;
    }

    // X1 and X2 are the closest points on lines
    // we want to find X1 (u1 corresponds to normal)
    // solving the linear equation in r1 and r2: Xi = Pi + ri*ui
    // we are only interested in X1 so we only solve for r1.
    float a1 = u1*u1, b1 = u1*u2, c1 = u1*d;
    float a2 = u1*u2, b2 = u2*u2, c2 = u2*d;
    if (!(std::fabs(b1) > cher::EPSILON)){
        qWarning("getSkewLinesProjection: denominator is zero");
        return false;
    }
    if (!(a2!=-1 && a2!=1)){
        qWarning("getSkewLinesProjection: lines are parallel");
        return false;
    }

    double r1 = (c2 - b2*c1/b1)/(a2-b2*a1/b1);
    X1 = P1 + u1*r1;

    return true;
}

double Utilities::getSkewLinesDistance(const osg::Vec3d &r1, const osg::Vec3d &r2, const osg::Vec3d &v1, const osg::Vec3d &v2)
{
    osg::Vec3d u1 = r2-r1;
    osg::Vec3d u2 = v2-v1;
    osg::Vec3d u3 = u1^u2;
    if (u3.length() == 0) return 1;
    u3.normalize();
    osg::Vec3d dir = v1 - r1;
    return std::fabs((dir*u3));
}

bool Utilities::getLinesIntersection(const osg::Vec3f &La1, const osg::Vec3f &La2, const osg::Vec3f &Lb1, const osg::Vec3f &Lb2, osg::Vec3f &intersection)
{
    // first check if lines have exact intersection point
    // do it by checking id the shortest distance is 0
    auto distance = Utilities::getSkewLinesDistance(La1, La2, Lb1, Lb2);
    if (distance == 0){
        qDebug("3d lines have exact intersection point");
        auto C = La2;
        auto D = Lb2;
        auto e = La1-La2;
        auto f = Lb1-Lb2;
        auto g = D-C;
        if ((f^g).length()==0 || (f^e).length()==0){
            qWarning("lines have no intersection, are they parallel?");
            return false;
        }

        auto fgn = f^g; fgn.normalize();
        auto fen = f^e; fen.normalize();
        int di = -1;
        if (fgn == fen) // same direction?
            di *= -1;

        intersection = C + e*di*( (f^g).length() / (f^e).length() );
        return true;
    }

    // use notations as in getSkewLinesProjection
    auto P1 = La2;
    auto P2 = Lb2;
    auto d = P2 - P1;
    auto u1 = La1 - La2;
    u1.normalize();
    auto u2 = Lb1 - Lb2;
    u2.normalize();
    auto u3 = u1^u2; // cross product

    if (std::fabs(u3.x())<=cher::EPSILON && std::fabs(u3.y())<=cher::EPSILON && std::fabs(u3.z())<=cher::EPSILON){
        qWarning("getLinesIntersection: lines are almost parallel. Cannot obtain the intersection");
        return false;
    }

    // X1 and X2 are the closest points on lines
    // we want to find both X1 and X2
    // solving the linear equation in r1 and r2: Xi = Pi + ri*ui
    // for both r1 and r2
    float a1 = u1*u1, b1 = u1*u2, c1 = u1*d;
    float a2 = u1*u2, b2 = u2*u2, c2 = u2*d;
    if (!(std::fabs(b1) > cher::EPSILON)){
        qWarning("getLinesIntersection: denominator is zero");
        return false;
    }
    if (!(a2!=-1 && a2!=1)){
        qWarning("getLinesIntersection: lines are parallel");
        return false;
    }

    double r1 = (c2 - b2*c1/b1)/(a2-b2*a1/b1);
    double r2 = (a1*r1 - c1)/b1;

    auto X1 = P1 + u1*r1;
    auto X2 = P2 + u2*r2;

    intersection = (X1 + X2)/2.f;

    return true;
}

int Utilities::getCanvasesIntersection(entity::Canvas *current, entity::Canvas *previous, osg::Vec3f &p1, osg::Vec3f &p2, osg::Vec3f &p3, osg::Vec3f &p4)
{
    /* find intersection line between current and previous */
    osg::Vec3f iP, u;
    int is = getPlanesIntersection(current, previous, iP, u);

    /* if intersection found */
    if (is == 2){
        /* get four local vertices of current canvas */
        const osg::Vec3Array* vertices = current->getFrameVertices();
        osg::Vec3f v1 = (*vertices)[0];
        osg::Vec3f v2 = (*vertices)[1];
        osg::Vec3f v3 = (*vertices)[2];
        osg::Vec3f v4 = (*vertices)[3];

        /* translate local to global coordiantes */
        osg::Matrix M, invM;
        getModel(current, M, invM);
        osg::Vec3f V1=cher::CENTER, V2=cher::CENTER, V3=cher::CENTER, V4=cher::CENTER;
        getGlobalFromLocal(v1, M, V1);
        getGlobalFromLocal(v2, M, V2);
        getGlobalFromLocal(v3, M, V3);
        getGlobalFromLocal(v4, M, V4);

        /* project each global vertex onto global intersection line*/
        osg::Vec3f P1=cher::CENTER, P2=cher::CENTER, P3=cher::CENTER, P4=cher::CENTER;
        P1 = projectPointOnLine(iP, u, V1);
        P2 = projectPointOnLine(iP, u, V2);
        P3 = projectPointOnLine(iP, u, V3);
        P4 = projectPointOnLine(iP, u, V4);

        /* project global back to local since we need to feed local coords to canvas drawables */
        getLocalFromGlobal(P1, invM, p1);
        getLocalFromGlobal(P2, invM, p2);
        getLocalFromGlobal(P3, invM, p3);
        getLocalFromGlobal(P4, invM, p4);
    }
    return is;
}

int Utilities::getPlanesIntersection(entity::Canvas *canvas1, entity::Canvas *canvas2, osg::Vec3f &iP, osg::Vec3f &u)
{
    /* cross produc of normals */
    osg::Vec3f n1 = canvas1->getNormal();
    osg::Vec3f n2 = canvas2->getNormal();
    u = n1^n2;

    float ax = (u.x() >= 0 ? u.x() : -u.x());
    float ay = (u.y() >= 0 ? u.y() : -u.y());
    float az = (u.z() >= 0 ? u.z() : -u.z());

    osg::Vec3f C1 = canvas1->getCenter();
    osg::Vec3f C2 = canvas2->getCenter();

    /* are two planes parallel? */
    if (std::fabs(ax+ay+az) < cher::EPSILON) {
        /* normals are near parallel */
        /* are they disjoint or coincide? */
        osg::Vec3f v = C2- C1;
        v.normalize();
        /* coincide */
        if (n1*v == 0) return 1;
        /* disjoint */
        else return 0;
    }

    /* canvases intersect in a line */
    int maxc;
    if (ax > ay) {
        if (ax > az) maxc =  1;
        else maxc = 3;
    }
    else {
        if (ay > az) maxc =  2;
        else maxc = 3;
    }

    /* obtain a point on the intersect line:
     * zero the max coord, and solve for the other two */
    float d1 = -n1*C1; // note: could be pre-stored  with plane
    float d2 = -n2*C2; // the constants in the 2 plane equations

    float xi, yi, zi;
    switch (maxc) {
    case 1:
        xi = 0;
        yi = (d2*n1.z() - d1*n2.z()) /  u.x();
        zi = (d1*n2.y() - d2*n1.y()) /  u.x();
        break;
    case 2:                     // intersect with y=0
        xi = (d1*n2.z() - d2*n1.z()) /  u.y();
        yi = 0;
        zi = (d2*n1.x() - d1*n2.x()) /  u.y();
        break;
    case 3:                     // intersect with z=0
        xi = (d2*n1.y() - d1*n2.y()) /  u.z();
        yi = (d1*n2.x() - d2*n1.x()) /  u.z();
        zi = 0;
    }
    iP = osg::Vec3f(xi, yi, zi);
    return 2;
}

/* prpject AP onto v and add resulting vector to A */
osg::Vec3f Utilities::projectPointOnLine(const osg::Vec3f &iP, const osg::Vec3f &u, const osg::Vec3f &P)
{
    return iP + u * ((P-iP)*u)/(u*u);
}

double Utilities::distanceTwoPoints(const osg::Vec3f &P1, const osg::Vec3f &P2)
{
    auto dx = P1.x() - P2.x(),
            dy = P1.y() - P2.y(),
            dz = P1.z() - P2.z();
    return std::sqrt(dx*dx + dy*dy + dz*dz);
}

osg::Vec3f Utilities::getAnchorLineSegment(const osg::Vec3f &P0, const osg::Vec3f &P1)
{
    // local coordinates of anchor axis
    osg::Vec3f u = osg::Vec3f(1,0,0);
    osg::Vec3f v = osg::Vec3f(0,1,0);

    // projection of P1 onto local axis
    osg::Vec3f uP1 = projectPointOnLine(P0, u, P1);
    osg::Vec3f vP1 = projectPointOnLine(P0, v, P1);

    // compare distances
    double du = distanceTwoPoints(P1, uP1);
    double dv = distanceTwoPoints(P1, vP1);

    return (du <= dv)? uP1 : vP1;
}

QCursor Utilities::getCursorFromMode(cher::MOUSE_MODE mode)
{
    QCursor cur = Qt::ArrowCursor;
    switch (mode) {
    case cher::SELECT_ENTITY:
        cur = QCursor(Data::sceneSelectPixmap(), 0, 0);
        break;
    case cher::ENTITY_MOVE:
        cur = QCursor(Data::sceneImageMovePixmap(), -1, -1);
        break;
    case cher::ENTITY_SCALE:
        cur = QCursor(Data::sceneImageScalePixmap(), 0, 0);
        break;
    case cher::ENTITY_ROTATE:
        cur = QCursor(Data::sceneImageRotatePixmap(), -1, -1);
        break;
    case cher::ENTITY_FLIPH:
        cur = QCursor(Data::sceneImageFlipHPixmap(), -1, -1);
        break;
    case cher::ENTITY_FLIPV:
        cur = QCursor(Data::sceneImageFlipVPixmap(), -1, -1);
        break;

    case cher::SELECT_CANVAS:
        cur = QCursor(Data::sceneSelect3DPixmap(), 0,0);
        break;
    case cher::MOUSE_CANVAS:
        cur = QCursor(Qt::CrossCursor);
        break;
    case cher::CANVAS_OFFSET:
        cur = QCursor(Data::sceneCanvasOffsetCursor(), -1, -1);
        break;
    case cher::CANVAS_ROTATE_UPLUS:
    case cher::CANVAS_ROTATE_UMINUS:
    case cher::CANVAS_ROTATE_VPLUS:
    case cher::CANVAS_ROTATE_VMINUS:
        cur = QCursor(Data::sceneCanvasRotateCursor(), -1, -1);
        break;

    case cher::PEN_SKETCH:
        cur = QCursor(Data::sceneSketchPixmap(), 0, Data::sceneSketchPixmap().height());
        break;
    case cher::PEN_POLYGON:
        cur = QCursor(Data::scenePolygonPixmap(), 0, Data::scenePolygonPixmap().height());
        break;
    case cher::PEN_DELETE:
        cur = QCursor(Data::editDeleteCursor(), 0, 0);
        break;
    case cher::PEN_ERASE:
        cur = QCursor(Data::sceneEraserPixmap(), -1, -1);
        break;
    case cher::PEN_LINESEGMENT:
        cur = QCursor(Data::sceneLinesegmentPixmap(), 0, Data::sceneLinesegmentPixmap().height());
        break;

    case cher::CAMERA_ORBIT:
        cur = QCursor(Data::sceneOrbitPixmap(), 0, 0);
        break;
    case cher::CAMERA_ZOOM:
        cur = QCursor(Data::sceneZoomPixmap(), 0, 0);
        break;
    case cher::CAMERA_PAN:
        cur = QCursor(Data::scenePanPixmap(), 0, 0);
        break;
    case cher::CAMERA_FIXEDVIEW:
        cur = Qt::ArrowCursor;
        break;

    case cher::CREATE_CANVASCLONE:
    case cher::CREATE_CANVASSEPARATE:
        cur = QCursor(Data::sceneCanvasCloneCursor());
        break;

    case cher::SVM_DRAG_POINT:
    case cher::SVM_DRAG_WIRE:
    case cher::SVM_HOVER_POINT:
    case cher::SVM_HOVER_WIRE:
    case cher::SVM_IDLE:
        cur = Qt::CrossCursor;
        break;

    case cher::CAMPOSE_IDLE:
        cur = Qt::CrossCursor;
        break;
    case cher::CAMPOSE_EYE:
        cur = QCursor(Data::sceneImageMovePixmap(), -1, -1);
        break;
    case cher::CAMPOSE_CENTER:
        cur = QCursor(Data::sceneImageRotatePixmap(), -1, -1);
        break;
    case cher::CAMPOSE_FOCAL:
        cur = QCursor(Data::sceneImageFlipVPixmap(), -1, -1);
        break;
    case cher::PHOTOSCALE_MODELPLANE:
    case cher::PHOTOSCALE_PHOTOPLANE:
    case cher::PHOTOSCALE_BOOKAMRK:
        cur = QCursor(Data::sceneSelect3DPixmap(), 0, 0);
        break;
    default:
        break;
    }

    return cur;
}

bool Utilities::getCameraPosition(entity::SVMData *svm, osg::Vec3f &eye, osg::Vec3f &center, osg::Vec3f &up)
{
    eye = cher::CENTER;
    up = cher::UP;
    const int nPoints = 4;
    if (!svm) return false;
    entity::DraggableWire* wire1 = svm->getWallWire();
    entity::DraggableWire* wire2 = svm->getFlootWire();
    center = wire1->getCenter3D();
    if (!wire1 || !wire2) return false;
    for (int i=0; i<nPoints; ++i){
        int j = (i==3)? 0 : i+1; // neighbor
        auto Pa1 = wire1->getPoint3D(i);
        auto Pa2 = wire2->getPoint3D(i);
        auto Pb1 = wire1->getPoint3D(j);
        auto Pb2 = wire2->getPoint3D(j);
        osg::Vec3f iss;
        bool obtained = Utilities::getLinesIntersection(Pa1, Pa2, Pb1, Pb2, iss);
        if (!obtained){
            qWarning("Could not obtain rays intersection, camera pose will not be updated");
            return false;
        }
        eye += iss;
    }
    eye /= nPoints;

    osg::Vec3f ce = center - eye;
    osg::Vec3f normal = wire2->getPlane().getNormal();
    osg::Vec3f right = normal^ce;
    up = ce^right;
    return true;
}

//bool Utilities::getProjectionMatrix(entity::SVMData *svm, osg::Matrixd &projection)
//{
//    if (!svm){
//        qWarning("SVM structure is NULL");
//        return false;
//    }

//    std::vector<cv::Point3f> object;
//    std::vector<cv::Point2f> image;
//    for (int i=0; i<4; ++i){
//        object.push_back(cv::Point3f(svm->getGlobalFloor(i).x(), svm->getGlobalFloor(i).y(), svm->getGlobalFloor(i).z()));
//        image.push_back(cv::Point2f(svm->getLocalWall(i).x(), svm->getLocalWall(i).y()));
//        std::cout << "object=" << object[i] << " image=" << image[i] << std::endl;
//    }
//    // use solvePNP method
//    cv::Mat rvec(3,1,cv::DataType<double>::type);
//    cv::Mat tvec(3,1,cv::DataType<double>::type);
//    cv::Mat1f K = (cv::Mat1f(3,3) << 1,0,0,0,1,0,0,0,1);

//    cv::solvePnP(object, image, K, cv::noArray(), rvec, tvec/*, cv::SOLVEPNP_P3P*/);
//    // get rotation matrix format
//    cv::Mat R;
//    cv::Rodrigues(rvec, R);

//    std::vector<cv::Point2f> projected;
//    cv::projectPoints(object, rvec, tvec, K, cv::noArray(), projected);
//    float error = 0.f;
//    for(unsigned int i = 0; i < projected.size(); ++i)
//    {
//        auto ei = image[i]-projected[i];
//        error += ei.x*ei.x + ei.y*ei.y;
//    }
//    error = std::sqrt(error);
//    qDebug() << "PROJECTION ERROR=" << error;

//    // OpenCV solution provides the projection in camera coordinates, convert it to global coordinates
//    // for info, see: http://stackoverflow.com/questions/18637494/camera-position-in-world-coordinate-from-cvsolvepnp#18643735
//    R = R.t();  // rotation of inverse
//    tvec = -R * tvec; // translation of inverse

//    // copy OCV data to OSG
//    auto r00 = R.at<double>(0,0);
//    auto r01 = R.at<double>(0,1);
//    auto r02 = R.at<double>(0,2);
//    auto r10 = R.at<double>(1,0);
//    auto r11 = R.at<double>(1,1);
//    auto r12 = R.at<double>(1,2);
//    auto r20 = R.at<double>(2,0);
//    auto r21 = R.at<double>(2,1);
//    auto r22 = R.at<double>(2,2);
//    auto t00 = tvec.at<double>(0,0);
//    auto t10 = tvec.at<double>(1,0);
//    auto t20 = tvec.at<double>(2,0);

//    // OpenGL frame: multiply by matrix rotated 180 degrees around x axis
//    osg::Matrixd R180 = osg::Matrixd::rotate(cher::PI, 1,0,0);
//    projection = osg::Matrixd(
//                r00, r10, r20, 0,
//                r01, r11, r21, 0,
//                r02, r12, r22, 0,
//                t00, t10, t20, 1 ) * R180;
//    qDebug() << "Projection_osg=";
//    qDebug()<<  " = " << projection(0,0) << projection(0,1) << projection(2,0) << projection(3,0);
//    qDebug()<<  " = " << projection(0,1) << projection(1,1) << projection(2,1) << projection(3,1);
//    qDebug()<<  " = " << projection(0,2) << projection(1,2) << projection(2,2) << projection(3,2);
//    qDebug()<<  " = " << projection(0,3) << projection(1,3) << projection(2,3) << projection(3,3);
//    return true;
//}
