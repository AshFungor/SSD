from conan import ConanFile

class SoundServer(ConanFile):
    version = "0.0"
    settings = ["os", "compiler", "arch", "build_type"]
    generators = ["CMakeToolchain", "CMakeDeps"]
    requires = ["plog/1.1.10", "gtest/1.14.0", "protobuf/3.21.12"]