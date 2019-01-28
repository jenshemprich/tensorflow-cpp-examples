#include "pch.h"

#include <iostream>

#include "opencv2/opencv.hpp"

#include "tensorflow/core/platform/env.h"
#include "tensorflow/core/public/session.h"
#include "Eigen/Dense"
#include "tensorflow/core/public/session.h"
#include "tensorflow/cc/ops/standard_ops.h"

#include "FramesPerSecond.h"
#include "PoseEstimator.h"
#include "PoseEstimation.h"

using namespace std;
using namespace cv;
using namespace tensorflow;

int main(int argc,  char* const argv[]) {
	if (argc < 2) {
		cout << "Graph path not specified" << "\n";
		return 1;
	}

	VideoCapture cap;
	try {
		boolean haveFile = argc > 2;
		if (argc > 2) {
			if (!cap.open(argv[2])) {
				cout << "File not found: " << argv[2] << "\n";
				return -1;
			}
		} else {
			if (!cap.open(0, CAP_DSHOW)) { // TODO Open issue @ OpenCV since for CAP_MSMF=default aspect ratio is 16:9 on 4:3 chip size cams
				cout << "Webcam not found: " << argv[2] << "\n";
				return 2;
			}
			cap.set(cv::CAP_PROP_FRAME_WIDTH, 640);
			cap.set(cv::CAP_PROP_FRAME_HEIGHT, 480);
		}

		// TODO crop input mat so that resized image size is multiple of 16
		// TODO Better: adjust image size to multiple of heatmat * upsample_size
		const int image_size_factor = 2;
		const int heat_mat_upsample_size = 4;
		const int gauss_kernel_filter_size = 25;
		const int frame_max_size = 320;

		auto aspect_corrected_inference_size = [](const cv::Size& image, const int desired)->cv::Size {
			const auto aspect_ratio = image.aspectRatio();
			return aspect_ratio > 1.0 ? cv::Size(desired, desired / aspect_ratio) : cv::Size(desired / aspect_ratio, desired);
		};

		const Size frame_size = Size(cap.get(cv::CAP_PROP_FRAME_WIDTH), cap.get(cv::CAP_PROP_FRAME_HEIGHT));
		PoseEstimator pose_estimator(argv[1], aspect_corrected_inference_size(frame_size, frame_max_size));
		pose_estimator.loadModel();
		pose_estimator.setGaussKernelSize(gauss_kernel_filter_size);

		const string window_name = haveFile ? argv[2] : "this is you, smile! :)";
		const bool displaying_images = cap.getBackendName().compare("CV_IMAGES") == 0;

		FramesPerSecond fps;
		for (;;) {
			Mat image;
			cap >> image;
			if (image.empty()) break; // end of video stream

			const vector<Human> humans = pose_estimator.inference(image, heat_mat_upsample_size);
			pose_estimator.draw_humans(image, humans);
			fps.update(image);

			if (displaying_images) {
				namedWindow(window_name, WINDOW_AUTOSIZE | WINDOW_KEEPRATIO );
			}
			imshow(window_name, image);
			if (waitKey(10) == 27) break; // ESC -> exit
			else if (displaying_images) {
				waitKey();
			}
		}
	}
	catch (const tensorflow::Status& e) {
		cout << e << endl;
	}
	catch (std::exception& e) {
		cout << e.what() << endl;
	}
	
	cap.release();
	return 0;
}
