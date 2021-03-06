/**
 * @file multi_prefixed_tf_broadcaster_node.cpp
 */

/*
 * 0. Set parameter for URDFs in order of rviz_tiny_plugins/MultiRobotStateDisplay
 *    - 'mptf_datasize' Default:7
 *    - 'mptf_[num]/robot_description' EX: "0/robot_description" If not set, use 'robot_description'
 * 1. Subscribe rviz_tiny_plugins/MultiRobotStateDisplay
 * 2. Broadcast a set of tf, where prefix is added to each tf for a robot
 * 3. Set "RobotModel" rviz plugins for each robot in RViz
 * 4. You will get multi robot states to be shown
 */

#include "tiny_rviz_plugins/fixed_state_broadcaster.h"
#include "tiny_rviz_plugins/MultiRobotStateDisplay.h"
#include <kdl_parser/kdl_parser.hpp>

using namespace std;
using namespace fixed_state_publisher;

typedef std::map<std::string, urdf::JointMimicSharedPtr> MimicMap;

class MultiPrefixedTFBroadcaster
{
private:
  void callback(const tiny_rviz_plugins::MultiRobotStateDisplay::ConstPtr& msg);

  ros::NodeHandle nh_;
  unsigned int data_size_;
  std::vector<urdf::Model> models_;
  std::vector<KDL::Tree> trees_;
  std::vector<MimicMap> mimicmaps_;
  std::vector<FixedStatePublisherPtr> pub_ptrs_;

  tf2_ros::StaticTransformBroadcaster st_tf2_broadcaster_;

public:
  /// 
  MultiPrefixedTFBroadcaster(int data_size = 7);
  //virtual ~MultiPrefixedTFBroadcaster();

  void run();
};

MultiPrefixedTFBroadcaster::MultiPrefixedTFBroadcaster(int data_size)
{
  int size;
  if (nh_.getParam("mptb_datasize", size))
  {
    data_size_ = size;
  }
  else
  {
    data_size_ = data_size;
  }
  ROS_INFO("size of array for MultiRobotStateDisplay is set to %d", data_size_);
  models_.resize(data_size_);
  trees_.resize(data_size_);
  mimicmaps_.resize(data_size_);
  pub_ptrs_.resize(data_size_);

  for (int i = 0; i < data_size_; i++)
  {
    if (!models_[i].initParam("mptb_"+to_string(i)+"/robot_description"))
    {
      ROS_WARN("Could not load [%s], then use parameter [robot_descrition]", (to_string(i)+"/robot_description").c_str());
      if (!models_[i].initParam("robot_description"))
      {
        ROS_ERROR("Failed to read parameter");
        abort();
      }
    }

    if (!kdl_parser::treeFromUrdfModel(models_[i], trees_[i]))
    {
      ROS_ERROR("Failed to extract kdl tree from xml robot description");
      abort();
    }

    for (map<string, urdf::JointSharedPtr>::iterator iter =
             models_[i].joints_.begin();
         iter != models_[i].joints_.end(); iter++)
    {
      if (iter->second->mimic)
      {
        mimicmaps_[i].insert(make_pair(iter->first, iter->second->mimic));
      }
    }

    pub_ptrs_[i] = make_shared<FixedStatePublisher>(trees_[i], models_[i]);
  }
}

void MultiPrefixedTFBroadcaster::callback(const tiny_rviz_plugins::MultiRobotStateDisplay::ConstPtr& msg)
{
  // Check whether msg is valid or not

  // Publish
  for (int idx = 0; idx < data_size_; ++idx)
  {
    map<string, double> joint_positions;
    auto js_msg = msg->joint_states[idx];
    for (int i = 0; i < js_msg.name.size(); ++i)
    {
      joint_positions.insert(make_pair(js_msg.name[i], js_msg.position[i]));
    }

    /// @TODO 全てをtf_staticにする
    string tf_prefix = "mptb_" + to_string(idx);
    pub_ptrs_[idx]->publishTransforms(joint_positions, msg->header.stamp, tf_prefix);
    pub_ptrs_[idx]->publishFixedTransforms(tf_prefix, true);

  }

  // link_base tf broadcaster
  st_tf2_broadcaster_.sendTransform(msg->transforms_to_root);
}

void MultiPrefixedTFBroadcaster::run()
{
  ros::Subscriber sub = nh_.subscribe("/multi_robot_state", 1, &MultiPrefixedTFBroadcaster::callback, this);

  ROS_INFO("Subscribing topic");

  ros::spin();
}

int main(int argc, char **argv)
{
  ros::init(argc, argv, "multi_prefixed_tf_broadcaster_node");
  ros::NodeHandle nh;

  MultiPrefixedTFBroadcaster mptb;
  mptb.run();

  return 0;
}
