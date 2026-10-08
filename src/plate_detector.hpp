#pragma once
#include "opencv_compat.hpp"
#include <array>
#include <string>
#include <vector>

class PlateDetector {
public:
    explicit PlateDetector(const std::string& cascadePath);
    std::vector<cv::Rect> detect(const cv::Mat& gray) const;
    static cv::Mat extractAndDeskew(const cv::Mat& frame, const cv::Rect& box);
    // Input corners can have any cyclic order; output TL, TR, BR, BL.
    static std::array<cv::Point2f,4> orderCorners(std::array<cv::Point2f,4> points);
    static cv::Mat warpPlate(const cv::Mat& frame, std::array<cv::Point2f,4> points);
private:
#if PLATE_HAS_CASCADE
    mutable cv::CascadeClassifier cascade_;
#endif
    bool cascadeLoaded_ = false;
    std::vector<cv::Rect> detectWithCascade(const cv::Mat& gray) const;
    std::vector<cv::Rect> detectWithContours(const cv::Mat& gray) const;
};
