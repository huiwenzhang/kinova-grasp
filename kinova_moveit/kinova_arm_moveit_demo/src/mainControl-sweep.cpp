/**************************************************************
* Final version code for arm control
* Two operation mode: sweep and suck
****************************************************************/

// Std C++ headers
#include <iostream>
#include <map>
#include <string>
#include <vector>

// custom headers
#include "darknet_ros_msgs/TargetPoint.h" // kinect info
#include "darknet_ros_msgs/TargetPoints.h"
#include "id_data_msgs/ID_Data.h" //using for notie event
#include <add_scene_objects.h>    // handle scene obstacles
#include <pick_place.h>

#define PREGRASP_OFFSET 0.2
#define POSITION_TOLERANCE 0.04
#define ORIENTATION_TOLERANCE 0.1
#define MAX_PARALLEL_ATTEMPTS 10
#define DEBUG true
#define SHELF_EDGE_Y -0.4

using namespace std;

// Flag variables
int error_no = 0;
bool kinect_target_valid = true;

bool dashgo_act_finished_flag = false;

// kinect distance
double joy_up_distance = 0;
double joy_down_distance = 0;
double joy_left_distance = 0;
double joy_right_distance = 0;
double joy_deep_distance = 0;

// alvin
bool switch_suck_flag = false;
bool begin_suck_flag = false;
bool stop_suck_flag = false;

// main loop globals: arm control section, id=4
bool arm_start_sweep_flag = false; // data[0]=1
bool arm_stop_sweep_flag = false;  // data[0]=0
bool arm_start_suck_flag = false;
bool arm_stop_suck_flag = false;
bool arm_move2_rest_flag = false;   // move to rest pose for recognition
bool arn_move2_home_flag = false;   // move to home pose, prepare for sweep or suck
bool arm_msg_rec_flag = false;      // data[0]=14
bool arm_act_finished_flag = false; // data[0]=15

geometry_msgs::Pose home_pose_low; // move to home task
geometry_msgs::Pose home_pose_mid;
geometry_msgs::Pose home_pose_high;
geometry_msgs::Pose home_pose;
geometry_msgs::Pose scan_pose; // move to rest task
geometry_msgs::Pose box_pose;     // box pose to place object

// Pose used by Alvin
geometry_msgs::Pose sweep_pose; // Plan target from kinect --global variable
geometry_msgs::Pose presweep_pose;

geometry_msgs::Pose suck_pose; // For suck task
geometry_msgs::Pose presuck_pose;

geometry_msgs::Pose current_pose; // used to indicate arm state
geometry_msgs::Pose box_pose;     // box pose to place object



/***************************NOTICE CLASS****************************/
class notice_pub_sub {
public:
    boost::function<void(const id_data_msgs::ID_Data::ConstPtr&)> notice_pub_sub_msgCallbackFun;

    notice_pub_sub();
    void notice_pub_sub_pulisher(id_data_msgs::ID_Data id_data);
    void notice_display(id_data_msgs::ID_Data notice_msg, bool set);
    void notice_sub_spinner(char set);
    actionlib::SimpleActionClient<control_msgs::FollowJointTrajectoryAction>* ac;

private:
    ros::NodeHandle notice_handle;
    ros::Subscriber notice_subscriber;
    ros::Publisher notice_publisher;
    ros::SubscribeOptions notice_ops;
    ros::AsyncSpinner* notice_spinner;
    ros::CallbackQueue notice_callbackqueue;
    void notice_msgCallback(const id_data_msgs::ID_Data::ConstPtr& notice_msg);
    // ros::ServiceClient jaco_estop_client;
};

notice_pub_sub::notice_pub_sub()
{
    notice_pub_sub_msgCallbackFun = boost::bind(&notice_pub_sub::notice_msgCallback, this, _1);
    notice_ops = ros::SubscribeOptions::create<id_data_msgs::ID_Data>(
        "/notice", 10, notice_pub_sub_msgCallbackFun, ros::VoidPtr(), &notice_callbackqueue);
    notice_subscriber = notice_handle.subscribe(notice_ops);
    notice_spinner = new ros::AsyncSpinner(1, &notice_callbackqueue);

    notice_publisher = notice_handle.advertise<id_data_msgs::ID_Data>("/notice", 10);
    // jaco_estop_client = notice_handle.serviceClient<wpi_jaco_msgs::EStop>(
    //     "/jaco_arm/software_estop");
}

void notice_pub_sub::notice_pub_sub_pulisher(id_data_msgs::ID_Data id_data)
{
    notice_publisher.publish(id_data);
}

