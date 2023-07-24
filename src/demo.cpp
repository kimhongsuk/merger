#include "monitor/demo.hpp"


MonitorDemo::MonitorDemo()
: Node("MonitorDemo")
{
  // Information
  RCLCPP_INFO(this->get_logger(), "Initialization Start.");

  rclcpp::QoS QOS_RKL10V = rclcpp::QoS(rclcpp::KeepLast(10)).reliable().durability_volatile();

  // Subscriber
  using std::placeholders::_1;
  this->image_subscriber_ = this->create_subscription<sensor_msgs::msg::Image>("/camera/raw_image", QOS_RKL10V, std::bind(&MonitorDemo::image_callback, this, _1));
  this->detections_subscriber_ = this->create_subscription<vision_msgs::msg::Detection2DArray>("/detections", QOS_RKL10V, std::bind(&MonitorDemo::detections_receive, this, _1));

  // Information
  RCLCPP_INFO(this->get_logger(), "[Demo] Finish initialization.");
}

MonitorDemo::~MonitorDemo()
{
  RCLCPP_INFO(this->get_logger(), "[Demo] Terminate system.\n");
}

void MonitorDemo::image_callback(const sensor_msgs::msg::Image::SharedPtr image)
{
  cv_bridge::CvImagePtr tmp_cv_image = cv_bridge::toCvCopy(*image, image->encoding);

  if (tmp_cv_image->encoding == "bayer_rggb8")
  {
    cv::Mat rgb8_image;
    cv::cvtColor(tmp_cv_image->image, rgb8_image, cv::COLOR_BayerRG2RGB);
    cv::swap(tmp_cv_image->image, rgb8_image);

    tmp_cv_image->encoding = "rgb8";
  }

  // Append a image on queue
  pthread_mutex_lock(&mutex_image);
  this->image_queue_.push(tmp_cv_image);
  pthread_mutex_unlock(&mutex_image);
}

void MonitorDemo::detections_receive(const vision_msgs::msg::Detection2DArray::SharedPtr detections)
{
  // Select image
  cv_bridge::CvImagePtr cv_image = nullptr;

  pthread_mutex_lock(&mutex_image);

  int detections_stamp = static_cast<int>(rclcpp::Time(detections->header.stamp).seconds() * 1000.0);
  while (this->image_queue_.size())
  {
    int queued_image_stamp = static_cast<int>(rclcpp::Time(this->image_queue_.front()->header.stamp).seconds() * 1000.0);

    if (queued_image_stamp != detections_stamp)
    {
      // Consume image
      if (queued_image_stamp < detections_stamp)
      {
        this->image_queue_.pop();
      }
      if (queued_image_stamp > detections_stamp)
      {
        break;
      }
    }
    else
    {
      // Save image
      cv_image = this->image_queue_.front();

      this->image_queue_.pop();
    }
  }

  pthread_mutex_unlock(&mutex_image);

  // Check image
  if (cv_image == nullptr)
  {
    return;
  }

  // Draw bounding boxes
  draw_image(cv_image, detections);

  // Show image
  cv::imshow("Result image", cv_image->image);
  cv::waitKey(10);
}

void MonitorDemo::draw_image(cv_bridge::CvImagePtr cv_image, const vision_msgs::msg::Detection2DArray::SharedPtr detections)
{
  for (size_t i = 0; i < detections->detections.size(); i++) {
    // Get rectangle from 1 object
    cv::Rect r = cv::Rect(round(detections->detections[i].bbox.center.x - detections->detections[i].bbox.size_x),
                          round(detections->detections[i].bbox.center.y - detections->detections[i].bbox.size_y),
                          round(2 * detections->detections[i].bbox.size_x),
                          round(2 * detections->detections[i].bbox.size_y));

    // draw_box
    cv::rectangle(cv_image->image, r, cv::Scalar(0x27, 0xC1, 0x36), 2);

    // put id
    cv::putText(cv_image->image, detections->detections[i].tracking_id, cv::Point(r.x, r.y - 1), cv::FONT_HERSHEY_PLAIN, 1.2, cv::Scalar(0xFF, 0xFF, 0xFF), 2);
  }
}
