import os, sys
import SCons

Decider('MD5-timestamp')

# Add the path to our build utilities to the path.
sys.path = [os.path.abspath(os.path.join('Development', 'SCons'))] + sys.path

import Util

# Configurable build options.
opts = Variables('.options.cache')
opts.AddVariables(
    ('APP',   'Application to build (Test)', 'Test'),
    ('BUILD', 'Build type to perform (Debug, Release)', 'Release'),
)

env = Environment(ENV = os.environ, variables = opts)
opts.Save('.options.cache', env)

helpText = \
    'The following build options can be set on the command line. These will be saved\n' + \
    'for later invocations of SCons, so you do not need to specify them every time:\n' + \
    opts.GenerateHelpText(env)
Help(helpText)

# Set up pretty build output.
if not ARGUMENTS.get('V'):
    def CompileString(msg, var):
        if sys.platform.startswith('win32'):
            return '%8s %s' % (msg, var)
        else:
            return '\033[1;34m%8s\033[0m %s' % (msg, var)
    env['ARCOMSTR']        = CompileString('AR',     '$TARGET')
    env['CCCOMSTR']        = CompileString('CC',     '$SOURCE')
    env['SHCCCOMSTR']      = CompileString('CC',     '$SOURCE')
    env['CXXCOMSTR']       = CompileString('CXX',    '$SOURCE')
    env['SHCXXCOMSTR']     = CompileString('CXX',    '$SOURCE')
    env['LINKCOMSTR']      = CompileString('LINK',   '$TARGET')
    env['SHLINKCOMSTR']    = CompileString('SHLINK', '$TARGET')
    env['RANLIBCOMSTR']    = CompileString('RANLIB', '$TARGET')
    env['GENCOMSTR']       = CompileString('GEN',    '$TARGET')
    env['OBJECTGENCOMSTR'] = CompileString('OBJGEN', '$TARGET')

##################
# Compiler setup #
##################

buildTypes = {
    'Debug': {
        'CPPDEFINES': {
            'ORION_BUILD_DEBUG': 1
        },
    },
    'Release': {
        'CPPDEFINES': {
            'ORION_BUILD_RELEASE': 1,
            'NDEBUG': None
        },
    }
}

if not env['BUILD'] in buildTypes:
    Util.StopError("Invalid build type '%s'." % (env['BUILD']))

env['CPPPATH'] = ['${TARGET.dir}']
env['LIBPATH'] = []
env['LIBS'] = []

if sys.platform.startswith('linux'):
    env['PLATFORM'] = 'Linux'
    env['CPPDEFINES'] = {'ORION_PLATFORM_LINUX': 1}

    platformBuildTypes = {
        'Debug': {
            'CCFLAGS': ['-g'],
            'LINKFLAGS': [],
        },
        'Release': {
            'CCFLAGS': ['-O3', '-g'],
            'LINKFLAGS': [],
        }
    }

    env['CC'] = 'clang'
    env['CXX'] = 'clang++'
    env['CCFLAGS'] += [
        '-Wall', '-Wextra', '-Wno-variadic-macros', '-Wno-unused-parameter',
        '-Wwrite-strings', '-Wmissing-declarations', '-Wredundant-decls',
        '-Wno-format', '-Wno-unused-function', '-Wno-comment',
        '-Wno-unused-private-field',
    ]
    env['CXXFLAGS'] += [
        '-Wsign-promo', '-Wno-undefined-var-template',
        '-std=c++14'
    ]
    env['LINKFLAGS'] += ['-pthread']
    env['LIBS'] += ['dl']
elif sys.platform.startswith('win32'):
    env['PLATFORM'] = 'win32'
    env['CPPDEFINES'] = {'ORION_PLATFORM_WIN32': 1}

    platformBuildTypes = {
        'Debug': {
            'CCFLAGS': ['/Od', '/Z7'],
            'LINKFLAGS': ['/DEBUG'],
        },
        'Release': {
            'CCFLAGS': ['/O2'],
            'LINKFLAGS': ['/DEBUG'],
        }
    }

    env['CCFLAGS'] += ['/W2', '/EHsc', '/MT']
    env['LINKFLAGS'] += ['/SUBSYSTEM:CONSOLE']
