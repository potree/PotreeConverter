
#pragma once

#include <string>
#include <memory>
#include <fstream>

#include "Structures.h"

using std::string;
using std::shared_ptr;
using std::make_shared;
using std::fstream;

namespace bluenoise {


	struct WriterNode{

		string name;
		Vector3<double> min;
		Vector3<double> max;

		uint64_t numPoints = 0;
		uint64_t byteOffset = 0;
		uint64_t byteSize = 0;

		WriterNode* parent = nullptr;
		shared_ptr<WriterNode> children[8] = {
			nullptr, nullptr, nullptr, nullptr,
			nullptr, nullptr, nullptr, nullptr};

		WriterNode(string name, Vector3<double> min, Vector3<double> max){
			this->name = name;
			this->min = min;
			this->max = max;
		}

	};

	struct PotreeWriter {

		string path;
		bool closed = false;

		fstream fout;

		Vector3<double> min;
		Vector3<double> max;
		Vector3<double> scale = {0.001, 0.001, 0.001};

		uint64_t bytesWritten = 0;

		shared_ptr<WriterNode> root = nullptr;

		PotreeWriter(string path, Vector3<double> min, Vector3<double> max);

		~PotreeWriter();

		shared_ptr<WriterNode> findOrCreateWriterNode(string name);

		void writeNode(Node* node);

		void finishOctree();

		void writeMetadata();

		void writeHierarchy();

		void close();

	};

}