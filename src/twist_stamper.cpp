/*!
  \file         twist_stamper.cpp
  \author       Arnaud Ramey <arnaud.a.ramey@gmail.com>
                -- Robotics Lab, University Carlos III of Madrid
  \date         2012/1

  ______________________________________________________________________________

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
  ______________________________________________________________________________

   This node will subscribe to a Twist topc, stamp the actual date,
   and reemit it on another topic
  */

#include <ros/ros.h>
#include <geometry_msgs/Twist.h>
#include <geometry_msgs/TwistStamped.h>

ros::Publisher vel_pub;

geometry_msgs::TwistStamped msg_out;
////////////////////////////////////////////////////////////////////////////////

void twist_callback(const geometry_msgs::TwistConstPtr & msg_in) {
  ROS_INFO_THROTTLE(1, "twist_callback()");
  msg_out.header.frame_id = "/base_link";
  msg_out.header.stamp = ros::Time::now();
  msg_out.twist = *msg_in;
  vel_pub.publish(msg_out);
}

////////////////////////////////////////////////////////////////////////////////

int main(int argc, char** argv) {
  ros::init(argc, argv, "twist_stamper");
  ros::NodeHandle nh_public, nh_private("~");
  std::string input_topic, output_topic;
  nh_private.param<std::string>("input_topic", input_topic, "cmd_vel");
  nh_private.param<std::string>("ouput_topic", output_topic, "cmd_vel_stamped");
  ROS_INFO("Stamping twists from '%s' to '%s'",
           input_topic.c_str(), output_topic.c_str());

  ros::Subscriber sub = nh_public.subscribe(input_topic, 1, twist_callback);
  vel_pub = nh_public.advertise<geometry_msgs::TwistStamped>(output_topic, 1);
  ros::spin();
  return 0;
}


