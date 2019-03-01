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
		// TODO Better: adjust image size to multiple of heatmat * upsample_size ??
		// TODO crop input mat or add insets so that resized image size is multiple of 16 as in python code - why?
		// TODO Resize to multiple of heat/paf mat?
		// TODO Do not resize images -> do not resize unless specified on command line -> add dlib command line parser
		// TODO -> reallocate tensor for each frame / image

		PoseEstimator pose_estimator(argv[1]);
		pose_estimator.loadModel();
		
		const int heat_mat_upsample_size = 4;
		const int frame_max_size = 320;
		const int gauss_kernel_size = 25;
		//const cv::Size inset = cv::Size(0, 0);
		//const cv::Size inset = cv::Size(16, 16);
		const cv::Size inset = cv::Size(32, 32);

		const string window_name = haveFile ? argv[2] : "this is you, smile! :)";
		const bool displaying_images = cap.getBackendName().compare("CV_IMAGES") == 0;
		const bool render_insets = inset != cv::Size(0, 0);
		auto aspect_corrected_inference_size = [](const cv::Size& image, const int desired)->cv::Size {
			const auto aspect_ratio = image.aspectRatio();
			return aspect_ratio > 1.0 ? cv::Size(desired, desired / aspect_ratio) : cv::Size(desired / aspect_ratio, desired);
		};
		const Size image_size = cv::Size(cap.get(cv::CAP_PROP_FRAME_WIDTH), cap.get(cv::CAP_PROP_FRAME_HEIGHT));
		auto render_size = displaying_images ? image_size : aspect_corrected_inference_size(image_size, frame_max_size);

		TensorMat input(render_size, inset / 4);
		pose_estimator.setGaussKernelSize(gauss_kernel_size);

		FramesPerSecond fps;
		for (;;) {
			Mat frame;
			cap >> frame;
			if (frame.empty()) break; // end of video stream

			const vector<Human> humans = pose_estimator.inference(input.copyFrom(frame), heat_mat_upsample_size);
			Mat display = render_insets ? Mat(inset + image_size + inset, CV_8UC3) : frame;
			const Rect roi = cv::Rect(cv::Point(inset), frame.size());
			if (inset != Size(0, 0)) {
				display = 0;
				Mat view = display(roi);
				frame.copyTo(view);
			}
			pose_estimator.draw_humans(display, roi, humans);
			fps.update(display);

			if (displaying_images) {
				namedWindow(window_name, WINDOW_AUTOSIZE | WINDOW_KEEPRATIO );
			}

			imshow(window_name, display);
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
