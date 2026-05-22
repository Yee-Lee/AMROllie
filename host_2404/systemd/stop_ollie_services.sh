#! /bin/bash

sudo systemctl stop ollie_description.service
sudo systemctl stop ollie_lidar.service
sudo systemctl stop ollie_microros.service

./ollie_status.sh
