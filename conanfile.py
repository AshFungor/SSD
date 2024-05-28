from conan import ConanFile
from conan.tools.cmake import cmake_layout

class SoundServer(ConanFile):
    version = "0.0"
    generators = ["CMakeToolchain", "CMakeDeps"]
    settings = "os", "compiler", "build_type", "arch"
    tool_requires = ["protobuf/3.21.12"]

    def requirements(self):
        for req in [
            "plog/1.1.10",
            "gtest/1.14.0",
            "protobuf/3.21.12",
            "nlohmann_json/3.11.3",
            "boost/1.84.0",
            "libalsa/1.2.10"
        ]:
            self.requires(req)

    def layout(self):
        cmake_layout(self)
