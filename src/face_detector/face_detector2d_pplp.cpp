/*!
  \file        face_detector2d_pplp.cpp
  \author      Arnaud Ramey <arnaud.a.ramey@gmail.com>
                -- Robotics Lab, University Carlos III of Madrid
  \date        2014/2/14

________________________________________________________________________________

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
________________________________________________________________________________

\todo Description of the file

\section Parameters
  - \b rgb_topic
    [std::string] (default: "rgb")
    where to get the rgb images

\section Subscriptions
  - \b {rgb_topic}
    [sensor_msgs/Image]
    The rgb image

\section Publications
  - \b "~ppl"
        [people_msgs::People]
        The found people heads
 */
// AD
#include "vision_utils/images2ppl.h"
#include "vision_utils/opencv_face_detector.h"
#include "vision_utils/rect_center.h"
#include "vision_utils/rectangle_intersection.h"
#include "vision_utils/remove_including_rectangles.h"
#include "vision_utils/rgb_skill.h"
#include "vision_utils/timer.h"

class FaceDetector2DPPLP : public vision_utils::RgbSkill {
public:
  typedef people_msgs::People PPL;
  typedef cv::Point3d Pt3d;

  FaceDetector2DPPLP() : RgbSkill("FACE_DETECTOR2D_PPLP_START", "FACE_DETECTOR2D_PPLP_STOP")
  {
    _cascade_classifier = vision_utils::create_face_classifier();

    // get camera model
    image_geometry::PinholeCameraModel rgb_camera_model;
    vision_utils::read_camera_model_files
        (vision_utils::DEFAULT_KINECT_SERIAL(), _default_depth_camera_model, rgb_camera_model);

    //  PPLPublisherTemplate
    _ppl_resolved_topic = _nh_private.resolveName("ppl");
    // TODO change advertise in create_subscribers_and_publishers()
    _ppl_pub = _nh_public.advertise<PPL>(_ppl_resolved_topic, 1);
  }

  //////////////////////////////////////////////////////////////////////////////

  virtual void create_subscribers_and_publishers() {
    // params
    // 320 240 1.2 1 10: 1424 ms
    // 320 240 1.3 0 5: 1044 ms
    _nh_private.param("resize_max_width", _resize_max_width, 320);
    _nh_private.param("resize_max_height", _resize_max_height, 240);
    _nh_private.param("scale_factor", _scale_factor, 1.2);
    _nh_private.param("min_neighbors", _min_neighbors, 1);
    _nh_private.param("min_width", _min_width, 10);

    printf("FaceDetector2DPPLP: getting rgb on '%s', "
           "publish People results on '%s'\n",
           get_rgb_topic().c_str(), _ppl_resolved_topic.c_str());
  }

  //////////////////////////////////////////////////////////////////////////////

  virtual void shutdown_subscribers_and_publishers() {}

  ///////////////////////////////////////////////////////////////////////////////

  virtual void process_rgb(const cv::Mat3b & rgb) {
    vision_utils::Timer timer;
    // clear data
    _faces2d.clear();
    _faces_centers_3d.clear();

    // detect with opencv
    vision_utils::detect_with_opencv
        (rgb, _cascade_classifier,
         _small_img, _faces2d,
         _resize_max_width, _resize_max_height, _scale_factor,
         _min_neighbors, _min_width);
    DEBUG_PRINT("time for detect_with_opencv(): %g ms, %i faces\n",
               timer.time(), _faces2d.size());

    // remove including faces
    std::vector< cv::Rect > faces_not_filtered_orig = _faces2d;
    vision_utils::remove_including_rectangles
        (faces_not_filtered_orig, _faces2d);
    DEBUG_PRINT("time for remove_including_rectangles(): %g ms, %i faces\n",
               timer.time(), _faces2d.size());

    _faces_centers_3d.resize(_faces2d.size());
      // 3D ray
    for (unsigned int user_idx = 0; user_idx < _faces2d.size(); ++user_idx) {
      double depth = 1.; // meter, see if we can find something more clever
      cv::Point2d center = vision_utils::rect_center<cv::Rect, cv::Point2d>
                           (_faces2d[user_idx]);
      _faces_centers_3d[user_idx] =vision_utils::pixel2world_depth<Pt3d>
                                   (center, _default_depth_camera_model, depth);
    }

    if (_ppl_pub.getNumSubscribers() > 0)
      build_ppl_message(rgb);
    //if (_display)
      //display(rgb, depth);

    DEBUG_PRINT("time for process_rgb(): %g ms\n", timer.time());
  } // end process_rgb();

