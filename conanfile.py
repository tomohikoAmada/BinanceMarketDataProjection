from pathlib import Path
from typing import ClassVar

from conan import ConanFile
from conan.tools.cmake import CMakeDeps, CMakeToolchain, cmake_layout


class BinanceMarketDataProjectionConan(ConanFile):
    name = "binance-market-data-projection"
    version = "0.1.0"
    package_type = "library"
    settings = "os", "compiler", "build_type", "arch"
    options: ClassVar[dict[str, list[bool]]] = {
        "shared": [True, False],
        "fPIC": [True, False],
        "proto_adapter": [True, False],
    }
    default_options: ClassVar[dict[str, bool]] = {
        "shared": False,
        "fPIC": True,
        "proto_adapter": False,
    }

    def configure(self):
        if self.options.proto_adapter:
            self.options["binance-market-data-contracts-cpp/*"].shared = self.options.shared

    def requirements(self):
        if self.options.proto_adapter:
            self.requires(
                "binance-market-data-contracts-cpp/0.1.0#7fd3efe3d289462fb16c78ffeced1682",
                transitive_headers=True,
                transitive_libs=True,
            )
        self.test_requires("gtest/1.17.0")
        self.test_requires("benchmark/1.9.5")

    def layout(self):
        cmake_layout(self, generator="Ninja")

    def generate(self):
        dependencies = CMakeDeps(self)
        dependencies.generate()

        toolchain = CMakeToolchain(self)
        toolchain.user_presets_path = False
        toolchain.variables["BUILD_SHARED_LIBS"] = self.options.shared
        toolchain.variables["BMD_PROJECTION_BUILD_PROTO_ADAPTER"] = self.options.proto_adapter
        if self.options.proto_adapter:
            contracts = self.dependencies.host["binance-market-data-contracts-cpp"]
            toolchain.variables["BinanceMarketDataContracts_DIR"] = str(
                Path(contracts.package_folder) / "lib" / "cmake" / "BinanceMarketDataContracts"
            )
        toolchain.generate()
