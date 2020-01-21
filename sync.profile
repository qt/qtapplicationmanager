%modules = ( # path to module name map
    "QtAppManCommon" => "$basedir/src/common-lib",
    "QtAppManCrypto" => "$basedir/src/crypto-lib",
    "QtAppManApplication" => "$basedir/src/application-lib",
    "QtAppManPackage" => "$basedir/src/package-lib",
    "QtAppManNotification" => "$basedir/src/notification-lib",
    "QtAppManManager" => "$basedir/src/manager-lib",
    "QtAppManSharedMain" => "$basedir/src/shared-main-lib",
    "QtAppManMain" => "$basedir/src/main-lib",
    "QtAppManWindow" => "$basedir/src/window-lib",
    "QtAppManLauncher" => "$basedir/src/launcher-lib",
    "QtAppManPluginInterfaces" => "$basedir/src/plugin-interfaces",
    "QtAppManMonitor" => "$basedir/src/monitor-lib",
    "QtAppManDBus" => "$basedir/src/dbus-lib",
    "QtAppManIntentServer" => "$basedir/src/intent-server-lib",
    "QtAppManIntentClient" => "$basedir/src/intent-client-lib",
);
%moduleheaders = ( # restrict the module headers to those found in relative path
);
@allmoduleheadersprivate = ();
%classnames = (
);
%deprecatedheaders = (
);
# Module dependencies.
# Every module that is required to build this module should have one entry.
# Each of the module version specifiers can take one of the following values:
#   - A specific Git revision.
#   - any git symbolic ref resolvable from the module's repository (e.g. "refs/heads/master" to track master branch)
#   - an empty string to use the same branch under test (dependencies will become "refs/heads/master" if we are in the master branch)
#
%dependencies = (
    "qtbase" => "",
    "qtdeclarative" => "",
    "qtwayland" => "",
);
%inject_headers = (
    "$basedir/src/launcher-lib" => [
        "^qwayland-qtam-extension.h",
        "^wayland-qtam-extension-client-protocol.h",
    ],
    "$basedir/src/window-lib" => [
        "^qwayland-server-qtam-extension.h",
        "^wayland-qtam-extension-server-protocol.h"
    ],
);
@private_headers = ( qr/^qwayland-.*\.h/, qr/^wayland-.*-protocol\.h/ );
