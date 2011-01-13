/*
 * Software License Agreement (BSD License)
 *
 *  Copyright (c) 2010
 *    Radu Bogdan Rusu <rusu@willowgarage.com>
 *    Suat Gedikli <gedikli@willowgarage.com>
 *    Patrick Mihelich <mihelich@willowgarage.com>
 * 
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   * Neither the name of Willow Garage, Inc. nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *
 */

/// @todo Organize includes
#include <openni_camera/openni_driver.h>
#include <openni_camera/openni_device.h>
#include <sstream>
#include <sensor_msgs/image_encodings.h>
#include <sensor_msgs/fill_image.h>
#include <image_transport/image_transport.h>
#include <boost/make_shared.hpp>
#include <XnContext.h>
#include <openni_camera/OpenNIConfig.h>
#include <sensor_msgs/Image.h>
#include <sensor_msgs/CameraInfo.h>
#include <sensor_msgs/Imu.h>
#include <stereo_msgs/DisparityImage.h>

#include <ros/ros.h>
#include <message_filters/synchronizer.h>
#include <message_filters/sync_policies/approximate_time.h>
#include <image_transport/image_transport.h>
#include <dynamic_reconfigure/server.h>
#include <openni_camera/OpenNIConfig.h>

// Branch on whether we have the changes to CameraInfo in unstable
#if ROS_VERSION_MINIMUM(1, 3, 0)
#include <sensor_msgs/distortion_models.h>
#endif

using namespace openni_wrapper;
using namespace std;
using namespace ros;

namespace openni_camera
{

class OpenNINode
{
public:
  //typedef pcl::PointCloud<pcl::PointXYZRGB> PointCloud;
  typedef OpenNIConfig Config;
  typedef dynamic_reconfigure::Server<Config> ReconfigureServer;

  typedef union
  {

    struct /*anonymous*/
    {
      unsigned char Blue; // Blue channel
      unsigned char Green; // Green channel
      unsigned char Red; // Red channel
      unsigned char Alpha; // Alpha channel
    };
    float float_value;
    long long_value;
  } RGBValue;
public:
  OpenNINode (NodeHandle comm_nh, NodeHandle param_nh, boost::shared_ptr<OpenNIDevice> device, const string& topic);
  ~OpenNINode ();
  void run ();

protected:
  void imageCallback (const Image& image, void* cookie);
  void depthCallback (const DepthImage& depth, void* cookie);
  void configCallback (Config &config, uint32_t level);
  void updateDeviceSettings (const Config &config); /// @todo Not implemented
  int mapMode (const XnMapOutputMode& output_mode) const;
  void mapMode (int mode, XnMapOutputMode& output_mode) const;
  unsigned getFPS (int mode) const;
  unsigned image_width_;
  unsigned image_height_;
  unsigned depth_width_;
  unsigned depth_height_;
  boost::shared_ptr<OpenNIDevice> device_;
  ros::Publisher pub_rgb_info_;
  image_transport::Publisher pub_rgb_image_, pub_gray_image_;
  image_transport::CameraPublisher pub_depth_image_;
  ros::Publisher pub_disparity_;
  ros::Publisher pub_depth_points2_;
  bool is_primesense_device_;

  string topic_;
  NodeHandle comm_nh_;
  NodeHandle param_nh_;
  ReconfigureServer reconfigure_server_;

