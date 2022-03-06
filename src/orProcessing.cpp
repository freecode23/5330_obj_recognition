//**********************************************************************************************************************
// FILE: orProcessing.cpp
//
// DESCRIPTION
// Contains implementation for applying various object recognition filters
//
// AUTHOR
// Sherly Hartono
//**********************************************************************************************************************

#include "orProcessing.hpp"

#include <algorithm>
#include <cmath>
#include <opencv2/core/utility.hpp>
#include "orUtil.hpp"
#include "csv_util.hpp"

using namespace std;

/*
 * task 1
 */
void rgb_to_hsv(cv::Mat &src, cv::Mat &dst) {
    dst.create(src.size(), CV_8UC1);  //  uchar 0-255
    // for each ith row
    for (int i = 0; i < src.rows; i++) {
        // at each jth column (pixel)
        for (int j = 0; j < src.cols; j++) {
            uchar r = src.at<cv::Vec3b>(i, j)[0];
            uchar g = src.at<cv::Vec3b>(i, j)[1];
            uchar b = src.at<cv::Vec3b>(i, j)[2];

            // gat max, min, and difference
            int c_max = std::max(r, std::max(g, b));  // maximum of r, g, b
            int c_min = std::min(r, std::min(g, b));  // minimum of r, g, b
            int diff = c_max - c_min;                 // diff of cmax and cmin.
            long s;

            if (c_max == 0) {
                s = 0;  // saturation is 0
            } else {
                s = 255 * diff / c_max;  // get value in 255
            }

            dst.at<uchar>(i, j) = (uchar)s;
        }
    }
}

void thresholding_sat(cv::Mat &src, cv::Mat &dst) {
    // 1. blur
    cv::Mat blur_result;
    blur3x3(src, blur_result);

    // 2. convert to hsv space
    cv::Mat hsv_result;
    rgb_to_hsv(blur_result, hsv_result);

    // 3. allocate destination immge use size and type of the source image
    dst.create(src.size(), CV_8UC1);  // 2 saturation channel

    // 4. get saturation channel only
    // for each ith row
    for (int i = 0; i < src.rows; i++) {
        int saturation;
        // at each jth column (pixel)
        for (int j = 0; j < src.cols; j++) {
            // takes channel saturation's value at this pixel

            saturation = hsv_result.at<uchar>(i, j);

            if (saturation < 35) {        // if its darker than 45
                dst.at<uchar>(i, j) = 0;  // set it to white
            } else {
                dst.at<uchar>(i, j) = 255;  // set to black
            }
        }
    }
}

void thresholding_rgb(cv::Mat &src, cv::Mat &dst) {
    // 1. allocate destination immge use size and type of the source image.
    // we only need 1 color channel
    dst.create(src.size(), CV_8UC1);

    // 2. blur the image
    cv::Mat blur_result;
    blur3x3(src, blur_result);

    // 4. convert to 1 channel mode
    // for each ith row
    for (int i = 0; i < blur_result.rows; i++) {
        int red;
        int green;
        int blue;
        // at each jth column (pixel)
        for (int j = 0; j < blur_result.cols; j++) {
            // take average
            red = blur_result.at<cv::Vec3b>(i, j)[0];
            green = blur_result.at<cv::Vec3b>(i, j)[1];
            blue = blur_result.at<cv::Vec3b>(i, j)[2];
            int average = (red + green + blue) / 3;

            if (average < 110)  // if its darker than 10
            {
                average = 255;  // set it to foreground (white)
            } else {
                average = 0;  // set to background black
            }
            // assign green values to dst
            dst.at<uchar>(i, j) = average;
        }
    }
}

/*
 * task 2
 */
void clean_up(cv::Mat &src, cv::Mat &dst) {
    cv::Mat inter;
    // make filter kernel
    inter.create(src.size(), src.type());

    // 1. mask
    cv::Mat mask = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(3, 3));

    // 2. close and open
    cv::morphologyEx(src, inter, cv::MORPH_CLOSE, mask, cv::Point(-1, -1), 20);
    cv::morphologyEx(inter, dst, cv::MORPH_OPEN, mask, cv::Point(-1, -1), 6);
}

/*
 * task 3
 */

