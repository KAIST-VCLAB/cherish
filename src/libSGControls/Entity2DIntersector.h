#ifndef ENTITY2DINTERSECTOR_H
#define ENTITY2DINTERSECTOR_H

#include <vector>

#include <osg/ref_ptr>
#include <osgUtil/LineSegmentIntersector>

#include "Utilities.h"

// TODO: instead of using intersectors for each entity type, use this one with
// provided template, e.g. entity::Stroke, entity::Photo, etc.

template <typename EntityType>
class Entity2DIntersector : public osgUtil::LineSegmentIntersector
{
public:
    Entity2DIntersector()
        : osgUtil::LineSegmentIntersector(MODEL, 0.f, 0.f)
        , m_offset(0.05f)
    {
        m_hitIndices.clear();
    }

    Entity2DIntersector(const osg::Vec3& start, const osg::Vec3& end)
        : osgUtil::LineSegmentIntersector(start, end)
        , m_offset(0.05f)
    {
        m_hitIndices.clear();
    }

    Entity2DIntersector(CoordinateFrame cf, double x, double y)
        : osgUtil::LineSegmentIntersector(cf, x, y)
        , m_offset(0.05f)
    {
        m_hitIndices.clear();
    }

    Entity2DIntersector(CoordinateFrame cf, const osg::Vec3d& start, const osg::Vec3d& end)
        : osgUtil::LineSegmentIntersector(cf, start, end)
        , m_offset(0.05f)
    {
        m_hitIndices.clear();
    }

    void setOffset(float offset)
    {
        m_offset = offset;
    }

    float getOffset() const
    {
        return m_offset;
    }

    void getHitIndices(int& first, int& last) const
    {
        if (m_hitIndices.empty()){
            first = -1;
            last = -1;
        }
        else {
            first = m_hitIndices.front();
            last = m_hitIndices.back();
        }
    }

    virtual Intersector* clone( osgUtil::IntersectionVisitor& iv )
    {
        if ( _coordinateFrame==MODEL && iv.getModelMatrix()==0 )
        {
            osg::ref_ptr<Entity2DIntersector> cloned = new Entity2DIntersector( _start, _end );
            cloned->_parent = this;
            cloned->m_offset = m_offset;
            return cloned.release();
        }

        osg::Matrix matrix;
        switch ( _coordinateFrame )
        {
        case WINDOW:
            if (iv.getWindowMatrix()) matrix.preMult( *iv.getWindowMatrix() );
            if (iv.getProjectionMatrix()) matrix.preMult( *iv.getProjectionMatrix() );
            if (iv.getViewMatrix()) matrix.preMult( *iv.getViewMatrix() );
            if (iv.getModelMatrix()) matrix.preMult( *iv.getModelMatrix() );
            break;
        case PROJECTION:
            if (iv.getProjectionMatrix()) matrix.preMult( *iv.getProjectionMatrix() );
            if (iv.getViewMatrix()) matrix.preMult( *iv.getViewMatrix() );
            if (iv.getModelMatrix()) matrix.preMult( *iv.getModelMatrix() );
            break;
        case VIEW:
            if (iv.getViewMatrix()) matrix.preMult( *iv.getViewMatrix() );
            if (iv.getModelMatrix()) matrix.preMult( *iv.getModelMatrix() );
            break;
        case MODEL:
            if (iv.getModelMatrix()) matrix = *iv.getModelMatrix();
            break;
        }

        osg::Matrix inverse = osg::Matrix::inverse(matrix);
        osg::ref_ptr<Entity2DIntersector> cloned = new Entity2DIntersector( _start*inverse, _end*inverse );
        cloned->_parent = this;
        cloned->m_offset = m_offset;
        return cloned.release();
    }

    virtual void intersect( osgUtil::IntersectionVisitor& iv, osg::Drawable* drawable )
    {
        osg::BoundingBox bb = drawable->getBoundingBox();
        bb.xMin() -= m_offset; bb.xMax() += m_offset;
        bb.yMin() -= m_offset; bb.yMax() += m_offset;
        bb.zMin() -= m_offset; bb.zMax() += m_offset;

        osg::Vec3d s(_start), e(_end);
        if (!intersectAndClip(s, e, bb)) return;
        if (iv.getDoDummyTraversal()) return;

        EntityType* geometry = dynamic_cast<EntityType*>(drawable->asGeometry());
        if (geometry)
        {
            osg::Vec3Array* vertices = dynamic_cast<osg::Vec3Array*>(geometry->getVertexArray());
            if (!vertices) return;

            for (unsigned int i=1; i<vertices->size(); ++i)
            {

                double distance = Utilities::getSkewLinesDistance(s,e,(*vertices)[i], (*vertices)[i-1]);

                if (m_offset<distance) continue;

                Intersection hit;
                hit.ratio = distance;
                hit.nodePath = iv.getNodePath();
                hit.drawable = drawable;
                hit.matrix = iv.getModelMatrix();
                hit.localIntersectionPoint = (*vertices)[i];
                m_hitIndices.push_back(i);
                insertIntersection(hit);
            }
        }
    }

protected:
    virtual ~Entity2DIntersector(){}

private:
    float m_offset;
    std::vector<unsigned int> m_hitIndices;
};

#endif // ENTITY2DINTERSECTOR_H
