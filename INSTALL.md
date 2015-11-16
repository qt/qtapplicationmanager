## How to compile the application-manager and its tools

### Prerequisites
* Qt 5.5 or higher
* On Linux, the system development packages for the following 3rd-party components are recommended:
  * libz (1.2.5 or higher)
  * libyaml (1.6 or higher)
  * openssl (1.0 or higher)
  * libarchive (3.0 or higher)

  On Debian based systems, this will install all the required packages:
  ```
  apt-get install zlib1g-dev libyaml-dev libarchive-dev libssl-dev
  ```
* On Windows and Android, the bundled versions of these libraries can be used instead. Make sure you
  are aware of the licensing implications, since these bundled 3rdparty libs will be linked in as
  static libraries! This option is not meant for production, but for development and testing
  environments only.

### Building
The application-manager is using qmake for its build-system. This means that the basic
installation steps are:
```
qmake && make && make install
```

There are various options that can be applied to the `qmake` step to tailor the build to your needs:

| Option | Description |
| ------ | ----------- |
| `-config bundled-libz`          | Build and link against the bundled version of zlib, instead of using a system version.
| `-config bundled-libcrypto`     | Build and link against the bundled version of openssl, instead of using a system version.
| `-config force-singleprocess`   | Force a single-process build, even if Qt's Wayland `compositor` module is available.
| `-config force-multiprocess`    | Force a multi-process build - this will break if Qt's Wayland `compositor` module is not available.
| `-config enable-tests`          | Enable building of all unit-tests.
| `-config disable-installer`     | Disable the installer part.
| `-config install-prefix=<path>` | Uses `path` as the base directory for `make install`. (default: `/usr/local`)
| `-config hardware-id=<id>`      | Compiles with a hard-coded hardware-id (used for store-signed packages, default: ethernet MAC).
| `-config hardware-id-from-file=<file>` | The hardware-id will be read from the specified file at runtime (used for store-signed packages, default: ethernet MAC).
| `-config systemd-workaround`    | Paramount if you are running systemd and plan on supporting SD-Card installations. Works around systemd interfering with loopback mounts.

All executables (including the unit-tests) will be in the `bin` directory after compiling.

### Coverage

Instead of doing a normal build, you can create a coverage build by running `make coverage`. Using
a build like this enables you to generate HTML coverage reports by executing
```
make check-coverage
```
or
```
make check-branch-coverage
```
in the root build directory. The command-line output will tell you the url to the generated report.
