
import {spawn} from "child_process";

let cmd = "./build/Release/PotreeConverter.exe";

let easyCases = [

	{
		input: ["D:/dev/pointclouds/testdata/heidentor.las"],
		output: "D:/temp/test/heidentor.las",
		arguments: [],
	},{
		input: ["D:/dev/pointclouds/testdata/heidentor.laz"],
		output: "D:/temp/test/heidentor.laz",
		arguments: [],
	},{
		input: ["D:/dev/pointclouds/testdata/heidentor.laz"],
		output: "D:/temp/test/heidentor.laz_only_rgb",
		arguments: ["--attributes", "rgb"],
	},{
		input: ["D:/dev/pointclouds/testdata/ca13"],
		output: "D:/temp/test/ca13",
		arguments: [],
	},{
		input: ["D:/dev/pointclouds/testdata/ca13"],
		output: "D:/temp/test/ca13_rgb_intensity_classification",
		arguments: ["--attributes", "rgb", "intensity", "classification"],
	},{
		input: ["D:/dev/pointclouds/testdata/lion.laz"],
		output: "D:/temp/test/lion.laz",
		arguments: [],
	},{
		input: ["D:/dev/pointclouds/testdata/eclepens.laz"],
		output: "D:/temp/test/eclepens.laz",
		arguments: [],
	},{
		input: ["D:/dev/pointclouds/testdata/retz.laz"],
		output: "D:/temp/test/retz.laz",
		arguments: [],
	},{
		input: ["D:/dev/pointclouds/testdata/vol_total.laz"],
		output: "D:/temp/test/vol_total.laz",
		arguments: [],
	},{
		input: ["D:/dev/pointclouds/testdata/affandi"],
		output: "D:/temp/test/affandi",
		arguments: [],
	},{
		input: ["D:/dev/pointclouds/testdata/ahn3"],
		output: "D:/temp/test/ahn3",
		arguments: [],
	},{
		input: ["D:/dev/pointclouds/testdata/Grab15.laz"],
		output: "D:/temp/test/Grab15.laz",
		arguments: [],
	},{
		input: ["D:/dev/pointclouds/testdata/tahoe.laz"],
		output: "D:/temp/test/tahoe.laz",
		arguments: [],
	},{
		input: ["D:/dev/pointclouds/testdata/railway_000039.laz"],
		output: "D:/temp/test/railway_000039.laz",
		arguments: [],
	},
];

let problematicCases = [
	{
		input: ["D:/dev/pointclouds/testdata/Bargello10Mtest.las"],
		output: "D:/temp/test/Bargello10Mtest.las",
		arguments: [],
	},{
		input: ["D:/dev/pointclouds/testdata/crash.las"],
		output: "D:/temp/test/crash.las",
		arguments: [],
	},{
		input: ["D:/dev/pointclouds/testdata/aerial_lidar2019_2524_1197_laz14_dip.laz"],
		output: "D:/temp/test/aerial_lidar2019_2524_1197_laz14_dip.laz",
		arguments: [],
	},{
		input: ["D:/dev/pointclouds/testdata/trott_crashing"],
		output: "D:/temp/test/trott_crashing",
		arguments: [],
	},{
		input: ["D:/dev/pointclouds/testdata/d85034_COO_VERRIERES_CORCELLES_MLS_2019_000170_dip.laz"],
		output: "D:/temp/test/d85034_COO_VERRIERES_CORCELLES_MLS_2019_000170_dip.laz",
		arguments: [],
	},
];

async function runTestcase(testcase){

	let args = [
		testcase.input, 
		"-o", testcase.output,
		...testcase.arguments,
	];

	let strArgs = args.join(" ");

	console.log("");
	console.log(`starting testcase: ${strArgs}`);

	return new Promise(resolve => {

		let process = spawn(cmd, args);

		process.stdout.on('data', (data) => {
			//console.log(`stdout: ${data}`);
		});

		process.stderr.on('data', (data) => {
			//console.error(`stderr: ${data}`);
		});

		process.on('close', (code) => {
			// console.log(`child process exited with code ${code}`);

			if(code === 0){
				console.log("SUCCESS!");
			}else if(code !== 0){
				console.error(`ERROR: exited with code ${code}`);
			}

			resolve();
		});

	});
}

async function run(){

	// let testcases = [problematicCases[0]];

	let testcases = [...easyCases, ...problematicCases];

	for(let testcase of testcases){
		await runTestcase(testcase);
	}

}

run();

