from conan import ConanFile
from conan.tools.cmake import CMake, CMakeDeps, CMakeToolchain, cmake_layout
from conan.tools.files import get, copy
from conan.tools.scm import Version
import os

required_conan_version = ">=2.0"


class PlyEngineConan(ConanFile):
    name = "plyengine"
    description = ("Open-source 2D game engine (C++20, OpenGL 3.3) with "
                   "phone-as-controller multiplayer")
    license = "MIT"
    homepage = "https://github.com/plyndra/plyEngine"
    url = "https://github.com/plyndra/plyEngine"
    topics = ("game-engine", "2d", "ecs", "opengl", "party-game")

    package_type = "static-library"
    settings = "os", "arch", "compiler", "build_type"

    options = {
        "fPIC": [True, False],
    }
    default_options = {
        "fPIC": True,
    }

    def requirements(self):
        # Only the dependencies a consumer must also see. The header-only ones
        # (stb, miniaudio, mdns) are downloaded as sources below and compiled
        # into the engine, so they are not requirements.
        self.requires("sdl/3.4.20")
        self.requires("glm/1.0.3")
        self.requires("nlohmann_json/3.12.0")
        self.requires("crow/1.3.5")

    def layout(self):
        cmake_layout(self, src_folder="src")

    def source(self):
        get(self, **self.conan_data["sources"][self.version], strip_root=True)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.cache_variables["PLY_USE_SYSTEM_DEPS"] = "ON"
        tc.cache_variables["PLY_BUILD_TESTS"] = "OFF"
        tc.cache_variables["PLY_BUILD_EXAMPLES"] = "OFF"
        tc.cache_variables["PLY_BUILD_DEPS_INSTALL"] = "OFF"
        tc.cache_variables["PLY_INSTALL"] = "ON"
        # Header-only deps with no CMake package, staged in source() below.
        tc.cache_variables["PLY_STB_SOURCE_DIR"] = os.path.join(
            self.source_folder, "third_party_src", "stb")
        tc.cache_variables["PLY_MINIAUDIO_SOURCE_DIR"] = os.path.join(
            self.source_folder, "third_party_src", "miniaudio")
        tc.cache_variables["PLY_MDNS_SOURCE_DIR"] = os.path.join(
            self.source_folder, "third_party_src", "mdns")
        tc.generate()
        deps = CMakeDeps(self)
        deps.generate()

    def _stage_headers(self):
        """Fetch the three header-only dependencies into the source tree."""
        stage = os.path.join(self.source_folder, "third_party_src")
        for name, url, sha in (
            ("stb", self.conan_data["stb"]["url"], self.conan_data["stb"]["sha256"]),
            ("miniaudio", self.conan_data["miniaudio"]["url"],
             self.conan_data["miniaudio"]["sha256"]),
            ("mdns", self.conan_data["mdns"]["url"], self.conan_data["mdns"]["sha256"]),
        ):
            get(self, url=url, sha256=sha,
                destination=stage, strip_root=True)

    def build(self):
        self._stage_headers()
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(self, "LICENSE", src=self.source_folder,
             dst=os.path.join(self.package_folder, "licenses"))
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "plyEngine")
        self.cpp_info.set_property("cmake_target_name", "plyEngine::plyEngine")
        self.cpp_info.libs = ["plyEngine", "glad_gl_core_33", "stb_impl",
                              "miniaudio_impl"]
