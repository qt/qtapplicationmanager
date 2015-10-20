# generate code coverage information using gcov/lcov/genhtml

CONFIG(debug,debug|release) {
    equals(TEMPLATE, "subdirs") {
        sub-coverage.target = coverage
        sub-coverage.CONFIG = recursive
        sub-check-coverage.target = check-coverage
        sub-check-coverage.CONFIG = recursive
        sub-check-branch-coverage.target = check-branch-coverage
        sub-check-branch-coverage.CONFIG = recursive
        QMAKE_EXTRA_TARGETS += sub-coverage sub-check-coverage sub-check-branch-coverage
    } else {
        coverage_pre.commands += @echo && echo "Building with coverage support..." && echo $(eval CXXFLAGS += -O0 -fprofile-arcs -ftest-coverage)$(eval LFLAGS += -O0 -fprofile-arcs -ftest-coverage)

        coverage.CONFIG += recursive
        coverage.commands = @echo && echo "Finished building with coverage support." && echo
        build_pass|!debug_and_release:coverage.depends = coverage_pre all

        equals(TEMPLATE, "lib") {
            CLEAR_AND_RUN="true"
        } else {
            CLEAR_AND_RUN="lcov -z -d . -b . && $$COVERAGE_RUNTIME ./$(TARGET)"
        }

        check-coverage.commands = @echo && echo "Checking coverage..." && $$CLEAR_AND_RUN && lcov -c -d . -b . -o $(TARGET).gcov-info
        check-coverage.depends = coverage

        check-branch-coverage.commands = @echo && echo "Checking branch coverage..." && $$CLEAR_AND_RUN && lcov -c -d . -b . -o $(TARGET).gcov-info --rc lcov_branch_coverage=1
        check-branch-coverage.depends = coverage

        QMAKE_CLEAN += $(OBJECTS_DIR)/*.gcda $(OBJECTS_DIR)/*.gcno $(TARGET).gcov-info
        QMAKE_EXTRA_TARGETS *= coverage_pre coverage check-coverage check-branch-coverage
    }
}
