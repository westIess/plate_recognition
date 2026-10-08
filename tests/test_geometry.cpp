#include "plate_detector.hpp"
#include <algorithm>
#include <iostream>
#include <filesystem>
#include <stdexcept>
void check(bool ok,const char* message) {if(!ok) throw std::runtime_error(message);}
int main(int argc, char** argv) {
    try {
        cv::Mat frame(200,500,CV_8UC3,cv::Scalar(0,0,0));
        std::array<cv::Point2f,4> q{{{50,50},{430,70},{410,155},{70,130}}};
        std::array<cv::Scalar,4> colors{{{0,0,255},{0,255,0},{255,0,0},{0,255,255}}};
        for(int i=0;i<4;++i) cv::circle(frame,q[i],12,colors[i],-1);
        std::array<int,4> permutation{{0,1,2,3}};
        do {
            std::array<cv::Point2f,4> shuffled;
            for(int i=0;i<4;++i) shuffled[i]=q[permutation[i]];
            auto warped=PlateDetector::warpPlate(frame,shuffled);
            check(!warped.empty() && warped.cols>warped.rows*3,"warp dimensions");
            std::array<cv::Point,4> corners{{{1,1},{warped.cols-2,1},{warped.cols-2,warped.rows-2},{1,warped.rows-2}}};
            for(int i=0;i<4;++i) {
                auto pixel=warped.at<cv::Vec3b>(corners[i]);
                for(int c=0;c<3;++c) check(std::abs(pixel[c]-colors[i][c])<40,"corner orientation");
            }
        } while(std::next_permutation(permutation.begin(),permutation.end()));
        check(PlateDetector::warpPlate(frame,{{{1,1},{1,1},{2,2},{2,2}}}).empty(),"degenerate rejection");
        cv::Mat blank(80,300,CV_8UC1,cv::Scalar(120));
        check(!PlateDetector::extractAndDeskew(blank,cv::Rect(-10,-10,250,70)).empty(),"clipped fallback");
        check(PlateDetector::extractAndDeskew(blank,cv::Rect(500,500,20,20)).empty(),"outside box");
        // Contour extraction and warp should flatten a real trapezoid boundary.
        cv::Mat trapezoid(200,500,CV_8UC3,cv::Scalar(20,20,20));
        std::vector<cv::Point> polygon{{50,50},{430,70},{410,155},{70,130}};
        cv::fillConvexPoly(trapezoid,polygon,cv::Scalar(240,240,240));
        auto extracted=PlateDetector::extractAndDeskew(trapezoid,cv::boundingRect(polygon));
        check(!extracted.empty() && cv::mean(extracted)[0]>210,"approxPolyDP perspective extraction");
        PlateDetector detector("-");
        cv::Mat gray; cv::cvtColor(trapezoid,gray,cv::COLOR_BGR2GRAY);
        check(!detector.detect(gray).empty(),"contour-only detection");
        // Regression: adding Haar must never replace or reorder the contour
        // prefix, including when overlapping boxes are suppressed by IoU.
        if (argc > 1) {
            const std::filesystem::path root=argv[1];
            PlateDetector combined((root/"data/haarcascade_russian_plate_number.xml").string());
            for (const char* name : {"car1.jpg","car2.jpg","car3.jpg"}) {
                auto input=cv::imread((root/"test_images"/name).string(),cv::IMREAD_GRAYSCALE);
                check(!input.empty(),"regression image missing");
                auto contours=detector.detect(input), merged=combined.detect(input);
                check(!contours.empty(),"regression needs contour candidates");
                check(merged.size()>=contours.size(),"Haar removed contour candidates");
                check(std::equal(contours.begin(),contours.end(),merged.begin()),"Haar changed contour priority");
            }
        }
        std::cout << "geometry tests passed\n";return 0;
    } catch(const std::exception& e) {std::cerr << e.what() << '\n';return 1;}
}
