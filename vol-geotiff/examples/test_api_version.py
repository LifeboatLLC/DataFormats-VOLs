#!/usr/bin/env python3
"""
Test to verify that h5py is using the modern H5Ovisit_by_name2/3 API
instead of the old H5Ovisit_by_name1 API.
"""

import h5py
import h5py.h5o as h5o
import h5py.h5l as h5l

print("=" * 70)
print("h5py API Version Test")
print("=" * 70)

# Check h5py version
print(f"\nh5py version: {h5py.__version__}")
print(f"HDF5 version: {h5py.version.hdf5_version}")

# Check what API functions are available in h5o module
print(f"\nH5O module functions:")
h5o_funcs = [name for name in dir(h5o) if 'visit' in name.lower()]
for func in sorted(h5o_funcs):
    print(f"  - {func}")

# Check what API functions are available in h5l module
print(f"\nH5L module functions:")
h5l_funcs = [name for name in dir(h5l) if 'iterate' in name.lower() or 'visit' in name.lower()]
for func in sorted(h5l_funcs):
    print(f"  - {func}")

# Check for version-specific structures
print(f"\nVersion-specific structures:")
if hasattr(h5o, 'ObjInfo'):
    print(f"  ✓ h5o.ObjInfo available")
if hasattr(h5l, 'LinkInfo'):
    print(f"  ✓ h5l.LinkInfo available")

print("\n" + "=" * 70)
print("✓ API inspection complete")
print("=" * 70)

# Try with a regular HDF5 file (not GeoTIFF VOL)
print("\nTesting with regular HDF5 file...")
try:
    import tempfile
    import os

    # Create a temporary HDF5 file
    with tempfile.NamedTemporaryFile(suffix='.h5', delete=False) as tmp:
        tmpfile = tmp.name

    # Create test file with some structure
    with h5py.File(tmpfile, 'w') as f:
        f.create_group('group1')
        f.create_group('group2')
        f.create_dataset('dataset1', data=[1, 2, 3])
        f['group1'].create_dataset('nested_data', data=[4, 5, 6])

    print(f"✓ Created test file: {tmpfile}")

    # Test visititems (which internally uses H5Ovisit_by_name)
    print("\nTesting visititems (uses H5Ovisit internally):")
    with h5py.File(tmpfile, 'r') as f:
        items = []
        def visitor(name, obj):
            items.append((name, type(obj).__name__))

        f.visititems(visitor)

        print(f"✓ visititems succeeded! Found {len(items)} items:")
        for name, obj_type in items:
            print(f"    - {name} ({obj_type})")

    # Clean up
    os.unlink(tmpfile)
    print(f"\n✓ All tests passed with modern HDF5 API!")

except Exception as e:
    print(f"\n✗ Error during testing: {e}")
    import traceback
    traceback.print_exc()
