from conan import ConanFile

class SoundServer(ConanFile):
    version = "0.0"
    settings = ["os", "compiler", "arch", "build_type"]
    generators = ["CMakeToolchain", "CMakeDeps"]
    requires = [
        "plog/1.1.10",
        "gtest/1.14.0",
        "protobuf/3.21.12",
        "nlohmann_json/3.11.3",
        "boost/1.84.0",
        "libalsa/1.2.10"]
    tool_requires = ["protobuf/3.21.12"]
