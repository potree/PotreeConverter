import os
import subprocess
import time
from pathlib import Path

import streamlit as st
from clean_file import clean_las

POTREE_CONVERTER_PATH = "/app/build/PotreeConverter"
BASE_OUTPUT_FOLDER = Path("/app/potree/crescer")

BASE_OUTPUT_FOLDER.mkdir(exist_ok=True)

def convert_file(in_file: Path, output_dir: Path, remove_int64: bool = False):
	potree_command = [
		POTREE_CONVERTER_PATH,
		str(in_file),
		"-o",
		str(output_dir),
		"-p",
		in_file.stem,
	]

	if remove_int64:
		try:
			clean_las(in_file)
		except Exception as e:
			display_error(f"Error cleaning file {in_file}: {e}")
			return

	with st.status(f"Converting: {in_file.name}", expanded=True) as status:
		process = subprocess.Popen(
			potree_command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True
		)

		st.session_state.potree_process = process

		for line in process.stdout:
			if st.session_state.stop_requested:
				process.terminate()
				status.update(label="Conversion Stopped by User", state="error", expanded=True)
				break
			st.text(line.strip())

		process.wait()

		if not st.session_state.stop_requested:
			if process.returncode == 0:
				status.update(label="Conversion Successful!", state="complete", expanded=False)
			else:
				status.update(label="Conversion Failed!", state="error", expanded=True)

	st.session_state.potree_process = None  # clear process reference after done
	st.session_state.stop_requested = False



def convert_directory(in_dir: Path, output_dir: Path, remove_int64: bool = False, progress_bar=None, file_index=None, total_files=None):
	output_dir.mkdir(parents=True, exist_ok=True)
	
	files_in_dir = [entry for entry in in_dir.iterdir() if entry.is_file() and entry.suffix in [".las", ".laz"]]
	dirs_in_dir = [entry for entry in in_dir.iterdir() if entry.is_dir()]

	for entry in files_in_dir:
		convert_file(entry, output_dir, remove_int64)
		if progress_bar is not None and file_index is not None and total_files is not None:
			file_index[0] += 1
			progress_bar.progress(file_index[0] / total_files)

	for entry in dirs_in_dir:
		convert_directory(entry, output_dir / entry.name, remove_int64, progress_bar, file_index, total_files)


def get_output_folder(input_folder: Path) -> Path:
	"""Determine the default output folder based on input type."""
	if input_folder.is_dir():
		default_output_folder = BASE_OUTPUT_FOLDER / input_folder.name
	else:
		default_output_folder = BASE_OUTPUT_FOLDER
	return default_output_folder


def display_error(message: str):
	"""Display an error message in red."""
	st.markdown(f"<span style='color:red'>{message}</span>", unsafe_allow_html=True)


def convert(input_folder: Path, output_folder: Path, need_cleaning: bool = False):
	"""Perform conversion based on input type."""
	
	if input_folder.is_dir():
		files_to_convert = list(input_folder.rglob("*.las")) + list(input_folder.rglob("*.laz"))
		total_files = len(files_to_convert)

		if total_files > 0:
			progress_bar = st.progress(0)
			file_index = [0]  # Use a list to pass by reference
			
			# Add a placeholder for time estimates
			time_estimate = st.empty()
			start_time = time.time()
			avg_time_per_file = None
			
			# Process files and update time estimate
			for i, file_path in enumerate(files_to_convert):
				if st.session_state.stop_requested:
					break
					
				file_start_time = time.time()
				convert_file(file_path, output_folder / file_path.relative_to(input_folder).parent, need_cleaning)
				file_end_time = time.time()
				
				# Update progress
				file_index[0] = i + 1
				progress = file_index[0] / total_files
				progress_bar.progress(progress)
				
				# Calculate and update time estimate
				if avg_time_per_file is None:
					# First file completed, make initial estimate
					avg_time_per_file = file_end_time - file_start_time
				else:
					# Update running average (giving more weight to recent files)
					avg_time_per_file = 0.7 * avg_time_per_file + 0.3 * (file_end_time - file_start_time)
				
				remaining_files = total_files - file_index[0]
				est_remaining_time = remaining_files * avg_time_per_file
				elapsed_time = time.time() - start_time
				
				# Format time estimates
				elapsed_str = time.strftime("%H:%M:%S", time.gmtime(elapsed_time))
				remaining_str = time.strftime("%H:%M:%S", time.gmtime(est_remaining_time))
				
				time_estimate.info(f"Progress: {file_index[0]}/{total_files} files | " 
								   f"Elapsed: {elapsed_str} | Estimated remaining: {remaining_str}")
			
			if not st.session_state.stop_requested:
				st.success("Conversion Complete!")
		else:
			st.warning("No LAS/LAZ files found in the input directory.")

	else:
		convert_file(input_folder, output_folder, need_cleaning)
		st.success("Conversion Complete!")
	st.session_state.stop_requested = False
	st.session_state.potree_process = None  



def main():
	if "potree_process" not in st.session_state:
		st.session_state.potree_process = None
	if "stop_requested" not in st.session_state:
		st.session_state.stop_requested = False

	st.set_page_config(page_title="Potree Converter", initial_sidebar_state="collapsed")
	st.title("Potree Converter")

	with st.sidebar:
		st.header("Configuration")
		need_cleaning = st.checkbox("Need Cleaning", value=False, help="Check this if you want to fix the laz file format before conversion")

	# Input fields
	input_path_str = st.text_input("Input Folder/File", help="Enter the path to the input folder or file", value="")
	input_folder = Path(input_path_str) if input_path_str else Path()
	has_input = False

	output_folder = st.text_input(
		"Output Folder", value=str(get_output_folder(input_folder))
	)

	output_path = Path(output_folder)
	potree_url = str(output_path).replace(str(BASE_OUTPUT_FOLDER), "http://wall-e:1234/crescer")

	if input_path_str != "" and not input_folder.exists():
		st.error(f"Input Folder/File does not exist: {input_folder}")
		has_input = False
	else:
		has_input = True

	st.write("Converted files will be accessible here:", potree_url)

	if has_input and input_path_str != "" and input_folder.is_dir():
		files_to_convert = list(input_folder.rglob("*.las")) + list(input_folder.rglob("*.laz"))
		total_files = len(files_to_convert)
		if total_files == 0:
			st.warning("No LAS/LAZ files found in the input directory.")
		else:
			st.success(f"Found {total_files} LAS/LAZ files in the directory")
			with st.expander(f"View Files"):
				for file in files_to_convert:
					st.write(file.relative_to(input_folder))

	if st.button("Start/Stop", type="primary"):
		if st.session_state.potree_process:
			st.session_state.stop_requested = True
			time.sleep(3)
			st.session_state.potree_process = None
			st.session_state.stop_requested = False
		else:
			st.session_state.stop_requested = False
			st.warning("Do Not Close the Browser Tab , and wait for the conversion to finish")
			convert(input_folder, Path(output_folder), need_cleaning)
		

if __name__ == "__main__":
	main()