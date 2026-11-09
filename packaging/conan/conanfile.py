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
        # sdl: Conan Center's reference name (latest 3.4.16 as of 2026-11-09);
        # ranged so new center releases are picked up automatically.
        self.requires("sdl/[>=3.4.16 <4]")
        self.requires("glm/1.0.3")
        self.requires("nlohmann_json/3.12.0")
        # asio: Conan Center HAS a live recipe; Crow's staged CMake then finds
        # it via find_package(asio). Conan Center's old 'crow' recipe is a
        # dead Conan-1.x artifact (crow/0.1, requires boost 1.69) with no
        # CrowCpp 1.x release, so Crow itself is staged in full instead.
        self.requires("asio/[>=1.36 <2]")

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
        # Full Crow checkout (see _stage_headers); the engine add_subdictories it
        # and Crow's own CMake resolves asio::asio from the conan-generated
        # package above.
        tc.cache_variables["PLY_CROW_SOURCE_DIR"] = os.path.join(
            self.source_folder, "third_party_src", "crow")
        tc.generate()
        deps = CMakeDeps(self)
        deps.generate()

    def _stage_headers(self):
        """Fetch the three header-only dependencies + a full Crow checkout.

        crow is staged complete (not header-only) because the engine's
        system-deps mode consumes it via add_subdirectory(PLY_CROW_SOURCE_DIR)
        when no Crow CMake config package exists.
        """
        stage = os.path.join(self.source_folder, "third_party_src")
        for name, url, sha in (
            ("stb", self.conan_data["stb"]["url"], self.conan_data["stb"]["sha256"]),
            ("miniaudio", self.conan_data["miniaudio"]["url"],
             self.conan_data["miniaudio"]["sha256"]),
            ("mdns", self.conan_data["mdns"]["url"], self.conan_data["mdns"]["sha256"]),
            ("crow", self.conan_data["crow"]["url"], self.conan_data["crow"]["sha256"]),
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
