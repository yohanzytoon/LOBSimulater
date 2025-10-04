import pandas as pd

# Create a comprehensive overview of the project files
project_files = [
    {"File/Directory", "Type", "Purpose", "Key Features"},
    {"README.md", "Documentation", "Project overview and quick start", "Features, installation, usage examples"},
    {"CMakeLists.txt", "Build System", "Top-level build configuration", "Modern CMake, dependencies, options"},
    {"LICENSE", "Legal", "MIT license", "Open source permissions"},
    {".gitignore", "Version Control", "Git ignore patterns", "Build files, IDE files, OS files"},
    {".clang-format", "Code Style", "Code formatting rules", "Google C++ style with customizations"},
    {".clang-tidy", "Static Analysis", "Code quality checks", "Comprehensive rule set for C++17"},
    {".github/workflows/ci.yml", "CI/CD", "Continuous integration", "Multi-platform builds, tests, sanitizers"},
    
    # Core Headers
    {"include/lob/order_book.hpp", "Core Header", "Main order book implementation", "Price-time priority, L2/L3 data, O(1) operations"},
    {"include/lob/signals.hpp", "Core Header", "Microstructure signal analytics", "Order imbalance, microprice, market quality"},
    {"include/lob/backtester.hpp", "Core Header", "Event-driven backtesting engine", "Strategy framework, portfolio management"},
    {"include/lob/metrics.hpp", "Core Header", "Performance and risk analytics", "Sharpe ratio, VaR, drawdown analysis"},
    
    # Implementation Files
    {"src/order_book.cpp", "Implementation", "Order book core logic", "Matching engine, trade generation"},
    {"src/signals.cpp", "Implementation", "Signal calculation algorithms", "Advanced microstructure metrics"},
    {"src/backtester.cpp", "Implementation", "Backtesting engine logic", "Event handling, execution simulation"},
    {"src/metrics.cpp", "Implementation", "Analytics calculations", "Statistical and risk computations"},
    {"src/main.cpp", "Application", "Example usage driver", "Demonstration of core functionality"},
    
    # Python Integration
    {"bindings/pybind_module.cpp", "Python Bindings", "pybind11 wrapper code", "Full API exposure to Python"},
    {"bindings/CMakeLists.txt", "Build System", "Python module build config", "Cross-platform Python integration"},
    {"bindings/setup.py", "Python Package", "Pip-installable package", "NumPy/Pandas integration"},
    
    # Testing
    {"tests/test_order_book.cpp", "Unit Tests", "Order book functionality tests", "100+ test cases, edge cases"},
    {"tests/CMakeLists.txt", "Build System", "Test build configuration", "GoogleTest integration, sanitizers"},
    
    # Benchmarking
    {"benchmarks/bench_order_book.cpp", "Performance Tests", "Latency and throughput benchmarks", "Google Benchmark, profiling"},
    {"benchmarks/CMakeLists.txt", "Build System", "Benchmark build configuration", "Performance monitoring"},
    
    # Examples
    {"examples/strategy_examples.cpp", "Examples", "Trading strategy implementations", "Market maker, momentum, signal-based"},
    
    # Documentation
    {"docs/Doxyfile", "Documentation", "API documentation config", "Automatic code documentation"},
    {"docs/architecture.md", "Documentation", "System design overview", "Component interactions"},
    {"docs/signals.md", "Documentation", "Signal descriptions", "Mathematical formulations"},
]

# Convert to DataFrame for better formatting
df = pd.DataFrame(project_files[1:], columns=project_files[0])

# Save to CSV
df.to_csv('project_structure.csv', index=False)

print("Project Structure Overview:")
print("=" * 80)
print(f"Total Files: {len(df)}")
print(f"C++ Headers: {len(df[df['Type'] == 'Core Header'])}")
print(f"C++ Implementation: {len(df[df['Type'] == 'Implementation'])}")
print(f"Test Files: {len(df[df['Type'] == 'Unit Tests'])}")
print(f"Build Files: {len(df[df['Type'] == 'Build System'])}")
print(f"Documentation: {len(df[df['Type'] == 'Documentation'])}")
print(f"Python Integration: {len(df[df['Type'].str.contains('Python')])}")

# Group by type for summary
type_counts = df.groupby('Type').size().sort_values(ascending=False)
print("\nFile Types Distribution:")
for file_type, count in type_counts.items():
    print(f"  {file_type}: {count} files")

print(f"\nProject saved to: project_structure.csv")