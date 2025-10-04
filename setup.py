from pybind11.setup_helpers import Pybind11Extension, build_ext
from setuptools import setup
import pybind11

__version__ = "1.0.0"

ext_modules = [
    Pybind11Extension(
        "lob_backtester",
        [
            "pybind_module.cpp",
        ],
        include_dirs=[
            # Path to pybind11 headers
            pybind11.get_cmake_dir(),
            "../include",  # LOB headers
        ],
        libraries=["LOBCore"],
        library_dirs=["../build/lib"],
        cxx_std=17,
        define_macros=[("VERSION_INFO", '"{}"'.format(__version__))],
    ),
]

setup(
    name="lob-backtester",
    version=__version__,
    author="Your Name",
    author_email="your.email@example.com",
    url="https://github.com/yourusername/lob-backtester",
    description="High-performance Limit Order Book Simulator & Event-Driven Backtester",
    long_description="""
    A professional-grade C++17 implementation of a limit order book simulator
    and event-driven backtesting engine for quantitative finance research and
    algorithmic trading development. 
    
    Features:
    - Sub-microsecond latency order book operations
    - Full L2/L3 market data support  
    - Comprehensive microstructure signals (order imbalance, microprice, etc.)
    - Event-driven backtesting framework
    - Portfolio performance analytics
    - Python bindings for research workflows
    """,
    long_description_content_type="text/plain",
    ext_modules=ext_modules,
    cmdclass={"build_ext": build_ext},
    zip_safe=False,
    python_requires=">=3.8",
    install_requires=[
        "numpy>=1.19.0",
        "pandas>=1.2.0",
    ],
    extras_require={
        "dev": [
            "pytest>=6.0",
            "black>=21.0",
            "mypy>=0.900",
            "jupyter>=1.0.0",
            "matplotlib>=3.3.0",
            "seaborn>=0.11.0",
        ],
        "docs": [
            "sphinx>=4.0.0",
            "sphinx-rtd-theme>=0.5.0",
        ],
    },
    classifiers=[
        "Development Status :: 4 - Beta",
        "Intended Audience :: Developers",
        "Intended Audience :: Financial and Insurance Industry",
        "Intended Audience :: Science/Research",
        "License :: OSI Approved :: MIT License",
        "Operating System :: OS Independent",
        "Programming Language :: C++",
        "Programming Language :: Python :: 3",
        "Programming Language :: Python :: 3.8",
        "Programming Language :: Python :: 3.9",
        "Programming Language :: Python :: 3.10",
        "Programming Language :: Python :: 3.11",
        "Topic :: Office/Business :: Financial",
        "Topic :: Scientific/Engineering",
        "Topic :: Software Development :: Libraries :: Python Modules",
    ],
    keywords="finance, trading, orderbook, backtesting, hft, quantitative, algorithmic-trading, market-microstructure",
    project_urls={
        "Bug Reports": "https://github.com/yourusername/lob-backtester/issues",
        "Source": "https://github.com/yourusername/lob-backtester",
        "Documentation": "https://lob-backtester.readthedocs.io/",
    },
)