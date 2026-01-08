#!/bin/bash
# Setup script for Multi-Object Tracking dependencies

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

print_status() {
    echo -e "${GREEN}==>${NC} $1"
}

print_error() {
    echo -e "${RED}Error:${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}Warning:${NC} $1"
}

# Function to check if command exists
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# Function to check system dependencies
check_system_dependencies() {
    print_status "Checking system dependencies..."
    
    local missing_deps=()
    
    # Check for required commands
    local required_commands=("cmake" "git" "pkg-config")
    for cmd in "${required_commands[@]}"; do
        if ! command_exists "$cmd"; then
            missing_deps+=("$cmd")
        fi
    done
    
    # Check for required packages (Linux)
    if [[ "$(uname)" == "Linux" ]]; then
        if ! dpkg -l | grep -q "libopencv-dev"; then
            missing_deps+=("libopencv-dev")
        fi
        if ! dpkg -l | grep -q "libgoogle-glog-dev"; then
            missing_deps+=("libgoogle-glog-dev")
        fi
        if ! dpkg -l | grep -q "libeigen3-dev"; then
            missing_deps+=("libeigen3-dev")
        fi
    fi
    
    if [[ ${#missing_deps[@]} -gt 0 ]]; then
        print_error "Missing system dependencies: ${missing_deps[*]}"
        print_status "Please install them using your package manager:"
        if [[ "$(uname)" == "Linux" ]]; then
            print_status "sudo apt update && sudo apt install -y ${missing_deps[*]}"
        fi
        return 1
    fi
    
    print_status "All system dependencies are available"
}

# Function to show usage
show_usage() {
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "This script checks system dependencies for the vision-tracking project."
    echo "For inference backend setup (ONNX Runtime, TensorRT, etc.), please refer to:"
    echo "https://github.com/olibartfast/object-detection-inference#-requirements"
    echo ""
    echo "Options:"
    echo "  --help                     Show this help message"
    echo ""
    echo "Examples:"
    echo "  $0                         # Check system dependencies"
}

# Main script
main() {
    # Parse command line arguments
    while [[ $# -gt 0 ]]; do
        case $1 in
            --help)
                show_usage
                exit 0
                ;;
            *)
                print_error "Unknown option: $1"
                show_usage
                exit 1
                ;;
        esac
    done
    
    print_status "Multi-Object Tracking Dependency Checker"
    print_status "========================================"
    
    # Check system dependencies
    if ! check_system_dependencies; then
        exit 1
    fi
    
    print_status ""
    print_status "System dependencies check completed successfully!"
    print_status ""
    print_status "For inference backend setup (ONNX Runtime, TensorRT, etc.), please refer to:"
    print_status "https://github.com/olibartfast/object-detection-inference"
    print_status ""
    print_status "You can now build the project with:"
    print_status "mkdir build && cd build"
    print_status "cmake -DDEFAULT_BACKEND=ONNX_RUNTIME .."
    print_status "cmake --build ."
}

# Run main function with all arguments
main "$@"
