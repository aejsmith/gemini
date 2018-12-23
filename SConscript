Import('manager')

# Build the engine.
SConscript(dirs = ['Engine'])

# Build games.
manager.baseEnv['GAME_DIR'] = Dir('.')
SConscript(dirs = ['Games'])
