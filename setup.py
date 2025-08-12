import subprocess
import os
import shutil
import urllib.request
import winreg
import sys

def run_command(command, cwd = None, capture_output = False):
    try:
        process = subprocess.Popen(
            command, shell = True, cwd = cwd,
            stdout=subprocess.PIPE, stderr=subprocess.PIPE, text = True
        )

        output_lines = []
        for line in process.stdout:
            print(line, end = "")  # Print live output
            output_lines.append(line.strip())

        process.wait()

        if process.returncode != 0:
            print(f"Error running command: {command}")
            print(f"Exit Code: {process.returncode}")
            for line in process.stderr:
                print(line, end = "") 
            hold_window()
            return None  # Return None to indicate failure

        return "\n".join(output_lines) if capture_output else True  # Return full output if needed
    except Exception as e:
        print(f"Unexpected error running command: {command}\n{e}")
        hold_window()
        return None

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
        with winreg.OpenKey(winreg.HKEY_CURRENT_USER,
                            r"SOFTWARE\Microsoft\Windows\CurrentVersion\Explorer\Shell Folders") as key:
            downloads_folder = winreg.QueryValueEx(key, "{374DE290-123F-4565-9164-39C4925E467B}")[0]
            return downloads_folder
    except Exception as e:
        print(f"Error retrieving downloads folder: {e}")
        hold_window()
        return None
        
def upgrade_vcpkg_and_dependencies(vcpkg_path):
    print("Resetting vcpkg repository to a clean state...")
    if run_command("git reset --hard HEAD", cwd=vcpkg_path) is None:
        print("Failed to reset vcpkg. Continuing with existing versions.")
        return
    
    print("Updating vcpkg repository to get the latest package versions...")
    if run_command("git pull origin master", cwd=vcpkg_path) is None:
        print("Failed to update vcpkg. Continuing with existing versions.")
        return

    print("\nChecking for outdated packages and upgrading them...")
    upgrade_command = "vcpkg.exe upgrade --no-dry-run"
    if run_command(upgrade_command, cwd=vcpkg_path) is None:
        print("Package upgrade process failed.")
        hold_window()

def install_dependencies(vcpkg_path, dependencies):
    print("Installing dependencies...")
    install_command = "vcpkg.exe install --recurse --triplet x64-windows " + " ".join(dependencies)
    if run_command(install_command, cwd=vcpkg_path) is None:
        print("Dependency installation failed.")
        hold_window()
        return

def verify_dependencies(vcpkg_path, dependencies):
    print("Verifying dependency installations...")
    installed_output = run_command("vcpkg.exe list", cwd = vcpkg_path, capture_output = True)
    if installed_output is None:
        print("ERROR: Failed to check installed packages.")
        hold_window()
        return

    installed_packages = installed_output.splitlines()
    installed_set = {pkg.split(":")[0] for pkg in installed_packages if pkg}

    simple_dependencies = [dep.split('[')[0] for dep in dependencies]

    missing = [dep.split(":")[0] for dep in simple_dependencies if dep.split(":")[0] not in installed_set]

    if not missing:
        print("All dependencies are successfully installed!")
    else:
        print("ERROR: The following dependencies were not installed successfully:")
        for dep in missing:
            print(f" - {dep}")
        hold_window()

def hold_window():
    input("Press Enter to exit...")

def main():
    current_dir = os.path.dirname(os.path.abspath(__file__))
    print(current_dir)

    print("Checking for .gitmodules...")
    if path_exists(os.path.join(current_dir, ".gitmodules")):
        run_command("git submodule update --init --recursive", cwd = current_dir)

    vcpkg_path = os.path.join(current_dir, "Engine", "External", "vcpkg")   
    print(f"vcpkg path is: {vcpkg_path}")

    print("Updating vcpkg repository...")
    if run_command("git pull origin master", cwd=vcpkg_path) is None:
        print("Failed to update vcpkg. Aborting.")
        hold_window()
        return

    print("Bootstrapping vcpkg to build the tool...")
    if run_command("bootstrap-vcpkg.bat", cwd = vcpkg_path) is None:
        print("Failed to bootstrap vcpkg. Aborting.")
        hold_window()
        return

    print("\nChecking for outdated packages and upgrading them...")
    upgrade_command = "vcpkg.exe upgrade --no-dry-run"
    if run_command(upgrade_command, cwd=vcpkg_path) is None:
        print("Package upgrade process failed.")

    dependencies = [
        "msdfgen[core,extensions]:x64-windows",
        "freetype[core]:x64-windows",
        "glfw3:x64-windows",
        "stb:x64-windows",
        "miniaudio:x64-windows",
        "shader-slang:x64-windows",
    ]

    install_dependencies(vcpkg_path, dependencies)
    verify_dependencies(vcpkg_path, dependencies)

if __name__ == "__main__":
    try:
        main()
    except Exception as e:
        print(f"An unexpected error occurred: {e}")
    finally:
        input("Press Enter to exit...")