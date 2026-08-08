from pathlib import Path

from conan import ConanFile
from conan.tools.cmake import CMakeDeps, CMakeToolchain


class ProtoAdapterConsumerConan(ConanFile):
    settings = "os", "arch", "compiler", "build_type"

    def requirements(self):
        self.requires(
            "binance-market-data-contracts-cpp/0.1.0#7fd3efe3d289462fb16c78ffeced1682"
        )

    def generate(self):
        CMakeDeps(self).generate()
        toolchain = CMakeToolchain(self)
        contracts = self.dependencies.host["binance-market-data-contracts-cpp"]
        toolchain.variables["BinanceMarketDataContracts_DIR"] = str(
            Path(contracts.package_folder) / "lib" / "cmake" / "BinanceMarketDataContracts"
        )
        toolchain.generate()
