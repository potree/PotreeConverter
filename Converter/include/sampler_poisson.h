
#pragma once

#include <execution>

#include "structures.h"
#include "Attributes.h"
#include "PotreeConverter.h"


struct SamplerPoisson : public Sampler {

	// subsample a local octree from bottom up
	void sample(Node* node, Attributes attributes, double baseSpacing, 
		function<void(Node*)> onNodeCompleted, 
		function<void(Node*)> onNodeDiscarded
	) {

		struct Point {
			double x;
			double y;
			double z;
			int32_t pointIndex;
			int32_t childIndex;
		};

		function<void(Node*, function<void(Node*)>)> traversePost = [&traversePost](Node* node, function<void(Node*)> callback) {
			for (auto child : node->children) {

				if (child != nullptr && !child->sampled) {
					traversePost(child.get(), callback);
				}
			}

			callback(node);
		};

		int64_t bytesPerPoint = attributes.bytes;
		Vector3 scale = attributes.posScale;
		Vector3 offset = attributes.posOffset;

		traversePost(node, [bytesPerPoint, baseSpacing, scale, offset, &onNodeCompleted, &onNodeDiscarded, attributes](Node* node) {
			node->sampled = true;

			int64_t numPoints = node->numPoints;

			auto max = node->max;
			auto min = node->min;
			auto size = max - min;
			auto scale = attributes.posScale;
			auto offset = attributes.posOffset;

			bool isLeaf = node->isLeaf();

			if (isLeaf) {
				return false;
			}

			// =================================================================
			// SAMPLING
			// =================================================================
			//
			// first, check for each point whether it's accepted or rejected
			// save result in an array with one element for each point

			int64_t numPointsInChildren = 0;
			for (auto child : node->children) {
				if (child == nullptr) {
					continue;
				}

				numPointsInChildren += child->numPoints;
			}

			vector<Point> points;
			points.reserve(numPointsInChildren);

			vector<vector<int8_t>> acceptedChildPointFlags;
			vector<int64_t> numRejectedPerChild(8, 0);
			int64_t numAccepted = 0;

			for (int64_t childIndex = 0; childIndex < 8; childIndex++) {
				auto child = node->children[childIndex];

				if (child == nullptr) {
					acceptedChildPointFlags.push_back({});
					numRejectedPerChild.push_back({});

					continue;
				}

				vector<int8_t> acceptedFlags(child->numPoints, 0);
				acceptedChildPointFlags.push_back(acceptedFlags);

				for (int64_t i = 0; i < child->numPoints; i++) {
					int64_t pointOffset = i * attributes.bytes;
					int32_t* xyz = reinterpret_cast<int32_t*>(child->points->data_u8 + pointOffset);

					double x = (xyz[0] * scale.x) + offset.x;
					double y = (xyz[1] * scale.y) + offset.y;
					double z = (xyz[2] * scale.z) + offset.z;

					Point point = { x, y, z, i, childIndex };

					points.push_back(point);
				}

			}

			unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();

			thread_local vector<Point> dbgAccepted(1'000'000);
			int64_t dbgNumAccepted = 0;
			double spacing = baseSpacing / pow(2.0, node->level());
			double squaredSpacing = spacing * spacing;

			auto squaredDistance = [](Point& a, Point& b) {
				double dx = a.x - b.x;
				double dy = a.y - b.y;
				double dz = a.z - b.z;

				double dd = dx * dx + dy * dy + dz * dz;

				return dd;
			};

			auto center = (node->min + node->max) * 0.5;

			//int dbgChecks = -1;
			//int dbgSumChecks = 0;
			//int dbgMaxChecks = 0;

			auto checkAccept = [/*&dbgChecks, &dbgSumChecks,*/ &dbgNumAccepted, spacing, squaredSpacing, &squaredDistance, center /*, &numDistanceChecks*/](Point candidate) {

				auto cx = candidate.x - center.x;
				auto cy = candidate.y - center.y;
				auto cz = candidate.z - center.z;
				auto cdd = cx * cx + cy * cy + cz * cz;
				auto cd = sqrt(cdd);
				auto limit = (cd - spacing);
				auto limitSquared = limit * limit;

				int64_t j = 0;
				for (int64_t i = dbgNumAccepted - 1; i >= 0; i--) {

					auto& point = dbgAccepted[i];

					//dbgChecks++;
					//dbgSumChecks++;

					// check distance to center
					auto px = point.x - center.x;
					auto py = point.y - center.y;
					auto pz = point.z - center.z;
					auto pdd = px * px + py * py + pz * pz;
					//auto pd = sqrt(pdd);

					// stop when differences to center between candidate and accepted exceeds the spacing
					// any other previously accepted point will be even closer to the center.
					if (pdd < limitSquared) {
						return true;
					}

					double dd = squaredDistance(point, candidate);

					if (dd < squaredSpacing) {
						return false;
					}

					j++;

					// also put a limit at x distance checks
					if (j > 10'000) {
						return true;
					}
				}

				return true;

			};

			auto parallel = std::execution::par_unseq;
			std::sort(parallel, points.begin(), points.end(), [center](Point a, Point b) -> bool {

				auto ax = a.x - center.x;
				auto ay = a.y - center.y;
				auto az = a.z - center.z;
				auto add = ax * ax + ay * ay + az * az;

				auto bx = b.x - center.x;
				auto by = b.y - center.y;
				auto bz = b.z - center.z;
				auto bdd = bx * bx + by * by + bz * bz;

				// sort by distance to center
				return add < bdd;

				// sort by manhattan distance to center
				//return (ax + ay + az) < (bx + by + bz);

				// sort by z axis
				//return a.z < b.z;
			});

			for (Point point : points) {

				//dbgChecks = 0;

				bool isAccepted = checkAccept(point);

				//dbgMaxChecks = std::max(dbgChecks, dbgMaxChecks);

				if (isAccepted) {
					dbgAccepted[dbgNumAccepted] = point;
					dbgNumAccepted++;
					numAccepted++;
				} else {
					numRejectedPerChild[point.childIndex]++;
				}

				//{ // debug: store sample time in GPS time attribute
				//		auto child = node->children[point.childIndex];
				//		auto data = child->points->data_u8;

				//		//static double value = 0.0;


				//		//value += 0.1;

				//		//if(node->level() <= 2){
				//			std::lock_guard<mutex> lock(mtx_debug);

				//			debug += 0.01;
				//			memcpy(data + point.pointIndex * attributes.bytes + 20, &debug, 8);
				//		//}
				//		//double value = now();

				//		//point.pointIndex
				//}

				acceptedChildPointFlags[point.childIndex][point.pointIndex] = isAccepted ? 1 : 0;

				//abc++;

			}

			auto accepted = make_shared<Buffer>(numAccepted * attributes.bytes);
			for (int64_t childIndex = 0; childIndex < 8; childIndex++) {
				auto child = node->children[childIndex];

				if (child == nullptr) {
					continue;
				}

				auto numRejected = numRejectedPerChild[childIndex];
				auto& acceptedFlags = acceptedChildPointFlags[childIndex];
				auto rejected = make_shared<Buffer>(numRejected * attributes.bytes);

				for (int64_t i = 0; i < child->numPoints; i++) {
					auto isAccepted = acceptedFlags[i];
					int64_t pointOffset = i * attributes.bytes;

					if (isAccepted) {
						accepted->write(child->points->data_u8 + pointOffset, attributes.bytes);
						// rejected->write(child->points->data_u8 + pointOffset, attributes.bytes);
					} else {
						rejected->write(child->points->data_u8 + pointOffset, attributes.bytes);
					}
				}

				if (numRejected == 0 && child->isLeaf()) {
					onNodeDiscarded(child.get());

					node->children[childIndex] = nullptr;
				} if (numRejected > 0) {
					child->points = rejected;
					child->numPoints = numRejected;

					onNodeCompleted(child.get());
				} else if(numRejected == 0) {
					// the parent has taken all points from this child, 
					// so make this child an empty inner node.
					// Otherwise, the hierarchy file will claim that 
					// this node has points but because it doesn't have any,
					// decompressing the nonexistent point buffer fails
					// https://github.com/potree/potree/issues/1125
					child->points = nullptr;
					child->numPoints = 0;
					onNodeCompleted(child.get());
				}
			}

			node->points = accepted;
			node->numPoints = numAccepted;

			//{ // debug
			//	auto avgChecks = dbgSumChecks / points.size();
			//	string msg = "#checks: " + formatNumber(dbgSumChecks) + ", maxChecks: " + formatNumber(dbgMaxChecks) + ", avgChecks: " + formatNumber(avgChecks) + "\n";
			//	cout << msg;
			//}

			return true;
		});
	}

};

