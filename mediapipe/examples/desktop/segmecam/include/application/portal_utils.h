#pragma once

#include <string>
#include <opencv2/opencv.hpp>

namespace segmecam {

// Attempts to load an image from the document portal or file chooser portal when direct access fails.
// Returns true and fills image_out/resolved_path on success.
bool LoadBackgroundImageWithPortal(const std::string& original_path,
                                   cv::Mat& image_out,
                                   std::string& resolved_path);

bool OpenBackgroundImagePortalDialog(cv::Mat& image_out,
                                     std::string& resolved_path);

}
