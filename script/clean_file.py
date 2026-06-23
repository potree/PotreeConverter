# from iris.las_tools import load_las
import laspy as  lp
import numpy as np
from pathlib import Path
import pandas as pd
import tqdm
from icecream import ic
import traceback
from concurrent.futures import ProcessPoolExecutor


def load_las(filename, sort=False, col_type="all"):
    """Loads a las file into pandas dataframe, removes NaNs and optionally sorts the df. Defaults to False.

    Note: Following features are included in the dataframe - x, y, z, intensity, return num., num. of returns, class

    Args:
        filename (str): Laspy filename
        sorted (bool, optional): If True, sort the dataframe by all features except classification.
        col_type (str, optional): There can be two col types. "features" is used to get all columns that are used by model and "all" returns a dataframe containing all columns used for making prediction.

    Returns:
        dataframe: Pandas dataframe of points, possibly sorted.
    """
    las = lp.read(filename)
    columns = ["x", "y", "z"]
    if col_type == "features":
        columns = columns + [
            "intensity", "return_number", "number_of_returns",
            "classification", "withheld"
        ]
    elif col_type == "all":
        columns = columns + list(las.point_format.dimension_names)
    else:
        raise Exception(
            f"col_type can only be features or all but {col_type} provided")
    df = pd.DataFrame()
    for col in columns:
        df[col] = np.array(las.__getattr__(col))
    df.dropna(inplace=True)
    if sort:
        # TODO Sort and save?
        df = df.sort_values(by=[
            'x', 'y', 'z', 'intensity', 'return_number', 'number_of_returns'
        ])
        df = df.reset_index(drop=True)
    return df


def create_las(
    df,
    file_loc,
    input_file = None,
    add_debug_headers=False,
    change_header_shape=False,
):

    header = lp.header.LasHeader(version="1.4", point_format=6)
    if input_file:
        input_las = lp.read(input_file)
        header.scale = input_las.header.scale
        header.offset = input_las.header.offset
    else:
        scale = 1e6
        header.scale = [1.0 / scale, 1.0 / scale, 1.0 / scale]
        header.offset = [
            np.floor(np.min(df["x"])),
            np.floor(np.min(df["y"])),
            np.floor(np.min(df["z"])),
        ]

    if change_header_shape:
        header.point_count = len(df)
    # Add headers if required

    if add_debug_headers:
        for col in df.columns:
            if col in ["x", "y", "z"]:
                continue  # These are populated separately above
            if col in ["unclassified", "manually_labelled"]:
                header.add_extra_dim(lp.ExtraBytesParams(name=col, type=np.uint8))

    new_header = lp.LasHeader(version=header.version, point_format=header.point_format)
    new_header.offsets = header.offsets
    new_header.scales = header.scales
    new_header.point_count = header.point_count

    outfile = lp.LasData(new_header)

    available_columns_for_las = set(
        list(outfile.point_format.dimension_names) + ["x", "y", "z"]
    )
    for col in list(set(df.columns).intersection(available_columns_for_las)):
        outfile[col] = df[col].values

    outfile.write(file_loc)

def process_file(file_loc):
    """
    Processes a single LAS/LAZ file by loading, cleaning, and saving it back to its location.
    
    Parameters:
    file_loc (Path): The file path to process.
    """
    try:
        df = load_las(file_loc)  # Load LAS/LAZ data into a DataFrame

        for col in df.columns:
            if df[col].dtype == 'int64':
                df[col] = df[col].astype('int32')
            if df[col].dtype == 'uint64':
                df[col] = df[col].astype('uint32')

        create_las(df, file_loc, file_loc)  # Clean and save it back
    except Exception as e:
        print(f"Error processing {file_loc}: {e}")
        print(traceback.format_exc())

def clean_las(path):
    '''
    Cleans LAS/LAZ files in the given directory or file path using parallel processing.
    This function processes LAS/LAZ files by loading them, performing necessary
    cleaning operations, and then saving the cleaned data back to the original file location.
    
    Parameters:
    path (str): The directory or file path containing the LAS/LAZ files to be cleaned.
    
    Raises:
    Exception: If an error occurs during the processing of the files, the exception
               is caught and its message along with the traceback is printed.
    '''
    try:
        path = Path(path)
        if path.is_dir():
            all_las = list(path.rglob("*.las")) + list(path.rglob("*.laz"))
        else:
            all_las = [path]

        # Use ProcessPoolExecutor for parallel processing
        with ProcessPoolExecutor() as executor:
            list(tqdm.tqdm(executor.map(process_file, all_las), total=len(all_las)))

    except Exception as e:
        print(e)
        print(traceback.format_exc())