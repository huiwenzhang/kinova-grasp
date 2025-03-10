
#include "jrc18sia_motion_planner/jrc18sia_motion_planner.h"

int evaluateMoveitPlan(moveit::planning_interface::MoveGroup::Plan& plan)
{
    moveit_msgs::RobotTrajectory trajectory = plan.trajectory_;
    // std::vector<trajectory_msgs::JointTrajectoryPoint> points =
    // trajectory.joint_trajectory.points;
    return trajectory.joint_trajectory.points.size();
}
// Cartesian line plan
void moveLineTarget(const geometry_msgs::Pose& goal)
{
    ROS_INFO("Begin cartesian line plan");
    moveit::planning_interface::MoveGroup group("arm");
    group.setGoalPositionTolerance(0.01);    // 3cm
    group.setGoalOrientationTolerance(0.02); // 5.729576129 * 2 deg
    group.setMaxVelocityScalingFactor(0.5);
    group.allowReplanning(true);
    group.setPlanningTime(5.0);

    geometry_msgs::PoseStamped current = group.getCurrentPose();
    geometry_msgs::Pose start = current.pose;
    geometry_msgs::Pose way_pose = start;

    std::vector<geometry_msgs::Pose> waypoints;
    waypoints.push_back(way_pose); // first pose waypoint
    int num_waypoint = 6;
    float delta_x = (goal.position.x - start.position.x) / (num_waypoint - 1);
    float delta_y = (goal.position.y - start.position.y) / (num_waypoint - 1);
    float delta_z = (goal.position.z - start.position.z) / (num_waypoint - 1);

    // interplotate between current pose and target pose
    for (int i = 0; i < num_waypoint - 1; i++) {
        way_pose.position.x += delta_x;
        way_pose.position.y += delta_y;
        way_pose.position.z += delta_z;
        waypoints.push_back(way_pose);
        ROS_INFO_STREAM(way_pose);
    }


    moveit_msgs::RobotTrajectory trajectory;
    const double jump_threshold = 0.0;
    const double eef_step = 0.02;
    double fraction = group.computeCartesianPath(waypoints, eef_step, jump_threshold, trajectory);
    if (fraction == 1.0) {
        std::cout << "computeCartesionPath Successfully" << std::endl;
    } else if (fraction == -1.0) {
        ROS_ERROR("computeCartesionPath ERROR!");
    } else {
        ROS_ERROR_STREAM("computeCartesionPath : " << fraction * 100 << " %");
    }
    moveit::planning_interface::MoveGroup::Plan cartesian_plan;
    cartesian_plan.trajectory_ = trajectory;

    int plan_steps = evaluateMoveitPlan(cartesian_plan);
    ROS_INFO_STREAM("Line plan steps: " << plan_steps);
    if (plan_steps < 100) {
        ROS_INFO_STREAM("Plan found in " << cartesian_plan.planning_time_ << " seconds with "
                                         << plan_steps << " steps");
        group.execute(cartesian_plan);
    } else {
        exit(0);
    }
}


