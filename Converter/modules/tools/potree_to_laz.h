
#include <string>

#include "laszip/laszip_api.h"

#include "json/json.hpp"



#include "Attributes.h"
#include "unsuck/unsuck.hpp"
#include "Vector3.h"

using std::string;
using json = nlohmann::json;

namespace potree_to_laz {

	struct Point {
		double x;
		double y;
		double z;
		uint16_t r;
		uint16_t g;
		uint16_t b;
	};

	struct Node {
		string name;

		int nodeType = 2;
		int64_t numPoints = 0;
		int64_t byteOffset = 0;
		int64_t byteSize = 0;

		vector<shared_ptr<Node>> children = vector<shared_ptr<Node>>(8, nullptr);
		shared_ptr<Node> parent = nullptr;


		void traverse(function<void(Node*, int level)> callback, int level = 0) {

			callback(this, level);

			for (auto child : children) {
				if (child != nullptr) {
					child->traverse(callback, level + 1);
				}
			}

		}
	};

	Attributes getAttributes(json& jsMetadata) {

		vector<Attribute> attributeList;
		auto jsAttributes = jsMetadata["attributes"];

		for (auto jsAttribute : jsAttributes) {

			string name = jsAttribute["name"];
			string description = jsAttribute["description"];
			int size = jsAttribute["size"];
			int numElements = jsAttribute["numElements"];
			int elementSize = jsAttribute["elementSize"];
			AttributeType type = typenameToType(jsAttribute["type"]);

			Attribute attribute(name, size, numElements, elementSize, type);

			attributeList.push_back(attribute);
		}

		double scaleX = jsMetadata["scale"][0];
		double scaleY = jsMetadata["scale"][1];
		double scaleZ = jsMetadata["scale"][2];

		double offsetX = jsMetadata["offset"][0];
		double offsetY = jsMetadata["offset"][1];
		double offsetZ = jsMetadata["offset"][2];

		Attributes attributes(attributeList);
		attributes.posScale = { scaleX, scaleY, scaleZ };
		attributes.posOffset = { offsetX, offsetY, offsetZ };

		return attributes;
	}

	shared_ptr<Node> loadHierarchy(string path, json& js) {
		auto buffer = readBinaryFile(path + "/hierarchy.bin");

		auto jsHierarchy = js["hierarchy"];
		int64_t firstChunkSize = jsHierarchy["firstChunkSize"];
		int stepSize = jsHierarchy["stepSize"];

		// only load first hierarchy chunk

		int64_t hierarchyByteOffset = 0;
		int64_t hierarchyByteSize = firstChunkSize;
		int64_t bytesPerNode = 22;
		int64_t numNodes = buffer->size / bytesPerNode;

		auto root = make_shared<Node>();
		root->name = "r";
		vector<shared_ptr<Node>> nodes = { root };

		for (int i = 0; i < numNodes; i++) {
			auto current = nodes[i];

			uint8_t type = buffer->data_u8[i * bytesPerNode + 0];
			uint8_t childMask = buffer->data_u8[i * bytesPerNode + 1];
			uint32_t numPoints = reinterpret_cast<uint32_t*>(buffer->data_u8 + i * bytesPerNode + 2)[0];
			int64_t byteOffset = reinterpret_cast<uint64_t*>(buffer->data_u8 + i * bytesPerNode + 6)[0];
			int64_t byteSize = reinterpret_cast<uint64_t*>(buffer->data_u8 + i * bytesPerNode + 14)[0];

			

			if (current->nodeType == 2) {
				// replace proxy with real node
				current->byteOffset = byteOffset;
				current->byteSize = byteSize;
				current->numPoints = numPoints;
			} else if (type == 2) {
				// load proxy
				//current->hierarchyByteOffset = byteOffset;
				//current->hierarchyByteSize = byteSize;
				current->numPoints = numPoints;
			} else {
				// load real node 
				current->byteOffset = byteOffset;
				current->byteSize = byteSize;
				current->numPoints = numPoints;
			}
		
			current->nodeType = type;

			for (int childIndex = 0; childIndex < 8; childIndex++) {

				bool childExists = ((1 << childIndex) & childMask) != 0;

				if (!childExists) {
					continue;
				}

				string childName = current->name + to_string(childIndex);

				auto child = make_shared<Node>();
				child->name = childName;
				
				current->children[childIndex] = child;
				child->parent = current;

				nodes.push_back(child);

			}


		}


		return root;
	}

