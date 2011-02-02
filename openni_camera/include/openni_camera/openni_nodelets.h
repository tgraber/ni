/*
 * Software License Agreement (BSD License)
 *
 *  Copyright (c) 2011
 *    Suat Gedikli <gedikli@willowgarage.com>
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
#ifndef OPENNI_NODELET_OPENNI_H_
#define OPENNI_NODELET_OPENNI_H_

#include <nodelet/nodelet.h>
#include "openni_camera/openni_driver.h"
#include <boost/shared_ptr.hpp>
#include <dynamic_reconfigure/server.h>
#include <openni_camera/OpenNIConfig.h>
#include <image_transport/image_transport.h>

namespace openni_camera
{
  ////////////////////////////////////////////////////////////////////////////////////////////
  class OpenNINodelet : public nodelet::Nodelet
  {
    public:
      virtual ~OpenNINodelet ()
      {
      }
    private:
      typedef OpenNIConfig Config;
      typedef dynamic_reconfigure::Server<Config> ReconfigureServer;

      /** \brief Nodelet initialization routine. */
      virtual void onInit ();

      void configCallback (Config &config, uint32_t level);
      int mapXnMode2ConfigMode (const XnMapOutputMode& output_mode) const;
      XnMapOutputMode mapConfigMode2XnMode (int mode) const;

      // callback methods
      void imageCallback (const openni_wrapper::Image& image, void* cookie);

      // published topics
      image_transport::Publisher pub_rgb_image_, pub_gray_image_, pub_depth_image_;

      // publish methods
      void publishRgbImageColor (const openni_wrapper::Image& image, ros::Time time) const;
      void publishRgbImageMono (const openni_wrapper::Image& image, ros::Time time) const;

      /** \brief the actual openni device*/
      boost::shared_ptr<openni_wrapper::OpenNIDevice> device_;

      ReconfigureServer reconfigure_server_;
      Config config_;

      string rgb_frame_id_;
      string depth_frame_id_;
      unsigned image_width_;
      unsigned image_height_;
      unsigned depth_width_;
      unsigned depth_height_;
  };
}

#endif  //#ifndef OPENNI_NODELET_OPENNI_H_
