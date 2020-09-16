
import {promises as fsp} from "fs";


async function runDiff(){
	let data = await fsp.readFile("D:/temp/compression/r/rgb.bin");
	let numPoints = data.byteLength / 6;

	console.log(`numPoints: ${numPoints}`);

	let data_r = new Uint8Array(numPoints);
	let data_g = new Uint8Array(numPoints);
	let data_b = new Uint8Array(numPoints);

	for(let i = 0; i < numPoints; i++){

		let r = data.readUInt8(i * 6 + 1);

		data_r[i] = r;
	}

	let diff_r = new Uint8Array(numPoints);
	diff_r[0] = data_r[0];
	for(let i = 1; i < numPoints; i++){
		let r_prev = data_r[i - 1];
		let r_next = data_r[i];

		let diff = r_next - r_prev;
		let udiff = (diff + 256) % 256;

		diff_r[i] = udiff;
	}
	


	console.log(data);
	console.log(data_r);
	console.log(diff_r);

	fsp.writeFile("D:/temp/compression/r/__data_r.bin", data_r);
	fsp.writeFile("D:/temp/compression/r/__diff_r.bin", diff_r);
}

async function runMorton(){

}


// runDiff();


