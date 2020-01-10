
#pragma once

#include <limits>
#include <string>

using namespace std;

static double Infinity = std::numeric_limits<double>::infinity();

template<typename T>
struct Vector3{

	T x = T(0.0);
	T y = T(0.0);
	T z = T(0.0);

	Vector3<T>() {

	}

	Vector3<T>(T x, T y, T z) {
		this->x = x;
		this->y = y;
		this->z = z;
	}

	T squaredDistanceTo(const Vector3<T>& right) {
		double dx = right.x - x;
		double dy = right.y - y;
		double dz = right.z - z;

		double dd = dx * dx + dy * dy + dz * dz;

		return dd;
	}

	T distanceTo(const Vector3<T>& right) {
		double dx = right.x - x;
		double dy = right.y - y;
		double dz = right.z - z;

		double dd = dx * dx + dy * dy + dz * dz;
		double d = std::sqrt(dd);

		return d;
	}

	T max() {
		return std::max(std::max(x, y), z);
	}

	string toString() {
		string str = to_string(x) + ", " + to_string(y) + ", " + to_string(z);

		return str;
	}

	Vector3<T> operator-(const Vector3<T>& right) const {
		return Vector3<T>(x - right.x, y - right.y, z - right.z);
	}

	Vector3<T> operator+(const Vector3<T>& right) const {
		return Vector3<T>(x + right.x, y + right.y, z + right.z);
	}

	Vector3<T> operator+(const double& scalar) const {
		return Vector3<T>(x + scalar, y + scalar, z + scalar);
	}

	Vector3<T> operator/(const double& scalar) const {
		return Vector3<T>(x / scalar, y / scalar, z / scalar);
	}

	Vector3<T> operator*(const Vector3<T>& right) const {
		return Vector3<T>(x * right.x, y * right.y, z * right.z);
	}

	Vector3<T> operator*(const double& scalar) const {
		return Vector3<T>(x * scalar, y * scalar, z * scalar);
	}

};