int main(int argc, char** argv)
{
    ros::init(argc, argv, "jrc18sia_motion_planner_demo");
    ros::NodeHandle nh;
    ros::AsyncSpinner spinner(2);
    spinner.start();

//    JRCPickPlace jrc_pick_place(nh);

//    jrc_pick_place.pickPlacePipleline();

    JRCMotionPlanner motion_planner(nh);


    geometry_msgs::Pose pose1, current;
    tf::Quaternion temp_q;
    temp_q.setRPY(-1.37, -1.45, 2.82); 
    
    pose1.orientation.x = temp_q.x();
    pose1.orientation.y = temp_q.y();
    pose1.orientation.z = temp_q.z();
    pose1.orientation.w = temp_q.w();
    pose1.position.x =  0.18;
    pose1.position.y = -0.28;
    pose1.position.z =  0.52;

    // motion_planner.moveToTargetBestTime(pose1);


    // // moveLineTarget(pose1);
    // current = motion_planner.getCurrentPoseFromMoveit();
    // ROS_INFO_STREAM(current);
    // pose1 = current;
    
    // pose1.position.x += 0.15;
    // pose1.position.y += -0.15;
    // pose1.position.z += 0.15;
    // motion_planner.moveLineTarget(current,pose1);

    // pose1.position.x += 0.0;
    // pose1.position.y += -0.2;
    // pose1.position.z += 0.0;
    // // moveLineTarget(pose1);
    // current = motion_planner.getCurrentPoseFromMoveit();
    // ROS_INFO_STREAM(current);
    // motion_planner.moveLineTarget(current,pose1);

    // pose1.position.x += 0.0;
    // pose1.position.y -= 0;
    // pose1.position.z += -0.2;
    // // moveLineTarget(pose1);
    // current = motion_planner.getCurrentPoseFromMoveit();
    // ROS_INFO_STREAM(current);
    // motion_planner.moveLineTarget(current,pose1);

    // pose1.position.x += 0.0;
    // pose1.position.y += 0.2;
    // pose1.position.z += 0.0;
    // // moveLineTarget(pose1);
    // current = motion_planner.getCurrentPoseFromMoveit();
    // ROS_INFO_STREAM(current);
    // motion_planner.moveLineTarget(current,pose1);
    // exit(0);



    motion_planner.cartesionPathPlanner(0.0,0.0,0.0,90,0,0,100,2);
    geometry_msgs::Pose pregrasp_pose;
    pregrasp_pose.orientation = tf::createQuaternionMsgFromRollPitchYaw(1.57,-1.57,0);
    motion_planner.cartesionPathPlanner(0.05,-0.05,0.05);
    motion_planner.cartesionPathPlanner(-0.05,0.05,-0.05);

    // std::vector<double> joint_values = motion_planner.getCurrentJointState();
    // for(int i=0;i<joint_values.size();i++)
    // {
    //     std::cout << joint_values[i] << std::endl;
    // }
    // std::vector<double> joint_values1 = motion_planner.getCurrentJointStateFromMoveit();
    //  for(int i=0;i<joint_values1.size();i++)
    // {
    //     std::cout << joint_values1[i] << std::endl;
    // }
    // joint_values[0] += 0.5;
    // motion_planner.setJointValueTarget(joint_values);
    // joint_values[0] -= 0.5;
    // motion_planner.setJointValueTarget(joint_values);

    // motion_planner.setJointValueTarget(0, 0.8);setJointValueTarget
    // motion_planner.setJointValueTarget(0, -0.5);

    // motion_planner.cartesionPathPlanner(0,-0.15,0,100,2);
    geometry_msgs::Pose current_pose;
    current_pose = motion_planner.getCurrentPoseFromDriver();
    pregrasp_pose.position = current_pose.position;
    pregrasp_pose.position.y -= 0.15;
    pregrasp_pose.position.z -= 0.05;
    motion_planner.moveToTargetBestTime(pregrasp_pose);
    motion_planner.cartesionPathPlanner(0.0,0.0,0.0,90,0,0,100,2);
//    return 0;
    current_pose = motion_planner.getCurrentPoseFromDriver();
    current_pose.orientation = pregrasp_pose.orientation;
    ROS_INFO_STREAM(current_pose);
    current_pose.position.y -= 0.1;
    current_pose.position.x += 0.05;
    current_pose.position.z += 0.05;
    motion_planner.moveLineTarget(current_pose);
    motion_planner.cartesionPathPlanner(0.05,-0.05,0.05);
    current_pose.position.y += 0.05;
    current_pose.position.x -= 0.05;
    current_pose.position.z -= 0.05;
    motion_planner.moveLineTarget(current_pose);

    // motion_planner.cartesionPathPlanner(0.01,0.01,0.1,100,2);
    // motion_planner.cartesionPathPlanner(0.0,0.01,0.01,90,0,0,100,2);
//    motion_planner.cartesionPathPlanner(0.0,0.1,0.0,90,0,0,100,2);

    // current_pose = motion_planner.getCurrentPoseFromDriver();
    current_pose.orientation = pregrasp_pose.orientation;
    current_pose.position.z -= 0.05;
    // motion_planner.moveToTargetBestTime(current_pose);
    motion_planner.moveToTargetNamed("Home");

    ros::Duration timer(0.5);

    ros::shutdown();
    return 0;
}