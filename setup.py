import subprocess
import os
import shutil
import urllib.request
import winreg

current_dir = os.getcwd()

def run_command(command, cwd=None):
    """Runs a shell command and returns its output. Raises an error if it fails."""
    try:
        result = subprocess.run(command, shell=True, cwd=cwd, check=True, capture_output=True, text=True)
        print(result.stdout)  # Print command output
        return result.stdout.strip()  # Return stripped output for verification
    except subprocess.CalledProcessError as e:
        print(f"Error running command: {command}")
        print(f"Exit Code: {e.returncode}")
        print(f"Error Output: {e.stderr}")
        raise RuntimeError(f"Command failed: {command}") from e

def path_exists(path):
    return os.path.exists(path)

def ensure_directory(directory):
    if not os.path.exists(directory):
        os.makedirs(directory)

def delete_directory(directory):
    if os.path.exists(directory):
        shutil.rmtree(directory)

def download_file(url, destination):
    with urllib.request.urlopen(url) as response, open(destination, 'wb') as out_file:
        shutil.copyfileobj(response, out_file)

def get_downloads_folder():
    try:
        with winreg.OpenKey(winreg.HKEY_CURRENT_USER, r"SOFTWARE\Microsoft\Windows\CurrentVersion\Explorer\Shell Folders") as key:
            downloads_folder = winreg.QueryValueEx(key, "{374DE290-123F-4565-9164-39C4925E467B}")[0]
            return downloads_folder
    except Exception as e:
        print(f"Error retrieving downloads folder: {e}")
        return None

# List of dependencies to install
dependencies = [
    "msdfgen:x64-windows",
    "freetype:x64-windows",
    "vulkan:x64-windows"
]

print("Checking if there exists a .gitmodules file")
if path_exists(os.path.join(current_dir, ".gitmodules")):
    run_command("git submodule update --init --recursive")

print("Bootstrapping vcpkg")
vcpkg_path = os.path.join(current_dir, "Engine", "External", "vcpkg")
if not path_exists(os.path.join(vcpkg_path, "vcpkg.exe")):
    run_command("bootstrap-vcpkg.bat", cwd=vcpkg_path)

print("Installing dependencies")
install_command = "vcpkg.exe install " + " ".join(dependencies)
run_command(install_command, cwd=vcpkg_path)

print("Verifying dependencies installation...")
try:
    installed_packages = run_command("vcpkg.exe list", cwd=vcpkg_path).split("\n")

    installed_set = {pkg.split()[0] for pkg in installed_packages}

    missing_dependencies = [dep for dep in dependencies if dep.split(":")[0] not in installed_set]

    if not missing_dependencies:
        print("All dependencies installed successfully!")
    else:
        print("ERROR: The following dependencies were not installed successfully:")
        for missing in missing_dependencies:
            print(f" - {missing}")
        exit(1)
except RuntimeError:
    print("ERROR: Failed to check installed packages.")
    exit(1)

print("Checking for Vulkan SDK installation...")
if not path_exists(os.path.join(os.environ.get("VULKAN_SDK", ""), "Include")):
    print("Vulkan SDK not found. Trying to install...")
    downloads_folder = get_downloads_folder()
    if downloads_folder:
        download_path = os.path.join(downloads_folder, "vulkan_sdk.exe")
        print(f"Downloading Vulkan SDK to {download_path}...")
        download_file("https://sdk.lunarg.com/sdk/download/latest/windows/vulkan_sdk.exe", download_path)
    else:
        print("Could not retrieve the default downloads folder.")
    exit(1)