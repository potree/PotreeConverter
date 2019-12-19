
#pragma once

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

	Vector3<T> operator-(const Vector3<T>& right) const {
		return Vector3<T>(x - right.x, y - right.y, z - right.z);
	}

};