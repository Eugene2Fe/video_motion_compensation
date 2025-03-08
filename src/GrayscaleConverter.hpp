#ifndef GRAYSCALE_CONVERTER_H
#define GRAYSCALE_CONVERTER_H

#include <opencv2/opencv.hpp>

class GrayscaleConverter {
   public:
    static cv::Mat convertToGray(const cv::Mat& colorImage);
};

#endif  // GRAYSCALE_CONVERTER_H
