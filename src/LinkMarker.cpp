#include <boost/bind.hpp>
#include <boost/format.hpp>
#include <boost/make_shared.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/range/adaptor/map.hpp>
#include <ros/ros.h>
#include "or_conversions.h"
#include "LinkMarker.h"

using boost::adaptors::transformed;
using boost::algorithm::join;
using boost::format;
using boost::str;
using geometry_msgs::Vector3;
using OpenRAVE::KinBodyPtr;
using OpenRAVE::RobotBase;
using OpenRAVE::RobotBasePtr;
using OpenRAVE::EnvironmentBasePtr;
using visualization_msgs::Marker;
using visualization_msgs::MarkerPtr;
using visualization_msgs::InteractiveMarker;
using visualization_msgs::InteractiveMarkerPtr;
using visualization_msgs::InteractiveMarkerControl;
using visualization_msgs::InteractiveMarkerFeedbackConstPtr;
using interactive_markers::InteractiveMarkerServer;

typedef OpenRAVE::KinBody::LinkPtr LinkPtr;
typedef OpenRAVE::RobotBase::ManipulatorPtr ManipulatorPtr;
typedef OpenRAVE::KinBody::Link::GeometryPtr GeometryPtr;
typedef boost::shared_ptr<InteractiveMarkerServer> InteractiveMarkerServerPtr;

// TODO: Don't hardcode this.
static std::string const kWorldFrameId = "/world";