  //////////////////////////////////////////////////////////////////////////////

  /*! share the poses of the detected users in a
   *  people_msgs::People msg */
  void build_ppl_message(const cv::Mat3b & rgb) {
    // printf("build_ppl_message()\n");
    // share the poses
    unsigned int n_faces = _faces_centers_3d.size();
    _ppl.header = _images_header;
    _ppl.people.resize(n_faces);

    // build the Person
    cv::Mat3b face_img;
    face_img.create(rgb.size());
    for (unsigned int user_idx = 0; user_idx < n_faces; ++user_idx) {
      people_msgs::Person* pp = &(_ppl.people[user_idx]);
      vision_utils::set_method(*pp, "face_detector2d");
      // people_pose.name = vision_utils::cast_to_string(user_idx);
      pp->name = "NOREC";
      pp->reliability = 1;

      // pose
      vision_utils::copy3(_faces_centers_3d[user_idx], pp->position);

      // image
      face_img.setTo(0);
      rgb(_faces2d[user_idx]).copyTo(face_img);
      if (!_images2pp.rgb2user_and_convert(*pp, &face_img, NULL, false))
        continue;
    } // end loop user_idx
    _ppl_pub.publish(_ppl);
  } // end build_ppl_message()

  //////////////////////////////////////////////////////////////////////////////

  virtual void display(const cv::Mat3b & rgb) {
    rgb.copyTo(_img_out);
    // kept faces in green
    for (unsigned int user_idx = 0; user_idx < _faces2d.size(); ++user_idx)
      cv::rectangle(_img_out, _faces2d[user_idx], CV_RGB(0, 255, 0), 2);

    cv::imshow("rgb", rgb);
    cv::imshow("FaceDetector2DPPLP", _img_out);
    int key_code = (char) cv::waitKey(1);
    if (key_code == 27)
      exit(-1);
  }

  //////////////////////////////////////////////////////////////////////////////

private:
  //  PPLPublisherTemplate
  std::string _ppl_resolved_topic;
  ros::Publisher _ppl_pub;
  unsigned int _ppl_sent_nb;

  /* face detection */
  //! the redim image
  cv::Mat3b _small_img;
  //! the list of faces found
  std::vector< cv::Rect > _faces2d;
  //! the classifier
  cv::CascadeClassifier _cascade_classifier;
  int _resize_max_width, _resize_max_height, _min_neighbors, _min_width;
  double _scale_factor;

  /* reprojection and filtering */
  double _max_sample_depth, _max_sample_width;
  image_geometry::PinholeCameraModel _default_depth_camera_model;

  // shared data! the three vectors have the same size
  //! the list of faces, with ROS orientation, in RGB frame
  std::vector<Pt3d> _faces_centers_3d;

  /* visu */
  //! an image for drawing stuff
  cv::Mat3b _img_out;

  // PPL
  people_msgs::People _ppl;
  vision_utils::Images2PP _images2pp;
}; // end class FaceDetector2DPPLP

////////////////////////////////////////////////////////////////////////////////

int main(int argc, char** argv) {
  ros::init(argc, argv, "FaceDetector2DPPLP");
  FaceDetector2DPPLP skill;
  skill.check_autostart();
  ros::spin();
  return 0;
}
