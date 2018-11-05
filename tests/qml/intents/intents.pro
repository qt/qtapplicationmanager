AM_CONFIG = am-config.yaml
TEST_FILES = tst_intents.qml

APPS = intents1 intents2 cannot-start
for (app, APPS): OTHER_FILES += apps/$${app}/*.yaml apps/$${app}/*.qml  apps/$${app}/*.png

load(am-qml-testcase)
