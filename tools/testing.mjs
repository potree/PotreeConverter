
import {execute} from "./utils.mjs";
import {readFile, writeFile, mkdir} from "fs/promises";


let cmdConverter = "Converter.exe";
let workdir = "../Converter/bin/x64_Release";
let targetDir = "D:/temp/test";

let cases = [
	{
		sources: ["D:/dev/pointclouds/mschuetz/lion.las"],
		target: `lion.las`,
	},{
		sources: ["D:/dev/pointclouds/bunny_20M.las"],
		target: `bunny_20M.las`,
	},{
		sources: ["D:/dev/pointclouds/archpro/heidentor.las"],
		target: `heidentor.las`,
	},{
		sources: ["D:/dev/pointclouds/archpro/heidentor.laz"],
		target: `heidentor.laz`,
	},{
		sources: [
			"D:/dev/pointclouds/CA13/ot_35120B4117C_1.laz",
			"D:/dev/pointclouds/CA13/ot_35120B4121A_1.laz",
			"D:/dev/pointclouds/CA13/ot_35120B4121B_1.laz",
			"D:/dev/pointclouds/CA13/ot_35120B4121C_1.laz",
			"D:/dev/pointclouds/CA13/ot_35120B4121D_1.laz",
			"D:/dev/pointclouds/CA13/ot_35120B4122A_1.laz",
			"D:/dev/pointclouds/CA13/ot_35120B4122B_1.laz",
		],
		target: `ca13_selection`,
	},{
		sources: ["D:/dev/pointclouds/epalinges/aabi_drive1_lv95_nf02_vuxlr_d1_180417_120307_no_Airpoints.laz"],
		target: `aabi_drive1_lv95_nf02_vuxlr_d1_180417_120307_no_Airpoints.laz`,
	},{
		sources: ["D:/dev/pointclouds/heidelberg/dechen_cave.laz"],
		target: `dechen_cave.laz`,
	},{
		sources: ["D:/dev/pointclouds/pix4d/eclepens.las"],
		target: `eclepens.las`,
	},{
		sources: ["D:/dev/pointclouds/pix4d/eclepens.laz"],
		target: `eclepens.laz`,
	},{
		sources: ["D:/dev/pointclouds/pix4d/eclepens_sorted.las"],
		target: `eclepens_sorted.las`,
	},{
		sources: ["D:/dev/pointclouds/pix4d/matterhorn.laz"],
		target: `matterhorn.laz`,
	},{
		sources: ["D:/dev/pointclouds/riegl/retz.laz"],
		target: `retz.laz`,
	},{
		sources: ["D:/dev/pointclouds/riegl/niederweiden.laz"],
		target: `niederweiden.laz`,
	},{
		sources: ["D:/dev/pointclouds/scott_reed/DoverMillRuins.laz"],
		target: `DoverMillRuins.laz`,
	},{
		sources: ["D:/dev/pointclouds/sigeom/vol_total.laz"],
		target: `vol_total.laz`,
	},{
		sources: ["D:/dev/pointclouds/sitn/potree2/aerial_lidar2016_2522500_1197000_las14.las"],
		target: `aerial_lidar2016_2522500_1197000_las14.las`,
	},{
		sources: ["D:/dev/pointclouds/sitn/potree2/aerial_lidar2016_2574500_1214500_laz14.laz"],
		target: `aerial_lidar2016_2574500_1214500_laz14.laz`,
	},{
		sources: ["D:/dev/pointclouds/sitn/potree2/aerial_lidar2019_2524_1197_las14.las"],
		target: `aerial_lidar2019_2524_1197_las14.las`,
	},{
		sources: ["D:/dev/pointclouds/sitn/potree2/aerial_lidar2019_2524_1197_laz14_dip.laz"],
		target: `aerial_lidar2019_2524_1197_laz14_dip.laz`,
	},{
		sources: ["D:/dev/pointclouds/sitn/AV1MARS_LV95_NF02_200410_110252_VUX1-HA_non_cale.laz"],
		target: `AV1MARS_LV95_NF02_200410_110252_VUX1-LR_non_cale.laz`,
	},{
		sources: ["D:/dev/pointclouds/sitn/lidar/85034_COO_VERRIERES_CORCELLES_MLS_2019_000170.las"],
		target: `85034_COO_VERRIERES_CORCELLES_MLS_2019_000170.las`,
	},{
		sources: ["D:/dev/pointclouds/sitn/lidar/85034_COO_VERRIERES_CORCELLES_MLS_2019_000170.laz"],
		target: `85034_COO_VERRIERES_CORCELLES_MLS_2019_000170.laz`,
	},{
		sources: ["D:/dev/pointclouds/sitn/lidar/d85034_COO_VERRIERES_CORCELLES_MLS_2019_000170_dip.las"],
		target: `d85034_COO_VERRIERES_CORCELLES_MLS_2019_000170_dip.las`,
	},{
		sources: ["D:/dev/pointclouds/tuwien_baugeschichte/candi Banyunibo"],
		target: `candi_banyunibo`,
	},{
		sources: ["D:/dev/pointclouds/tuwien_baugeschichte/candi Sari"],
		target: `candi_sari`,
	},{
		sources: [
			"D:/dev/pointclouds/tuwien_baugeschichte/Museum Affandi_las export/Affandi 1_01 - SINGLESCANS - 160401_013042.laz",
			"D:/dev/pointclouds/tuwien_baugeschichte/Museum Affandi_las export/Affandi 1_02 - SINGLESCANS - 160401_015040.laz",
			"D:/dev/pointclouds/tuwien_baugeschichte/Museum Affandi_las export/Affandi 1_03 - SINGLESCANS - 160401_020437.laz",
		],
		target: `affandi_selection`,
	},{
		sources: ["D:/dev/pointclouds/nvidia.las"],
		target: `nvidia`,
	}




];


async function run(){

	let template = await readFile("./template.html", 'utf8').catch(e => console.log("abc"));

	try{
		await mkdir(`${targetDir}/pages`);
	}catch(e){};

	for(let item of cases){

		let {sources, target} = item;

		let cmdSources = sources.map(s => `"${s}"`).join(" ");
		let cmdTarget = `${targetDir}/${target}`;

		await execute(`${cmdConverter} ${cmdSources} -o ${cmdTarget}`, workdir);

		let url = `../${target}/metadata.json`;
		let page = template.replace(/URL_GOES_HERE/g, `"${url}"`);
		await writeFile(`${targetDir}/pages/${target}.html`, page);

	}

}

run();
