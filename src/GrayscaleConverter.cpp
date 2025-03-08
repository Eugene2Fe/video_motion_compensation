#include "GrayscaleConverter.hpp"

cv::Mat GrayscaleConverter::convertToGray(const cv::Mat& colorImage) {
    if (colorImage.empty()) {
        throw std::runtime_error("ERROR: empty imput image!");
    }

    cv::Mat grayImage(colorImage.rows, colorImage.cols, CV_8UC1); // Empty matrix 8bit 1ch

    for (int y = 0; y < colorImage.rows; ++y) {
        for (int x = 0; x < colorImage.cols; ++x) {
            // Get for every pixel his colors vector[3]
            cv::Vec3b pixel = colorImage.at<cv::Vec3b>(y, x);

            uint8_t grayValue = static_cast<uint8_t>(0.114 * pixel[0] +  // Blue
                                                     0.587 * pixel[1] +  // Green
                                                     0.299 * pixel[2]);  // Red

            grayImage.at<uint8_t>(y, x) = grayValue;
        }
    }
    return grayImage;
}
