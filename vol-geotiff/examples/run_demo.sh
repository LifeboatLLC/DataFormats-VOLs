#!/bin/bash
#
# GeoTIFF VOL Connector Visualization Demo
#
# This script orchestrates the complete workflow:
# 1. Build the demo image creator
# 2. Generate a GeoTIFF with colorful concentric rings
# 3. Check and install Python dependencies
# 4. Set up HDF5 VOL connector environment variables
# 5. Run Python visualization using h5py + xarray
#

set -e  # Exit on error

# Color output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}GeoTIFF VOL Connector Visualization Demo${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

# Determine script directory and project root
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_ROOT/build"
EXAMPLES_DIR="$SCRIPT_DIR"

echo -e "${BLUE}[1/6]${NC} Checking project structure..."
echo "  Script directory: $SCRIPT_DIR"
echo "  Project root: $PROJECT_ROOT"
echo "  Build directory: $BUILD_DIR"

# Check if build directory exists
if [ ! -d "$BUILD_DIR" ]; then
    echo -e "${RED}ERROR: Build directory not found: $BUILD_DIR${NC}"
    echo "Please build the project first:"
    echo "  cd $PROJECT_ROOT"
    echo "  mkdir build && cd build"
    echo "  cmake .."
    echo "  make"
    exit 1
fi

# Check if VOL connector library exists
VOL_CONNECTOR_LIB="$BUILD_DIR/src/libgeotiff_vol_connector.so"
if [ ! -f "$VOL_CONNECTOR_LIB" ]; then
    echo -e "${RED}ERROR: VOL connector library not found: $VOL_CONNECTOR_LIB${NC}"
    echo "Please build the project first."
    exit 1
fi
echo -e "${GREEN}✓${NC} Found VOL connector library"

# Check if demo image creator exists
DEMO_CREATOR="$BUILD_DIR/examples/create_demo_image"
if [ ! -f "$DEMO_CREATOR" ]; then
    echo -e "${YELLOW}Building demo image creator...${NC}"
    cd "$BUILD_DIR"
    make create_demo_image
    if [ ! -f "$DEMO_CREATOR" ]; then
        echo -e "${RED}ERROR: Failed to build create_demo_image${NC}"
        exit 1
    fi
fi
echo -e "${GREEN}✓${NC} Demo image creator ready"

# Create the demo GeoTIFF
echo ""
echo -e "${BLUE}[2/6]${NC} Creating demo GeoTIFF image..."
cd "$EXAMPLES_DIR"
if "$DEMO_CREATOR"; then
    echo -e "${GREEN}✓${NC} Demo image created: demo_rings.tif"
else
    echo -e "${RED}ERROR: Failed to create demo image${NC}"
    exit 1
fi

# Check Python installation
echo ""
echo -e "${BLUE}[3/6]${NC} Checking Python environment..."
if ! command -v python3 &> /dev/null; then
    echo -e "${RED}ERROR: python3 not found${NC}"
    echo "Please install Python 3"
    exit 1
fi
echo -e "${GREEN}✓${NC} Python 3 found: $(python3 --version)"

# Check and install Python dependencies
echo ""
echo -e "${BLUE}[4/6]${NC} Checking Python dependencies..."

REQUIRED_PACKAGES=("numpy" "h5py" "xarray" "matplotlib")
MISSING_PACKAGES=()

for package in "${REQUIRED_PACKAGES[@]}"; do
    if python3 -c "import $package" &> /dev/null; then
        echo -e "${GREEN}✓${NC} $package installed"
    else
        echo -e "${YELLOW}✗${NC} $package not found"
        MISSING_PACKAGES+=("$package")
    fi
done

# Install missing packages
if [ ${#MISSING_PACKAGES[@]} -gt 0 ]; then
    echo ""
    echo -e "${YELLOW}Installing missing packages: ${MISSING_PACKAGES[*]}${NC}"
    echo "This may take a few minutes..."

    # Check if pip is available
    if ! command -v pip3 &> /dev/null; then
        echo -e "${RED}ERROR: pip3 not found${NC}"
        echo "Please install pip3 or install these packages manually:"
        echo "  ${MISSING_PACKAGES[*]}"
        exit 1
    fi

    # Install missing packages
    if pip3 install "${MISSING_PACKAGES[@]}" --user; then
        echo -e "${GREEN}✓${NC} Successfully installed missing packages"
    else
        echo -e "${RED}ERROR: Failed to install packages${NC}"
        echo "Please install manually:"
        echo "  pip3 install ${MISSING_PACKAGES[*]}"
        exit 1
    fi
else
    echo -e "${GREEN}✓${NC} All required Python packages are installed"
fi

# Set up environment variables for HDF5 VOL connector
echo ""
echo -e "${BLUE}[5/6]${NC} Setting up HDF5 VOL connector environment..."

export HDF5_PLUGIN_PATH="$BUILD_DIR/src"
export HDF5_VOL_CONNECTOR="geotiff_vol_connector"

echo "  HDF5_PLUGIN_PATH=$HDF5_PLUGIN_PATH"
echo "  HDF5_VOL_CONNECTOR=$HDF5_VOL_CONNECTOR"
echo -e "${GREEN}✓${NC} Environment configured"

# Run the Python visualization script
echo ""
echo -e "${BLUE}[6/6]${NC} Running visualization script..."
echo ""

cd "$EXAMPLES_DIR"
if python3 visualize_demo.py demo_rings.tif; then
    echo ""
    echo -e "${GREEN}========================================${NC}"
    echo -e "${GREEN}Demo completed successfully!${NC}"
    echo -e "${GREEN}========================================${NC}"
    echo ""
    echo "Output files created in: $EXAMPLES_DIR"
    echo "  - demo_rings.tif (256x256 RGB GeoTIFF with concentric rings)"
    echo "  - demo_visualization.png (matplotlib visualization)"
    echo ""
    echo "What happened:"
    echo "  1. Created a GeoTIFF file with colorful concentric rings"
    echo "  2. Loaded it through h5py using the GeoTIFF VOL connector"
    echo "  3. Converted to xarray DataArray"
    echo "  4. Visualized with matplotlib"
    echo "  5. Saved and displayed the result"
    echo ""
else
    echo ""
    echo -e "${RED}========================================${NC}"
    echo -e "${RED}Demo failed${NC}"
    echo -e "${RED}========================================${NC}"
    echo ""
    echo "Troubleshooting:"
    echo "  - Check that HDF5 develop branch is installed"
    echo "  - Verify h5py is built against the same HDF5 version"
    echo "  - Check $HDF5_PLUGIN_PATH contains libgeotiff_vol_connector.so"
    echo ""
    exit 1
fi
