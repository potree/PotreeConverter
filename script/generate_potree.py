import os
import sys
import subprocess
import laspy
from clean_file import clean_las , load_las
from pathlib import Path
from icecream import ic

POTREE_CONVERTER_PATH = "/app/PotreeConverter/build/PotreeConverter"

def process_file(file_path, output_dir, remove_int64):
    """
    Processes a single LAS or LAZ file. Cleans it using the clean_las function,
    and then runs PotreeConverter to generate visualization.

    Args:
        file_path (str): Path to the LAS/LAZ file.
        output_dir (str): Directory to store the generated pointcloud.
        remove_int64 (bool): Whether to allow cleaning of int64/uint64 columns.

    Raises:
        ValueError: If cleaning is required but `remove_int64` is not passed.
    """
    try:
        # Check and clean file using clean_las
        if remove_int64:
            clean_las(file_path)
            
        # Prepare the output directory for PotreeConverter
        os.makedirs(output_dir, exist_ok=True)

        # Run PotreeConverter
        potree_command = f"{POTREE_CONVERTER_PATH} {file_path} -o {output_dir} -p {Path(file_path).stem}"
        ic(f"Running PotreeConverter: {potree_command}")
        print(f"Running PotreeConverter: {potree_command}")
        subprocess.run(potree_command, shell=True, check=True)

    except Exception as e:
        print(f"Error processing file {file_path}: {e}")


def process_directory(dir_path, output_dir , remove_int64):
    """
    Processes all `.las` and `.laz` files in the given directory and subdirectories.

    Args:
        dir_path (str): Path to the directory.
        remove_int64 (bool): Whether to remove `int64`/`uint64` columns.
    """
    
    ic(output_dir)
    os.makedirs(output_dir, exist_ok=True)
    
    for entry in os.listdir(dir_path):
        entry_path = os.path.join(dir_path, entry)
        ic(entry_path)
        # If entry is a directory, call the function recursively
        if os.path.isdir(entry_path):
            sub_dir = os.path.join(output_dir, entry_path.split("/")[-1])
            process_directory(entry_path, sub_dir, remove_int64)
            
        
        # If entry is a file, process if it has `.las` or `.laz` extension
        elif entry_path.endswith((".las", ".laz")):
            process_file(entry_path, output_dir, remove_int64)


def main():
    """
    Main function to parse arguments and execute the script.

    Raises:
        ValueError: If input file or directory does not exist.
    """
    if len(sys.argv) < 2:
        print("Usage: ./add_visualisation <file_name>/<dirname> [--remove_int64]")
        sys.exit(1)
    
    input_path = sys.argv[1]
    output_dir = sys.argv[2] 
    remove_int64 = "--remove_int64" in sys.argv
    
    if not os.path.exists(input_path):
        raise ValueError(f"File or directory {input_path} does not exist.")
    
    if os.path.isfile(input_path):
        process_file(input_path, output_dir, remove_int64)
    elif os.path.isdir(input_path):
        process_directory(input_path,output_dir, remove_int64)
    else:
        raise ValueError(f"Invalid input {input_path}. Provide a file or directory.")

    print(f"Access your page at: server:1234/crescer/pointclouds/{Path(input_path).name}")

if __name__ == "__main__":
    main()
