#ifndef MANA_PP_UTILITIES_HPP
#define MANA_PP_UTILITIES_HPP

#include <osg/Polytope>
#include <osg/Geometry>
#include <osg/Texture1D>
#include <osg/Texture2D>
#include <osg/Texture3D>
#include <osg/TextureCubeMap>
#include <osg/Camera>
#include "Global.h"
struct SMikkTSpaceContext;

namespace osgVerse
{
    /** Create 2D noises. e.g. for SSAO use */
    extern osg::Texture* generateNoises2D(int numRows, int numCols);

    /** Create poisson noises. e.g. for PCF shadow use */
    extern osg::Texture* generatePoissonDiscDistribution(int numCols, int numRows = 1);

    /** Create default texture for untextured model */
    extern osg::Texture2D* createDefaultTexture(const osg::Vec4& color);

    /** Create 2D texture from an input image */
    extern osg::Texture2D* createTexture2D(osg::Image* img, osg::Texture::WrapMode m = osg::Texture::REPEAT);

    /** Create a XOY quad, often for screen-rendering use */
    extern osg::Geode* createScreenQuad(const osg::Vec3& corner, float width, float height,
                                        const osg::Vec4& uvRange);

    /** Create a standard pre-render RTT camera, may contain a quad for deferred use */
    extern osg::Camera* createRTTCamera(osg::Camera::BufferComponent buffer, osg::Texture* tex,
                                        osg::GraphicsContext* gc, bool screenSpaced);

    /** Create a list of RTT cameras to render a cubemap */
    extern osg::Group* createRTTCube(osg::Camera::BufferComponent buffer, osg::TextureCubeMap* tex,
                                     osg::Node* child, osg::GraphicsContext* gc);

    /** Create a standard post-render HUD camera, may contain a quad for display use */
    extern osg::Camera* createHUDCamera(osg::GraphicsContext* gc, int w, int h, const osg::Vec3& quadPt,
        float quadW, float quadH, bool screenSpaced);

    /** The tangent/binormal computing visitor */
    class TangentSpaceVisitor : public osg::NodeVisitor
    {
    public:
        TangentSpaceVisitor(const float angularThreshold = 180.0f);
        virtual ~TangentSpaceVisitor();
        virtual void apply(osg::Drawable& node) {}  // do nothing
        virtual void apply(osg::Geometry& geometry) {}  // do nothing
        virtual void apply(osg::Geode& node);

    protected:
        SMikkTSpaceContext* _mikkiTSpace;
        float _angularThreshold;
    };

    /** The normal-map & specular-map generator */
    class NormalMapGenerator : public osg::NodeVisitor
    {
    public:
        NormalMapGenerator(double nStrength = 2.0, double spScale = 0.2,
                           double spContrast = 1.0, bool nInvert = false);
        void setTextureUnits(int n, int sp) { _normalMapUnit = n; _specMapUnit = sp; }
        void setCacheFolder(const std::string& folder) { _cacheFolder = folder; }

        virtual void apply(osg::Drawable& node) {}  // do nothing
        virtual void apply(osg::Geometry& geometry) {}  // do nothing
        virtual void apply(osg::Node& node);
        virtual void apply(osg::Geode& node);
        void apply(osg::StateSet& ss);

    protected:
        std::string _cacheFolder;
        double _nStrength, _spScale, _spContrast;
        int _normalMapUnit, _specMapUnit;
        bool _nInvert;
    };

    /** The frustum geometry which is used by shadow computation */
    struct Frustum
    {
        osg::Vec3d corners[8];
        osg::Vec3d centerNearPlane, centerFarPlane;
        osg::Vec3d center, frustumDir;

        /** Create frustum polytope from given MVP matrices, may change near/far if needed */
        void create(const osg::Matrix& modelview, const osg::Matrix& proj,
                    double preferredNear = -1.0, double preferredFar = -1.0);

        /** Compute minimum light-space bounding box from given frustum and some reference points.
            Result in light-space can be used for setting shadow camera pojection matrix */
        osg::BoundingBoxd createShadowBound(const std::vector<osg::Vec3d>& refPoints,
                                            const osg::Matrix& worldToLocal);
    };

    class DisableBoundingBoxCallback : public osg::Drawable::ComputeBoundingBoxCallback
    {
    public:
        virtual osg::BoundingBox computeBound(const osg::Drawable&) const
        { return osg::BoundingBox(); }
    };

    class DisableDrawableCallback : public osg::Drawable::CullCallback
    {
    public:
        virtual bool cull(osg::NodeVisitor*, osg::Drawable* drawable, osg::State*) const
        { return true; }
    };

    class DisableNodeCallback : public osg::NodeCallback
    {
    public:
        virtual void operator()(osg::Node* node, osg::NodeVisitor* nv)
        { /*traverse(node, nv);*/ }
    };
}

#if OSG_LIBRARY_STATIC
#   define USE_OSG_PLUGINS() \
    USE_OSGPLUGIN(glsl) \
    USE_OSGPLUGIN(trans) \
    USE_OSGPLUGIN(rot) \
    USE_OSGPLUGIN(scale) \
    USE_OSGPLUGIN(osg) \
    USE_OSGPLUGIN(osg2) \
    USE_OSGPLUGIN(rgb) \
    USE_OSGPLUGIN(bmp) \
    USE_DOTOSGWRAPPER_LIBRARY(osg) \
    USE_DOTOSGWRAPPER_LIBRARY(osgSim) \
    USE_DOTOSGWRAPPER_LIBRARY(osgTerrain) \
    USE_DOTOSGWRAPPER_LIBRARY(osgText) \
    USE_DOTOSGWRAPPER_LIBRARY(osgViewer) \
    USE_SERIALIZER_WRAPPER_LIBRARY(osg) \
    USE_SERIALIZER_WRAPPER_LIBRARY(osgSim) \
    USE_SERIALIZER_WRAPPER_LIBRARY(osgTerrain) \
    USE_SERIALIZER_WRAPPER_LIBRARY(osgText) \
    USE_GRAPHICSWINDOW()
// Note: plugins depending on external libraries should be called manually
//  USE_OSGPLUGIN(jpg)
//  USE_OSGPLUGIN(png)
//  USE_OSGPLUGIN(tiff)
//  USE_OSGPLUGIN(freetype)
#else
#   define USE_OSG_PLUGINS()
#endif

#if VERSE_STATIC_BUILD
#   define USE_VERSE_PLUGINS() \
    USE_OSGPLUGIN(verse_ept) \
    USE_OSGPLUGIN(verse_fbx) \
    USE_OSGPLUGIN(verse_gltf) \
    USE_OSGPLUGIN(verse_ktx) \
    USE_OSGPLUGIN(verse_web) \
    USE_OSGPLUGIN(verse_leveldb) \
    USE_OSGPLUGIN(pbrlayout)
// Note: plugins depending on external libraries should be called manually
//  USE_OSGPLUGIN(verse_ms)
//  USE_OSGPLUGIN(verse_cesium)
//  USE_OSGPLUGIN(verse_vdb)
#else
#   define USE_VERSE_PLUGINS()
#endif

#endif