void segment_and_color(cv::Mat &src, cv::Mat &dst,
                       std::vector<cv::Vec3b> random_colors, int max_regions,
                       bool isColorful, int &out_area,
                       cv::Point &out_centroid_of_interest) {
    vector<int> out_ids_to_keep;
    cv::Mat stats;
    cv::Mat out_label;
    int out_id_of_interest;
    // 1. segment
    segmentation(src, max_regions, out_label, out_ids_to_keep,
                 out_id_of_interest, stats, out_centroid_of_interest);

    out_area = stats.at<int>(3, cv::CC_STAT_AREA);  // area
    // cout << "region of interest has area: " << out_area << endl;
    // cout << "centroids of interest are: " << out_centroid_of_interest.x << ",
    // "
    //  << out_centroid_of_interest.y << endl;
    // coloring
    // 2. create a map of region_id and colors
    std::map<int, cv::Vec3b> regionid_colors;

    // 3. choose black for background
    regionid_colors.insert(
        pair<int, cv::Vec3b>(out_ids_to_keep[0], cv::Vec3b(0, 0, 0)));

    // 4. choose random colors for the rest
    for (int i = 1; i < max_regions; i++) {
        regionid_colors.insert(
            pair<int, cv::Vec3b>(out_ids_to_keep[i], random_colors[i]));
    }

    // 5. make colorful
    if (isColorful) {
        // 3 channels
        dst.create(src.size(), CV_8UC3);
        for (int r = 0; r < dst.rows; ++r) {
            for (int c = 0; c < dst.cols; ++c) {
                // get the region id at this coordinate
                int region_id = out_label.at<int>(r, c);

                // if the region id at this pixel should be kept, color it
                if (std::count(out_ids_to_keep.begin(), out_ids_to_keep.end(),
                               region_id)) {
                    dst.at<cv::Vec3b>(r, c) = random_colors[region_id];
                } else {
                    dst.at<cv::Vec3b>(r, c) =
                        cv::Vec3b(0, 0, 0);  // else set it black
                }
            }
        }
        cv::putText(dst, "Include all regions not discarding those near edges.",
                    cv::Point(40, 40),  // Coordinates (Bottom-left corner of
                                        // the text string in the image)
                    cv::FONT_HERSHEY_COMPLEX_SMALL,  // Font
                    0.8,                             // Scale. 2.0 = 2x bigger
                    cv::Scalar(255, 255, 255),       // BGR Color
                    2,                               // Line Thickness
                    cv::LINE_4);
    } else {
        // single channel
        dst.create(src.size(), CV_8UC1);
        // 6. filter out regions not of interest
        for (int r = 0; r < dst.rows; ++r) {
            for (int c = 0; c < dst.cols; ++c) {
                // get the region id at this coordinate
                int region_id = out_label.at<int>(r, c);

                // if this is region of interest
                if (region_id == out_id_of_interest) {
                    dst.at<uchar>(r, c) = 255;  // white
                } else {
                    dst.at<uchar>(r, c) = 0;  // else set it black
                }
            }
        }
    }
}

void segmentation(cv::Mat &src, int max_regions, cv::Mat &out_label,
                  vector<int> &out_ids_to_keep, int &out_id_of_interest,
                  cv::Mat &out_stats, cv::Point &out_centroid_of_interest) {
    // 1. get regions
    out_label.create(src.size(), CV_32S);
    cv::Mat centroids;
    cv::connectedComponentsWithStats(src, out_label, out_stats, centroids);
    int n_regions = out_stats.rows;

    // 2. get areas of each regions ordered by region_id
    std::vector<int> areas;
    for (int i = 0; i < out_stats.rows; i++) {
        int area = out_stats.at<int>(i, cv::CC_STAT_AREA);  // area
        areas.push_back(area);
    }

    // 3. filter by area
    // get top 6 largest area by index
    out_ids_to_keep = get_top_N_largest_areas_index(areas, max_regions);

    // 4. filter by centroid
    vector<int> img_center = get_center_coordinates(src);  // image center

    // get single id of interest
    out_id_of_interest = get_id_with_most_center_centroids(
        img_center, centroids, out_ids_to_keep);
    out_centroid_of_interest =
        cv::Point(centroids.at<double>(out_id_of_interest, 0),
                  centroids.at<double>(out_id_of_interest, 1));
}

/*
 * task 4
 */
void compute_features(cv::Mat &src, cv::Mat &dst,
                      vector<cv::Vec3b> random_colors, int max_regions,
                      vector<float> &out_features) {
    // 1. do segmentation to only get 1 region
    cv::Mat cleaned_img;
    cv::Mat binary_img;
    cv::Mat binary_one_region;

    // threshold and cleanup
    thresholding_rgb(src, binary_img);
    clean_up(binary_img, cleaned_img);

    // get a binary image of single region most center and its area
    int pixel_area;
    cv::Point centroid_of_interest;
    segment_and_color(cleaned_img, binary_one_region, random_colors,
                      max_regions, false, pixel_area, centroid_of_interest);

    // 2. get contour of interest
    vector<cv::Point> contour_of_interest;
    get_contour_of_interest(binary_one_region, contour_of_interest);

    // 3. feature 1: hu moments
    // get hu
    double hu_arr[7];
    compute_log_scale_hu(contour_of_interest, hu_arr);
    for (int i = 0; i < 7; i++) {
        out_features.push_back(hu_arr[i]);
    }

    // 4. feature 2: get perc fill
    // get rotated bd box
    cv::RotatedRect rot_rect = cv::minAreaRect(contour_of_interest);
    double perc_fill =
        pixel_area / (rot_rect.size.width * rot_rect.size.height) * 100;
    out_features.push_back(perc_fill);

    // 5. feature 3: height width ratio
    double height_width_rat = rot_rect.size.width / rot_rect.size.height;
    out_features.push_back(height_width_rat);

    // 6. display
    // print features
    int id = 0;
    for (float feat : out_features) {
        cout << id << ": " << feat << ", ";
        id += 1;
    }

    // write text
    src.copyTo(dst);
    vector<string> infos;
    infos.push_back("perc_fill: " + to_string(perc_fill) + " %");
    infos.push_back("w/h_ratio:" + to_string(height_width_rat));
    int i = 0;
    for (string info : infos) {
        // write features on the image
        cv::putText(dst, info,
                    cv::Point(centroid_of_interest.x,
                              centroid_of_interest.y + i +
                                  100),  // Coordinates (Bottom-left corner
                                         // of the text string in the image)
                    cv::FONT_HERSHEY_DUPLEX,  // Font
                    0.8,                      // Scale. 2.0 = 2x bigger
                    cv::Scalar(0, 255, 0),    // BGR Color
                    1,                        // Line Thickness
                    cv::LINE_4);
        i += 30;
    }

    // draw rotated box
    cv::Point2f vertices[4];
    rot_rect.points(vertices);  // get the points from rectangle and output
                                // as vertices points
    for (int i = 0; i < 4; i++) {
        // draw line based on vertices
        cv::line(dst, vertices[i], vertices[(i + 1) % 4], cv::Scalar(0, 255, 0),
                 2);
    }
}


