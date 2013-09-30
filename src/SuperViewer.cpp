#include "SuperViewer.h"
//#include "RenderWindow.h"
#include <qapplication.h>
#include <openrave/plugin.h>
#include <openrave/config.h>
#include <QDockWidget>
#include <OgreCamera.h>
#include "Converters.h"
#include <rviz/display.h>
#include <rviz/display_wrapper.h>
#include <rviz/default_plugin/marker_display.h>
#include <rviz/default_plugin/markers/marker_base.h>
#include <rviz/default_plugin/markers/shape_marker.h>
#include <visualization_msgs/Marker.h>
#include <OgreSceneManager.h>
#include <qtimer.h>

using namespace OpenRAVE;
using namespace rviz;


OpenRAVE::InterfaceBasePtr CreateInterfaceValidated(OpenRAVE::InterfaceType type, const std::string& interfacename, std::istream& sinput, OpenRAVE::EnvironmentBasePtr penv)
{
    RAVELOG_INFO("Creating superviewer");

    if( type == OpenRAVE::PT_Viewer && interfacename == "superviewer" )
    {
        RAVELOG_INFO("success");
        return OpenRAVE::InterfaceBasePtr(new superviewer::SuperViewer(penv));
    }
    RAVELOG_INFO("Failure!\n");
    return OpenRAVE::InterfaceBasePtr();
}

void GetPluginAttributesValidated(OpenRAVE::PLUGININFO& info)
{
    info.interfacenames[  OpenRAVE::PT_Viewer].push_back("SuperViewer");
}

OPENRAVE_PLUGIN_API void DestroyPlugin()
{
    return;
}

namespace superviewer
{



    SuperViewer::SuperViewer(OpenRAVE::EnvironmentBasePtr env, QWidget * parent, Qt::WindowFlags flags) :
            QMainWindow(parent, flags),
            OpenRAVE::ViewerBase(env),
            m_rvizManager(NULL),
            m_mainRenderPanel(NULL),
            m_autoSync(false),
            m_name("Superviewer")
    {
        m_mainRenderPanel = new rviz::RenderPanel();
        m_rvizManager = new rviz::VisualizationManager(m_mainRenderPanel);
        setCentralWidget(m_mainRenderPanel);


        m_mainRenderPanel->initialize( m_rvizManager->getSceneManager(), m_rvizManager );
        m_rvizManager->initialize();
        m_rvizManager->startUpdate();
        setUpdatesEnabled(true);

        QTimer* timer = new QTimer(this);
        timer->setInterval(100);
        timer->setSingleShot(false);
        timer->start();

        connect(timer, SIGNAL(timeout()), this, SLOT(syncUpdate()));

    }


    SuperViewer::~SuperViewer()
    {

    }

    int SuperViewer::main(bool showWindow)
    {
        qApp->setActiveWindow(this);

        if(showWindow)
        {
            show();
        }

        return qApp->exec();
    }

    void SuperViewer::quitmainloop()
    {
        qApp->quit();
    }

    void SuperViewer::Reset()
    {

    }


    void SuperViewer::SetBkgndColor(const OpenRAVE::RaveVector<float> &color)
    {
        m_mainRenderPanel->setBackgroundColor(Ogre::ColourValue(color.x, color.y, color.z));
    }

    // registers a function with the viewer that gets called everytime mouse button is clicked
    OpenRAVE::UserDataPtr SuperViewer::RegisterItemSelectionCallback(const ItemSelectionCallbackFn &fncallback)
    {
        // TODO: Implement
        return OpenRAVE::UserDataPtr();
    }

    // registers a function with the viewer that gets called for every new image rendered.
    OpenRAVE::UserDataPtr SuperViewer::RegisterViewerImageCallback(const ViewerImageCallbackFn &fncallback)
    {
        //TODO: Implement
        return OpenRAVE::UserDataPtr();
    }


    // registers a function with the viewer that gets called in the viewer's GUI thread for every cycle the viewer refreshes at
    OpenRAVE::UserDataPtr SuperViewer::RegisterViewerThreadCallback(const ViewerThreadCallbackFn &fncallback)
    {
        //TODO: Implement
        return OpenRAVE::UserDataPtr();
    }

    // controls whether the viewer synchronizes with the newest environment automatically
    void SuperViewer::SetEnvironmentSync(bool update)
    {
        SetAutoSync(update);
    }


