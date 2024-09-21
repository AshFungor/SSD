from conan import ConanFile
from conan.tools.cmake import cmake_layout

class SoundServer(ConanFile):
    version = '0.0'
    generators = 'CMakeToolchain', 'CMakeDeps'
    settings = 'os', 'compiler', 'build_type', 'arch'
    tool_requires = 'protobuf/5.27.0'

    def requirements(self):
        for req in [
            'zlib/1.3.1',           # Deflate-based compression library: https://conan.io/center/recipes/zlib
            'c-ares/1.33.1',        # Async DNS requests: https://conan.io/center/recipes/c-ares
            're2/20240702',         # Regular Expressions: https://conan.io/center/recipes/re2
            'openssl/3.3.1',        # TLS & SSL: https://conan.io/center/recipes/openssl
            'abseil/20240116.2',    # Common Google libs: https://conan.io/center/recipes/abseil
            'plog/1.1.10',          # Synchronized logging library: https://conan.io/center/recipes/plog
            'gtest/1.14.0',         # Testing library: https://conan.io/center/recipes/gtest
            'protobuf/5.27.0',      # Protocol buffers: https://conan.io/center/recipes/protobuf
            'nlohmann_json/3.11.3', # Json support: https://conan.io/center/recipes/nlohmann_json
            'fftw/3.3.10'           # Fast Farrier Transform library: https://conan.io/center/recipes/fftw
        ]:
            # override abseil versions
            if req.startswith('abseil'):
                self.requires(req, override=True)
            else:
                self.requires(req)

    def layout(self):
        cmake_layout(self)
