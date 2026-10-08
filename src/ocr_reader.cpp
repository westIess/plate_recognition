#include "ocr_reader.hpp"
#include <tesseract/baseapi.h>
#include <fstream>
#include <stdexcept>

OcrReader::OcrReader(const std::string& tessdata):api_(std::make_unique<tesseract::TessBaseAPI>()) {
    if (api_->Init(tessdata.empty()?nullptr:tessdata.c_str(),"eng",tesseract::OEM_LSTM_ONLY)!=0)
        throw std::runtime_error("Tesseract Init failed: install eng.traineddata or pass --tessdata DIR");
    api_->SetPageSegMode(tesseract::PSM_SINGLE_LINE);
    // Keep common confusions visible to position-aware normalization.
    api_->SetVariable("tessedit_char_whitelist","ABEKMHOPCTYX0123456789QDISZL");
}
OcrReader::~OcrReader() = default;
cv::Mat OcrReader::preprocessForOcr(const cv::Mat& roi, bool adaptive) {
    if (roi.empty()) return {};
    cv::Mat gray;
    if (roi.channels()==3) cv::cvtColor(roi,gray,cv::COLOR_BGR2GRAY);
    else if (roi.channels()==4) cv::cvtColor(roi,gray,cv::COLOR_BGRA2GRAY);
    else gray=roi.clone();
    double scale=std::clamp(96.0/gray.rows,0.25,4.0);
    cv::resize(gray,gray,cv::Size(),scale,scale,cv::INTER_CUBIC);
    cv::GaussianBlur(gray,gray,cv::Size(3,3),0);
    cv::Mat binary;
    if (adaptive) cv::adaptiveThreshold(gray,binary,255,cv::ADAPTIVE_THRESH_GAUSSIAN_C,cv::THRESH_BINARY,31,9);
    else cv::threshold(gray,binary,0,255,cv::THRESH_BINARY|cv::THRESH_OTSU);
    if (cv::mean(binary)[0]<127) cv::bitwise_not(binary,binary);
    cv::copyMakeBorder(binary,binary,10,10,10,10,cv::BORDER_CONSTANT,cv::Scalar(255));
    return binary;
}
plate::Result OcrReader::recognize(const cv::Mat& roi,const std::string& prefix) {
    plate::Result best;
    if (roi.empty()) return best;
    std::ofstream log;
    if (!prefix.empty()) {
        log.open(prefix+"_ocr.txt");
        if (!log) throw std::runtime_error("Cannot write OCR debug log");
    }
    for (bool adaptive : {false,true}) {
        auto binary=preprocessForOcr(roi,adaptive);
        auto suffix=adaptive?"_adaptive.png":"_otsu.png";
        if (!prefix.empty() && !cv::imwrite(prefix+suffix,binary))
            throw std::runtime_error("Cannot write binary debug image");
        api_->Clear();
        api_->SetImage(binary.data,binary.cols,binary.rows,1,static_cast<int>(binary.step));
        api_->SetSourceResolution(300);
        std::unique_ptr<char[]> raw(api_->GetUTF8Text());
        auto result=plate::normalize(raw?raw.get():"",static_cast<float>(api_->MeanTextConf()));
        if (log) log << (adaptive?"adaptive":"otsu") << ": raw=[" << (raw?raw.get():"")
            << "] normalized=" << result.text << " valid=" << result.valid << " corrections="
            << result.corrections << " confidence=" << result.confidence << " score=" << result.score() << '\n';
        if (plate::better(result,best)) best=result;
    }
    return best;
}
