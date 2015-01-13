#include <boost/format.hpp>
#include <openrave/openrave.h>
#include <openrave/geometry.h>
#include <interactive_markers/interactive_marker_server.h>
#include "markers/JointMarker.h"
#include "util/or_conversions.h"

using boost::format;
using boost::str;
using interactive_markers::InteractiveMarkerServer;
using visualization_msgs::InteractiveMarkerControl;
using visualization_msgs::InteractiveMarkerFeedback;
using visualization_msgs::InteractiveMarkerFeedbackConstPtr;
using OpenRAVE::dReal;
using OpenRAVE::KinBody;
using OpenRAVE::KinBodyPtr;

using namespace or_interactivemarker::util;

typedef boost::shared_ptr<InteractiveMarkerServer> InteractiveMarkerServerPtr;
typedef OpenRAVE::KinBody::JointPtr JointPtr;

namespace or_interactivemarker {
namespace markers {

JointMarker::JointMarker(InteractiveMarkerServerPtr server, JointPtr joint)
    : server_(server)
    , joint_(joint)
    , joint_pose_(GetJointPose(joint))
    , joint_initial_(joint->GetValue(0))
    , joint_delta_(0)
    , created_(false)
    , force_update_(true)
    , active_(false)
{
    BOOST_ASSERT(joint);

    // TODO: Support more types of joints.
    if (joint->GetType() != OpenRAVE::KinBody::JointRevolute) {
        return;
    }
    
    // Ignore static and mimic joints since we can't directly control them.
    if (joint->IsStatic()) {
        return;
    } else if (joint->IsMimic()) {
        return;
    }

    marker_.header.frame_id = kDefaultWorldFrameId;
    marker_.name = id();
    marker_.description = "";
    marker_.pose = toROSPose(pose());
    // TODO: Infer a good scale for the control.
    marker_.scale = 0.25;

    InteractiveMarkerControl control;
    // TODO: Why isn't this rotation about the x-axis?
    control.orientation.w = 1;
    control.orientation.x = 0;
    control.orientation.y = 1;
    control.orientation.z = 0;
    control.name = "rotate";
    control.interaction_mode = InteractiveMarkerControl::ROTATE_AXIS;
    marker_.controls.push_back(control);

    server_->insert(marker_);
    server_->setCallback(marker_.name,
        boost::bind(&JointMarker::JointCallback, this, _1));
}

JointMarker::~JointMarker()
{
    server_->erase(marker_.name);
}

std::string JointMarker::id() const
{
    JointPtr const joint = this->joint();
    OpenRAVE::KinBodyPtr const body = joint->GetParent();
    OpenRAVE::EnvironmentBasePtr const env = body->GetEnv();
    int const environment_id = OpenRAVE::RaveGetEnvironmentId(env);

    return str(format("Environment[%d].KinBody[%s].Joint[%s]")
               % environment_id % body->GetName() % joint->GetName());
}

JointPtr JointMarker::joint() const
{
    return joint_.lock();
}

OpenRAVE::Transform JointMarker::pose() const
{
    return joint_pose_;
}

void JointMarker::set_pose(OpenRAVE::Transform const &pose)
{
    if (!active_) {
        joint_pose_ = pose;
    }
}

double JointMarker::angle() const
{
    return joint_initial_ + joint_delta_;
}

void JointMarker::set_parent_frame(std::string const &frame_id)
{
    marker_.header.frame_id = frame_id;
    force_update_ = true;
}

bool JointMarker::EnvironmentSync()
{
    // Re-create the marker, if necessary.
    if (force_update_) {
        server_->insert(marker_);
        force_update_ = false;
    }

    // Update pose.
    server_->setPose(marker_.name, toROSPose(pose()));

    if (!active_) {
        joint_initial_ = joint()->GetValue(0);
        joint_delta_ = 0;
    }

    created_ = true;
    return false;
}

void JointMarker::JointCallback(InteractiveMarkerFeedbackConstPtr const &feedback)
{
    if (feedback->event_type == InteractiveMarkerFeedback::MOUSE_DOWN) {
        active_ = true;
    } else if (feedback->event_type == InteractiveMarkerFeedback::MOUSE_UP) {
        active_ = false;
    } else if (feedback->event_type == InteractiveMarkerFeedback::POSE_UPDATE) {
        // Get the pose of the handle relative to the current joint position.
        // TODO: Why is this a rotation about the z-axis? It should be the y-axis.
        OpenRAVE::Transform const pose = this->pose().inverse() * toORPose<dReal>(feedback->pose);
        OpenRAVE::Vector const axis_angle = OpenRAVE::geometry::axisAngleFromQuat(pose.rot);

        // TODO: Why is this negated?
        joint_delta_ = -axis_angle[2];
    }
}

OpenRAVE::Transform JointMarker::GetJointPose(JointPtr joint)
{
    OpenRAVE::Transform pose = OpenRAVE::geometry::transformLookat(
        OpenRAVE::Vector(0, 0, 0), joint->GetAxis(), OpenRAVE::Vector(1, 0, 0));
    pose.trans = joint->GetAnchor();
    return pose;
}

}
}
