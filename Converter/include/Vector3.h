
#pragma once

#include <string>
#include <cmath>
#include <limits>
#include <sstream>
#include <algorithm>
#include <string>

using std::string;

struct Vector3{

	double x = double(0.0);
	double y = double(0.0);
	double z = double(0.0);

	Vector3() {

	}

	Vector3(double x, double y, double z) {
		this->x = x;
		this->y = y;
		this->z = z;
	}

	Vector3(double value[3]) {
		this->x = value[0];
		this->y = value[1];
		this->z = value[2];
	}

	double squaredDistanceTo(const Vector3& right) {
		double dx = right.x - x;
		double dy = right.y - y;
		double dz = right.z - z;

		double dd = dx * dx + dy * dy + dz * dz;

		return dd;
	}

	double distanceTo(const Vector3& right) {
		double dx = right.x - x;
		double dy = right.y - y;
		double dz = right.z - z;

		double dd = dx * dx + dy * dy + dz * dz;
		double d = std::sqrt(dd);

		return d;
	}

	double length() {
		return sqrt(x * x + y * y + z * z);
	}

	double max() {
		double value = std::max(std::max(x, y), z);
		return value;
	}

	Vector3 operator-(const Vector3& right) const {
		return Vector3(x - right.x, y - right.y, z - right.z);
	}

	Vector3 operator+(const Vector3& right) const {
		return Vector3(x + right.x, y + right.y, z + right.z);
	}

	Vector3 operator+(const double& scalar) const {
		return Vector3(x + scalar, y + scalar, z + scalar);
	}

	Vector3 operator/(const double& scalar) const {
		return Vector3(x / scalar, y / scalar, z / scalar);
	}

	Vector3 operator*(const Vector3& right) const {
		return Vector3(x * right.x, y * right.y, z * right.z);
	}

	Vector3 operator*(const double& scalar) const {
		return Vector3(x * scalar, y * scalar, z * scalar);
	}

	string toString() {

		std::stringstream ss;
		ss << x << ", " << y << ", " << z;

		return ss.str();

	}

};