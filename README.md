Description

- building blocks for a program to query Minecraft servers
- using C++ and the boost::asio library

---

Goals

- simple interface, simple usage examples
- single-threaded base, but possible to be used in multi-threaded enviroments
- imperative and object-oriented version

---

Protocal support

1. the simple TCP protocol (used in the minecraft client when listing the servers) 
2. the basic stat of the [UDP/UT3/gamespot protocol](http://wiki.vg/Query)
3. the full stat of this protocol
4. the [ForgeEssentials snooper](http://github.com/ForgeEssentials/ForgeEssentialsMain/wiki/Snooper-Info)

The first priority is to support the basic UDP protocol and provide documentation with it
