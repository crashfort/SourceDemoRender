This project uses vcpkg for external dependencies. These are the dependencies used:
* concurrentqueue:x86-windows
* fmt:x86-windows
* minhook:x86-windows
* nlohmann-json:x86-windows
* stb:x86-windows
* catch2:x86-windows
* restclient-cpp:x86-windows

ffmpeg should be in this list too, but at this time there are issues in vcpkg which prevents it from being able to build. Therefore it has to be installed manually for now https://ffmpeg.zeranoe.com/builds/.
