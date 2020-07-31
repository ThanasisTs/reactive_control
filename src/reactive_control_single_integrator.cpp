#include "reactive_control/reactive_control.h"

float dis_points;
float Ka, Kb, min_dis, max_dis, Ka_exp, Kb_exp, c;
bool exp_flag;
std_msgs::Float64MultiArray gain_array;
std_msgs::Float64 dis_all, dis_max;
float x_error, y_error, z_error;


float euclidean_distance (std::shared_ptr<std::vector<float>> v1, std::shared_ptr<std::vector<float>> v2){
	float temp = 0;
	for (short int i=0; i<v1->size(); i++){
		temp += pow((v1->at(i) - v2->at(i)), 2);
	}
	return sqrt(temp);
}

void human_motion_callback(const geometry_msgs::PointStampedConstPtr human_msg){
	received_point = true;
	if (count == 0){
		init_x = human_msg->point.x + xOffset;
		init_y = human_msg->point.y + yOffset;
		init_z = human_msg->point.z + zOffset;
		std::cout << init_x << init_y << init_z << std::endl;
	}
	else{
		if (count == 1){
			start_time = ros::Time::now().toSec();
			dis_x = human_msg->point.x + xOffset - init_x;
			dis_y = human_msg->point.y + yOffset - init_y;
			dis_z = human_msg->point.z + zOffset - init_z;
		}
		timenow = ros::Time::now();
		desired_robot_position->header.stamp = timenow;
		robot_state->header.stamp = timenow;
		state_pub_low_f.publish(*robot_state);
		vis_robot_pub.publish(*marker_robot);
	  	// ROS_INFO("Num of low freq robot state: %d", count);
		dis.data = sqrt(pow(desired_robot_position->point.x - robot_state->pose.position.x, 2) 
			+ pow(desired_robot_position->point.y - robot_state->pose.position.y, 2));
		dis_pub.publish(dis);
		dis_max.data = sqrt(pow(human_msg->point.x + xOffset - robot_state->pose.position.x, 2) 
			+ pow(human_msg->point.y + yOffset - robot_state->pose.position.y, 2));
		dis_max_pub.publish(dis_max);
		marker_robot->points.push_back(robot_state->pose.position);
		init_point = true;
	}

	count += 1;
	desired_robot_position->point.x = human_msg->point.x + xOffset - dis_x;
	desired_robot_position->point.y = human_msg->point.y + yOffset - dis_y;
	temp_z = human_msg->point.z - dis_y;
	if (temp_z > 10){
		desired_robot_position->point.z = human_msg->point.z - 10 + zOffset;
	}
	else{
		desired_robot_position->point.z = human_msg->point.z + zOffset;		
	}
	desired_robot_position->header.stamp = ros::Time::now();
	control_points_pub.publish(*desired_robot_position);
	marker_human->header.stamp = ros::Time::now();

    marker_human->points.push_back(desired_robot_position->point);
  	vis_human_pub.publish(*marker_human);

	if (init_point){
		end_time = ros::Time::now().toSec();
	}
}

void state_callback (const trajectory_execution_msgs::PoseTwist::ConstPtr state_msg){
	robot_state->pose.position.x = state_msg->pose.position.x;
	robot_state->pose.position.y = state_msg->pose.position.y;
	robot_state->pose.position.z = state_msg->pose.position.z;
	robot_state->header.stamp = ros::Time::now();


	if (received_point){
		if (count == 1){
			vel_control->linear.x = (desired_robot_position->point.x - robot_state->pose.position.x);
			vel_control->linear.y = (desired_robot_position->point.y - robot_state->pose.position.y);
			vel_control->linear.z = (desired_robot_position->point.z - robot_state->pose.position.z);
			vel_control->angular.x = 0;
			vel_control->angular.y = 0;
			vel_control->angular.z = 0;
			pub.publish(*vel_control);
		}
		else{
			dis_all.data = sqrt(pow(desired_robot_position->point.x - robot_state->pose.position.x, 2) 
				+ pow(desired_robot_position->point.y - robot_state->pose.position.y, 2));
			dis_all_pub.publish(dis_all);
			if (var){
				v1->push_back(robot_state->pose.position.x);
				v1->push_back(robot_state->pose.position.y);
				v1->push_back(robot_state->pose.position.z);
				v2->push_back(desired_robot_position->point.x);
				v2->push_back(desired_robot_position->point.y);
				v2->push_back(desired_robot_position->point.z);
				dis_points = euclidean_distance(v1, v2);
				x_error = abs(robot_state->pose.position.x - desired_robot_position->point.x);
				y_error = abs(robot_state->pose.position.y - desired_robot_position->point.y);
				z_error - abs(robot_state->pose.position.z - desired_robot_position->point.z);

				ROS_INFO("The distance is: %f", euclidean_distance(v1, v2));
				if (exp_flag){
					Dx = Ka/(1+exp(Ka_exp*(x_error-min_dis)))+Kb/(1+exp(-Kb_exp*(x_error-max_dis)))+c;
					Dy = Ka/(1+exp(Ka_exp*(y_error-min_dis)))+Kb/(1+exp(-Kb_exp*(y_error-max_dis)))+c;
					Dz = Ka/(1+exp(Ka_exp*(z_error-min_dis)))+Kb/(1+exp(-Kb_exp*(z_error-max_dis)))+c;
					D = Ka/(1+exp(Ka_exp*(dis_points-min_dis)))+Kb/(1+exp(-Kb_exp*(dis_points-max_dis)))+c;	
				}
				else{
					D = Ka/dis_points; // + Kb/(max_dis-dis_points);
					if (dis_points < 0.005){
						D=1;
					}
					if (dis_points > 0.1){
						D=6;
					}
					std::cout << D << std::endl;
				}

				gain_array.data.push_back(dis_points);
				gain_array.data.push_back(D);
				gain_array.data.push_back(ros::Time::now().toSec());
				gain_pub.publish(gain_array);
				gain_array.data.clear();
				ROS_INFO("D: %f", D);
				v1->clear();
				v2->clear();
				D_v.push_back(D);
			}


			vel_control->linear.x = Dx*(desired_robot_position->point.x - robot_state->pose.position.x);
			vel_control->linear.y = Dy*(desired_robot_position->point.y - robot_state->pose.position.y);
			vel_control->linear.z = Dz*(desired_robot_position->point.z - robot_state->pose.position.z);
			vel_control->angular.x = 0;
			vel_control->angular.y = 0;
			vel_control->angular.z = 0;
			pub.publish(*vel_control);
			std::cout << *vel_control << std::endl;
		}
		if (abs(robot_state->pose.position.x - init_x) < 0.005
		 and abs(robot_state->pose.position.y - init_y) < 0.005 
		 and abs(robot_state->pose.position.z - init_z) < 0.005
		 and not init_point){
		 	// ROS_INFO("Reached the first point");
		}
		if (init_point){
			state_pub_high_f.publish(*state_msg);
		}
	}
}


