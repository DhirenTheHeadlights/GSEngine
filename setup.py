import subprocess
import os
import shutil
import urllib.request
import winreg
import sys

def run_command(command, cwd=None):
    """Run a shell command and return its output. Raises an error if it fails."""
    try:
        result = subprocess.run(
            command, shell=True, cwd=cwd, check=True,
            capture_output=True, text=True
        )
        print(result.stdout)
        return result.stdout.strip()
    except subprocess.CalledProcessError as e:
        print(f"Error running command: {command}")
        print(f"Exit Code: {e.returncode}")
        print(f"Error Output: {e.stderr}")
        hold_window()
        return None  # Instead of exiting, return None so script continues

def path_exists(path):
    """Check if a given file or directory exists."""
    return os.path.exists(path)

def ensure_directory(directory):
    """Create directory if it does not exist."""
    if not os.path.exists(directory):
        os.makedirs(directory)

def delete_directory(directory):
    """Delete directory if it exists."""
    if os.path.exists(directory):
        shutil.rmtree(directory)

def download_file(url, destination):
    """Download a file from the specified URL to the destination."""
    with urllib.request.urlopen(url) as response, open(destination, 'wb') as out_file:
        shutil.copyfileobj(response, out_file)

def get_downloads_folder():
    """Retrieve the current user's Downloads folder path."""
    try:
        with winreg.OpenKey(winreg.HKEY_CURRENT_USER,
                            r"SOFTWARE\Microsoft\Windows\CurrentVersion\Explorer\Shell Folders") as key:
            downloads_folder = winreg.QueryValueEx(key, "{374DE290-123F-4565-9164-39C4925E467B}")[0]
            return downloads_folder
    except Exception as e:
        print(f"Error retrieving downloads folder: {e}")
        hold_window()
        return None

def install_dependencies(vcpkg_path, dependencies):
    print("Installing dependencies...")
    install_command = "vcpkg.exe install " + " ".join(dependencies)
    if run_command(install_command, cwd=vcpkg_path) is None:
        print("Dependency installation failed.")
        hold_window()
        return

def verify_dependencies(vcpkg_path, dependencies):
    print("Verifying dependency installations...")
    installed_output = run_command("vcpkg.exe list", cwd=vcpkg_path)
    if installed_output is None:
        print("ERROR: Failed to check installed packages.")
        hold_window()
        return

    installed_packages = installed_output.splitlines()
    installed_set = {pkg.split(":")[0] for pkg in installed_packages if pkg}  # Extract package names before ':'

    missing = [dep.split(":")[0] for dep in dependencies if dep.split(":")[0] not in installed_set]

    if not missing:
        print("All dependencies are already installed and up to date!")
    else:
        print("ERROR: The following dependencies were not installed successfully:")
        for dep in missing:
            print(f" - {dep}")
        hold_window()

def hold_window():
    """Keep the CMD window open by waiting for user input."""
    input("Press Enter to exit...")

def main():
    current_dir = os.getcwd()

    # Initialize submodules if .gitmodules exists.
    print("Checking for .gitmodules...")
    if path_exists(os.path.join(current_dir, ".gitmodules")):
        run_command("git submodule update --init --recursive")

    # Bootstrap vcpkg.
    print("Bootstrapping vcpkg...")
    vcpkg_path = os.path.join(current_dir, "Engine", "External", "vcpkg")
    if not path_exists(os.path.join(vcpkg_path, "vcpkg.exe")):
        run_command("bootstrap-vcpkg.bat", cwd=vcpkg_path)

    # Define dependencies.
    dependencies = [
        "msdfgen:x64-windows",
        "freetype:x64-windows"
    ]

    install_dependencies(vcpkg_path, dependencies)
    verify_dependencies(vcpkg_path, dependencies)

    hold_window()

if __name__ == "__main__":
    try:
        main()
    except Exception as e:
        print(f"An unexpected error occurred: {e}")
    finally:
        input("Press Enter to exit...")
