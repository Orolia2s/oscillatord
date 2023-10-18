import os
from conan import ConanFile
from conan.tools.scm import Git
from conan.tools.files import save, load, copy
from conan.tools.cmake import CMake, CMakeToolchain, CMakeDeps

class UbloxCfgConan(ConanFile):
    name = 'ubloxcfg'
    version = '1.9'
    author = 'Philippe Kehl'
    url = 'https://github.com/phkehl/ubloxcfg.git'
    license = 'GPLv3'
    description = 'u-blox 9 positioning receivers configuration library and tool'
    topics = ('GNSS')

    settings = ('os', 'arch', 'compiler', 'build_type')
    options = {
        'shared': [True, False],
        'fPIC': [True, False]
    }
    default_options = {
        'shared': False,
        'fPIC': True
    }

    exports_sources = 'ff/*', 'ubloxcfg/*', 'README.md', 'config.h.*', 'cmake/*', '3rdparty/stuff/*'


    def source(self):
        Git(self).clone(self.url, '.')

    def configure(self):
        self.settings.rm_safe('compiler.libcxx')
        self.settings.rm_safe('compiler.cppstd')
        if self.options.shared:
            self.options.rm_safe("fPIC")

    def generate(self):
        CMakeToolchain(self).generate()
        CMakeDeps(self).generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure(build_script_folder='cmake')
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()
        for file in ('README.md'):
            copy(self, file, self.source_folder, self.package_folder)
        copy(self, 'LICENSE', os.path.join(self.source_folder, 'ff'), self.package_folder)

    def package_info(self):
        self.cpp_info.libs = ['ubloxcfg']
