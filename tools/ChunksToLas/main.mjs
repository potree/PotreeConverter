
import {promises as fsp} from "fs";
import { parse } from "path";

let chunkDir = "D:/temp/converted_bad/chunks";

class Attributes{

	constructor(){
		this.list = [];
		this.bytes = 0;
	}

	getOffset(attributeName){
		let offset = 0;

		for(let attribute of this.list){
			if(attribute.name === attributeName){
				return offset;
			}else{
				offset += attribute.size;
			}
		}

		return null;
	}

}

function parseAttributes(metadata){
	let list = [];
	let bytes = 0;

	for(let attribute of metadata.attributes){
		list.push(attribute);
		bytes += attribute.size;
	}

	let attributes = new Attributes();
	attributes.list = list;
	attributes.bytes = bytes;

	return attributes;
}


async function getChunkFiles(){
	let files = await fsp.readdir(chunkDir);
	files = files.filter(filename => filename.endsWith(".bin"));

	return files;
}

async function transformChunk(chunkpath, metadata, attributes){


	let data = await fsp.readFile(chunkpath);
	let numPoints = data.byteLength / attributes.bytes;

	let buff_header = Buffer.alloc(375);
	let buff_points = Buffer.alloc(numPoints * 26);

	console.log(metadata);

	{
		buff_header[0] = "L".charCodeAt(0);
		buff_header[1] = "A".charCodeAt(0);
		buff_header[2] = "S".charCodeAt(0);
		buff_header[3] = "F".charCodeAt(0);

		buff_header[24] = 1;
		buff_header[25] = 4;

		buff_header.writeUInt16LE(375, 94);
		buff_header.writeUInt32LE(375, 96);
		buff_header[104] = 2;
		buff_header.writeUInt16LE(26, 105);

		buff_header.writeDoubleLE(metadata.scale[0], 131);
		buff_header.writeDoubleLE(metadata.scale[1], 139);
		buff_header.writeDoubleLE(metadata.scale[2], 147);

		buff_header.writeDoubleLE(metadata.offset[0], 155);
		buff_header.writeDoubleLE(metadata.offset[1], 163);
		buff_header.writeDoubleLE(metadata.offset[2], 171);

		buff_header.writeDoubleLE(metadata.min[0], 187);
		buff_header.writeDoubleLE(metadata.min[1], 203);
		buff_header.writeDoubleLE(metadata.min[2], 219);

		buff_header.writeDoubleLE(metadata.max[0], 179);
		buff_header.writeDoubleLE(metadata.max[1], 195);
		buff_header.writeDoubleLE(metadata.max[2], 211);

		buff_header.writeBigUInt64LE(BigInt(numPoints), 247);
	}

	for(let i = 0; i < numPoints; i++){
		let offsetPointTarget = i * 26;
		let offsetPointSource = i * attributes.bytes;

		let offsetRGB = attributes.getOffset("rgb");

		data.copy(buff_points, offsetPointTarget, offsetPointSource, offsetPointSource + 12);
		data.copy(buff_points, offsetPointTarget + 20, offsetPointSource + offsetRGB, offsetPointSource + offsetRGB + 6);

	}

	let chunkPathLas = `${chunkpath}.las`
	await fsp.writeFile(chunkPathLas, buff_header);
	await fsp.writeFile(chunkPathLas, buff_points, {flag: "a"});

	console.log(buff_header);

}

async function run(){

	let data = await fsp.readFile(`${chunkDir}/metadata.json`, "utf8");

	let metadata = JSON.parse(data);
	let attributes = parseAttributes(metadata);

	let chunkFiles = await getChunkFiles(chunkDir);

	chunkFiles = ["r060.bin", "r062.bin", "r063.bin", "r064.bin", "r066.bin"];
	for(let chunkfile of chunkFiles){
		transformChunk(chunkDir + "/" + chunkfile, metadata, attributes);
	}

	// transformChunk(chunkDir + "/" + chunkFiles[0], metadata, attributes);
	
}

run();



