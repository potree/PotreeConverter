#pragma once

#include "tbb/concurrent_queue.h"
#include "json/json.hpp"
#include <atomic>
#include <iostream>
#include <memory>
#include <vector>
#include "Vector3.h"
#include "Attributes.h"
#include "unsuck/unsuck.hpp"
#include "converter_utils.h"

using json = nlohmann::json;

struct Point
{
	public:
	double x = 0.0, y = 0.0, z = 0.0;
	uint16_t r = 0, g = 0, b = 0;
	uint16_t intensity = 0;
	uint8_t return_number = 0;
	uint8_t number_of_returns = 0;
	uint8_t classification = 0;
	// synthetic, keypoint, withheld, overlap (LAS 1.4 point formats 6+ only).
	uint8_t classification_flags = 0;
	uint8_t user_data = 0;
	int8_t scan_angle_rank = 0;
	int16_t scan_angle = 0;
	uint16_t point_source_ID = 0;
	double gps_time = 0.0;
};

struct Node
{
	public:
	int nodeType = 0;
	uint64_t byteOffset = 0;
	uint64_t byteSize = 0;
	uint32_t numPoints = 0;
	std::string name;
};

class PointsArray
{
	public:
	std::vector<Point> points;
	// num_extra_bytes bytes per point, in the order of Attributes::getExtraAttributes().
	std::vector<uint8_t> extra_bytes;
};

class parallel_handler
{

public:
	int extra_bytes = 0;
	std::string version = "";
	uint64_t expected_number_of_points = 0;
	std::atomic<uint64_t> number_of_points{0};
	// Nodes that hold points. Filled by loadHierarchy() before the worker threads start.
	std::vector<std::shared_ptr<Node>> nodes;
	std::atomic<size_t> next_node{0};
	json js;
	// Cubic bounding box of the octree root.
	Vector3 min, max;
	std::string source_path;
	std::string target_path = "";
	std::string las_name = "";
	std::string octreeDir = "";
	Attributes attributes;
	// nullptr marks the end of the stream.
	tbb::concurrent_bounded_queue<PointsArray*>* points_queue;
	std::atomic<bool> end_writing{false};
	std::string encoding = "";

	void parseMetadata(std::string source_path);
	parallel_handler(std::string source_path, std::string target_path);
	~parallel_handler();
	// Returns false if writing the las file failed.
	bool createFillPointsThreads();
};
