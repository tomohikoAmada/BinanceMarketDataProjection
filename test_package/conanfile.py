from conan import ConanFile
from conan.tools.build import can_run
from conan.tools.cmake import CMake, CMakeConfigDeps, CMakeToolchain, cmake_layout


class BinanceMarketDataProjectionTestPackage(ConanFile):
    test_type = "explicit"
    settings = "os", "arch", "compiler", "build_type"

    def requirements(self):
        self.requires(self.tested_reference_str)

    def layout(self):
        cmake_layout(self)

    def generate(self):
        # The tested package owns the canonical config and sets cmake_find_mode
        # to none. CMakeConfigDeps propagates its builddirs through Conan's
        # generated path metadata without creating a shadow Projection config.
        dependencies = CMakeConfigDeps(self)
        # Contracts' canonical config asks for "Protobuf". Normalize Conan's
        # generated dependency filename so that host-installed Protobuf cannot
        # preempt the exact package in this test graph.
        dependencies.set_property("protobuf", "cmake_file_name", "Protobuf")
        dependencies.generate()
        CMakeToolchain(self).generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def test(self):
        if can_run(self):
            self.run(self.cpp.build.bindirs[0] + "/projection_test_package", env="conanrun")
