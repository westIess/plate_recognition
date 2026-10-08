#pragma once
#include <opencv2/core.hpp>
#include <opencv2/core/version.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#if CV_VERSION_MAJOR >= 5
#include <opencv2/geometry.hpp>
#endif
#ifndef PLATE_HAS_CASCADE
#define PLATE_HAS_CASCADE 0
#endif
#if PLATE_HAS_CASCADE
#if CV_VERSION_MAJOR >= 5
#include <opencv2/xobjdetect.hpp>
#else
#include <opencv2/objdetect.hpp>
#endif
#endif
