# Build the test executable
cmake --build build --target OAO_Tests

# Run the test suite via CTest
cd build
ctest --output-on-failure
