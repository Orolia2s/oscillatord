import os
from conan import ConanFile

class PPSToolsConan(ConanFile):
    name = 'pps-tools'
    version = '1.0.3'
    url = 'https://github.com/redlab-i/pps-tools.git'
    license = 'GPLv2.0'
    description = ' User-space tools for LinuxPPS'

    exports_sources = '*.h', 'README.md', 'COPYING'
    no_copy_source = True


    def source(self):
        Git(self).clone(self.url, '.')

    def package(self):
        copy(self, '*.h', self.source_folder,
             os.path.join(self.package_folder, 'include'))
        for file in ('README.md', 'COPYING'):
            copy(self, file, self.source_folder, self.package_folder)

    def package_info(self):
         self.cpp_info.bindirs = []
         self.cpp_info.libdirs = []
