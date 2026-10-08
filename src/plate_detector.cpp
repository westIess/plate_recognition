#include "plate_detector.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>

PlateDetector::PlateDetector(const std::string& cascadePath) {
#if PLATE_HAS_CASCADE
    if (!cascadePath.empty() && cascadePath != "-") {
        try { cascadeLoaded_ = cascade_.load(cascadePath); }
        catch (const cv::Exception& e) { std::cerr << e.what() << "\n"; }
    }
#endif
    if (!cascadeLoaded_) {
        std::cerr << "[PlateDetector] WARNING: не удалось загрузить каскад "
                  << cascadePath << " — будет использован только контурный метод.\n";
    }
}

std::vector<cv::Rect> PlateDetector::detect(const cv::Mat& gray) const {
    std::vector<cv::Rect> result;

    if (cascadeLoaded_) {
        result = detectWithCascade(gray);
    }

    // Also try contours when Haar fires on an unrelated object.
    auto contours = detectWithContours(gray);
    result.insert(result.end(), contours.begin(), contours.end());
    std::vector<cv::Rect> unique;
    for (const auto& box : result) {
        bool duplicate = false;
        for (const auto& prev : unique) {
            double intersection = (box & prev).area();
            if (intersection / (box.area() + prev.area() - intersection) > 0.65) {
                duplicate = true; break;
            }
        }
        if (!duplicate) unique.push_back(box);
    }
    return unique;
}

std::vector<cv::Rect> PlateDetector::detectWithCascade(const cv::Mat& gray) const {
    std::vector<cv::Rect> plates;
#if PLATE_HAS_CASCADE
    cv::Mat equalized;
    cv::equalizeHist(gray, equalized);

    // minNeighbors=4 и minSize подобраны эмпирически под кадры с камеры
    // въезда (номер занимает от ~5% ширины кадра).
    cascade_.detectMultiScale(
        equalized, plates,
        /*scaleFactor=*/1.05,
        /*minNeighbors=*/4,
        /*flags=*/0,
        cv::Size(std::max(24, gray.cols / 20), std::max(8, gray.rows / 30)));

    // Сортируем по площади (крупный номер = кандидат ближе к камере,
    // как правило самый релевантный при въезде).
    std::sort(plates.begin(), plates.end(), [](const cv::Rect& a, const cv::Rect& b) {
        return a.area() > b.area();
    });

#else
    (void)gray;
#endif

    return plates;
}

std::vector<cv::Rect> PlateDetector::detectWithContours(const cv::Mat& gray) const {
    std::vector<cv::Rect> candidates;

    cv::Mat blurred, edges;
    cv::bilateralFilter(gray, blurred, 11, 17, 17);
    cv::Canny(blurred, edges, 30, 200);

    // Замыкаем разрывы контура рамки номера морфологическим закрытием.
    cv::Mat closed;
    cv::morphologyEx(edges, closed, cv::MORPH_CLOSE,
                      cv::getStructuringElement(cv::MORPH_RECT, cv::Size(15, 5)));

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(closed, contours, cv::RETR_LIST, cv::CHAIN_APPROX_SIMPLE);

    struct Scored {
        cv::Rect box;
        double score;
    };
    std::vector<Scored> scored;

    for (const auto& c : contours) {
        double area = cv::contourArea(c);
        if (area < 500) continue; // отсекаем шум

        cv::RotatedRect rr = cv::minAreaRect(c);
        cv::Rect box = rr.boundingRect() & cv::Rect(0, 0, gray.cols, gray.rows);
        if (box.width <= 0 || box.height <= 0) continue;

        double aspect = static_cast<double>(std::max(rr.size.width,rr.size.height)) / std::max(1.0f,std::min(rr.size.width,rr.size.height));
        // Стандартный номер РФ имеет соотношение сторон примерно 4.6:1,
        // но с учётом наклона/обрезки допускаем диапазон.
        if (aspect < 2.0 || aspect > 6.5) continue;

        double fillRatio = area / std::max(1.0f, rr.size.area());
        if (fillRatio < 0.3) continue; // слишком "рваный" контур для рамки номера

        // Чем ближе aspect к эталону и выше заполненность — тем выше score.
        double aspectScore = 1.0 - std::min(1.0, std::abs(aspect - 4.6) / 4.6);
        double score = aspectScore * 0.6 + fillRatio * 0.4;

        scored.push_back({box, score});
    }

    std::sort(scored.begin(), scored.end(),
              [](const Scored& a, const Scored& b) { return a.score > b.score; });

    for (const auto& s : scored) candidates.push_back(s.box);
    return candidates;
}

