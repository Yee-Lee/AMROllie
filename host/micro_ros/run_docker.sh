#! /bin/bash


docker run -it --rm \
    --name micro_ros_agent \
    --privileged \
    --net=host \
    -e ROS_DOMAIN_ID=30 \
    -v /dev:/dev \
    microros/micro-ros-agent:humble \
    serial --dev /dev/ollie_core -b 460800
