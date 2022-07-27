// Copyright 2021 Kazuki Ohtake <okzk.phys@gmail.com>
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <Arduino.h>
#include <M5Atom.h>
#include <WiFi.h>
#include <micro_ros_arduino.h>
#include <rcl/error_handling.h>
#include <rcl/rcl.h>
#include <rclc/executor.h>
#include <rclc/rclc.h>
#include <sensor_msgs/msg/imu.h>
#include <stdio.h>

#define RCCHECK(fn)                \
  {                                \
    rcl_ret_t temp_rc = fn;        \
    if ((temp_rc != RCL_RET_OK)) { \
      error_loop();                \
    }                              \
  }

rcl_allocator_t allocator;
rclc_support_t support;
rcl_node_t node;
rcl_publisher_t imu_publisher;
rcl_timer_t imu_timer;
rclc_executor_t executor;
sensor_msgs__msg__Imu imu_msg;

void error_loop()
{
  while (1) {
    M5.dis.fillpix(0xff0000);
    delay(1000);
    M5.dis.fillpix(0x000000);
    delay(1000);
  }
}

void imu_timer_callback(rcl_timer_t * timer, int64_t last_call_time)
{
  RCLC_UNUSED(last_call_time);

  float gx = 0.0;
  float gy = 0.0;
  float gz = 0.0;
  float ax = 0.0;
  float ay = 0.0;
  float az = 0.0;

  M5.IMU.getAccelData(&ax, &ay, &az);
  M5.IMU.getGyroData(&gx, &gy, &gz);

  imu_msg.linear_acceleration.x = ax;
  imu_msg.linear_acceleration.y = ay;
  imu_msg.linear_acceleration.z = az;
  imu_msg.angular_velocity.x = gx;
  imu_msg.angular_velocity.y = gy;
  imu_msg.angular_velocity.z = gz;

  RCCHECK(rcl_publish(&imu_publisher, &imu_msg, NULL));
}

void setup(void)
{
  // Initialize M5 ATOM
  M5.begin(true, false, true);
  M5.IMU.Init();
  M5.dis.begin();
  M5.IMU.SetAccelFsr(M5.IMU.AFS_4G);     // 2, 4, 8, 16
  M5.IMU.SetGyroFsr(M5.IMU.GFS_500DPS);  // 250, 500, 1000, 2000

  Serial.begin(115200);
  set_microros_transports();

  delay(2000);

  // Initialize micro-ROS
  allocator = rcl_get_default_allocator();
  RCCHECK(rclc_support_init(&support, 0, NULL, &allocator));
  RCCHECK(rclc_node_init_default(&node, "m5_atom_node", "", &support));
  RCCHECK(rclc_publisher_init_default(
    &imu_publisher, &node, ROSIDL_GET_MSG_TYPE_SUPPORT(sensor_msgs, msg, Imu), "imu/data"));

  const unsigned int imu_timer_period = RCL_MS_TO_NS(1000);  // 1 [Hz]
  RCCHECK(rclc_timer_init_default(&imu_timer, &support, imu_timer_period, imu_timer_callback));
  RCCHECK(rclc_executor_init(&executor, &support.context, 1, &allocator));
  RCCHECK(rclc_executor_add_timer(&executor, &imu_timer));
}

void loop(void) { rclc_executor_spin_some(&executor, RCL_MS_TO_NS(500)); }