    // forces synchronization with the environment, returns when the environment is fully synchronized.
    void SuperViewer::EnvironmentSync()
    {

        RAVELOG_INFO("Syncing environment...\n");
        GetEnv()->GetMutex().lock();

        std::vector<OpenRAVE::KinBodyPtr> bodies;
        GetEnv()->GetBodies(bodies);
        rviz::DisplayWrapper* boxDisplay = m_rvizManager->createDisplay( "rviz/Marker", "box", true );

        if(!boxDisplay)
        {
            return;
        }

        rviz::Display* display = boxDisplay->getDisplay();
        rviz::MarkerDisplay* marker = dynamic_cast<rviz::MarkerDisplay*>(display);
        marker->setFixedFrame("world");

        for(size_t i = 0; i < bodies.size(); i++)
        {
            const OpenRAVE::KinBodyPtr& body = bodies.at(i);
            const std::vector<OpenRAVE::KinBody::LinkPtr>& links = body->GetLinks();

            for(size_t j = 0; j < links.size(); j++)
            {
                const OpenRAVE::KinBody::LinkPtr& link = links.at(i);
                const std::vector<OpenRAVE::KinBody::Link::GeometryPtr>& geoms = link->GetGeometries();

                for(size_t k = 0; k < geoms.size(); k++)
                {
                    const OpenRAVE::KinBody::Link::GeometryPtr& geometry = geoms.at(k);

                    switch(geometry->GetType())
                    {
                        case OpenRAVE::GT_Box:
                        case OpenRAVE::GT_Cylinder:
                        case OpenRAVE::GT_Sphere:
                        case OpenRAVE::GT_TriMesh:
                        default:
                        {

                            OpenRAVE::Transform ident;
                            ident.identity();
                            OpenRAVE::AABB aabb = geometry->ComputeAABB(ident);
                           if(isnan(aabb.extents.x))
                           {
                               continue;
                           }

                           RAVELOG_INFO("AABB extents are: %f %f %f\n", aabb.extents.x, aabb.extents.y, aabb.extents.z);


                            rviz::ShapeMarker* shapeMarker = new rviz::ShapeMarker(marker, m_rvizManager, m_rvizManager->getSceneManager()->getRootSceneNode());


                            visualization_msgs::Marker markerMsg;
                            OpenRAVE::RaveVector<double> translation = (link->GetTransform() * geometry->GetTransform()).trans;
                            OpenRAVE::RaveVector<double> rotation = (link->GetTransform() * geometry->GetTransform()).rot;
                            RAVELOG_INFO("Object translation are: %f %f %f\n", translation.x, translation.y, translation.z);
                            markerMsg.header.frame_id = "world";
                            markerMsg.header.stamp = ros::Time();
                            markerMsg.color.r = geometry->GetDiffuseColor().x;
                            markerMsg.color.g = geometry->GetDiffuseColor().y;
                            markerMsg.color.b = geometry->GetDiffuseColor().z;
                            markerMsg.color.a = 1.0f;
                            markerMsg.scale.x = aabb.extents.x;
                            markerMsg.scale.y = aabb.extents.y;
                            markerMsg.scale.z = aabb.extents.z;
                            markerMsg.action = visualization_msgs::Marker::ADD;
                            markerMsg.pose.position.x = translation.x;
                            markerMsg.pose.position.y = translation.y;
                            markerMsg.pose.position.z = translation.z;
                            markerMsg.pose.orientation.x = rotation.x;
                            markerMsg.pose.orientation.y = rotation.y;
                            markerMsg.pose.orientation.z = rotation.z;
                            markerMsg.pose.orientation.w = rotation.w;


                            markerMsg.type = visualization_msgs::Marker::CUBE;
                            shapeMarker->setMessage(markerMsg);

                            break;
                        }
                    }
                    break;
                }
                break;
            }
            break;
        }
        GetEnv()->GetMutex().unlock();
    }

    // Viewer size and position can be set outside in the
    // OpenRAVE API
    void SuperViewer::SetSize (int w, int h)
    {
        resize(w, h);
    }

    void SuperViewer::Move (int x, int y)
    {
        move(x, y);
    }

    // Name is set by OpenRAVE outside
    void SuperViewer::SetName (const std::string &name)
    {
        m_name = name;
    }

    const std::string & SuperViewer::GetName () const
    {
        return m_name;
    }

    // Keeps camera transform consistent
    void  SuperViewer::UpdateCameraTransform()
    {
        //TODO: Implement
    }

    // Set the camera transformation.
    void SuperViewer::SetCamera (const OpenRAVE::RaveTransform<float> &trans, float focalDistance)
    {
        Ogre::Camera* camera = m_mainRenderPanel->getCamera();
        camera->setPosition(converters::ToOgreVector(trans.trans));
        camera->setOrientation(converters::ToOgreQuaternion(trans.rot));
        camera->setFocalLength(std::max<float>(focalDistance, 0.01f));

        RAVELOG_INFO("Setting camera parameters: %f\n", focalDistance);
    }

    // Return the current camera transform that the viewer is rendering the environment at.
    OpenRAVE::RaveTransform<float>  SuperViewer::GetCameraTransform() const
    {
        OpenRAVE::RaveTransform<float> toReturn;
        Ogre::Camera* camera = m_mainRenderPanel->getCamera();
        toReturn.trans = converters::ToRaveVector(camera->getPosition());
        toReturn.rot = converters::ToRaveQuaternion(camera->getOrientation());
        return toReturn;
    }