void notice_pub_sub::notice_display(id_data_msgs::ID_Data notice_msg, bool set)
{

    if (set) {
        printf("REC Notice message,ID: %d,Data: ", notice_msg.id);
        for (char i = 0; i < 8; i++) {
            printf("%d ", notice_msg.data[i]);
            if (i == 7) printf("\n");
        }
    }
}

void notice_pub_sub::notice_msgCallback(const id_data_msgs::ID_Data::ConstPtr& notice_msg)
{

    id_data_msgs::ID_Data notice_message;
    notice_message.id = 0;
    for (char i = 0; i < 8; i++) notice_message.data[i] = 0;

    notice_message.id = notice_msg->id;
    for (char i = 0; i < 8; i++) notice_message.data[i] = notice_msg->data[i];

    notice_pub_sub::notice_display(notice_message, true);

    /************************Hand messsage**********************/

    /***** Object level *****/

    /***** dashgo_act_finished_flag *****/
    if (notice_message.id == 2 && notice_message.data[0] == 8) {
        dashgo_act_finished_flag = true;
        ROS_WARN_STREAM("dashgo_act_finished_flag=true");
    }

    /********************** Adjust mobile bsee *****************/

    /***** communication with Jaco arm *****/

    if (notice_message.id == 4 && notice_message.data[0] == 1 && notice_message.data[1] == 2) {
        ROS_INFO("Receive command: sweep object");
        float x = notice_message.data[2] / 1000.0; // object pose, coordinate x
        float y = notice_message.data[3] / 1000.0; // object pose, coordinate y
        float z = notice_message.data[4] / 1000.0; // object pose, coordinate z
        ROS_INFO("receive pose form kinect %f, %f, %f", x, y, z);
        if (fabs(x) < 0.5 && y < -0.3) {
            ROS_INFO("Receive valid object position from kinect");
            sweep_pose.position.x = x;
            sweep_pose.position.y = y;
            sweep_pose.position.z = z;
            // kinect_target_valid = true;
            arm_start_sweep_flag = true;
        }
    }

    if (notice_message.id == 4 && notice_message.data[0] == 1 && notice_message.data[1] == 8) {
        ROS_INFO("Receive command: suck object");
        float x = notice_message.data[2] / 1000.0; // object pose, coordinate x
        float y = notice_message.data[3] / 1000.0; // object pose, coordinate y
        float z = notice_message.data[4] / 1000.0; // object pose, coordinate z
        if (abs(x) < 0.5 && y < -0.3) {
            ROS_INFO("Receive valid object position from kinect");
            suck_pose.position.x = x;
            suck_pose.position.y = y + 0.03; // correction
            suck_pose.position.z = z + 0.1;  // correction
            // kinect_target_valid = true;
            arm_start_sweep_flag = true;
        }
    }

    if (notice_message.id == 4 && notice_message.data[0] == 0) // main loop stop arm to fetch flag
    {
        arm_stop_sweep_flag = true;
    }
    if (notice_message.id == 4 && notice_message.data[0] == 2) // main loop keep arm to fetch flag
    {
        arm_keep_fetch_flag = true;
    }
    if (notice_message.id == 4 && notice_message.data[0] == 3) // main loop let arm release joy flag
    {
        arm_release_obj_flag = true;
    }
}

void notice_pub_sub::notice_sub_spinner(char set)
{
    if (set == 1) notice_spinner->start();
    if (set == 0) notice_spinner->stop();
}

