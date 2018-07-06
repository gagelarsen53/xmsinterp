import os
from conans import ConanFile, CMake
from conans.errors import ConanException


class XmsinterpConan(ConanFile):
    name = "xmsinterp"
    version = None
    license = "XMSNG Software License"
    url = "https://github.com/Aquaveo/xmsinterp"
    description = "Interpolation library for XMS products"
    settings = "os", "compiler", "build_type", "arch"
    options = {"xms": [True, False], "pybind": [True, False]}
    default_options = "xms=False", "pybind=False"
    generators = "cmake"
    build_requires = "cxxtest/4.4@aquaveo/stable"
    exports = "CMakeLists.txt", "LICENSE"
    exports_sources = "xmsinterp/*"

    def configure(self):
        # Set verion dynamically using XMS_VERSION env variable.
        self.version = self.env.get('XMS_VERSION', 'master')

        # Raise ConanExceptions for Unsupported Versions
        s_os = self.settings.os
        s_compiler = self.settings.compiler
        s_compiler_version = self.settings.compiler.version

        self.options['xmscore'].xms = self.options.xms
        self.options['xmscore'].pybind = self.options.pybind

        if s_compiler != "Visual Studio" and s_compiler != "apple-clang":
            self.options['boost'].fPIC = True

        if s_compiler == "apple-clang" and s_os == 'Linux':
            raise ConanException("Clang on Linux is not supported.")

        if s_compiler == "apple-clang" \
                and s_os == 'Macos' \
                and s_compiler_version < "9.0":
            raise ConanException("Clang > 9.0 is required for Mac.")

    def requirements(self):
        if self.options.xms and self.settings.compiler.version == "12":
            self.requires("boost/1.60.0@aquaveo/testing")
            self.requires("xmscore/1.0.26@aquaveo/stable")
        else:
            self.requires("boost/1.66.0@conan/stable")
            self.requires("xmscore/1.0.26@aquaveo/stable")
        # Pybind if not Visual studio 2013
        if not (self.settings.compiler == 'Visual Studio' \
                and self.settings.compiler.version == "12") \
                and self.options.pybind:
            self.requires("pybind11/2.2.2@aquaveo/stable")

    def build(self):
        xms_run_tests = self.env.get('XMS_RUN_TESTS', None)
        run_tests = xms_run_tests != 'None' and xms_run_tests is not None

        cmake = CMake(self)

        cmake.definitions["IS_PYTHON_BUILD"] = self.options.pybind

        if self.settings.compiler == 'Visual Studio' \
           and self.settings.compiler.version == "12":
            cmake.definitions["XMS_BUILD"] = self.options.xms

        # CXXTest doesn't play nice with PyBind. Also, it would be nice to not
        # have tests in release code. Thus, if we want to run tests, we will
        # build a test version (without python), run the tests, and then (on
        # sucess) rebuild the library without tests.
        if run_tests:
            cmake.definitions["IS_PYTHON_BUILD"] = False
            cmake.definitions["BUILD_TESTING"] = run_tests
            cmake.configure(source_folder=".")
            cmake.build()

            print("***********(0.0)*************")
            try:
                cmake.test()
            except ConanException:
                raise
            finally:
                if os.path.isfile("TEST-cxxtest.xml"):
                    with open("TEST-cxxtest.xml", "r") as f:
                        for line in f.readlines():
                            no_newline = line.strip('\n')
                            print(no_newline)
                print("***********(0.0)*************")

        # Make sure we build without tests
        cmake.definitions["IS_PYTHON_BUILD"] = self.options.pybind
        cmake.definitions["BUILD_TESTING"] = False
        cmake.configure(source_folder=".")
        cmake.build()


    def package(self):
        self.copy("*.h", dst="include/xmsinterp", src="xmsinterp")
        self.copy("*.lib", dst="lib", keep_path=False)
        self.copy("*.exp", dst="lib", keep_path=False)
        self.copy("*.pyd", dst="lib", keep_path=False)
        self.copy("*.dll", dst="bin", keep_path=False)
        self.copy("*.dylib*", dst="lib", keep_path=False)
        self.copy("*.so", dst="lib", keep_path=False)
        self.copy("*.a", dst="lib", keep_path=False)
        self.copy("license", dst="licenses", ignore_case=True, keep_path=False)

    def package_info(self):
        if self.settings.build_type == 'Debug':
            self.cpp_info.libs = ["xmsinterp_d"]
        else:
            self.cpp_info.libs = ["xmsinterp"]