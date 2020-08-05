
#include <string>

#include "laszip/laszip_api.h"

#include "unsuck/unsuck.hpp"
#include "Vector3.h"

using std::string;

namespace sort_pointcloud {

	struct Point {
		double x = 0.0;
		double y = 0.0;
		double z = 0.0;

		uint16_t r = 0;
		uint16_t g = 0;
		uint16_t b = 0;
	};

	void save(string target, vector<Point>& points, laszip_header* header) {

		laszip_POINTER laszip_writer;
		laszip_point* laszip_point;

		laszip_create(&laszip_writer);
		laszip_set_header(laszip_writer, header);

		laszip_open_writer(laszip_writer, target.c_str(), false);

		laszip_get_point_pointer(laszip_writer, &laszip_point);

		double coordinates[3];

		for (Point point : points) {

			coordinates[0] = point.x;
			coordinates[1] = point.y;
			coordinates[2] = point.z;

			laszip_point->rgb[0] = point.r;
			laszip_point->rgb[1] = point.g;
			laszip_point->rgb[2] = point.b;

			//laszip_set_point(laszip_writer, laszip_point);
			laszip_set_coordinates(laszip_writer, coordinates);
			//laszip_set_point(laszip_writer, laszip_point);
			laszip_write_point(laszip_writer);
		}

		laszip_close_writer(laszip_writer);
		laszip_destroy(laszip_writer);

	}

	void sort(string path, string target) {

		laszip_POINTER laszip_reader;
		laszip_header* header;
		laszip_point* laszip_point;
	
		laszip_create(&laszip_reader);

		laszip_BOOL request_reader = 1;
		laszip_request_compatibility_mode(laszip_reader, request_reader);

		laszip_BOOL is_compressed = iEndsWith(path, ".laz") ? 1 : 0;
		laszip_open_reader(laszip_reader, path.c_str(), &is_compressed);

		laszip_get_header_pointer(laszip_reader, &header);

		int64_t numPoints = (header->number_of_point_records ? header->number_of_point_records : header->extended_number_of_point_records);

		laszip_get_point_pointer(laszip_reader, &laszip_point);

		Vector3 min = {
			header->min_x,
			header->min_y,
			header->min_z
		};
		Vector3 max = {
			header->max_x,
			header->max_y,
			header->max_z
		};
		double largest = (max - min).max();
		Vector3 size = { largest, largest, largest };
		max = min + size;
		

		double coordinates[3];

		vector<Point> points;
		points.reserve(numPoints);

		cout << "read" << endl;
		for (int i = 0; i < numPoints; i++) {
			laszip_read_point(laszip_reader);
			laszip_get_coordinates(laszip_reader, coordinates);

			Point point;
			point.x = coordinates[0];
			point.y = coordinates[1];
			point.z = coordinates[2];
			point.r = laszip_point->rgb[0];
			point.g = laszip_point->rgb[1];
			point.b = laszip_point->rgb[2];

			points.push_back(point);
		}


		auto mortonOrder = [min, max](Point a, Point b) {
			int32_t factor = 100;
			auto mc1 = mortonEncode_magicbits(
				(a.x + min.x) * factor, 
				(a.y + min.y) * factor, 
				(a.z + min.z) * factor
			);
			auto mc2 = mortonEncode_magicbits(
				(b.x + min.x) * factor, 
				(b.y + min.y) * factor, 
				(b.z + min.z) * factor
			);

			return mc1 < mc2;
		};

		//auto alongX = [](Point a, Point b) {
		//	return a.x < b.x;
		//};

		cout << "sort" << endl;
		std::sort(points.begin(), points.end(), mortonOrder);

		cout << "save" << endl;
		save(target, points, header);

	}

}