  static const string rgb_frame_id_;
  static const string depth_frame_id_;
};

/// @todo These need to be parameters
const string OpenNINode::rgb_frame_id_ = "openni_rgb_optical_frame";
const string OpenNINode::depth_frame_id_ = "openni_depth_optical_frame";

OpenNINode::OpenNINode (NodeHandle comm_nh, NodeHandle param_nh,
                        boost::shared_ptr<OpenNIDevice> device, const string& topic)
  : device_ (device)
  , topic_ (topic)
  , comm_nh_ (comm_nh)
  , param_nh_ (param_nh)
  , reconfigure_server_ (param_nh)
{
  ReconfigureServer::CallbackType reconfigure_callback =
    boost::bind (&OpenNINode::configCallback, this, _1, _2);
  reconfigure_server_.setCallback (reconfigure_callback);

  image_transport::ImageTransport image_transport (comm_nh);
  /// @todo 15 looks like overkill for the queue size
  pub_rgb_info_    = comm_nh_.advertise<sensor_msgs::CameraInfo> ("rgb/camera_info", 15);
  pub_rgb_image_   = image_transport.advertise ("rgb/image_color", 15);
  pub_gray_image_  = image_transport.advertise ("rgb/image_mono", 15);
  pub_depth_image_ = image_transport.advertiseCamera ("depth/image", 15);
  pub_disparity_   = comm_nh_.advertise<stereo_msgs::DisparityImage > ("depth/disparity", 15);
  // pub_depth_points2_ = comm_nh.advertise<PointCloud > ("depth/points2", 15);

  /// @todo Restore image/depth sync code
  //SyncPolicy sync_policy (4); // queue size
  //depth_rgb_sync_.reset (new Synchronizer (sync_policy));
  //depth_rgb_sync_->registerCallback (boost::bind (&OpenNIDriver::publishXYZRGBPointCloud, this, _1, _2));


  /// @todo Is this done in configCallback?
  XnMapOutputMode output_mode;
  device_->getImageOutputMode (output_mode);
  image_width_ = output_mode.nXRes;
  image_height_ = output_mode.nYRes;

  device_->getDepthOutputMode (output_mode);
  depth_width_ = output_mode.nXRes;
  image_height_ = output_mode.nYRes;

  // registering callback functions
  device_->registerImageCallback (&OpenNINode::imageCallback, *this, NULL);
  //device_->registerDepthCallback (&OpenNINode::depthCallback, *this, NULL);

  /// @todo Control by dynamic reconfigure
  //device_->setDepthRegistration (true);
  
  /// @todo Start and stop as needed
  device_->startImageStream ();
  //device_->startDepthStream ();
  //device_->startImageStream ();
}

OpenNINode::~OpenNINode ()
{

}

void OpenNINode::imageCallback (const Image& image, void* cookie)
{
  /// @todo Some sort of offset based on the device timestamp
  ros::Time time = ros::Time::now();

  /// @todo Only create when subscribed to
  sensor_msgs::CameraInfoPtr rgb_info = boost::make_shared<sensor_msgs::CameraInfo>();
  rgb_info->header.stamp    = time;
  rgb_info->header.frame_id = rgb_frame_id_;
  // No distortion (yet!)
#if ROS_VERSION_MINIMUM(1, 3, 0)
  rgb_info->D = std::vector<double>( 5, 0.0 );
  rgb_info->distortion_model = sensor_msgs::distortion_models::PLUMB_BOB;
#else
  rgb_info->D.assign( 0.0 );
#endif
  rgb_info->K.assign( 0.0 );
  rgb_info->R.assign( 0.0 );
  rgb_info->P.assign( 0.0 );
  // Simple camera matrix: square pixels, principal point at center
  double f = device_->getImageFocalLength(image_width_);
  rgb_info->K[0] = rgb_info->K[4] = f;
  rgb_info->K[2] = (image_width_  / 2) - 0.5;
  rgb_info->K[5] = (image_height_ / 2) - 0.5;
  rgb_info->K[8] = 1.0;
  // no rotation: identity
  rgb_info->R[0] = rgb_info->R[4] = rgb_info->R[8] = 1.0;
  // no rotation, no translation => P=K(I|0)=(K|0)
  rgb_info->P[0]    = rgb_info->P[5] = rgb_info->K[0];
  rgb_info->P[2]    = rgb_info->K[2];
  rgb_info->P[6]    = rgb_info->K[5];
  rgb_info->P[10]   = 1.0;
  rgb_info->width   = image_width_;
  rgb_info->height  = image_height_;

  pub_rgb_info_.publish(rgb_info);

  /// @todo Only create when RGB image is needed
  sensor_msgs::ImagePtr rgb_msg = boost::make_shared<sensor_msgs::Image>();
  rgb_msg->header.stamp    = time;
  rgb_msg->header.frame_id = rgb_frame_id_;
  rgb_msg->encoding = sensor_msgs::image_encodings::RGB8;
  rgb_msg->height   = image_height_;
  rgb_msg->width    = image_width_;
  rgb_msg->step     = image_width_ * 3;
  rgb_msg->data.resize (rgb_msg->height * rgb_msg->step);
  image.fillRGB(rgb_msg->width, rgb_msg->height, &rgb_msg->data[0], rgb_msg->step);

  pub_rgb_image_.publish(rgb_msg);

  /// @todo Also publish monochrome image if subscribed to
}

void OpenNINode::depthCallback (const DepthImage& depth, void* cookie)
{

}

int OpenNINode::mapMode (const XnMapOutputMode& output_mode) const
{
  if (output_mode.nXRes == 1280 && output_mode.nYRes == 1024)
    return OpenNI_SXGA_15Hz;
  else if (output_mode.nXRes == 640 && output_mode.nYRes == 480)
    return OpenNI_VGA_30Hz;
  else if (output_mode.nXRes == 320 && output_mode.nYRes == 240)
    return OpenNI_QVGA_30Hz;
  else if (output_mode.nXRes == 160 && output_mode.nYRes == 120)
    return OpenNI_QQVGA_30Hz;
  else
  {
    ROS_ERROR ("image resolution unknwon");
    exit (-1);
  }
  return -1;
}

void OpenNINode::mapMode (int resolution, XnMapOutputMode& output_mode) const
{
  switch (resolution)
  {
    case OpenNI_SXGA_15Hz: output_mode.nXRes = 1280;
      output_mode.nYRes = 1024;
      output_mode.nFPS = 15;
      break;
      break;
    case OpenNI_VGA_30Hz: output_mode.nXRes = 640;
      output_mode.nYRes = 480;
      output_mode.nFPS = 30;
      break;
      break;
    case OpenNI_QVGA_30Hz: output_mode.nXRes = 320;
      output_mode.nYRes = 240;
      output_mode.nFPS = 30;
      break;
      break;
    case OpenNI_QQVGA_30Hz: output_mode.nXRes = 160;
      output_mode.nYRes = 120;
      output_mode.nFPS = 30;
      break;
      break;
    default:
      ROS_ERROR ("image resolution unknwon");
      exit (-1);
      break;
  }
}

unsigned OpenNINode::getFPS (int mode) const
{
  switch (mode)
  {
    case OpenNI_SXGA_15Hz: return 15;
      break;
    case OpenNI_VGA_30Hz:
    case OpenNI_QVGA_30Hz:
    case OpenNI_QQVGA_30Hz:
      return 30;
      break;
    default:
      return 0;
      break;
  }
}

void OpenNINode::configCallback (Config &config, uint32_t level)
{
  // check if image resolution is supported
  XnMapOutputMode output_mode, compatible_mode;
  mapMode (config.image_mode, output_mode);
  if (!device_->findFittingImageMode (output_mode, compatible_mode))
  {
    device_->getDefaultImageMode (compatible_mode);
    ROS_WARN ("Could not find any compatible image output mode %d x %d @ %d. "
              "Falling back to default mode %d x %d @ %d.",
              output_mode.nXRes, output_mode.nYRes, output_mode.nFPS,
              compatible_mode.nXRes, compatible_mode.nYRes, compatible_mode.nFPS);

    config.image_mode = mapMode (compatible_mode);
    image_height_ = compatible_mode.nYRes;
    image_width_ = compatible_mode.nXRes;
  }
  else
  { // we found a compatible mode => set image width and height as well as the mode of image stream
    image_height_ = output_mode.nYRes;
    image_width_ = output_mode.nXRes;
  }
  device_->setImageOutputMode (compatible_mode);


  mapMode (config.depth_mode, output_mode);
  if (!device_->findFittingImageMode (output_mode, compatible_mode))
  {
    device_->getDefaultDepthMode (compatible_mode);
    ROS_WARN ("Could not find any compatible depth output mode %d x %d @ %d. "
              "Falling back to default mode %d x %d @ %d.",
              output_mode.nXRes, output_mode.nYRes, output_mode.nFPS,
              compatible_mode.nXRes, compatible_mode.nYRes, compatible_mode.nFPS);

    // reset to compatible mode
    config.depth_mode = mapMode (compatible_mode);
    depth_width_ = compatible_mode.nXRes;
    depth_height_ = compatible_mode.nYRes;
  }
  else
  {
    depth_width_ = output_mode.nXRes;
    depth_height_ = output_mode.nYRes;
  }
  device_->setDepthOutputMode (compatible_mode);

  // now
  /// @todo Registration?
}

void OpenNINode::run ()
{
  //while (comm_nh_.ok ());
  ros::spin();
}

} // namespace