namespace or_interactivemarker {

OpenRAVE::Vector const LinkMarker::kGhostColor(0, 1, 0, 0.2);

LinkMarker::LinkMarker(boost::shared_ptr<InteractiveMarkerServer> server,
                       LinkPtr link, bool is_ghost)
    : server_(server)
    , link_(link)
    , is_ghost_(is_ghost)
    , interactive_marker_(boost::make_shared<InteractiveMarker>())
    , render_mode_(RenderMode::kVisual)
{
    BOOST_ASSERT(server);
    BOOST_ASSERT(link);

    // TODO: How should we handle this?
    //manipulator_ = InferManipulator();

    interactive_marker_->header.frame_id = kWorldFrameId;
    interactive_marker_->name = id();
    interactive_marker_->description = "";
    interactive_marker_->pose = toROSPose(link->GetTransform());
    interactive_marker_->scale = 0.25;

    // Show the visual geometry.
    interactive_marker_->controls.resize(1);
    visual_control_ = &interactive_marker_->controls[0];
    visual_control_->orientation.w = 1;
    visual_control_->name = str(format("%s.Geometry[visual]") % id());
    visual_control_->orientation_mode = InteractiveMarkerControl::INHERIT;
    visual_control_->interaction_mode = InteractiveMarkerControl::BUTTON;
    visual_control_->always_visible = true;
}

LinkMarker::~LinkMarker()
{
    server_->erase(interactive_marker_->name);
}

std::string LinkMarker::id() const
{
    LinkPtr const link = this->link();
    KinBodyPtr const body = link->GetParent();
    EnvironmentBasePtr const env = body->GetEnv();
    int const environment_id = OpenRAVE::RaveGetEnvironmentId(env);

    return str(format("Environment[%d].KinBody[%s].Link[%s]")
               % environment_id % body->GetName() % link->GetName());
}

LinkPtr LinkMarker::link() const
{
    return link_.lock();
}

InteractiveMarkerPtr LinkMarker::interactive_marker()
{
    return interactive_marker_;
}

void LinkMarker::EnvironmentSync()
{
    LinkPtr const link = this->link();
    bool is_changed = false;

    // Check if we need to re-create the marker to propagate changes in the
    // OpenRAVE environment.
    for (GeometryPtr const &geometry : link->GetGeometries()) {
        // Check if visibility changed.
        auto const it = geometry_markers_.find(geometry.get());
        bool const is_missing = it == geometry_markers_.end();
        bool const is_visible = geometry->IsVisible();
        is_changed = is_changed || (is_visible == is_missing);

        // TODO  Check if color changed.
        // TODO: Check if the transform changed.
        // TODO: Check if the geometry changed.

        if (is_changed) {
            break;
        }
    }

    if (is_changed) {
        CreateGeometry();
        server_->insert(*interactive_marker_);
    }
    // Incrementally update the marker's pose. We can't do this if we just
    // created the markers because the InteraciveMarkerServer will SEGFAULT.
    else {
        OpenRAVE::Transform const link_pose = link->GetTransform();
        server_->setPose(interactive_marker_->name, toROSPose(link_pose));
    }
}

void LinkMarker::CreateGeometry()
{
    visual_control_->markers.clear();
    geometry_markers_.clear();

    LinkPtr const link = this->link();

    for (GeometryPtr const geometry : link->GetGeometries()) {
        if (!geometry->IsVisible()) {
            continue;
        }

        MarkerPtr new_marker = CreateGeometry(geometry);
        if (new_marker) {
            visual_control_->markers.push_back(*new_marker);
            geometry_markers_[geometry.get()] = &visual_control_->markers.back();
        }
        // This geometry is empty. Insert a dummy marker to simplify the
        // change-detection logic.
        else {
            geometry_markers_[geometry.get()] = NULL;
        }
    }
}
void LinkMarker::SetRenderMode(RenderMode::Type mode)
{
    render_mode_ = mode;
}

MarkerPtr LinkMarker::CreateGeometry(GeometryPtr geometry)
{
    MarkerPtr marker = boost::make_shared<Marker>();
    marker->pose = toROSPose(geometry->GetTransform());

    if (is_ghost_) {
        marker->color = toROSColor(kGhostColor);
    } else {
        marker->color = toROSColor(geometry->GetDiffuseColor());
        marker->color.a = 1.0 - geometry->GetTransparency();
    }

    // If a render filename is specified, then we should ignore the rest of the
    // geometry. This is true regardless of the mesh type.
    std::string render_mesh_path = geometry->GetRenderFilename();
    if (boost::algorithm::starts_with(render_mesh_path, "__norenderif__")) {
        render_mesh_path = "";
    }

    if (!render_mesh_path.empty()) {
        marker->type = Marker::MESH_RESOURCE;
        marker->scale = toROSVector(geometry->GetRenderScale());
        marker->mesh_resource = "file://" + render_mesh_path;
        marker->mesh_use_embedded_materials = !is_ghost_;
        return marker;
    }

    // Otherwise, we have to render the underlying geometry type.
    switch (geometry->GetType()) {
    case OpenRAVE::GeometryType::GT_None:
        return MarkerPtr();

    case OpenRAVE::GeometryType::GT_Box:
        // TODO: This may be off by a factor of two.
        marker->type = Marker::CUBE;
        marker->scale = toROSVector(geometry->GetBoxExtents());
        break;

    case OpenRAVE::GeometryType::GT_Sphere: {
        double const sphere_radius = geometry->GetSphereRadius();
        marker->type = Marker::SPHERE;
        marker->scale.x = sphere_radius;
        marker->scale.y = sphere_radius;
        marker->scale.z = sphere_radius;
        break;
    }

    case OpenRAVE::GeometryType::GT_Cylinder: {
        // TODO: This may be rotated and/or off by a factor of two.
        double const cylinder_radius = geometry->GetCylinderRadius();
        double const cylinder_height= geometry->GetCylinderHeight();
        marker->type = Marker::CYLINDER;
        marker->scale.x = cylinder_radius;
        marker->scale.y = cylinder_radius;
        marker->scale.z = cylinder_height;
        break;
    }

    case OpenRAVE::GeometryType::GT_TriMesh:
        // TODO: Fall back on the OpenRAVE's mesh loader if this format is not
        // supported by RViz.
        return MarkerPtr();
        break;

    default:
        RAVELOG_WARN("Unknown geometry type '%d'.\n", geometry->GetType());
        return MarkerPtr();
    }
    return marker;
}

ManipulatorPtr LinkMarker::InferManipulator()
{
    LinkPtr const link = this->link();
    std::vector<ManipulatorPtr> manipulators;

    // TODO: What if this link is part of multiple manipulators?
    KinBodyPtr const kinbody = link->GetParent();
    RobotBasePtr const robot = boost::dynamic_pointer_cast<RobotBase>(kinbody);
    if (!robot) {
        return ManipulatorPtr();
    }

    for (ManipulatorPtr const manipulator : robot->GetManipulators()) {
        LinkPtr const base_link = manipulator->GetBase();

        // Check if this link is a child of the manipulator (i.e. gripper).
        std::vector<LinkPtr> child_links;
        manipulator->GetChildLinks(child_links);
        auto const it = std::find(child_links.begin(), child_links.end(), link);
        if (it != child_links.end()) {
            manipulators.push_back(manipulator);
            continue;
        }

        // Check if this link is in the manipulator chain by searching from
        // leaf to root.
        LinkPtr curr_link = manipulator->GetEndEffector();
        RAVELOG_INFO("Searching for parent '%s' of '%s'\n",
            curr_link->GetName().c_str(),
            base_link->GetName().c_str()
        );

        while (curr_link != base_link) {
            if (curr_link == link) {
                manipulators.push_back(manipulator);
                break;
            }

            std::vector<LinkPtr> parent_links;
            curr_link->GetParentLinks(parent_links);
            BOOST_ASSERT(parent_links.size() == 1);
            curr_link = parent_links.front();
        }
    }


    if (manipulators.empty()) {
        return ManipulatorPtr();
    } else if (manipulators.size() == 1) {
        return manipulators.front();
    } else {
        std::stringstream manipulator_names;
        for (ManipulatorPtr const manipulator : manipulators) {
            manipulator_names << " " << manipulator->GetName();
        }

        RAVELOG_WARN("Link '%s' is a member of %d manipulators [%s ]"
                     " [ %s ]. It will only be associated with manipulator %s"
                     " in the viewer.\n",
            link->GetName().c_str(),
            manipulators.size(),
            manipulator_names.str().c_str(),
            manipulators.front()->GetName().c_str()
        );
        return manipulators.front();
    }
}

}
