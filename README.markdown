# MMOServer #

The MMOServer is the flagship project for the [SWG:ANH Team][1]. It is a cross platform massively multiplayer game server intended to emulate the [Star Wars Galaxies][2] Pre-Combat Upgrade experience. The base of the server is written in C++ with some LUA sprinkled in.


## Building/Installing ##

**Pre-Requisits**

*   CMake 2.8 or higher

    [Download][3] the latest version of CMake for your OS.

*   C++0x Compatible Compiler

    Windows: Visual Studio 2013 or higher is required
    Unix: GCC 4.6 or higher is required
    
### Windows Builds ###

To build the server on Windows simply double-click the BuildServer.bat file in the project root. This will download and build all the dependencies and sources and then generate a /bin directory with the server executables.

## Building on Linux <draft>

### Pre-Requisits
please manually install the following deps,

*   Cmake 3.2

*   Gtest

*   Zlib

*   Git

*   Scons

Clone the project form the git hub

    git clone http://gitgub.com/obi-two/unofficial_hope



open a terminal and run the following

    ./build_server.sh
    cd build
    cmake -DCMAKE_INSTALL_PREFIX=/opt/local ..
    make install



You can use the -DCMAKE\_INSTALL\_PREFIX flag to specify a custom output directory for the make install command. 

## Useful Links ##

*   [Bugtracker][4]
*   [Wiki][5]
*   [Forum][6]

  [1]: http://swganh.com/
  [2]: http://starwarsgalaxies.com/
  [3]: http://cmake.org/cmake/resources/software.html
  [4]: http://wiki2.swganh.com/
  [5]: http://wiki.swganh.org/
  [6]: http://www.swganh.com/anh_community/