	void save(string target, vector<Point>& points, Vector3 min, Vector3 max) {
		laszip_POINTER laszip_writer;
		laszip_point* laszip_point;
		laszip_header* header;

		laszip_create(&laszip_writer);
		laszip_get_header_pointer(laszip_writer, &header);

		header->version_major = 1;
		header->version_minor = 4;
		header->header_size = 375;
		header->offset_to_point_data = header->header_size;
		header->point_data_format = 2;
		header->point_data_record_length = 26;
		header->number_of_point_records = points.size();
		header->x_scale_factor = 0.001;
		header->y_scale_factor = 0.001;
		header->z_scale_factor = 0.001;
		header->x_offset = 0.0;
		header->y_offset = 0.0;
		header->z_offset = 0.0;
		header->min_x = min.x;
		header->min_y = min.y;
		header->min_z = min.z;
		header->max_x = max.x;
		header->max_y = max.y;
		header->max_z = max.z;

		header->extended_number_of_point_records = points.size();





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

	void convert(string path) {

		string strMetadata = readTextFile(path + "/metadata.json");

		json js = json::parse(strMetadata);

		json jsBoundingBox = js["boundingBox"];
		Vector3 min = {
			jsBoundingBox["min"][0].get<double>(),
			jsBoundingBox["min"][1].get<double>(),
			jsBoundingBox["min"][2].get<double>()
		};

		Vector3 max = {
			jsBoundingBox["max"][0].get<double>(),
			jsBoundingBox["max"][1].get<double>(),
			jsBoundingBox["max"][2].get<double>()
		};

		auto attributes = getAttributes(js);

		auto root = loadHierarchy(path, js);

		vector<vector<Point>> levels(10);

		root->traverse([&levels, path, attributes](Node* node, int level){
			//cout << node->name << ", " << node->byteOffset << ", " << node->byteSize << endl;

			vector<Point>& points = levels[level];

			auto buffer = readBinaryFile(path + "/octree.bin", node->byteOffset, node->byteSize);
			int bpp = attributes.bytes;
			int numPoints = buffer.size() / bpp;

			int64_t rgbOffset = 0;
			int64_t rgbOffsetFind = 0;
			for (Attribute attribute : attributes.list) {
				if (attribute.name == "rgb") {
					rgbOffset = rgbOffsetFind;
					break;
				}

				rgbOffsetFind += attribute.size;
			}

			for (int64_t i = 0; i < numPoints; i++) {
				int64_t pointOffset = i * bpp;
				
				int32_t ix = read<int32_t>(buffer, pointOffset + 0);
				int32_t iy = read<int32_t>(buffer, pointOffset + 4);
				int32_t iz = read<int32_t>(buffer, pointOffset + 8);

				double x = double(ix) * attributes.posScale.x + attributes.posOffset.x;
				double y = double(iy) * attributes.posScale.y + attributes.posOffset.y;
				double z = double(iz) * attributes.posScale.z + attributes.posOffset.z;

				uint16_t r = read<uint16_t>(buffer, pointOffset + rgbOffset + 0);
				uint16_t g = read<uint16_t>(buffer, pointOffset + rgbOffset + 2);
				uint16_t b = read<uint16_t>(buffer, pointOffset + rgbOffset + 4);


				Point point;
				point.x = x;
				point.y = y;
				point.z = z;
				point.r = r;
				point.g = g;
				point.b = b;

				points.push_back(point);
			}
		});

		int level = -1;
		for (auto& points : levels) {
			level++;

			if (points.size() == 0) {
				continue;
			}

			cout << points.size() << endl;

			string target = path + "/level_" + to_string(level) + ".laz";
			save(target, points, min, max);

		}



		


	}

}