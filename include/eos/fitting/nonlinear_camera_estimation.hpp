/*
 * Eos - A 3D Morphable Model fitting library written in modern C++11/14.
 *
 * File: include/eos/fitting/nonlinear_camera_estimation.hpp
 *
 * Copyright 2015 Patrik Huber
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#pragma once

#ifndef NONLINEARCAMERAESTIMATION_HPP_
#define NONLINEARCAMERAESTIMATION_HPP_

#include "eos/fitting/detail/nonlinear_camera_estimation_detail.hpp"

#include "glm/gtc/matrix_transform.hpp"

#include "Eigen/Geometry"
#include "unsupported/Eigen/NonLinearOptimization"

#include "opencv2/core/core.hpp"

#include <vector>
#include <cassert>

namespace eos {
	namespace fitting {

/**
 * @brief A class representing a camera viewing frustum. At the
 * moment used as orthographic camera only.
 */
struct Frustum
{
	float l, r, b, t;
	// optional<float> n, f;
};

/**
 * @brief Represents a set of estimated model parameters (rotation, translation) and
 * camera parameters (viewing frustum).
 *
 * The estimated rotation and translation transform the model from model-space to camera-space,
 * and, if one wishes to use OpenGL, can be used to build the model-view matrix.
 * The parameters are the inverse of the camera position in 3D space.
 *
 * The camera frustum describes the size of the viewing plane of the camera, and
 * can be used to build an OpenGL-conformant orthographic projection matrix.
 *
 * Together, these parameters fully describe the imaging process of a given model instance
 * (under an orthographic projection).
 *
 * The rotation values are given in radians and estimated using the RPY convention.
 * Yaw is applied first to the model, then pitch, then roll (R * P * Y * vertex).
 */
struct OrthographicRenderingParameters
{
	float r_x; // Pitch.
	float r_y; // Yaw. Positive means subject is looking left (we see her right cheek).
	float r_z; // Roll. Positive means the subject's right eye is further down than the other one (he tilts his head to the right).
	float t_x;
	float t_y;
	Frustum frustum;
};

/**
 * @brief This algorithm estimates the rotation angles and translation of the model, as
 * well as the viewing frustum of the camera, given a set of corresponding 2D-3D points.
 *
 * It assumes an orthographic camera and estimates 6 parameters,
 * [r_x, r_y, r_z, t_x, t_y, frustum_scale], where the first five describe how to transform
 * the model, and the last one describes the cameras viewing frustum (see CameraParameters).
 * This 2D-3D correspondence problem is solved using Eigen's LevenbergMarquardt algorithm.
 *
 * The method is slightly inspired by "Computer Vision: Models Learning and Inference",
 * Simon J.D. Prince, 2012, but different in a lot of respects.
 *
 * Eigen's LM implementation requires at least 6 data points, so we require >= 6 corresponding points.
 *
 * Notes/improvements:
 * The algorithm works reliable as it is, however, it could be improved with the following:
 *  - A better initial guess (see e.g. Prince)
 *  - Using the analytic derivatives instead of Eigen::NumericalDiff - they're easy to calculate.
 *
 * @param[in] image_points A list of 2D image points.
 * @param[in] model_points Corresponding points of a 3D model.
 * @param[in] width Width of the image (or viewport).
 * @param[in] height Height of the image (or viewport).
 * @return The estimated model and camera parameters.
 */
RenderingParameters estimate_orthographic_camera(std::vector<cv::Vec2f> image_points, std::vector<cv::Vec4f> model_points, int width, int height)
{
	using cv::Mat;
	assert(image_points.size() == model_points.size());
	assert(image_points.size() >= 6); // Number of correspondence points given needs to be equal to or larger than 6

	const float aspect = static_cast<float>(width) / height;

	// Set up the initial parameter vector and the cost function:
	int num_params = 6;
	Eigen::VectorXd parameters; // [rot_x_pitch, rot_y_yaw, rot_z_roll, t_x, t_y, frustum_scale]
	parameters.setConstant(num_params, 0.0); // Set all 6 values to zero (except frustum_scale, see next line)
	parameters[5] = 110.0; // This is just a rough hand-chosen scaling estimate - we could do a lot better. But it works.
	detail::OrthographicParameterProjection cost_function(image_points, model_points, width, height);

	// Note: we have analytical derivatives, so we should use them!
	Eigen::NumericalDiff<detail::OrthographicParameterProjection> cost_function_with_derivative(cost_function, 0.0001);
	// I had to change the default value of epsfcn, it works well around 0.0001. It couldn't produce the derivative with the default, I guess the changes in the gradient were too small.

	Eigen::LevenbergMarquardt<Eigen::NumericalDiff<detail::OrthographicParameterProjection>> lm(cost_function_with_derivative);
	auto info = lm.minimize(parameters); // we could or should use the return value
	// 'parameters' contains the solution now.

	Frustum camera_frustum{ -1.0f * aspect * parameters[5], 1.0f * aspect * parameters[5], -1.0f * parameters[5], 1.0f * parameters[5] };
	return RenderingParameters{ static_cast<float>(parameters[0]), static_cast<float>(parameters[1]), static_cast<float>(parameters[2]), static_cast<float>(parameters[3]), static_cast<float>(parameters[4]), camera_frustum };
};

	} /* namespace fitting */
} /* namespace eos */

#endif /* NONLINEARCAMERAESTIMATION_HPP_ */
