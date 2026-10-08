#include "plate_detector.hpp"
#include "ocr_reader.hpp"
#include "csv.hpp"
#include <algorithm>
#include <chrono>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
namespace fs=std::filesystem;

int main(int argc,char** argv) {
    try {
        if(argc<2 || std::string(argv[1])=="--help") {
            std::cout << "Usage: plate_recognizer INPUT_DIR [CASCADE.xml|-] [OUTPUT.csv]\n"
                         "  [--debug DIR] [--candidates N] [--tessdata DIR]\n";
            return argc<2?1:0;
        }
        fs::path input=argv[1],output="output/results.csv",debug;
        std::string cascade="data/haarcascade_russian_plate_number.xml",tessdata;
        int maxCandidates=6,positional=0;
        for(int i=2;i<argc;++i) {
            std::string arg=argv[i];
            if(arg=="--debug" || arg=="--candidates" || arg=="--tessdata") {
                if(++i==argc) throw std::runtime_error("Missing value for "+arg);
                if(arg=="--debug") debug=argv[i];
                if(arg=="--tessdata") tessdata=argv[i];
                if(arg=="--candidates") {
                    std::string n=argv[i];std::size_t used=0;
                    maxCandidates=std::stoi(n,&used);
                    if(used!=n.size() || maxCandidates<1 || maxCandidates>100)
                        throw std::runtime_error("--candidates must be 1..100");
                }
            } else if(arg.rfind("--",0)==0) throw std::runtime_error("Unknown option: "+arg);
            else if(positional++==0) cascade=arg;
            else if(positional==2) output=arg;
            else throw std::runtime_error("Too many positional arguments");
        }
        if(!fs::is_directory(input)) throw std::runtime_error("Input is not a directory");
        std::vector<fs::path> images;
        for(const auto& e:fs::directory_iterator(input)) {
            auto ext=e.path().extension().string();
            std::transform(ext.begin(),ext.end(),ext.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
            if(e.is_regular_file() && (ext==".jpg" || ext==".jpeg" || ext==".png" || ext==".bmp")) images.push_back(e.path());
        }
        std::sort(images.begin(),images.end());
        if(images.empty()) throw std::runtime_error("No images in input directory");
        PlateDetector detector(cascade); OcrReader ocr(tessdata);
        if(!output.parent_path().empty()) fs::create_directories(output.parent_path());
        if(!debug.empty()) fs::create_directories(debug);
        std::ofstream out(output);out.exceptions(std::ios::failbit|std::ios::badbit);
        out << "filename,plate_number,confidence,time_ms\n" << std::fixed << std::setprecision(3);
        int failures=0;
        for(std::size_t index=0;index<images.size();++index) {
            const auto& path=images[index];
            const auto start=std::chrono::steady_clock::now();
            plate::Result best;
            try {
                auto frame=cv::imread(path.string());
                if(frame.empty()) throw std::runtime_error("Cannot decode image");
                cv::Mat gray;cv::cvtColor(frame,gray,cv::COLOR_BGR2GRAY);
                auto boxes=detector.detect(gray);
                fs::path frameDebug;
                if(!debug.empty()) {
                    // Full filename and index avoid collisions (car.jpg / car.png).
                    frameDebug=debug/(std::to_string(index)+"_"+path.filename().string());
                    fs::create_directories(frameDebug);
                    auto annotated=frame.clone();
                    for(std::size_t i=0;i<boxes.size();++i) {
                        cv::rectangle(annotated,boxes[i],cv::Scalar(0,255,0),2);
                        cv::putText(annotated,std::to_string(i),boxes[i].tl(),cv::FONT_HERSHEY_SIMPLEX,0.6,cv::Scalar(0,0,255),2);
                    }
                    if(!cv::imwrite((frameDebug/"boxes.png").string(),annotated)) throw std::runtime_error("Cannot save boxes");
                }
                for(std::size_t i=0;i<boxes.size() && i<static_cast<std::size_t>(maxCandidates);++i) {
                    auto roi=PlateDetector::extractAndDeskew(frame,boxes[i]);
                    if(roi.empty()) continue;
                    std::string prefix;
                    if(!frameDebug.empty()) {
                        prefix=(frameDebug/("candidate_"+std::to_string(i))).string();
                        if(!cv::imwrite(prefix+"_crop.png",frame(boxes[i])) || !cv::imwrite(prefix+"_warp.png",roi))
                            throw std::runtime_error("Cannot save candidate images");
                    }
                    auto result=ocr.recognize(roi,prefix);
                    if(plate::better(result,best)) best=result;
                }
            } catch(const std::exception& e) {
                ++failures;best={};std::cerr << path.filename() << ": " << e.what() << '\n';
            }
            const double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
            out << csv::quote(path.filename().string()) << ',' << (best.valid?best.text:"") << ','
                << (best.valid?best.confidence:0) << ',' << ms << '\n';
            std::cout << path.filename().string() << " -> " << (best.valid?best.text:"[unrecognized]") << '\n';
        }
        out.close();
        return failures?2:0;
    } catch(const std::exception& e) {std::cerr << "Error: " << e.what() << '\n';return 1;}
}
