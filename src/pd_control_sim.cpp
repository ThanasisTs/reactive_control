// PD Controller for Gazebo
// Computes the end effector's real velocity by its position numericallly (bug in the returned velocity by the cartesian velocity controller)
 


#include "cartesian_trajectory_tracking/pd_control_sim.h"

extern void control_points_callback(const geometry_msgs::PointStampedConstPtr control_point);

// Velcocity control callback 
void ee_state_callback (const cartesian_state_msgs::PoseTwist::ConstPtr state_msg){
	if (vel_flag){
		// Computed robot velocity from robot position (bug in simulation) 
		control_cycle_duration = ros::Time::now().toSec() - robot_pose->header.stamp.toSec();
		robot_velocity->twist.linear.x = (state_msg->pose.position.x - robot_pose->pose.position.x)/control_cycle_duration;
		robot_velocity->twist.linear.y = (state_msg->pose.position.y - robot_pose->pose.position.y)/control_cycle_duration;
		robot_velocity->twist.linear.z = (state_msg->pose.position.z - robot_pose->pose.position.z)/control_cycle_duration;
		robot_velocity->header.stamp = ros::Time::now();
		real_robot_vel_pub.publish(*robot_velocity);
	}

	if (not vel_flag)
		vel_flag = true;

	// Robot position
	robot_pose->pose.position.x = state_msg->pose.position.x;
	robot_pose->pose.position.y = state_msg->pose.position.y;
	robot_pose->pose.position.z = state_msg->pose.position.z;
	robot_pose->header.stamp = ros::Time::now();

	// Follow human trajectory
	if (motion_started){
		// Spatial error
		spatial_error->twist.linear.x = desired_robot_position->point.x - robot_pose->pose.position.x;
		spatial_error->twist.linear.y = desired_robot_position->point.y - robot_pose->pose.position.y;
		spatial_error->twist.linear.z = desired_robot_position->point.z - robot_pose->pose.position.z;
		spatial_error->header.stamp = ros::Time::now();
		spatial_error_pub.publish(*spatial_error);

		// Commanded acceleration
		acc->accel.linear.x = -Dx*robot_velocity->twist.linear.x + Kx*spatial_error->twist.linear.x;
		acc->accel.linear.y = -Dy*robot_velocity->twist.linear.y + Ky*spatial_error->twist.linear.y;
		acc->accel.linear.z = -Dz*robot_velocity->twist.linear.z + Kz*spatial_error->twist.linear.z;

		// Commanded velocity
		vel_control->linear.x += acc->accel.linear.x*control_cycle_duration;
		vel_control->linear.y += acc->accel.linear.y*control_cycle_duration;
		vel_control->linear.z += acc->accel.linear.z*control_cycle_duration;

		// Commanded velocity with time info for debugging purposes
		vel_control_stamp->twist.linear.x = vel_control->linear.x;
		vel_control_stamp->twist.linear.y = vel_control->linear.y;
		vel_control_stamp->twist.linear.z = vel_control->linear.z;
		vel_control_stamp->header.stamp = ros::Time::now();
		
		// Check if the distance from the table is more than 3cm, otherwise halt the motion
		if (state_msg->pose.position.z > 0.03){
			// Publish velocites if not NaN
			if (!std::isnan(vel_control->linear.x) and !std::isnan(vel_control->linear.y) and !std::isnan(vel_control->linear.y)){
				ROS_INFO_STREAM("Valid commanded velocity");
				ROS_INFO_STREAM("The control time cycle is " << control_cycle_duration);
				command_stamp_pub.publish(*vel_control_stamp);
				command_pub.publish(*vel_control);
			}
			else{
				ROS_INFO_STREAM("The commanded acceleration is " << acc->accel.linear.x << " " << acc->accel.linear.y << " " << acc->accel.linear.z);
				ROS_INFO_STREAM("The control time cycle is " << control_cycle_duration);
				ROS_INFO_STREAM("Gonna publish previous velocities");
			}
		}
		else{
			// Halt the robot motion if it close to the table
			command_pub.publish(zero_vel);
		}
		// Publish the ee state when the motion starts
		robot_state_pub.publish(*state_msg);
	}
}


int main(int argc, char** argv){
	ros::init(argc, argv, "cartesian_trajectory_tracking");
	ros::NodeHandle n;
	ros::AsyncSpinner spinner(3);
	spinner.start();

	// Control gains
	n.param("cartesian_trajectory_tracking/Dx", Dx, 0.0f);
	n.param("cartesian_trajectory_tracking/Dy", Dy, 0.0f);
	n.param("cartesian_trajectory_tracking/Dz", Dz, 0.0f);
	n.param("cartesian_trajectory_tracking/Kx", Kx, 0.0f);
	n.param("cartesian_trajectory_tracking/Ky", Ky, 0.0f);
	n.param("cartesian_trajectory_tracking/Kz", Kz, 0.0f);
	
	// Human marker - Rviz
	marker_human->header.frame_id = "base_link";
	marker_human->type = visualization_msgs::Marker::LINE_STRIP;
	marker_human->action = visualization_msgs::Marker::ADD;
	marker_human->scale.x = 0.002;
	marker_human->scale.y = 0.002;
	marker_human->scale.z = 0.002;
	marker_human->color.r = 0.0f;
	marker_human->color.g = 1.0f;
	marker_human->color.b = 0.0f;
	marker_human->color.a = 1.0;
  	marker_human->lifetime = ros::Duration(100);
	
	// Robot marker - Rviz
	marker_robot->header.frame_id = "base_link";
	marker_robot->header.stamp = ros::Time::now();
	marker_robot->type = visualization_msgs::Marker::LINE_STRIP;
	marker_robot->action = visualization_msgs::Marker::ADD;
	marker_robot->scale.x = 0.0035;
	marker_robot->scale.y = 0.0035;
	marker_robot->scale.z = 0.0035;
	marker_robot->color.r = 0.0f;
	marker_robot->color.g = 0.0f;
	marker_robot->color.b = 1.0f;
	marker_robot->color.a = 1.0;
  	marker_robot->lifetime = ros::Duration(100);
  	
  	// Topic names
  	n.param("cartesian_trajectory_tracking/state_topic", ee_state_topic, std::string("/ur3_cartesian_velocity_controller_sim/ee_state"));
  	n.param("cartesian_trajectory_tracking/command_topic", ee_vel_command_topic, std::string("/ur3_cartesian_velocity_controller_sim/command_cart_vel"));

	// Publishers
	command_pub = n.advertise<geometry_msgs::Twist>(ee_vel_command_topic, 100);
	robot_state_pub = n.advertise<cartesian_state_msgs::PoseTwist>("/ee_state_topic", 100);
	spatial_error_pub = n.advertise<geometry_msgs::TwistStamped>("/spatial_error_topic", 100);
	command_stamp_pub = n.advertise<geometry_msgs::TwistStamped>("/vel_command_stamp_topic", 100);
	vis_human_pub = n.advertise<visualization_msgs::Marker>("/vis_human_topic", 100);
	vis_robot_pub = n.advertise<visualization_msgs::Marker>("/vis_robot_topic", 100);
	real_robot_vel_pub = n.advertise<geometry_msgs::TwistStamped>("/real_robot_vel_topic", 100);

	
	// Subscribers
	ros::Subscriber ee_state_sub = n.subscribe(ee_state_topic, 100, ee_state_callback);
	ros::Subscriber control_points_sub = n.subscribe("/control_points_topic", 100, control_points_callback);
	
	ros::waitForShutdown();
}
