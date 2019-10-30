
#pragma once

#include <string>
#include <vector>
#include <fstream>
#include <iostream>
#include <functional>

#include "ConversionInfos.h"

using std::string;
using std::vector;
using std::ifstream;
using std::ofstream;
using std::cout;
using std::endl;

namespace Potree {

	class Candidate {
	public:
		uint32_t x;
		uint32_t y;
		uint32_t z;
		uint32_t index;
	};

	struct Node {
		string name;
		int id;
		vector<Candidate> points;
		uint64_t cumPointCount = 0;

		Node* parent = nullptr;
		vector<Node*> children;
	};

	typedef unordered_map<uint64_t, Candidate> SGrid;


	class BatchIndexer {

	public:

		ConversionInfos infos;
		Batch* batch;
		uint64_t maxLevel = 18;

		BatchIndexer(ConversionInfos infos, Batch* batch) {
			this->infos = infos;
			this->batch = batch;
		}

		vector<SGrid> pointsToGrids(uint8_t* data) {
			int pointsize = infos.attributes.byteSize;

			vector<SGrid> grids;
			for (int i = 0; i <= maxLevel; i++) {
				SGrid grid;
				grids.push_back(grid);
			}

			for (uint64_t i = 0; i < batch->numPoints; i++) {

				uint64_t offset = i * pointsize;

				uint32_t ux = reinterpret_cast<uint32_t*>(data + offset)[0];
				uint32_t uy = reinterpret_cast<uint32_t*>(data + offset)[1];
				uint32_t uz = reinterpret_cast<uint32_t*>(data + offset)[2];

				Candidate candidate;
				candidate.x = ux;
				candidate.y = uy;
				candidate.z = uz;
				candidate.index = i;

				uint64_t level = 0;
				uint64_t divisions = 128 * pow(2.0, level);

				uint64_t rescale = infos.aabb.size.x / infos.scale;

				while (level <= maxLevel) {
					uint64_t cellIndexX = (ux * divisions) / rescale;
					uint64_t cellIndexY = (uy * divisions) / rescale;
					uint64_t cellIndexZ = (uz * divisions) / rescale;
					uint64_t cellIndex = cellIndexX | (cellIndexY << 21ll) | (cellIndexZ << 42ll);

					auto& grid = grids[level];

					auto it = grid.find(cellIndex);
					if (it != grid.end()) {
						level++;
						divisions = divisions * 2;
					}
					else {
						grid[cellIndex] = candidate;
						break;
					}
				}
			}

			return grids;
		}

		vector<unordered_map<uint64_t, Node*>> gridsToNodes(vector<SGrid>& grids) {
			uint64_t unitCount = infos.aabb.size.x / infos.scale;

			vector<unordered_map<uint64_t, Node*>> nodes;
			for (int i = 0; i <= maxLevel; i++) {
				nodes.push_back(unordered_map<uint64_t, Node*>());
			}

			int level = 0;
			for (auto& grid : grids) {

				if (grid.size() == 0) {
					continue;
				}

				double cells = pow(2.0, level);
				auto& nodesOnLevel = nodes[level];

				for (auto& it : grid) {
					Candidate& c = it.second;

					double x = double(c.x) * infos.scale;
					double y = double(c.y) * infos.scale;
					double z = double(c.z) * infos.scale;

					uint64_t ix = cells * (x / infos.aabb.size.x);
					uint64_t iy = cells * (y / infos.aabb.size.y);
					uint64_t iz = cells * (z / infos.aabb.size.z);

					uint64_t tix = ix >= cells ? cells - 1 : ix;
					uint64_t tiy = iy >= cells ? cells - 1 : iy;
					uint64_t tiz = iz >= cells ? cells - 1 : iz;

					uint64_t nodeid = (tix << (2 * maxLevel)) | (tiy << maxLevel) | tiz;

					Node* node = nullptr;

					auto it = nodesOnLevel.find(nodeid);
					if (it == nodesOnLevel.end()) {
						node = new Node();
						node->id = nodeid;

						string name = "r";

						for (int i = 0; i < level; i++) {
							int shift = (level - i) - 1;

							int cix = (tix >> shift) & 1;
							int ciy = (tiy >> shift) & 1;
							int ciz = (tiz >> shift) & 1;

							int index = (cix << 2) | (ciy << 1) | ciz;

							name = name + std::to_string(index);

							if (batch->index == 2 && name == "r1") {
								cout << "wait" << endl;
							}
						}

						node->name = name;
						nodesOnLevel[nodeid] = node;
					}
					else {
						node = it->second;
					}

					//if (node->name == "r0657") {
					//	cout << "wait" << endl;
					//}
					node->points.push_back(c);
				}

				level++;
			}

			return nodes;
		}

