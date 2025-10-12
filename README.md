# `ln`

`ln` is a personal collection of unique software and third-party libraries made
to work nicely together for FreeRTOS embedded systems development.

## Environment

```shell
sudo apt update && sudo apt upgrade -y
sudo apt install clang-format cmake
ARM_TOOLCHAIN_VERSION=$(curl -s https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads | grep -Po '<h4>Version \K.+(?=</h4>)')
curl -Lo gcc-arm-none-eabi.tar.xz "https://developer.arm.com/-/media/Files/downloads/gnu/${ARM_TOOLCHAIN_VERSION}/binrel/arm-gnu-toolchain-${ARM_TOOLCHAIN_VERSION}-x86_64-arm-none-eabi.tar.xz"
sudo mkdir /opt/gcc-arm-none-eabi
sudo tar xf gcc-arm-none-eabi.tar.xz --strip-components=1 -C /opt/gcc-arm-none-eabi
echo 'export PATH=$PATH:/opt/gcc-arm-none-eabi/bin' | sudo tee -a /etc/profile.d/gcc-arm-none-eabi.sh
source /etc/profile
arm-none-eabi-gcc --version
arm-none-eabi-g++ --version
arm-none-eabi-gdb --version
rm -rf gcc-arm-none-eabi.tar.xz
```

## Format

### Casing

The naming scheme mostly follows the [Rust Naming](https://rust-lang.github.io/api-guidelines/naming.html#casing-conforms-to-rfc-430-c-case).

Here are some important conventions:

- modular feature folder name and C++ namespace has to be the same and in
  snake_case. For example, the [`ln/drivers`](ln/drivers) folder contains
  features that are in the `ln::drivers` namespace.

## TODO

- shell backspace does not work.
- shell `repeat` command does not work for something like `repeat 1000 echo
  labas`. Also, `repeat 1000 repeat 1000 echo labas` should technically work.
- Consider switching to
  [jonenz/FreeRTOS-Cpp](https://github.com/jonenz/FreeRTOS-Cpp) instead of
  [freertos-addons](https://github.com/michaelbecker/freertos-addons). It is
  newer and more actively maintained, comes with CMake support, more modern C++,
  more familiar and arguably better project structure, testing and CI.
