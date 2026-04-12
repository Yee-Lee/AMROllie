#! /bin/bash

 docker run -it --rm   --net=host --privileged   -v /dev:/dev   -e ROS_DOMAIN_ID=30   microros/micro-ros-agent:humble serial --dev /dev/serial0 -b 460800
