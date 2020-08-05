
import {exec} from "child_process";

export function execute(cmd, workdir){

	console.log(`running: ${cmd}`);

	return new Promise(resolve => {

		exec(cmd, {cwd: workdir}, (error, stdout, stderr) => {

			if (error) {
				console.log(`error: ${error.message}`);
			}else{
				console.log("success!");
			}

			if (stderr) {
				console.log(`stderr: ${stderr}`);
			}

			resolve();
		});
	});

}