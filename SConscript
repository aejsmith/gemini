Import('manager')

# Build the engine.
SConscript(dirs = ['Engine'])

# Build applications.
manager.baseEnv['APP_DIR'] = Dir('.')
SConscript(dirs = ['Apps'])
