#include "ros/ros.h"
#include "std_msgs/String.h"

#include <sstream>

class SubPub
{
    public: SubPub()
    {
        pub = nodeHandle.advertise<std_msgs::String>("/ws", 1);

        sub = nodeHandle.subsciribe("/ws", 1, &SubPub::callback, this);
    }

    void callback(const std_msgs::String &input)
    {
        std_msgs::String output;
        //.... do something with the input and generate the output...
        pub.publish(output);
    }

    private:
        ros::NodeHandle nodeHandle;
        ros::Publisher pub;
        ros::Subscriber sub
};