else:
    Util.StopError("Unsupported platform.")

env['CCFLAGS'] += platformBuildTypes[env['BUILD']]['CCFLAGS']
env['LINKFLAGS'] += platformBuildTypes[env['BUILD']]['LINKFLAGS']
env['CPPDEFINES'].update(buildTypes[env['BUILD']]['CPPDEFINES'])

########################
# Component management #
########################

class BuildManager:
    def __init__(self, baseEnv):
        self.components = {}
        self.baseEnv = baseEnv

    def AddComponent(self, **kwargs):
        name = kwargs['name']

        depends = kwargs['depends'] if 'depends' in kwargs else []
        if type(depends) != list:
            depends = [depends]

        includePath = kwargs['includePath'] if 'includePath' in kwargs else []
        if type(includePath) != list:
            includePath = [includePath]

        includePath = [x if type(x) == SCons.Node.FS.Dir else self.SourceDir(x) for x in includePath]

        libPath = kwargs['libPath'] if 'libPath' in kwargs else []
        if type(libPath) != list:
            libPath = [libPath]

        libs = kwargs['libs'] if 'libs' in kwargs else []
        if type(libs) != list:
            libs = [libs]

        dlls = kwargs['dlls'] if 'dlls' in kwargs else []
        if type(dlls) != list:
            dlls = [dlls]

        objects = kwargs['objects'] if 'objects' in kwargs else []

        self.components[name] = {
            'depends': depends,
            'includePath': includePath,
            'libPath': libPath,
            'libs': libs,
            'dlls': dlls,
            'objects': objects,
        }

    def CreateEnvironment(self, **kwargs):
        env = self.baseEnv.Clone()

        env['COMPONENT_OBJECTS'] = []
        env['COMPONENT_DLLS'] = []

        def AddDepends(depends):
            for dep in depends:
                if not dep in self.components:
                    Util.StopError("Unknown component '%s'." % (dep))
                component = self.components[dep]
                AddDepends(component['depends'])
                env['CPPPATH'] += component['includePath']
                env['LIBPATH'] += component['libPath']
                env['LIBS'] += component['libs']
                env['COMPONENT_OBJECTS'] += component['objects']
                env['COMPONENT_DLLS'] += component['dlls']

        depends = kwargs['depends'] if 'depends' in kwargs else []
        if type(depends) != list:
            depends = [depends]
        AddDepends(depends)

        return env

    def SourceDir(self, path):
        return Dir(path).srcnode()

manager = BuildManager(env)
Export('manager')

def OrionBaseApplicationMethod(env, dir, **kwargs):
    name = kwargs['name']
    sources = kwargs['sources']
    flags = kwargs['flags'] if 'flags' in kwargs else {}

    target = env.Program(os.path.join(str(dir), name), sources + env['COMPONENT_OBJECTS'], **flags)

    for dll in env['COMPONENT_DLLS']:
        copy = Util.Copy(env, os.path.join(str(dir), os.path.basename(str(dll))), dll)
        Depends(target, copy)

    return target

def OrionApplicationMethod(env, **kwargs):
    return OrionBaseApplicationMethod(env, env['APP_DIR'], **kwargs)

def OrionInternalApplicationMethod(env, **kwargs):
    return OrionBaseApplicationMethod(env, Dir('.'), **kwargs)

env.AddMethod(OrionApplicationMethod, 'OrionApplication')
env.AddMethod(OrionInternalApplicationMethod, 'OrionInternalApplication')

#########################
# External dependencies #
#########################

SConscript(dirs = [os.path.join('Dependencies', env['PLATFORM'])])

##############
# Main build #
##############

SConscript('SConscript', variant_dir = os.path.join('Build', env['BUILD']), duplicate = 0)
