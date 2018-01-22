from os import environ

VariantDir('build/src', 'src', duplicate=0)
VariantDir('build/lib', 'lib', duplicate=0)
VariantDir('build/lib/data_contracts/cpp', 'lib', duplicate=0)
flags = [
  '-g',
  '-O3',
  '-march=native',
  '-std=c++14',
  '-I/usr/local/include/google/protobuf/',
  '-L/usr/local/lib',
  '-pthread',
  '-lpthread',
  '-lgrpc',
  '-lgrpc++',
  '-lprotobuf',
]

env = Environment(ENV       = environ,
                  CXX       = 'clang++',
                  CPPFLAGS  = ['-Wno-unused-value'],
                  CXXFLAGS  = flags,
                  LINKFLAGS = flags,
                  CPPPATH   = ['#simpleini', '#lib/include', '#src/include', '#lib/data_contracts/cpp'],
                  LIBS      = [])

env.Program('laines', Glob('build/*/*.cpp') + Glob('build/*/*/*.cpp') + Glob('build/*/*/*/*.cc'))
