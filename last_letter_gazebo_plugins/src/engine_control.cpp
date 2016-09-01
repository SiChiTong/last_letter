#include <boost/bind.hpp>
#include <gazebo/gazebo.hh>
#include <gazebo/physics/physics.hh>
#include <gazebo/common/common.hh>
#include <stdio.h>

#include "gazebo_msgs/ModelState.h"
#include "geometry_msgs/Wrench.h"
#include <gazebo/common/Plugin.hh>
#include <ros/ros.h>


namespace gazebo
{
  class engineControl : public ModelPlugin
  {
    public: void Load(physics::ModelPtr _parent, sdf::ElementPtr _sdf)
    {
      // Store the pointer to the model
      this->model = _parent;
      // Store the pointer to the motor shaft
      this->jointAxis = this->model->GetJoint("propeller_shaft");
      // Store teh point to the propeller link
      this->linkProp = this->model->GetLink("propeller");
      this->linkMot = this->model->GetLink("motor");
      this->linkFuse = this->model->GetLink("fuselage");

      if (!ros::isInitialized())
      {
        ROS_FATAL_STREAM("A ROS node for Gazebo has not been initialized, unable to load plugin. "
          << "Load the Gazebo system plugin 'libgazebo_ros_api_plugin.so' in the gazebo_ros package)");
        return;
      }

      // Listen to the update event. This event is broadcast every
      // simulation iteration.
      this->updateConnection = event::Events::ConnectWorldUpdateBegin(boost::bind(&engineControl::OnUpdate, this, _1));

      this->rosPub = this->rosHandle.advertise<gazebo_msgs::ModelState>("/" + this->model->GetName() + "/propState",100); //model states publisher
      this->rosSubMotor = this->rosHandle.subscribe("/" + this->model->GetName() + "/wrenchMotor",1,&engineControl::getWrenchMotor, this); //get the applied wrench

      ROS_INFO("engineControl plugin initialized");
    }

    // Called by the world update start event
    public: void OnUpdate(const common::UpdateInfo & /*_info*/)
    {

      // Read the linkProp state
      this->modelPose = this->linkProp->GetWorldPose();
        // Velocities required in the body frame
      this->modelVelLin = this->linkProp->GetRelativeLinearVel();
      this->modelVelAng = this->linkProp->GetRelativeAngularVel();

      this->propState.model_name = this->linkProp->GetName();
      this->propState.pose.position.x = this->modelPose.pos.x;
      this->propState.pose.position.y = this->modelPose.pos.y;
      this->propState.pose.position.z = this->modelPose.pos.z;
      this->propState.pose.orientation.x = this->modelPose.rot.x;
      this->propState.pose.orientation.y = this->modelPose.rot.y;
      this->propState.pose.orientation.z = this->modelPose.rot.z;
      this->propState.pose.orientation.w = this->modelPose.rot.w;
      this->propState.twist.linear.x = this->modelVelLin.x;
      this->propState.twist.linear.y = this->modelVelLin.y;
      this->propState.twist.linear.z = this->modelVelLin.z;
      this->propState.twist.angular.x = this->modelVelAng.x;
      this->propState.twist.angular.y = this->modelVelAng.y;
      this->propState.twist.angular.z = this->modelVelAng.z;
      this->propState.reference_frame = "propeller";

      this->rosPub.publish(this->propState);
    }

    public: void getWrenchMotor(const geometry_msgs::Wrench& wrench)
    {
      // ROS_INFO("Received motor force (XYZ): %g\t%g\t%g",wrench.force.x, wrench.force.y, wrench.force.z);
      // ROS_INFO("Received motor torque (XYZ): %g\t%g\t%g",wrench.torque.x, wrench.torque.y, wrench.torque.z);

      math::Quaternion BodyQuat(0,1,0,0); // Rotation quaternion from gazebo body frame to aerospace body frame
      this->relPose = this->linkMot->GetRelativePose();

      math::Vector3 inpForce(wrench.force.x, wrench.force.y, wrench.force.z); // in motor frame
      math::Vector3 inpTorque(wrench.torque.x, wrench.torque.y, wrench.torque.z); // in motor frame

      math::Vector3 newForce = this->relPose.rot.GetInverse()*inpForce; // in gazebo frame
      math::Vector3 newTorque = this->relPose.rot.GetInverse()*newTorque + this->relPose.pos.Cross(newForce);// in gazebo frame

      // ROS_INFO("Converted it to body force (XYZ): %g\t%g\t%g", newForce.x, newForce.y, newForce.z);
      // ROS_INFO("Converted it to body torque (XYZ): %g\t%g\t%g", newTorque.x, newTorque.y, newTorque.z);
      // this->jointAxis->SetVelocity(0,wrench.torque.y); //Abuse of the wrench struct

      this->linkFuse->AddRelativeForce(newForce);
      this->linkFuse->AddRelativeTorque(newTorque);
    }

    // Pointer to the model
    private:
      physics::ModelPtr model;
      math::Pose relPose;

    // Pointer to motor joint
    physics::JointPtr jointAxis;
    // Pointer to propeller link
    physics::LinkPtr linkProp, linkMot, linkFuse;

    // Pointer to the update event connection
    private: event::ConnectionPtr updateConnection;

    // ROS related variables
    private:
      ros::NodeHandle rosHandle;
      ros::Publisher rosPub;
      ros::Subscriber rosSubMotor;
      gazebo_msgs::ModelState propState;
      math::Pose modelPose;
      math::Vector3 modelVelLin, modelVelAng;
  };

  // Register this plugin with the simulator
  GZ_REGISTER_MODEL_PLUGIN(engineControl)
}