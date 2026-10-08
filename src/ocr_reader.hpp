#pragma once
#include "opencv_compat.hpp"
#include "plate_format.hpp"
#include <memory>
#include <string>
namespace tesseract { class TessBaseAPI; }
class OcrReader {
public:
    explicit OcrReader(const std::string& tessdata = "");
    ~OcrReader();
    OcrReader(const OcrReader&) = delete;
    OcrReader& operator=(const OcrReader&) = delete;
    plate::Result recognize(const cv::Mat& roi, const std::string& debugPrefix = "");
    static cv::Mat preprocessForOcr(const cv::Mat& roi, bool adaptive = false);
private:
    std::unique_ptr<tesseract::TessBaseAPI> api_;
};
