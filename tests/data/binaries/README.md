## Generation

Here's how to (re)generate the test binaries in this directory:

- `android-elf.so`:

  This is a copy of `Qt/6.7.0/android_armv7/lib/libQt6QuickParticles_armeabi-v7a.so`

- `windows-pe.exe`:

  This is a minimal C program, compiled with cmake + MSVC:
  - `main.c`:
    ```
    int __stdcall WinMainCRTStartup() { return 0; }
    ```

  - `CMakeLists.txt`:
    ```
    cmake_minimum_required(VERSION 3.5)
    project(windows-pe LANGUAGES C)
    add_executable(windows-pe WIN32 main.c)
    set_target_properties(windows-pe PROPERTIES LINK_FLAGS "/NODEFAULTLIB")
    ```

- `macos-macho` and `macos-macho-universal`:

  This is a minimal C program, compiled on the command line:
  - `main.c`:
    ```
    int main() { return 0; }
    ```

  - Compile with:
    ```
    clang main.c -o macos-macho
    clang main.c -arch x86_64 -arch arm64 -o macos-macho-universal
    ```


## Compression

The .zz files here are in Qt's `qCompress` format.

- compress `${file}` on the command line:
  ```
  $ printf "0: %08x" `stat -c "%s" $file` | xxd -r > $file.zz ; pigz -z <$file >>$file.zz
  ```

- decompress `${file}.zz` on the command line:
  ```
  $ dd if=${file}.zz bs=4 skip=1 | pigz -d >${file}
  ```