/********************************* MAIN PROGRAM ****************************/
int main(int argc, char** argv)
{
    ros::init(argc, argv, "jaco_moveit_control_main");
    ros::NodeHandle nh;
    kinova::PickPlace pick_place(nh);

    ros::AsyncSpinner spinner(2);
    spinner.start();

    notice_pub_sub notice_test; // initial a notice class
    int loop_hz = 100;
    ros::Rate loop_rate(loop_hz);
    id_data_msgs::ID_Data notice_data; // initial an empty data

    poseInit(); // initial predifined poses

    /**********************ADD COLLISION***************************/
    ROS_INFO("Add collision objects  into the world (kinect and mobile base)");
    moveit::planning_interface::PlanningSceneInterface planning_scene_interface;
    build_workScene buildWorkScene(nh);
    handleCollisionObj(buildWorkScene); // add object
    planning_scene_interface.addCollisionObjects(
        buildWorkScene.collision_objects); // add collision objects to world
    ROS_INFO("Collision setup finished");

    /*************************ARM tASK LOOP***************/
    int loop_cnt = 0;
    int wait_count = 0;
    string pose_name = "NO DEFINE";
    while (ros::ok()) {
        loop_cnt++;
        if (loop_cnt % 100 == 0) {
            ROS_INFO("Begin main loop, ready to receive command from ge_test");
            // ROS_INFO_STREAM("arm_start_fetch flag: " << arm_start_sweep_flag);
        }

        if (arm_start_sweep_flag) {
            // sweep mode
            ROS_INFO("Start sweep objects from shelf/desk");
            notice_data_clear(&notice_data);
            notice_data.id = 4;
            notice_data.data[0] = 14;
            notice_test.notice_pub_sub_pulisher(notice_data);

            // adjust mobile base position if necessary
            /******************************
            int left = 0; // unit: cm
            int right = 0;
            // low --- fisrt floor
            if (sweep_pose.position.z < 0.2) {
              if (sweep_pose.position.y < -0.85) // TODO 0.82
              {
                left = (int)((0.85 - sweep_pose.position.y) * 100);
                ROS_INFO_STREAM(mobile move left);
                notice_data.id = 2;
                notice_data.data[0] = 5;
                notice_data.data[1] = left; // -y direction in arm base frame
                notice_test.notice_pub_sub_pulisher(notice_data);
              } else {
                right = (int)((sweep_pose.position.y - 0.85) * 100);
                notice_data.id = 2;
                notice_data.data[0] = 6;
                notice_data.data[1] = right;
                notice_test.notice_pub_sub_pulisher(notice_data);
              }
            }

            // mid ---- desk
            else if (sweep_pose.position.z >= 0.15 &&
                     sweep_pose.position.z < 0.45) {
              if (sweep_pose.position.y < -0.8) // TODO 0.82
              {
                left = (int)((0.85 - sweep_pose.position.y) * 100);
                ROS_INFO_STREAM(mobile move left);
                notice_data.id = 2;
                notice_data.data[0] = 5;
                notice_data.data[1] = left; // -y direction in arm base frame
                notice_test.notice_pub_sub_pulisher(notice_data);
              } else {
                right = (int)((sweep_pose.position.y - 0.85) * 100);
                notice_data.id = 2;
                notice_data.data[0] = 6;
                notice_data.data[1] = right;
                notice_test.notice_pub_sub_pulisher(notice_data);
              }
            }
            // high -- second floor
            else if (sweep_pose.position.z >= 0.45 &&
                     sweep_pose.position.z < 0.65) {
              if (sweep_pose.position.y < -0.65) // TODO 0.82
              {
                left = (int)((0.85 - sweep_pose.position.y) * 100);
                ROS_INFO_STREAM(mobile move left);
                notice_data.id = 2;
                notice_data.data[0] = 5;
                notice_data.data[1] = left; // -y direction in arm base frame
                notice_test.notice_pub_sub_pulisher(notice_data);
              } else {
                right = (int)((sweep_pose.position.y - 0.85) * 100);
                notice_data.id = 2;
                notice_data.data[0] = 6;
                notice_data.data[1] = right;
                notice_test.notice_pub_sub_pulisher(notice_data);
              }
            }
            *********************/

            // sweep start
            presweep_pose = sweep_pose; // sweep pose is the target pose from kinect
            presweep_pose.position.y += PREGRASP_OFFSET;
            presweep_pose.position.z += 0.1;
            // ROS_INFO_STREAM("pregrasp pose: " << presweep_pose);

            // 1. pregrasp
            pose_name = "PRE-SWEEP POSE";
            confirmToAct(presweep_pose, pose_name);
            moveToTarget(presweep_pose); // plan to pre-grasp pose

            // 2. move forward
            geometry_msgs::Pose start;
            if (DEBUG) {
                start = presweep_pose; // virtual pose
            } else {
                start = pick_place.get_ee_pose(); // real pose from driver info.
                start.orientation = presweep_pose.orientation;
            }
            geometry_msgs::Pose goal = sweep_pose;
            goal.position.y -= PREGRASP_OFFSET;
            pose_name = "FORWARD";
            confirmToAct(start, goal, pose_name);
            moveLineTarget(start, goal);

            // 3. move down
            geometry_msgs::Pose start;
            if (DEBUG) {
                start = presweep_pose; // virtual pose
                start.position.z += 0.1;
            } else {
                start = pick_place.get_ee_pose(); // real pose from driver info.
                start.orientation = presweep_pose.orientation;
            }
            geometry_msgs::Pose goal = sweep_pose;
            pose_name = "DOWN";
            confirmToAct(start, goal, pose_name);
            moveLineTarget(start, goal);

            // 4. sweep to box
            if (DEBUG) {
                start = sweep_pose;
            } else {
                start = pick_place.get_ee_pose();
                start.orientation = presweep_pose.orientation;
            }

            goal = start;
            goal.position.y = SHELF_EDGE_Y;
            pose_name = "SWEEP (BACK)";
            confirmToAct(start, goal, pose_name);
            moveLineTarget(start, goal);

            // 5. move to home pose
            if (DEBUG) {
                start = sweep_pose;
                start.position.y = SHELF_EDGE_Y;
            } else {
                start = pick_place.get_ee_pose();
                start.orientation = presweep_pose.orientation;
            }
            pose_name = "HOME POSE";
            confirmToAct(presweep_pose, home_pose, pose_name);
            moveToTarget(home_pose); // nonlinear plan

            // notice main loop that sweep task finished
            notice_data_clear(&notice_data);
            notice_data.id = 4;
            notice_data.data[0] = 15;
            notice_test.notice_pub_sub_pulisher(notice_data);
            arm_start_sweep_flag = false;
            ROS_INFO("Sweep task finished");
        }

        if (arm_start_suck_flag) {
            // suck mode
            presuck_pose = suck_pose;
            presuck_pose.position.z += PREGRASP_OFFSET;
            presuck_pose.position.y += PREGRASP_OFFSET;

            // 1. presuck
            pose_name = "PRESUCK POSE";
            confirmToAct(presuck_pose, pose_name);
            moveToTarget(presuck_pose); // plan to pre-grasp pose

            // 2. move forward
            geometry_msgs::Pose start;
            if (DEBUG) {
                start = presuck_pose;
            } else {
                start = pick_place.get_ee_pose();
                start.orientation = presuck_pose.orientation;
            }
            geometry_msgs::Pose goal = presuck_pose;
            goal.position.y -= PREGRASP_OFFSET;
            pose_name = "SUCK FORWARD";
            confirmToAct(start, goal, pose_name);
            moveLineTarget(start, goal); // forward

            // 3. move down
            if (DEBUG) {
                start = goal;
            } else {
                geometry_msgs::Pose start = pick_place.get_ee_pose();
                start.orientation = presuck_pose.orientation;
            }
            goal = suck_pose;
            pose_name = "SUCK DOWN";
            confirmToAct(start, goal, pose_name);
            moveLineTarget(start, goal); // down

            // 4. suck
            notice_data_clear(&notice_data);
            notice_data.id = 1;
            notice_data.data[0] = 8;
            notice_test.notice_pub_sub_pulisher(notice_data);
            ROS_INFO("notice sucker to suck (1 8)");

            // data receive judge
            wait_count = 0;
            while (ros::ok()) {

                if (hand_msg_rec_flag == true) // 1 14
                {
                    hand_msg_rec_flag = false;
                    break;
                }
                wait_count++;
                if (wait_count % 100 == 0) // send msg again after waiting 1s
                {
                    ROS_ERROR("jaco didn't receive hand msg,Retrying...");
                    notice_test.notice_pub_sub_pulisher(notice_data);
                }
                notice_test.notice_sub_spinner(1);
                loop_rate.sleep();
            }

            // wait for hand to finished
            wait_count = 0;
            while (ros::ok()) {
                wait_count++;
                if (wait_count % 100 == 0) {
                    ROS_INFO("waiting for suck");
                }
                if (hand_act_finished_flag) // 1 2
                {
                    hand_act_finished_flag = false;
                    break;
                }
                notice_test.notice_sub_spinner(1);
                loop_rate.sleep();
            }

            // 5. move up
            if (DEBUG) {
                start = presuck_pose;
            } else {
                start = pick_place.get_ee_pose();
                start.orientation = presuck_pose.orientation;
            }
            goal = suck_pose;
            goal.position.z += PREGRASP_OFFSET;
            pose_name = "SUCK UP ";
            confirmToAct(start, goal, pose_name);
            moveLineTarget(start, goal); // up

            6. move to box(above) if (DEBUG)
            {
                start = goal; // last goal
            }
            else
            {
                start = pick_place.get_ee_pose();
                start.orientation = presuck_pose.orientation;
            }
            goal = presuck_pose;
            pose_name = "SUCK  BACK";
            confirmToAct(start, goal, pose_name);
            moveLineTarget(start, goal); // back
            pose_name = "SUCK box POSE";
            confirmToAct(presuck_pose, sucker_rest_pose);
            moveToTarget(box_pose);

            // 7. release, suck stop
            // TODO

            // notice main loop that suck task finished
            notice_data_clear(&notice_data);
            notice_data.id = 4;
            notice_data.data[0] = 15;
            notice_test.notice_pub_sub_pulisher(notice_data);
            arm_start_sweep_flag = false;
            ROS_INFO("Suck task finished");
        }

        notice_test.notice_sub_spinner(1);
        loop_rate.sleep();
    }
    ros::shutdown();
    return 0;
}
