

let min = {x: -8.0960000000000001, y: -4.7999999999999998, z: 1.6870000000000001 };
let max = {x: 7.4619999999999997, y: 10.757999999999999, z: 17.245000000000001 };
let size = {
	x: max.x - min.x,
	y: max.y - min.y,
	z: max.z - min.z
};
let center = {
	x: (max.x + min.x) / 2,
	y: (max.y + min.y) / 2,
	z: (max.z + min.z) / 2,
};
let center2 = {
	x: min.x + size.x * 0.5,
	y: min.y + size.y * 0.5,
	z: min.z + size.z * 0.5,
};

let p1 = {x: -7.9370000000000003, y: 2.9790000000000001, z: 2.0329999999999999};
let p2 = {x: -7.9740000000000002, y: 3.0569999999999999, z: 1.9600000000000000}

let gridSize = 128;
let dGridSize = gridSize;

function indexOf(point){
	let dix = dGridSize * (point.x - min.x) / size.x;
	let diy = dGridSize * (point.y - min.y) / size.y;
	let diz = dGridSize * (point.z - min.z) / size.z;

	console.log(dix, diy, diz);

	let ix = parseInt(Math.min(dix, dGridSize - 1.0));
	let iy = parseInt(Math.min(diy, dGridSize - 1.0));
	let iz = parseInt(Math.min(diz, dGridSize - 1.0));

	console.log(ix, iy, iz);

	let index = ix + iy * gridSize + iz * gridSize * gridSize;

	return index;
}


console.log("==============");
console.log(center);
console.log(center2);
console.log(indexOf(p1));
console.log(indexOf(p2));

