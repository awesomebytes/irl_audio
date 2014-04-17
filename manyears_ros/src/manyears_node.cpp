#include <manyears_ros/manyears_node.hpp>
#include "manyears_config.hpp"

using namespace manyears_ros;

ManyEarsNode::ManyEarsNode(ros::NodeHandle& n, ros::NodeHandle& np)
{
    manyears_context_ = createEmptyOverallContext();
    ParametersLoadDefault(manyears_context_.myParameters);

    if (!parseParams(np)) {
        ROS_ERROR("Could not parse ManyEars parameters correctly, the node "
                  "will not be initialized.");
        return;
    }

    ROS_INFO("Initializing ManyEars with %lu microphones...",
             mic_defs_.size());

    initPipeline();

    sub_audio_ = n.subscribe("audio_stream", 10, &ManyEarsNode::audioCB, this);
}

void ManyEarsNode::audioCB(const rt_audio_ros::AudioStream::ConstPtr& msg)
{
}

bool ManyEarsNode::parseParams(const ros::NodeHandle& np)
{
    if (!np.hasParam("config_file")) {
        ROS_ERROR("No 'config_file' parameter found.");
        return false;
    } else {
        std::string config_fn;
        np.getParam("config_file", config_fn);
        if (!manyears_ros::parseConfigFile(manyears_context_, config_fn)) {
            return false;
        }
    }

    if (!np.hasParam("microphones")) {
        ROS_ERROR("No 'microphones' parameter - cannot define array geometry.");
        return false;
    }

    using namespace XmlRpc;
    XmlRpcValue mic_n;
    np.getParam("microphones", mic_n);
    if (mic_n.getType() != XmlRpcValue::TypeArray) {
        ROS_ERROR("'microphones' is not an array.");
        return false;
    }

    for (int i = 0; i < mic_n.size(); ++i) {
        MicDef md;

        XmlRpcValue& v = mic_n[i];
        if (v.getType() != XmlRpcValue::TypeStruct) {
            ROS_ERROR("'microphones' element %i is not a struct, skipping",
                      i);
            continue;
        }

        if (!v.hasMember("gain")) {
            ROS_ERROR("No gain specified for microphone %i, using '1.0'",
                      i);
        } else {
            XmlRpcValue& gv = v["gain"];
            if (gv.getType() != XmlRpcValue::TypeDouble) {
                ROS_ERROR("Gain element of microphone %i is not a double, "
                          "using '1.0'",
                          i);
            } else {
                md.gain = gv;
            }
        }

        if (!v.hasMember("pos")) {
            ROS_ERROR("No pos specified for microphone %i, using '(0, 0, 0)'",
                      i);
        } else {
            XmlRpcValue& posv = v["pos"];
            if (posv.getType() != XmlRpcValue::TypeStruct) {
                ROS_ERROR("Pos element of microphone %i is not a struct, "
                          "using (0, 0, 0).");
            } else {
                if (posv.hasMember("x")) {
                    XmlRpcValue& xv = posv["x"];
                    if (xv.getType() == XmlRpcValue::TypeDouble) {
                        md.pos[0] = xv;
                    } else {
                        ROS_WARN("'x' element of microphone %i is not a "
                                 "double, using 0.0.",
                                 i);
                    }
                }
                if (posv.hasMember("y")) {
                    XmlRpcValue& yv = posv["y"];
                    if (yv.getType() == XmlRpcValue::TypeDouble) {
                        md.pos[1] = yv;
                    } else {
                        ROS_WARN("'y' element of microphone %i is not a "
                                 "double, using 0.0.",
                                 i);
                    }
                }
                if (posv.hasMember("z")) {
                    XmlRpcValue& zv = posv["z"];
                    if (zv.getType() == XmlRpcValue::TypeDouble) {
                        md.pos[2] = zv;
                    } else {
                        ROS_WARN("'z' element of microphone %i is not a "
                                 "double, using 0.0.",
                                 i);
                    }
                }
            }
        }

        mic_defs_.push_back(md);
    }

    if (mic_defs_.size() < 2) {
        ROS_ERROR("Less than two microphones were defined.");
        return false;
    }

    np.param("instant_time", instant_time_, true);

    return true;
}

void ManyEarsNode::initPipeline()
{
    microphonesInit(manyears_context_.myMicrophones, mic_defs_.size());
    for (int i = 0; i < mic_defs_.size(); ++i) {
        const MicDef& md = mic_defs_[i];
        microphonesAdd(manyears_context_.myMicrophones,
                      i,
                      md.pos[0],
                      md.pos[1],
                      md.pos[2],
                      md.gain);
    }

    preprocessorInit(       manyears_context_.myPreprocessor,
                            manyears_context_.myParameters,
                            manyears_context_.myMicrophones);
    beamformerInit(         manyears_context_.myBeamformer,
                            manyears_context_.myParameters,
                            manyears_context_.myMicrophones);
    mixtureInit(            manyears_context_.myMixture,
                            manyears_context_.myParameters);
    gssInit(                manyears_context_.myGSS,
                            manyears_context_.myParameters,
                            manyears_context_.myMicrophones);
    postfilterInit(         manyears_context_.myPostfilter,
                            manyears_context_.myParameters);
    postprocessorInit(      manyears_context_.myPostprocessorSeparated,
                            manyears_context_.myParameters);
    postprocessorInit(      manyears_context_.myPostprocessorPostfiltered,
                            manyears_context_.myParameters);
    potentialSourcesInit(   manyears_context_.myPotentialSources,
                            manyears_context_.myParameters);
    trackedSourcesInit(     manyears_context_.myTrackedSources,
                            manyears_context_.myParameters);
    separatedSourcesInit(   manyears_context_.mySeparatedSources,
                            manyears_context_.myParameters);
    postfilteredSourcesInit(manyears_context_.myPostfilteredSources,
                            manyears_context_.myParameters);

}

