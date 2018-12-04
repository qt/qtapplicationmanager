TEMPLATE = aux

OTHER_FILES += \
    *.prf \

#All the feature files we want to install and provide to other modules or to the examples
features.files += \
    am-systemui.prf \
    am-app.prf \
    am-package.prf
features.path = $$[QT_HOST_DATA]/mkspecs/features

INSTALLS += features

# Ensure files are copied to qtbase mkspecs for non-prefixed builds
!force_independent:if(!debug_and_release|!build_all|CONFIG(release, debug|release)) {
    defineReplace(stripSrcDir) {
        return($$relative_path($$1, $$_PRO_FILE_PWD_))
    }
    prffiles2build.input = features.files
    prffiles2build.output = $$[QT_HOST_DATA]/mkspecs/features/${QMAKE_FUNC_FILE_IN_stripSrcDir}
    prffiles2build.commands = $$QMAKE_COPY ${QMAKE_FILE_IN} ${QMAKE_FILE_OUT}
    prffiles2build.name = COPY ${QMAKE_FILE_IN}
    prffiles2build.CONFIG = no_link target_predeps
    QMAKE_EXTRA_COMPILERS += prffiles2build
}