		Node* connectNodes(vector<unordered_map<uint64_t, Node*>> levels) {

			unordered_map<string, Node*> nodes;

			// map of node names to nodes
			for (auto& level : levels) {
				for (auto it : level) {
					Node* node = it.second;

					nodes[node->name] = node;
				}
			}

			Node* root = nodes["r"];

			// connect nodes
			for (auto it : nodes) {
				
				Node* node = it.second;

				if (node->name == "r") {
					continue;
				}

				string nodeName = it.second->name;
				string parentName = nodeName.substr(0, nodeName.size() - 1);

				Node* parent = nodes[parentName];

				node->parent = parent;
				parent->children.push_back(node);
			}

			// compute cummulative point counts
			{
				std::function<uint64_t(Node * node)> computeCumulative;
				computeCumulative = [&computeCumulative](Node* node) {
					
					int sum = 0;

					for (Node* child : node->children) {
						uint64_t numPoints = computeCumulative(child);

						sum += numPoints;
					}

					sum += node->points.size();

					node->cumPointCount = sum;
					
					return sum;
				};

				computeCumulative(root);
			}

			return root;
		}

		void mergeSmallNodes(int32_t minPoints, Node* root) {

			// flatten the given node
			std::function<void(Node*)> flatten;
			flatten = [&flatten](Node* node) {

				for (Node* child : node->children) {
					flatten(child);

					node->points.insert(node->points.end(), child->points.begin(), child->points.end());

					delete child;
				}

				node->children.clear();
			};

			// traverse from root and look for nodes small enough to flatten
			std::function<void(Node*)> traverse;
			traverse = [minPoints, &traverse, &flatten](Node* node) {

				if (node->cumPointCount < minPoints) {
					flatten(node);
				} else {
					for (Node* child : node->children) {
						traverse(child);
					}
				}
			};

			traverse(root);
		}

		void run() {

			cout << "run batch indexer" << endl;

			string path = batch->path;

			std::ifstream in(path, std::ios::in | std::ios::binary);

			in.seekg(0, std::ios::end);
			uint64_t filesize = in.tellg();
			in.seekg(0, std::ios::beg);

			uint8_t* data = reinterpret_cast<uint8_t*>(malloc(filesize));
			in.read(reinterpret_cast<char*>(data), filesize);

			int pointsize = infos.attributes.byteSize;




			auto grids = pointsToGrids(data);

			auto nodes = gridsToNodes(grids);

			Node* root = connectNodes(nodes);

			mergeSmallNodes(10'000, root);




			// Print tree
			std::function<void(Node*)> traverse;
			traverse = [&traverse](Node* node) {

				if (node->name.size() > 2) {
					return;
				}

				for (int i = 0; i < node->name.size(); i++) {
					cout << "\t";
				}

				cout << node->name << ":" << node->cumPointCount << endl;

				for (Node* child : node->children) {
					traverse(child);
				}
				
			};
			traverse(root);






			string debugBatchDir = "C:/temp/";
			fs::create_directories(debugBatchDir);

			Batch* batch = this->batch;
			ConversionInfos infos = this->infos;
			std::function<void(Node*)> saveRecursive;
			saveRecursive = [debugBatchDir, batch, &saveRecursive, infos, data, pointsize](Node* node) {

				if (node->name.size() > 2) {
					return;
				}

				ofstream debugFile;
				string debugPath = debugBatchDir + std::to_string(batch->index) + "_" + node->name + ".csv";
				debugFile.open(debugPath);

				for (auto& c : node->points) {
					double x = double(c.x) * infos.scale;// + infos.aabb.min.x;
					double y = double(c.y) * infos.scale;// + infos.aabb.min.y;
					double z = double(c.z) * infos.scale;// + infos.aabb.min.z;

					int pointOffset = c.index * pointsize;
					int r = data[pointOffset + 12];
					int g = data[pointOffset + 13];
					int b = data[pointOffset + 14];

					debugFile << x << ", " << y << ", " << z << ", " << r << ", " << g << ", " << b << endl;
				}


				debugFile.close();

				for (Node* child : node->children) {
					saveRecursive(child);
				}

			};
			saveRecursive(root);




			//for (auto& nodesOnLevel : nodes) {

			//	for (auto it : nodesOnLevel) {
			//		Node* node = it.second;

			//		if (node->name.size() > 2) {
			//			continue;
			//		}


			//		ofstream debugFile;
			//		string debugPath = debugBatchDir + std::to_string(batch->index) + "_" + node->name + ".csv";


			//		debugFile.open(debugPath);

			//		for (auto& c : node->points) {
			//			double x = double(c.x) * infos.scale;// + infos.aabb.min.x;
			//			double y = double(c.y) * infos.scale;// + infos.aabb.min.y;
			//			double z = double(c.z) * infos.scale;// + infos.aabb.min.z;

			//			int pointOffset = c.index * pointsize;
			//			int r = data[pointOffset + 12];
			//			int g = data[pointOffset + 13];
			//			int b = data[pointOffset + 14];

			//			debugFile << x << ", " << y << ", " << z << ", " << r << ", " << g << ", " << b << endl;
			//		}


			//		debugFile.close();
			//	}
			//}




			delete data;
		}

	};

}

