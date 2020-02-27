# Copyright 2019-present PlatformIO <contact@platformio.org>
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import json
import os
import sys

from SCons.Script import DefaultEnvironment

from platformio import util
from platformio.proc import exec_command
from platformio.util import get_systype

Import("env")


print('Running freertos build script')
program = env.Program(os.path.join('$BUILD_DIR', env.subst('$PROGNAME')),
                      os.path.join('$PROJECT_DIR', 'src', 'main.c'))
env.Replace(PIOMAINPROG=program)
env.Replace(BuildProgram=program)
env.Exit(0)