int main(int argc, char** argv){
	ros::init(argc, argv, "reactive_control_node");
	ros::NodeHandle n;
	ros::AsyncSpinner spinner(3);
	spinner.start();
	safe_vel_control->linear.x = 0;
	safe_vel_control->linear.y = 0;
	safe_vel_control->linear.z = 0;
	n.param("reactive_control_node/Dx", Dx, 0.0f);
	n.param("reactive_control_node/Dy", Dy, 0.0f);
	n.param("reactive_control_node/Dz", Dz, 0.0f);
	n.param("reactive_control_node/xOffset", xOffset, 0.0f);
	n.param("reactive_control_node/yOffset", yOffset, 0.0f);
	n.param("reactive_control_node/zOffset", zOffset, 0.0f);
	n.param("reactive_control_node/var", var, false);
	n.param("reactive_control_node/var_gain", var_gain, 10.0f);
	n.param("reactive_control_node/sim", sim, true);
	n.param("reactive_control_node/Ka", Ka, 1.0f);	
	n.param("reactive_control_node/Kb", Kb, 1.0f);	
	n.param("reactive_control_node/Ka_exp", Ka_exp, 1.0f);	
	n.param("reactive_control_node/Kb_exp", Kb_exp, 1.0f);	
	n.param("reactive_control_node/min_dis", min_dis, 1.0f);	
	n.param("reactive_control_node/max_dis", max_dis, 1.0f);	
	n.param("reactive_control_node/exp_flag", exp_flag, true);	
	n.param("reactive_control_node/c", c, 1.0f);

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
	
	marker_robot->header.frame_id = "base_link";
	marker_robot->header.stamp = ros::Time::now();
	marker_robot->type = visualization_msgs::Marker::LINE_STRIP;
	marker_robot->action = visualization_msgs::Marker::ADD;
	marker_robot->scale.x = 0.01;
    marker_robot->scale.y = 0.01;
    marker_robot->scale.z = 0.01;
    marker_robot->color.r = 0.0f;
    marker_robot->color.g = 0.0f;
    marker_robot->color.b = 1.0f;
    marker_robot->color.a = 1.0;
  	marker_robot->lifetime = ros::Duration(100);

  	if (sim){
  		ee_state_topic = "/manos_cartesian_velocity_controller_sim/ee_state";
  		ee_vel_command_topic = "/manos_cartesian_velocity_controller_sim/command_cart_vel";	
  	}
  	else{
  		ee_state_topic = "/manos_cartesian_velocity_controller/ee_state";
  		ee_vel_command_topic = "/manos_cartesian_velocity_controller/command_cart_vel";  		
  	}

	pub = n.advertise<geometry_msgs::Twist>(ee_vel_command_topic, 100);
	state_pub_high_f = n.advertise<trajectory_execution_msgs::PoseTwist>("/ee_position_high_f", 100);
	state_pub_low_f = n.advertise<geometry_msgs::PoseStamped>("/ee_position_low_f", 100);
	dis_pub = n.advertise<std_msgs::Float64>("/response_topic", 100);
	dis_all_pub = n.advertise<std_msgs::Float64>("/distance_topic", 100);
	dis_max_pub = n.advertise<std_msgs::Float64>("/dis_max_topic", 100);
	gain_pub = n.advertise<std_msgs::Float64MultiArray>("/gain_topic", 100);
	vis_human_pub = n.advertise<visualization_msgs::Marker>("/vis_human_topic", 100);
	vis_robot_pub = n.advertise<visualization_msgs::Marker>("/vis_robot_topic", 100);
	control_points_pub = n.advertise<geometry_msgs::PointStamped>("trajectory_points_stamp", 100);	
	ros::Subscriber sub = n.subscribe(ee_state_topic, 100, state_callback);
	ros::Subscriber sub2 = n.subscribe("/trajectory_points", 100, human_motion_callback);
	
	ros::waitForShutdown();
}