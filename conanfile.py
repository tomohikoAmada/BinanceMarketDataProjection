from conan import ConanFile
from conan.tools.cmake import CMakeDeps, CMakeToolchain, cmake_layout


class BinanceMarketDataProjectionConan(ConanFile):
    name = "binance-market-data-projection"
    version = "0.1.0"
    package_type = "library"
    settings = "os", "compiler", "build_type", "arch"
    options = {"shared": [True, False], "fPIC": [True, False]}
    default_options = {"shared": False, "fPIC": True}

    def requirements(self):
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
        toolchain.generate()
