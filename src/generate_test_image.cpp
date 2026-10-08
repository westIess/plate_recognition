#include "opencv_compat.hpp"
#include "csv.hpp"
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

// Synthetic only: fixed seed, no clipping of rotated characters, paired labels.
cv::Mat makeCarWithPlate(const std::string& text,int variant) {
    cv::Mat img(480,640,CV_8UC3,cv::Scalar(90,90,95));
    cv::rectangle(img,cv::Rect(90,140,460,220),cv::Scalar(40,60,150),-1);
    cv::Mat plate(60,276,CV_8UC3,cv::Scalar(240,240,240));
    cv::rectangle(plate,cv::Rect(1,1,274,58),cv::Scalar(10,10,10),2);
    int baseline=0;
    auto size=cv::getTextSize(text,cv::FONT_HERSHEY_SIMPLEX,1,2,&baseline);
    double scale=std::min(240.0/size.width,36.0/size.height);
    size=cv::getTextSize(text,cv::FONT_HERSHEY_SIMPLEX,scale,2,&baseline);
    cv::putText(plate,text,cv::Point((276-size.width)/2,(60+size.height)/2),
                cv::FONT_HERSHEY_SIMPLEX,scale,cv::Scalar(10,10,10),2,cv::LINE_AA);
    std::array<cv::Point2f,4> src{{{0,0},{275,0},{275,59},{0,59}}};
    std::array<cv::Point2f,4> dst{{{182,286},{457,286},{457,345},{182,345}}};
    if(variant%3==1) dst={{{184,294},{450,278},{462,337},{183,350}}};
    if(variant%3==2) dst={{{180,275},{455,303},{443,355},{190,335}}};
    auto transform=cv::getPerspectiveTransform(src.data(),dst.data());
    cv::Mat projected,mask,white(plate.size(),CV_8UC1,cv::Scalar(255));
    cv::warpPerspective(plate,projected,transform,img.size(),cv::INTER_CUBIC);
    cv::warpPerspective(white,mask,transform,img.size(),cv::INTER_NEAREST);
    projected.copyTo(img,mask);
    cv::Mat floating,noise(img.size(),CV_32FC3);
    img.convertTo(floating,CV_32FC3);
    cv::RNG rng(42+variant);rng.fill(noise,cv::RNG::NORMAL,0,3);
    floating+=noise;floating.convertTo(img,CV_8UC3);
    cv::GaussianBlur(img,img,cv::Size(3,3),0.4);
    return img;
}
int main(int argc,char** argv) {
    try {
        if(argc>3) throw std::runtime_error("Usage: generate_test_image [OUTPUT_DIR] [COUNT 1..3]");
        std::filesystem::path dir=argc>1?argv[1]:"synthetic_images";
        int count=3;
        if(argc>2) {std::string n=argv[2];std::size_t used;count=std::stoi(n,&used);if(used!=n.size()) throw std::runtime_error("Invalid count");}
        if(count<1 || count>3) throw std::runtime_error("COUNT must be 1..3");
        const std::array<std::string,3> plates{{"A123BC777","O482MP199","K900XX77"}};
        std::filesystem::create_directories(dir);
        std::ofstream truth(dir/"ground_truth.csv");truth.exceptions(std::ios::failbit|std::ios::badbit);
        truth << "filename,plate_number\n";
        for(int i=0;i<count;++i) {
            auto name="car"+std::to_string(i+1)+".jpg";
            if(!cv::imwrite((dir/name).string(),makeCarWithPlate(plates[i],i))) throw std::runtime_error("imwrite failed");
            truth << name << ',' << plates[i] << '\n';
            std::cout << name << " " << plates[i] << '\n';
        }
        truth.close();return 0;
    } catch(const std::exception& e) {std::cerr << e.what() << '\n';return 1;}
}
