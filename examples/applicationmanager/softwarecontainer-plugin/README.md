# SoftwareContainer Plugin

This is a very basic PoC integration of Pelagicore's Software-Containers

[https://github.com/Pelagicore/softwarecontainer][]

Please also read the "Containers" page in the official Qt Application Manager
documentation:
[https://doc-snapshots.qt.io/qtapplicationmanager/containers.html]

Parts of the container configuration are hardcoded in `softwarecontainer.cpp`,
while all of the capability definition is in the JSON manifest at
`service-manifest.d/io.qt.ApplicationManager.Application/`.

The Wayland/OpenGL pass-through is tested against Intel GPUs and VMware's
virtual GPU.

The softwarecontainer-agent needs to be started as root. By default it will
register itself on the system D-Bus, so a policy file must be in place,
allowing it to register itself on the system-bus. If you want to run the
agent on the session bus instead (via the `--session-bus` parameter), you
have to add the following to one of your config.yaml files:
```
containers:
  softwarecontainer:
    dbus: 'session'
```

Passing the service-manifest directory that comes with the plugin via
`-m` is mandatory - otherwise the container setup will fail due to the
missing `io.qt.ApplicationManager.Application` capability.

You have to make sure that the agent has access to the same session-bus
that the application-manager is using, if you intend on forwarding this
bus. This is a bit tricky if the agent is run as root and the application-
manager as non-root user, since the default session-bus policy in most
distros disallows root to access user session-busses: the workaround is to
add a `<allow user="root"/>` policy within the `<policy context="default">`
element in `/etc/dbus-1/session.conf`.

Please do also not forget to tell the agent about your environment, when
running it via sudo:
```
sudo XDG_RUNTIME_DIR=$XDG_RUNTIME_DIR
DBUS_SESSION_BUS_ADDRESS=$DBUS_SESSION_BUS_ADDRESS softwarecontainer-agent -m
/path/to/application-manager/examples/softwarecontainer-plugin/service-manifest.d/
```

On the AM side, you need to activate the plugin by adding something like
this to one of your config.yaml files:
```
plugins:
   container: [ "/path/to/libsoftwarecontainer-plugin.so" ]
```
In order to actually run apps within softwarecontainers, you have to add a
container selection configuration:
[https://doc-snapshots.qt.io/qtapplicationmanager/containers.html#container-selection-configuration][]


Please be aware that for easier development on the desktop, you normally want
your $HOME directory mounted into the container in read-only mode, so you do
not have to install your application-manager into /usr/ after every build
(given that your build directory is somewhere in $HOME, the container would
not see the appman-launcher-qml binary).
This is *not* done by default, but you can activate this behavior by adding
this to one of your config.yaml files:

```
containers:
  softwarecontainer:
    bindMountHome: yes
```