using namespace openni_camera;

int main (int argc, char **argv)
{
  // Init ROS
  init (argc, argv, "openni_node");

  NodeHandle comm_nh ("openni_camera"); // for topics, services
  NodeHandle param_nh ("~"); // for parameters
  string deviceID = "";
  param_nh.getParam ("deviceID", deviceID);
  string topic = "";
  param_nh.getParam ("topic", topic);

  try
  {
    OpenNIDriver& driver = OpenNIDriver::getInstance ();
    if (driver.getNumberDevices () == 0)
    {
      ROS_ERROR ("No devices connected.");
      exit (-1);
    }
    ROS_INFO ("Number devices connected: %d", driver.getNumberDevices ());

    boost::shared_ptr<OpenNIDevice> device;
    if (deviceID == "")
    {
      ROS_WARN ("%s deviceID is not set! Using first device.", argv[0]);
      device = driver.getDeviceByIndex (0);
    }
    else
    {
      if (deviceID.find ('@') != string::npos)
      {
        cout << "search by address" << endl;
        size_t pos = deviceID.find ('@');
        unsigned bus = atoi (deviceID.substr (0, pos).c_str ());
        unsigned address = atoi (deviceID.substr (pos + 1, deviceID.length () - pos - 1).c_str ());
        ROS_INFO ("searching for device with bus@address = %d@%d", bus, address);
        device = driver.getDeviceByAddress (bus, address);
      }
      else if (deviceID.length () > 2)
      {
        cout << "search by serial number" << endl;
        ROS_INFO ("searching for device with serial number = %s", deviceID.c_str ());
        device = driver.getDeviceBySerialNumber (deviceID);
      }
      else
      {
        cout << "search by index" << endl;
        unsigned index = atoi (deviceID.c_str ());
        ROS_INFO ("searching for device with index = %d", index);
        device = driver.getDeviceByIndex (index);
      }
    }

    if (!device)
    {
      ROS_ERROR ("%s No matching device found.", argv[0]);
      exit (-1);
    }
    else
    {
      unsigned short vendor_id, product_id;
      unsigned char bus, address;
      device->getDeviceInfo (vendor_id, product_id, bus, address);

      string vendor_name = "unknown";
      if (vendor_id == 0x1d27)
        vendor_name = "Primesense";
      else if (vendor_id == 0x45e)
        vendor_name = "Kinect";

      ROS_INFO ("opened a %s device on bus %d:%d with serial number %s", vendor_name.c_str (), bus, address, device->getSerialNumber ());
    }

    OpenNINode openni_node (comm_nh, param_nh, device, topic);
    openni_node.run ();
  }
  catch (const OpenNIException& exception)
  {
    ROS_ERROR ("%s caught OpenNIException: %s", argv[0], exception.what ());
    exit (-1);
  }
  catch (...)
  {
    ROS_ERROR ("%s caught unknwon exception: ", argv[0]);
    exit (-1);
  }
  return (0);
}
