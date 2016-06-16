
linux:packagesExist("'libcrypto >= 1.0.1'") {
  PKGCONFIG_INCLUDEPATH = $$system($$pkgConfigExecutable() --cflags-only-I libcrypto)
  PKGCONFIG_INCLUDEPATH ~= s/^-I(.*)/\\1/g
  INCLUDEPATH += $$PKGCONFIG_INCLUDEPATH
}
