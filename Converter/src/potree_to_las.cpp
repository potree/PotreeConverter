#include <string>
#include "unsuck/unsuck.hpp"
#include "Vector3.h"
#include "arguments/Arguments.hpp"
#include "converter_utils.h"
#include <iostream>
#include <chrono>
#include "PotreeConverter.h"
#include "parallel_potree_to_las.h"

using std::string;
using json = nlohmann::json;

namespace potree_to_laz {

	constexpr int64_t bytesPerNode = 22; // type + childMask + numPoints + byteOffset + byteSize

	enum NodeType {
		NORMAL = 0,
		LEAF = 1,
		PROXY = 2,
	};

	// Parses one hierarchy chunk, mirroring Potree's 2.0 loader. The first entry of a chunk describes
	// chunkRoot itself, the remaining entries are its descendants in breadth-first order. Proxy
	// entries point to the chunk that holds the rest of their subtree.
	void loadHierarchyChunk(string path, int64_t start, int64_t size, shared_ptr<Node> chunkRoot, parallel_handler& ph)
	{
		auto buffer = make_shared<Buffer>(size);
		readBinaryFile(path, start, size, buffer->data);

		vector<shared_ptr<Node>> nodes = { chunkRoot };
		int64_t numNodes = size / bytesPerNode;

		for (int64_t i = 0; i < numNodes; i++) {
			if (i >= nodes.size()) {
				std::cout << "ERROR: hierarchy chunk at " << start << " has more entries than nodes. Exiting." << std::endl;
				exit(1);
			}

			auto current = nodes[i];
			uint8_t* entry = buffer->data_u8 + i * bytesPerNode;

			uint8_t type = entry[0];
			uint8_t childMask = entry[1];
			uint32_t numPoints;
			uint64_t byteOffset, byteSize;
			memcpy(&numPoints, entry + 2, 4);
			memcpy(&byteOffset, entry + 6, 8);
			memcpy(&byteSize, entry + 14, 8);

			if (type == PROXY) {
				// The children of a proxy are listed in its own chunk, not in this one.
				loadHierarchyChunk(path, byteOffset, byteSize, current, ph);
				continue;
			}

			current->nodeType = type;
			current->numPoints = numPoints;
			current->byteOffset = byteOffset;
			current->byteSize = byteSize;

			if (numPoints > 0) {
				ph.nodes.push_back(current);
				ph.expected_number_of_points += numPoints;
			}

			for (int childIndex = 0; childIndex < 8; childIndex++) {
				bool childExists = ((1 << childIndex) & childMask) != 0;

				if (!childExists) {
					continue;
				}

				auto child = make_shared<Node>();
				child->name = current->name + to_string(childIndex);
				nodes.push_back(child);
			}
		}
	}

	void loadHierarchy(string path, json& js, parallel_handler& ph) {
		if (ph.version == "2.0")
		{
			auto root = make_shared<Node>();
			root->name = "r";
			int64_t firstChunkSize = js["hierarchy"]["firstChunkSize"];

			loadHierarchyChunk(path + "/hierarchy.bin", 0, firstChunkSize, root, ph);
		}
		else if (ph.version == "1.7")
		{
			fs::path octreeDirPath = fs::path(path) / ph.octreeDir;

			for (auto& entry : fs::recursive_directory_iterator(octreeDirPath)) {
				if(entry.path().extension() != ".bin") {
					continue;
				}
				auto node = make_shared<Node>();
				node->name = entry.path().string();
				node->byteSize = fs::file_size(entry.path());
				ph.nodes.push_back(node);
			}
		}
	}

	Options parseArguments(int argc, char** argv) {
		Arguments args(argc, argv);

		args.addArgument("source,i,", "input files");
		args.addArgument("outdir,o", "output directory");

		if (!args.has("source")) {
			cout << "/PotreeToLas <source> -o <outdir>" << endl;

			exit(1);
		}

		vector<string> source = args.get("source").as<vector<string>>();

		if (source.size() == 0) {
			cout << "/PotreeToLas <source> -o <outdir>" << endl;

			exit(1);
		}

		string outdir = "";
		if (args.has("outdir"))
		{
			outdir = args.get("outdir").as<string>();
		}
		else
		{
			string sourcepath = source[0];

			fs::path path(sourcepath);

			if (!fs::exists(path)) {
				cout << "ERROR: file does not exist: " << source[0] << endl;

				exit(123);
			}

			path = fs::canonical(path);

			string suggestedBaseName = path.filename().string() + "_converted";
			outdir = sourcepath + "/../" + suggestedBaseName;

			int i = 1;
			while (fs::exists(outdir)) {
				outdir = sourcepath + "/../" + suggestedBaseName + "_" + std::to_string(i);

				if (i > 100) {
					cout << "ERROR: unsuccessfully tried to find empty output directory. stopped at 100 iteration" << endl;

					exit(123);
				}
				i++;
			}
		}

		outdir = fs::weakly_canonical(fs::path(outdir)).string();
		fs::create_directories(outdir);

		Options options;
		options.source = source;
		options.outdir = outdir;

		return options;
	}

	bool convert(string source_path, string target_path) {
		parallel_handler ph(source_path, target_path);

		std::cout << "Load Hierarchy" << std::endl;
		loadHierarchy(source_path, ph.js, ph);

		std::cout << "Write nodes" << std::endl;
		std::cout << "Total nodes to run over: " << ph.nodes.size() << std::endl;
		return ph.createFillPointsThreads();
	}
}

int main(int argc, char** argv) {
	auto start = std::chrono::high_resolution_clock::now();

	auto options = potree_to_laz::parseArguments(argc, argv);
	string sourceDir = options.source[0];
	string targetDir = options.outdir;
	bool success = potree_to_laz::convert(sourceDir, targetDir);
	auto end = std::chrono::high_resolution_clock::now();

	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
	std::cout << "Reconstruction Time: " << duration.count() / 1000.0 << std::endl;

	return success ? 0 : 1;
}