// Sort cyclically about the centroid, then choose the visually upper-left
// corner. Unlike sum/difference independent selections, no corner is reused.
std::array<cv::Point2f,4> PlateDetector::orderCorners(std::array<cv::Point2f,4> p) {
    cv::Point2f center(0,0);
    for (const auto& point : p) center += point * 0.25f;
    std::sort(p.begin(),p.end(),[&](auto a, auto b) {
        return std::atan2(a.y-center.y,a.x-center.x) < std::atan2(b.y-center.y,b.x-center.x);
    });
    auto first = std::min_element(p.begin(),p.end(),[](auto a,auto b) { return a.x+a.y < b.x+b.y; });
    std::rotate(p.begin(),first,p.end());
    // Plates are wider than tall: rotate by one if the first edge is the short one.
    double w = cv::norm(p[1]-p[0])+cv::norm(p[2]-p[3]);
    double h = cv::norm(p[3]-p[0])+cv::norm(p[2]-p[1]);
    if (w < h) std::rotate(p.begin(),p.begin()+1,p.end());
    return p;
}

cv::Mat PlateDetector::warpPlate(const cv::Mat& frame, std::array<cv::Point2f,4> p) {
    if (frame.empty()) return {};
    for (const auto& point : p)
        if (!std::isfinite(point.x) || !std::isfinite(point.y) || point.x < 0 || point.y < 0 ||
            point.x > frame.cols-1 || point.y > frame.rows-1) return {};
    p = orderCorners(p);
    std::vector<cv::Point2f> polygon(p.begin(),p.end());
    if (!cv::isContourConvex(polygon) || std::abs(cv::contourArea(polygon)) < 16) return {};
    double width = std::max(cv::norm(p[1]-p[0]),cv::norm(p[2]-p[3]));
    double height = std::max(cv::norm(p[3]-p[0]),cv::norm(p[2]-p[1]));
    if (height < 4 || width/height < 1.5 || width/height > 8) return {};
    int w = std::max(2,static_cast<int>(std::lround(width)));
    int h = std::max(2,static_cast<int>(std::lround(height)));
    std::array<cv::Point2f,4> dst{{{0,0},{float(w-1),0},{float(w-1),float(h-1)},{0,float(h-1)}}};
    cv::Mat warped;
    cv::warpPerspective(frame,warped,cv::getPerspectiveTransform(p.data(),dst.data()),
                        cv::Size(w,h),cv::INTER_CUBIC,cv::BORDER_REPLICATE);
    return warped;
}

cv::Mat PlateDetector::extractAndDeskew(const cv::Mat& frame, const cv::Rect& box) {
    if (frame.empty()) return {};
    const cv::Rect bounds(0,0,frame.cols,frame.rows);
    const auto safe = box & bounds;
    if (safe.empty()) return {};
    int px = std::max(2,safe.width/20), py = std::max(2,safe.height/5);
    const auto padded = cv::Rect(safe.x-px,safe.y-py,safe.width+2*px,safe.height+2*py) & bounds;
    cv::Mat crop = frame(padded).clone(), gray, edges;
    if (crop.channels()==3) cv::cvtColor(crop,gray,cv::COLOR_BGR2GRAY);
    else gray=crop;
    cv::Canny(gray,edges,40,150);
    // Use original edges for accurate corners, then a lightly closed fallback.
    std::array<cv::Point2f,4> best{};
    double bestScore = -1;
    for (int pass=0;pass<2;++pass) {
        cv::Mat mask=edges.clone();
        if (pass) cv::morphologyEx(edges,mask,cv::MORPH_CLOSE,
            cv::getStructuringElement(cv::MORPH_RECT,cv::Size(3,3)));
        std::vector<std::vector<cv::Point>> contours;
        cv::findContours(mask,contours,cv::RETR_LIST,cv::CHAIN_APPROX_SIMPLE);
        for (const auto& c:contours) {
            double area=std::abs(cv::contourArea(c));
            if (area < safe.area()*0.25 || area < 100) continue;
            for (double epsilon : {0.015,0.025,0.04}) {
                std::vector<cv::Point> poly;
                cv::approxPolyDP(c,poly,epsilon*cv::arcLength(c,true),true);
                if (poly.size()!=4 || !cv::isContourConvex(poly)) continue;
                std::array<cv::Point2f,4> q;
                for (int i=0;i<4;++i) q[i]=cv::Point2f(poly[i]);
                q=orderCorners(q);
                double w=(cv::norm(q[1]-q[0])+cv::norm(q[2]-q[3]))/2;
                double h=(cv::norm(q[3]-q[0])+cv::norm(q[2]-q[1]))/2;
                if (h<4 || w/h<2 || w/h>6.5) continue;
                double score=area/(1+std::abs(w/h-4.6));
                if (score>bestScore) { bestScore=score; best=q; }
            }
        }
    }
    if (bestScore>0) {
        auto warped=warpPlate(crop,best);
        if (!warped.empty()) return warped;
    }
    // No reliable quadrilateral: preserve the crop, never invent perspective.
    return crop;
}
