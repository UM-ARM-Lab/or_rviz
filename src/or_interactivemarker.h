#ifndef ORINTERACTIVEMARKER_H_
#define ORINTERACTIVEMARKER_H_
#include <boost/unordered_map.hpp>
#include <openrave/openrave.h>
#include <interactive_markers/interactive_marker_server.h>
#include "KinBodyMarker.h"

namespace or_interactivemarker {

class InteractiveMarkerViewer : public OpenRAVE::ViewerBase {
public:
    InteractiveMarkerViewer(OpenRAVE::EnvironmentBasePtr env);

    virtual void EnvironmentSync();

    virtual int main(bool bShow = true);
    virtual void quitmainloop();

    virtual OpenRAVE::UserDataPtr RegisterViewerThreadCallback(
        OpenRAVE::ViewerBase::ViewerThreadCallbackFn const &fncallback);

private:
    OpenRAVE::EnvironmentBasePtr env_;
    boost::shared_ptr<interactive_markers::InteractiveMarkerServer> server_;

    bool running_;
};

}

#endif
