
qt_standard_project_setup(REQUIRES 6.8)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/mount-test-data DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

qt_am_internal_add_qml_test(tst_bubblewrap
    CONFIG_YAML am-config.yaml
    EXTRA_FILES
        apps/TestApp/info.yaml
        apps/TestApp/info.yaml
        apps/TestApp/app.qml
        apps/TestAppWithCap/info.yaml
        apps/TestAppWithCap/icon.png
        apps/TestAppWithCap/app.qml
        netscript.sh
        good.env
        bad.env
    TEST_FILE tst_bubblewrap.qml
    CONFIGURATIONS
        CONFIG NAME multi-process CONDITION QT_FEATURE_am_multi_process ARGS --force-multi-process
)

# cgroup handling ('createControlGroupPerProcess') needs its own config, so it gets its own test target
# (it fakes /sys/fs/cgroup via the CGroupTestHelper module, so it does not require real privileges).
qt_am_internal_add_qml_test(tst_bubblewrap_cgroups
    CONFIG_YAML am-config-cgroups.yaml
    EXTRA_FILES
        apps/TestApp/info.yaml
        apps/TestApp/icon.png
        apps/TestApp/app.qml
    TEST_FILE tst_bubblewrap_cgroups.qml
    CONFIGURATIONS
        CONFIG NAME multi-process CONDITION QT_FEATURE_am_multi_process ARGS --force-multi-process
)

add_subdirectory(apps/helper)
add_subdirectory(cgroup-helper)