    // Return the closest camera intrinsics that the viewer is rendering the environment at.
    OpenRAVE::geometry::RaveCameraIntrinsics<float> SuperViewer::GetCameraIntrinsics()
    {
        OpenRAVE::geometry::RaveCameraIntrinsics<float> toReturn;
        Ogre::Camera* camera = m_mainRenderPanel->getCamera();
        Ogre::Matrix4 projectionMatrix = camera->getProjectionMatrix();
        toReturn.focal_length = camera->getFocalLength();
        toReturn.fx = projectionMatrix[0][0];
        toReturn.fy = projectionMatrix[1][1];
        toReturn.cx = projectionMatrix[0][2];
        toReturn.cy = projectionMatrix[1][2];
        toReturn.distortion_model = "";
        return toReturn;
    }

    // Renders a 24bit RGB image of dimensions width and height from the current scene.
    bool SuperViewer::GetCameraImage(std::vector<uint8_t> &memory, int width, int height, const OpenRAVE::RaveTransform<float> &t, const OpenRAVE::SensorBase::CameraIntrinsics &intrinsics)
    {
        //TODO: Implement
        return false;
    }


    // Overloading OPENRAVE drawing functions....
    OpenRAVE::GraphHandlePtr SuperViewer::plot3 (const float *ppoints, int numPoints, int stride, float fPointSize, const OpenRAVE::RaveVector< float > &color, int drawstyle)
    {
        //TODO: Implement
        return OpenRAVE::GraphHandlePtr();
    }

    OpenRAVE::GraphHandlePtr SuperViewer::plot3 (const float *ppoints, int numPoints, int stride, float fPointSize, const float *colors, int drawstyle, bool bhasalpha)
    {
        //TODO: Implement
        return OpenRAVE::GraphHandlePtr();
    }

    OpenRAVE::GraphHandlePtr SuperViewer::drawlinestrip (const float *ppoints, int numPoints, int stride, float fwidth, const OpenRAVE::RaveVector< float > &color)
    {
        //TODO: Implement
        return OpenRAVE::GraphHandlePtr();
    }

    OpenRAVE::GraphHandlePtr SuperViewer::drawlinestrip (const float *ppoints, int numPoints, int stride, float fwidth, const float *colors)
    {
        //TODO: Implement
        return OpenRAVE::GraphHandlePtr();
    }

    OpenRAVE::GraphHandlePtr SuperViewer::drawlinelist (const float *ppoints, int numPoints, int stride, float fwidth, const OpenRAVE::RaveVector< float > &color)
    {
        //TODO: Implement
        return OpenRAVE::GraphHandlePtr();
    }

    OpenRAVE::GraphHandlePtr SuperViewer::drawlinelist (const float *ppoints, int numPoints, int stride, float fwidth, const float *colors)
    {
        //TODO: Implement
        return OpenRAVE::GraphHandlePtr();
    }

    OpenRAVE::GraphHandlePtr SuperViewer::drawarrow (const OpenRAVE::RaveVector< float > &p1, const OpenRAVE::RaveVector< float > &p2, float fwidth, const OpenRAVE::RaveVector< float > &color)
    {
        //TODO: Implement
        return OpenRAVE::GraphHandlePtr();
    }

    OpenRAVE::GraphHandlePtr SuperViewer::drawbox (const OpenRAVE::RaveVector< float > &vpos, const OpenRAVE::RaveVector< float > &vextents)
    {
        //TODO: Implement
        return OpenRAVE::GraphHandlePtr();
    }

    OpenRAVE::GraphHandlePtr SuperViewer::drawplane (const OpenRAVE::RaveTransform< float > &tplane, const OpenRAVE::RaveVector< float > &vextents, const boost::multi_array< float, 3 > &vtexture)
    {
        //TODO: Implement
        return OpenRAVE::GraphHandlePtr();
    }

    OpenRAVE::GraphHandlePtr SuperViewer::drawtrimesh (const float *ppoints, int stride, const int *pIndices, int numTriangles, const OpenRAVE::RaveVector< float > &color)
    {
        //TODO: Implement
        return OpenRAVE::GraphHandlePtr();
    }

    OpenRAVE::GraphHandlePtr SuperViewer::drawtrimesh (const float *ppoints, int stride, const int *pIndices, int numTriangles, const boost::multi_array< float, 2 > &colors)
    {
        //TODO: Implement
        return OpenRAVE::GraphHandlePtr();
    }

    void SuperViewer::RemoveKinBody(OpenRAVE::KinBodyPtr kinBody)
    {
        //TODO: Implement
        return;
    }

    void SuperViewer::syncUpdate()
    {
        EnvironmentSync();
    }
}
