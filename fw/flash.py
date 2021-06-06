#!/usr/bin/python3

# Copyright 2021 Josh Pieper, jjp@pobox.com.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import platform
import subprocess
import sys
import tempfile


BINPREFIX = '' if platform.machine().startswith('arm') else 'arm-none-eabi-'

OBJCOPY = BINPREFIX + 'objcopy'
OPENOCD = 'openocd -f interface/stlink.cfg -f target/stm32g4x.cfg '


def main():
    tmpdir = tempfile.TemporaryDirectory()

    elffile = (sys.argv[1]
               if len(sys.argv) > 1 else
               'bazel-out/stm32g4-opt/bin/fw/power_dist.elf')

    subprocess.check_call(
        f'{OBJCOPY} -Obinary ' +
        f'{elffile} {tmpdir.name}/out.08000000.bin',
        shell=True)

    subprocess.check_call(
        f'{OPENOCD} -c "init" ' +
        f'-c "reset_config none separate; ' +
        f' program {tmpdir.name}/out.08000000.bin verify 0x8000000 ' +
        f' reset exit 0x08000000"',
        shell=True)


if __name__ == '__main__':
    main()
